// ============================================================
//  Phase 157.0: faults injected into the emulators, and the app's
//  safe answer to each (RADUK_EMU_FAULT, src/core/EmuFault.h).
//
//    fault_injection               every group (CI)
//    fault_injection eaf-stall     the focuser stops mid-sweep: autofocus
//                                  ends, says why, and drives no further
//    fault_injection camera-gone   the camera dies mid-recording: the run
//                                  stops, the frames so far are on disk
//                                  with their record, and the reason is said
//    fault_injection camera-blip   a short drop: the run rides it out
//    fault_injection slew-stuck    a GoTo never arrives: the mount keeps
//                                  saying so (the GoTo panel's timeout is
//                                  157.0b; the mosaic and satellite paths
//                                  already time out)
//
//  Real time, a minute or so a group.
// ============================================================

#define RADUK_EMULATOR 1
#include "app/AppCore.h"
#include "core/CaptureThread.cpp"
#include "core/FrameAnalyzer.cpp"
#include "core/CentroidTracker.cpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

using namespace raduk::app;
using namespace raduk::core;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    fflush(stdout);
    if (!ok) g_fail++;
}
static void SetFault(const char* v) {
    std::string e = std::string("RADUK_EMU_FAULT=") + (v ? v : "");
    _putenv(e.c_str());
}
static double Since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static void GroupEafStall() {
    printf("eaf-stall: the focuser stops mid-sweep\n");
    SetFault("eaf-stall@2");
    EAFFocuser f;
    Check(f.Open(0), "the emulated focuser opens, with a stall at 2 s");
    SetFault(nullptr);
    AutofocusRoutine af;
    af.SetFocuser(&f);
    AutofocusParams p; p.settleMs = 50; p.fineStage = false;
    af.SetParams(p);
    RecordingUiSink ui;
    af.SetLogger([&ui](const char* s) { ui.Line(s); printf("%s", s); });
    std::atomic<bool> done{false};
    AFResult res{};
    af.SetOnComplete([&](AFResult r) { res = r; done.store(true); });
    std::atomic<bool> feed{true};
    std::thread feeder([&] {
        while (feed.load()) {   // a plausible score, whatever the focus
            af.SubmitSharpness(50.0f + (float)(f.GetPosition() % 7), true, 120);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });
    const auto t0 = std::chrono::steady_clock::now();
    af.Start(f.GetPosition(), false);
    while (!done.load() && Since(t0) < 60.0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    feed.store(false); feeder.join();
    Check(done.load(), "autofocus ends instead of waiting for ever", std::to_string((int)Since(t0)) + " s");
    Check(!res.success, "and reports a failure", res.message);
    Check(ui.Saw("EAF stalled") || ui.Saw("timed out") || res.message.find("stall") != std::string::npos ||
          res.message.find("did not complete") != std::string::npos, "naming the stall", res.message);
    Check(!af.IsRunning(), "and drives the focuser no further");
    f.Close();
}

struct Pipeline {
    AppCore core;
    RecordingUiSink ui;
    fs::path dir;
    std::atomic<bool> complete{false};
    std::string path;
    std::vector<std::pair<ToastLevel, std::string>> toasts;
    std::mutex mu;

    bool Start() {
        core.SetUi(&ui);
        if (!core.cam.Open(0)) return false;
        core.cam.SetTargetName("Jupiter");
        core.cam.SetRegion(640, 480, 1, ImageType::Raw8);
        core.cam.SetExposure(8.0);
        if (!core.ring.Allocate(DEFAULT_RING_SIZE, FrameSizeBytes(640, 480, ImageType::Raw8))) return false;
        if (!core.capture.Start(core.cam, core.ring, 8.0)) return false;
        core.analyzer.Start();
        dir = fs::temp_directory_path() / "raduk_fault_injection";
        std::error_code ec; fs::remove_all(dir, ec); fs::create_directories(dir, ec);
        CaptureConfig cfg;
        cfg.outputDir = dir.string(); cfg.filePrefix = "Jupiter";
        cfg.limitType = LimitType::Time; cfg.limitTimeSec = 600.0;   // never reached here
        cfg.format = CaptureFormat::SER;
        core.capturer.SetConfig(cfg);
        RecordingHooks h;
        h.notify = [this](ToastLevel l, const std::string& s) { std::lock_guard<std::mutex> lk(mu); toasts.push_back({l, s}); };
        core.recCtl.SetHooks(h);
        core.capturer.SetOnComplete([this](const std::string& p, bool) { path = p; complete.store(true); });
        core.capture.SetFrameSink([this](const FrameSlot* s) { core.capturer.SubmitFromCaptureThread(s); });
        return core.recCtl.Begin(640, 480, 8, false);
    }
    // The render loop's part, for `seconds` or until the run ends.
    void Loop(double seconds) {
        const auto t0 = std::chrono::steady_clock::now();
        while (Since(t0) < seconds && core.capturer.IsRecording()) {
            if (FrameSlot* f = core.ring.GetLatestFrame()) core.ring.ReleaseRead(f);
            const double fps = core.capture.Stats().fps.load();
            core.recCtl.Service(false, 0.0f, RecordingFrameView{},
                                core.capture.Stats().lastFrameTimestamp.load(),
                                fps > 0.01 ? 1.0 / fps : 0.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    void Stop() {
        core.capture.SetFrameSink(nullptr);
        if (core.capturer.IsRecording()) core.capturer.StopRecording(false);
        core.analyzer.Stop();
        core.capture.Stop();
        core.cam.Close();
        std::error_code ec; fs::remove_all(dir, ec);
    }
};

static void GroupCameraGone() {
    printf("camera-gone: the camera dies mid-recording\n");
    SetFault("camera-drop@4");
    Pipeline p;
    Check(p.Start(), "a time-limited run starts; the camera will vanish at 4 s");
    SetFault(nullptr);
    const auto t0 = std::chrono::steady_clock::now();
    p.Loop(60.0);
    const double took = Since(t0);
    Check(!p.core.capturer.IsRecording(), "the run stops instead of staying open for ever",
          std::to_string((int)took) + " s");
    Check(took > 15.0 && took < 35.0, "after the silence limit, not before");
    Check(p.ui.Saw("stopped (the camera stopped sending frames)"), "the log says why");
    bool toast = false;
    { std::lock_guard<std::mutex> lk(p.mu); for (auto& t : p.toasts) toast |= t.first == ToastLevel::Error && t.second.find("no frames from the camera") != std::string::npos; }
    Check(toast, "and the bar says so");
    for (int i = 0; i < 100 && !p.complete.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::error_code ec;
    Check(p.core.capturer.FramesRecorded() > 0 && !p.path.empty() && fs::exists(p.path, ec),
          "the frames so far are on disk", std::to_string(p.core.capturer.FramesRecorded()) + " frames");
    Check(!p.path.empty() && fs::exists(ObservationPathFor(p.path), ec), "with their observation record");
    p.Stop();
}

static void GroupCameraBlip() {
    printf("camera-blip: a short drop is ridden out\n");
    SetFault("camera-drop@3+3");
    Pipeline p;
    Check(p.Start(), "a run starts; the camera drops at 3 s for 3 s");
    SetFault(nullptr);
    p.Loop(12.0);
    Check(p.core.capturer.IsRecording(), "the run is still going after the drop");
    const uint32_t before = p.core.capturer.FramesRecorded();
    p.Loop(2.0);
    Check(p.core.capturer.FramesRecorded() > before, "and frames are flowing again",
          std::to_string(before) + " -> " + std::to_string(p.core.capturer.FramesRecorded()));
    p.core.recCtl.End(true, "test done");
    p.Stop();
}

static void GroupSlewStuck() {
    printf("slew-stuck: a GoTo never arrives\n");
    SetFault("slew-stuck@0");
    MountEmulator m;
    Check(m.Connect("COM-EMU"), "the emulated mount connects, with GoTos stuck");
    SetFault(nullptr);
    double ra = 0, dec = 0; m.GetRADec(ra, dec);
    m.GoToRADec(ra + 1.0, dec + 5.0);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    Check(m.IsSlewing(), "the mount still says it is slewing after 4 s, going nowhere");
    m.CancelGoTo();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Check(!m.IsSlewing(), "and a cancel ends it");
    m.Disconnect();
}

int main(int argc, char** argv) {
    _putenv("RADUK_EMU_NEXSTAR=1");
    const std::string g = argc > 1 ? argv[1] : "all";
    printf("Fault injection (phase 157.0): %s\n", g.c_str());
    bool ran = false;
    if (g == "all" || g == "eaf-stall")   { GroupEafStall();   ran = true; }
    if (g == "all" || g == "camera-gone") { GroupCameraGone(); ran = true; }
    if (g == "all" || g == "camera-blip") { GroupCameraBlip(); ran = true; }
    if (g == "all" || g == "slew-stuck")  { GroupSlewStuck();  ran = true; }
    if (!ran) { printf("unknown group '%s'\n", g.c_str()); return 2; }
    if (g_fail) { printf("fault_injection: %d FAILED\n", g_fail); return 1; }
    printf("fault_injection: all passed\n");
    return 0;
}
