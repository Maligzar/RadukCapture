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
//                                  saying so, and the GoTo panel's watch
//                                  (157.0b) calls it a fault past its limit
//    fault_injection disk-slow-fits  157.0d: the disk slows under a bounded
//                                  run the buffer holds: no frame is lost
//    fault_injection disk-slow-overflow  157.0d: the buffer fills: every
//                                  frame lost is counted and said while it
//                                  happens, and the rest reach the file
//    fault_injection quit-mid-run  157.0d: the app closes while recording:
//                                  the SER counts its frames, the record is
//                                  written, nothing is stacked
//
//  Real time, a minute or so a group.
// ============================================================

#define RADUK_EMULATOR 1
#include "app/AppCore.h"
#include "core/CaptureThread.cpp"
#include "core/FrameAnalyzer.cpp"
#include "core/CentroidTracker.cpp"
#include "core/SlewWatch.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::atomic<bool> completeNormal{true};
    std::atomic<uint32_t> accepted{0};   // frames the manager took
    std::string path;
    std::vector<std::pair<ToastLevel, std::string>> toasts;
    std::mutex mu;

    bool Start(LimitType lt = LimitType::Time, uint32_t limitFrames = 0, int bufferCap = 0) {
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
        cfg.limitType = lt; cfg.limitTimeSec = 600.0;   // never reached here
        cfg.limitFrames = limitFrames;
        cfg.writeBufferCap = bufferCap;
        cfg.format = CaptureFormat::SER;
        core.capturer.SetConfig(cfg);
        RecordingHooks h;
        h.notify = [this](ToastLevel l, const std::string& s) { std::lock_guard<std::mutex> lk(mu); toasts.push_back({l, s}); };
        core.recCtl.SetHooks(h);
        core.capturer.SetOnComplete([this](const std::string& p, bool n) {
            path = p; completeNormal.store(n); complete.store(true);
        });
        core.capture.SetFrameSink([this](const FrameSlot* s) {
            if (core.capturer.SubmitFromCaptureThread(s)) accepted.fetch_add(1);
        });
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
        core.capturer.ClearSlowWrite();
        std::error_code ec; fs::remove_all(dir, ec);
    }
    void WaitComplete() {
        for (int i = 0; i < 600 && !complete.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    bool Toasted(ToastLevel l, const char* text) {
        std::lock_guard<std::mutex> lk(mu);
        for (auto& t : toasts) if (t.first == l && t.second.find(text) != std::string::npos) return true;
        return false;
    }
};

// A SER read from the specification, not from SERWriter: the header is 178
// bytes, "LUCAM-RECORDER" first, width, height and pixel depth the int32s
// at 26, 30 and 34, FrameCount at 38. Its frame count, and whether the
// file is exactly that many frames, with or without the timestamp trailer.
struct SerCheck { bool ok = false; int frames = -1; bool sizeMatches = false; };
static SerCheck ReadSer(const std::string& path) {
    SerCheck c;
    FILE* f = nullptr;
    if (path.empty() || fopen_s(&f, path.c_str(), "rb") != 0 || !f) return c;
    unsigned char h[178] = {};
    const size_t got = fread(h, 1, sizeof(h), f);
    fclose(f);
    if (got != sizeof(h) || memcmp(h, "LUCAM-RECORDER", 14) != 0) return c;
    auto i32 = [&](int o) {
        return (int)((unsigned)h[o] | ((unsigned)h[o + 1] << 8) | ((unsigned)h[o + 2] << 16) | ((unsigned)h[o + 3] << 24));
    };
    const int w = i32(26), hh = i32(30), depth = i32(34);
    c.frames = i32(38);
    std::error_code ec;
    const uint64_t size = (uint64_t)fs::file_size(path, ec);
    const uint64_t frameBytes = (uint64_t)w * hh * (depth > 8 ? 2 : 1);
    const uint64_t body = 178 + (uint64_t)c.frames * frameBytes;
    c.sizeMatches = !ec && (size == body || size == body + 8ull * (uint64_t)c.frames);
    c.ok = true;
    return c;
}

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
    // The emulator's handle now dies on an unplug, as the SDK's does, so
    // frames can only be flowing again because the camera was re-opened.
    Check(p.core.capture.Stats().reconnects.load() > 0,
          "because the camera was closed and opened again",
          std::to_string(p.core.capture.Stats().reconnects.load()) + " re-open(s)");
    {
        int w = 0, h = 0, b = 0; ImageType t{};
        p.core.cam.GetRegion(w, h, b, t);
        Check(w == 640 && h == 480 && t == ImageType::Raw8,
              "with the format it had, not the sensor default",
              std::to_string(w) + "x" + std::to_string(h));
    }
    p.core.recCtl.End(true, "test done");
    p.Stop();
}

