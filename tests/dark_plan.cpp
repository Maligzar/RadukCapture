// dark_plan: Phase 173, RUN DARKS. Tonight's lights (2026-11-21/22) give the
// plan; the runner is driven against a simulated camera and recorder.
#include "app/DarkRunController.h"
#include <cstdio>

using namespace raduk::core;
using namespace raduk::app;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static FoundRecord Rec(int64_t start, const char* type, int w, int h, int bits, double exp, double gain,
                       const char* target, const char* folder = "D:\\RadukCapture\\captures") {
    FoundRecord f;
    f.rec.startUnix = start; f.rec.frameType = type;
    f.rec.width = w; f.rec.height = h; f.rec.bitDepth = bits; f.rec.bin = 1;
    f.rec.exposureMs = exp; f.rec.gain = gain; f.rec.target = target;
    f.folder = folder;
    return f;
}

int main() {
    const int64_t night = 1000000;
    // 1. Tonight: Saturn at 25 ms/230 and 9.44 ms/324 (twice), Jupiter at
    //    9.23 ms/391; one record from last night; one without its gain.
    std::vector<FoundRecord> recs = {
        Rec(night + 10, "Light", 640, 480, 16, 25.0, 230, "Saturn"),
        Rec(night + 20, "Light", 640, 480, 16, 9.44, 324, "Saturn"),
        Rec(night + 30, "Light", 640, 480, 16, 9.44, 324, "Saturn"),
        Rec(night + 40, "Light", 640, 480, 16, 9.23, 391, "Jupiter"),
        Rec(night + 45, "Light", 640, 480, 16, 9.2301, 391, "Jupiter"),   // camera rounding: the same set
        Rec(night - 50000, "Light", 1280, 960, 8, 5.0, 300, "Mars"),     // last night
    };
    FoundRecord noGain = Rec(night + 50, "Light", 640, 480, 16, 9.0, 0, "Saturn");
    noGain.rec.gain = kObsUnknown;
    recs.push_back(noGain);
    auto plan = PlanDarks(recs, night);
    CHECK(plan.sets.size() == 3);
    CHECK(plan.lightsRead == 6);
    CHECK(plan.lightsUnusable == 1);
    CHECK(plan.sets[0].key.exposureMs == 25.0 && plan.sets[0].lights == 1);
    CHECK(plan.sets[1].lights == 2);
    CHECK(plan.sets[2].targets.size() == 1 && plan.sets[2].targets[0] == "Jupiter");
    CHECK(DarkPrefix(plan.sets[1].key) == "Dark_9.44ms_g324_640x480_16bit");
    std::printf("  %s\n", DescribeDarkSet(plan.sets[1]).c_str());

    // 2. A dark already taken tonight covers its set; one from last night does not.
    {
        auto r2 = recs;
        r2.push_back(Rec(night + 60, "Dark", 640, 480, 16, 25.0, 230, ""));
        r2.push_back(Rec(night - 100, "Dark", 640, 480, 16, 9.44, 324, ""));
        auto p2 = PlanDarks(r2, night);
        CHECK(p2.sets.size() == 2);
        CHECK(p2.setsAlreadyDark == 1);
    }

    // 3. The runner, against a simulated camera and recorder.
    DarkCameraState cam{1280, 960, 8, 12.0, 250, true, true};   // AE on and auto
    bool rec = false;
    std::vector<std::string> started, folders;
    std::vector<int> frameCounts;
    bool restored = false;
    DarkRunHooks h;
    h.capture = [&] { return cam; };
    h.applyFormat = [&](int w, int hh, int b) { cam.width = w; cam.height = hh; cam.bitDepth = b; return true; };
    h.setExposure = [&](double ms) { cam.exposureMs = ms; };
    h.setGain = [&](double g) { cam.gain = g; };
    h.setAeAuto = [&](bool on) { cam.aeAuto = on; };
    h.startDarkRecording = [&](const std::string& p, const std::string& f, int n) {
        // A dark is taken at exactly the set's settings, AE off.
        CHECK(!cam.aeAuto);
        started.push_back(p); folders.push_back(f); frameCounts.push_back(n); rec = true; return true;
    };
    h.recording = [&] { return rec; };
    h.restore = [&](const DarkCameraState& s) { cam = s; restored = true; };
    h.log = [](const std::string& s) { std::printf("    %s\n", s.c_str()); };
    {
        DarkRunController dr(h);
        CHECK(!dr.Confirm(0.0));                          // not armed: nothing runs
        const std::string text = dr.Arm(plan, 200, 105.0, 0.0);
        CHECK(text.find("3 sets") != std::string::npos);
        CHECK(text.find("Cap the scope") != std::string::npos);
        CHECK(dr.Armed() && started.empty());             // arming records nothing
        CHECK(dr.Confirm(5.0));
        CHECK(cam.width == 640 && cam.exposureMs == 25.0 && cam.gain == 230 && !cam.aeAuto);
        dr.Tick(6.0);                                     // still settling
        CHECK(started.empty());
        dr.Tick(8.1);                                     // settled: the first set records
        CHECK(started.size() == 1 && started[0] == "Dark_25ms_g230_640x480_16bit");
        CHECK(folders[0] == "D:\\RadukCapture\\captures\\Darks" && frameCounts[0] == 200);
        double t = 9.0;
        for (int i = 0; i < 20 && dr.State() != DarkRunController::Phase::Done; ++i) {
            rec = false;               // the file closed
            dr.Tick(t); t += 4.0;      // next set applied, settles
            dr.Tick(t); t += 1.0;      // and records
        }
        CHECK(dr.State() == DarkRunController::Phase::Done);
        CHECK(started.size() == 3 && dr.SetsDone() == 3);
        CHECK(started[2] == "Dark_9.23ms_g391_640x480_16bit");
        CHECK(restored && cam.width == 1280 && cam.exposureMs == 12.0 && cam.gain == 250 && cam.aeAuto);
    }
    // 4. Stop mid-run puts the camera back; an unconfirmed arm expires.
    {
        cam = {1280, 960, 8, 12.0, 250, true, true}; restored = false; started.clear(); rec = false;
        DarkRunController dr(h);
        dr.Arm(plan, 200, 105.0, 0.0);
        dr.Confirm(1.0);
        dr.Tick(5.0);
        CHECK(started.size() == 1);
        dr.Stop("ESC");
        CHECK(dr.State() == DarkRunController::Phase::Stopped && restored && cam.width == 1280);

        DarkRunController late(h);
        late.Arm(plan, 200, 105.0, 0.0);
        CHECK(!late.Confirm(DarkRunController::kArmSec + 1.0));   // too late: press again
    }
    // 5. Nothing to do says so and does not arm.
    {
        DarkRunController dr(h);
        DarkPlanResult empty;
        CHECK(dr.Arm(empty, 200, 105.0, 0.0).find("no lights") != std::string::npos);
        CHECK(!dr.Armed());
    }
    // 6. A camera that refuses the frame fails the run and restores.
    {
        cam = {1280, 960, 8, 12.0, 250, true, true}; restored = false;
        DarkRunHooks h2 = h;
        h2.applyFormat = [](int, int, int) { return false; };
        DarkRunController dr(h2);
        dr.Arm(plan, 200, 105.0, 0.0);
        CHECK(!dr.Confirm(1.0));
        CHECK(dr.State() == DarkRunController::Phase::Failed && restored);
    }

    std::printf("dark_plan: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