// Phase 157.0d. A bounded run the buffer holds outright: the disk is on
// no frame's critical path, so slowing it costs drain time and nothing else.
static void GroupDiskSlowFits() {
    printf("disk-slow-fits: the disk slows under a run the buffer holds\n");
    SetFault("disk-slow@0");
    Pipeline p;
    p.core.ApplyEmuFaults();   // what main.cpp does at startup
    SetFault(nullptr);
    Check(p.Start(LimitType::Frames, 60), "a 60-frame run starts; every write takes 200 ms");
    const auto t0 = std::chrono::steady_clock::now();
    p.Loop(120.0);
    p.WaitComplete();
    Check(!p.core.capturer.IsRecording() && p.complete.load(), "it ends at its limit",
          std::to_string((int)Since(t0)) + " s, drain included");
    Check(p.core.capturer.FramesDropped() == 0, "no frame was lost to the slow disk",
          std::to_string(p.core.capturer.FramesDropped()) + " dropped");
    Check(p.core.capturer.FramesRecorded() == 60, "all 60 reached the file",
          std::to_string(p.core.capturer.FramesRecorded()));
    const SerCheck c = ReadSer(p.path);
    Check(c.ok && c.frames == 60 && c.sizeMatches, "and the SER says so, read from the spec",
          std::to_string(c.frames) + " in the header");
    Check(!p.Toasted(ToastLevel::Warn, "disk is not keeping up"), "with nothing to warn about");
    p.Stop();
}

// Phase 157.0d. An unbounded run whose buffer fills (capped at 48 slots so
// it fills in seconds): the run goes on, and nothing is lost silently.
static void GroupDiskSlowOverflow() {
    printf("disk-slow-overflow: the write buffer fills\n");
    SetFault("disk-slow@1+8");
    Pipeline p;
    p.core.ApplyEmuFaults();
    SetFault(nullptr);
    Check(p.Start(LimitType::Time, 0, 48), "a run starts with 48 slots; the disk slows from 1 s to 9 s");
    p.Loop(14.0);
    const uint32_t dropped = p.core.capturer.FramesDropped();
    Check(dropped > 0, "the buffer overflowed", std::to_string(dropped) + " frames dropped");
    Check(p.core.capturer.IsRecording(), "and the run goes on, past the slow spell");
    Check(p.Toasted(ToastLevel::Warn, "disk is not keeping up"), "the bar said so while it happened");
    Check(p.ui.Saw("write buffer full"), "and the log");
    p.core.recCtl.End(true, "test done");
    p.WaitComplete();
    const DropReport dr = p.core.capturer.LastDropReport();
    Check(dr.droppedBuffer == p.core.capturer.FramesDropped(), "the run's ledger counts every overrun",
          std::to_string(dr.droppedBuffer) + " in the ledger");
    Check(p.core.capturer.FramesRecorded() == p.accepted.load(), "every frame the buffer took reached the file",
          std::to_string(p.core.capturer.FramesRecorded()) + " of " + std::to_string(p.accepted.load()));
    Check(dr.unexplained == 0, "the timestamps show no gap the ledger cannot explain",
          std::to_string(dr.gaps) + " gaps, " + std::to_string(dr.unexplained) + " unexplained");
    const SerCheck c = ReadSer(p.path);
    Check(c.ok && c.frames == (int)p.core.capturer.FramesRecorded() && c.sizeMatches,
          "the SER holds exactly the frames written", std::to_string(c.frames) + " in the header");
    p.Stop();
}

// Phase 157.0d. Quitting while recording, through the code main.cpp's
// shutdown runs (AppCore::StopForExit).
static void GroupQuitMidRun() {
    printf("quit-mid-run: the app closes while recording\n");
    Pipeline p;
    Check(p.Start(), "a run starts");
    p.Loop(4.0);
    Check(p.core.capturer.IsRecording(), "and is recording when the app closes");
    p.core.StopForExit();
    Check(!p.core.capturer.IsRecording(), "closing ends the run");
    p.WaitComplete();
    Check(p.complete.load() && !p.completeNormal.load(), "as not normal, so nothing is stacked");
    Check(p.ui.Saw("stopped (the app is closing)"), "the log says why");
    const uint32_t n = p.core.capturer.FramesRecorded();
    const SerCheck c = ReadSer(p.path);
    Check(n > 0 && c.ok && c.frames == (int)n && c.sizeMatches, "the SER's header counts the frames it holds",
          std::to_string(c.frames) + " in the header, " + std::to_string(n) + " written");
    Check(n == p.accepted.load(), "every frame taken was drained to it");
    std::error_code ec;
    Check(!p.path.empty() && fs::exists(ObservationPathFor(p.path), ec), "and the observation record is written");
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
    // Phase 157.0b: the GoTo panel's watch, on the stuck mount's answers.
    SlewWatch w; w.Start(0.0);
    Check(w.Update(SlewWatch::kTimeoutSec - 1.0, m.IsSlewing()) == SlewWatch::State::Slewing,
          "the panel's watch waits while inside its limit");
    Check(w.Update(SlewWatch::kTimeoutSec + 1.0, m.IsSlewing()) == SlewWatch::State::TimedOut,
          "and calls it a fault past it");
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
    if (g == "all" || g == "disk-slow-fits")     { GroupDiskSlowFits();     ran = true; }
    if (g == "all" || g == "disk-slow-overflow") { GroupDiskSlowOverflow(); ran = true; }
    if (g == "all" || g == "quit-mid-run")       { GroupQuitMidRun();       ran = true; }
    if (!ran) { printf("unknown group '%s'\n", g.c_str()); return 2; }
    if (g_fail) { printf("fault_injection: %d FAILED\n", g_fail); return 1; }
    printf("fault_injection: all passed\n");
    return 0;
}
