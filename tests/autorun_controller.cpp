// ============================================================
//  Phase 153.3: a sequence's wiring, out of main.cpp
//  (src/app/AutorunController.h), on the real AutorunRunner and
//  SeeingMonitor, with the rig faked behind the hooks.
//
//    1. A whole sequence: focus, runs through the RECORD path (told apart
//       by Pressing), each run noted, the ranked list written, the
//       summary and the notice.
//    2. A run that will not start ends the sequence, as a warning.
//    3. An abort mid-run closes the run through the one stop.
//    4. Refused at its first check (below the floor), it still says so.
//    5. A failed sweep is counted; the seeing hold and the Jupiter
//       deferral are announced once a change.
//    6. 2026-11-24, the RUNS baseline: no sweep before run 1, every
//       refocus centred on the baseline, a far or failed result sent back
//       to it, a run waiting for that move, every run logged against it.
// ============================================================

#include "app/AutorunController.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace raduk::app;
using Runner = raduk::core::AutorunRunner;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

struct Rig {
    raduk::core::SeeingMonitor seeing;
    RecordingUiSink ui;
    AutorunController c{seeing, ui};
    // The faked rig.
    bool recording = false, focusing = false, recordOk = true, pressingSeen = false;
    bool target = true, prime = false, seeingOpen = true;
    double alt = 45.0;
    int recTicksLeft = 0, focusTicksLeft = 0, focusStarts = 0, focusAborts = 0, stops = 0;
    uint32_t frames = 0;
    std::vector<std::pair<ToastLevel, std::string>> toasts;
    std::vector<std::string> sessions;
    std::vector<bool> defers;
    bool tbRunning = false;
    fs::path dir;
    double now = 1000.0;

    Rig() {
        dir = fs::temp_directory_path() / "raduk_autorun_ctl";
        std::error_code ec; fs::remove_all(dir, ec); fs::create_directories(dir, ec);
        AutorunHooks h;
        h.checkInputs = [this](Runner::Inputs& ai) {
            ai.targetSeen = target; ai.altDeg = alt; ai.seeingOpen = seeingOpen;
            ai.prime = prime; ai.diskHasRoom = true;
        };
        h.startFocus = [this] { ++focusStarts; focusing = true; focusTicksLeft = 2; };
        h.abortFocus = [this] { ++focusAborts; focusing = false; };
        h.startRecording = [this] {
            pressingSeen = c.Pressing();
            if (!recordOk) return false;
            recording = true; recTicksLeft = 3; frames = 0;
            c.SetLastPath((dir / ("run" + std::to_string(c.runner.Ordinal()) + ".ser")).string());
            return true;
        };
        h.stopRecording = [this] { ++stops; recording = false; };
        h.framesRecorded = [this] { return frames; };
        h.runNote = [] { return std::string("Red Spot near the meridian"); };
        h.deferChanged = [this](bool d) { defers.push_back(d); };
        h.session = [this](const std::string& s) { sessions.push_back(s); };
        h.notify = [this](ToastLevel l, const std::string& s) { toasts.push_back({l, s}); };
        h.toolbar = [this](bool r, int) { tbRunning = r; };
        h.rankingPath = [this](const std::string& stamp) { return (dir / ("autorun_" + stamp + ".txt")).string(); };
        c.SetHooks(h);
    }
    // One readout tick, after the faked rig has moved on.
    void Step() {
        now += 0.5;
        if (recording) { frames += 50; if (--recTicksLeft <= 0) recording = false; }
        if (focusing && --focusTicksLeft <= 0) focusing = false;
        c.Tick(now, recording, focusing, frames);
    }
    void RunUntilEnd(int maxTicks = 400) {
        for (int i = 0; i < maxTicks && (c.runner.Running() || recording); ++i) Step();
        Step();
    }
    std::string LastToast() const { return toasts.empty() ? "" : toasts.back().second; }
    ~Rig() { std::error_code ec; fs::remove_all(dir, ec); }
};

static Runner::Config Cfg(int runs, int refocus) {
    Runner::Config c;
    c.runs = runs; c.runSec = 30.0; c.gapSec = 1.0; c.refocusEvery = refocus; c.minAltDeg = 20.0;
    return c;
}

int main() {
    printf("Autorun controller (phase 153.3)\n");
    {
        Rig r;
        Check(r.c.Start(Cfg(2, 1), r.now, true, 0.8), "a sequence of two runs starts");
        r.RunUntilEnd();
        Check(r.focusStarts >= 1 && r.ui.Saw("[AUTORUN] focus before run 1"), "it focuses before the first run");
        Check(r.pressingSeen, "its RECORD press is marked as the sequence's");
        Check(r.c.Results().size() == 2 && r.c.Results()[0].ordinal == 1 && r.c.Results()[1].ordinal == 2,
              "both runs are noted", std::to_string(r.c.Results().size()));
        Check(!r.c.Results().empty() && r.c.Results()[0].note == "Red Spot near the meridian" &&
              r.c.Results()[0].frames > 0 && r.c.Results()[0].file.find("run1.ser") != std::string::npos,
              "each with its file, frames and Jupiter note");
        Check(r.ui.Saw("[AUTORUN] run 1 of 2") && r.ui.Saw("on the disc: Red Spot"), "the runs are logged");
        Check(r.ui.Saw("[AUTORUN] end: 2 of 2 runs") && r.ui.Saw("runs ranked in"), "the end is summarised and ranked");
        bool listed = false;
        for (auto& e : fs::directory_iterator(r.dir))
            if (e.path().filename().string().rfind("autorun_", 0) == 0) {
                std::ifstream in(e.path()); std::stringstream ss; ss << in.rdbuf();
                listed = ss.str().find("best run first") != std::string::npos;
            }
        Check(listed, "the ranked list is on disk");
        Check(!r.toasts.empty() && r.toasts.back().first == ToastLevel::Info &&
              r.LastToast() == "Sequence: 2 of 2 runs - all runs done", "a clean end is an info notice", r.LastToast());
        Check(!r.tbRunning && !r.sessions.empty() && r.sessions.back() == "Autorun ended: 2 of 2 runs: all runs done",
              "the toolbar and the session log follow");
    }
    {
        Rig r; r.recordOk = false;
        r.c.Start(Cfg(2, 0), r.now, false, 0.8);
        r.RunUntilEnd();
        Check(!r.c.runner.Running() && !r.toasts.empty() && r.toasts.back().first == ToastLevel::Warn &&
              r.LastToast().find("the recording would not start") != std::string::npos,
              "a run that will not start ends the sequence, as a warning", r.LastToast());
    }
    {
        Rig r;
        r.c.Start(Cfg(3, 0), r.now, false, 0.8);
        for (int i = 0; i < 6 && !r.recording; ++i) r.Step();
        Check(r.recording, "a run is under way");
        r.c.runner.Abort("stopped by the operator");
        r.Step();
        Check(r.stops == 1 && r.c.Results().size() == 1, "an abort closes the run through the one stop, and notes it");
        r.Step();
        Check(r.LastToast().find("stopped by the operator") != std::string::npos, "and the end says why", r.LastToast());
    }
    {
        Rig r; r.alt = 10.0;
        r.c.Start(Cfg(2, 0), r.now, false, 0.8);
        r.Step();
        Check(!r.c.runner.Running() && r.LastToast().find("below the altitude floor") != std::string::npos,
              "refused at its first check, it still says so", r.LastToast());
    }
    {
        Rig r;
        r.c.Start(Cfg(1, 1), r.now, false, 0.8);
        r.c.NoteFocusFailed();
        r.RunUntilEnd();
        Check(r.c.runner.FocusFailures() == 1 && r.ui.Saw("1 focus failed"), "a failed sweep is counted");
    }
    {
        Rig r; r.prime = true;
        Runner::Config c = Cfg(2, 1); c.deferFocusInPrime = true; c.primeDeferMaxSec = 1800.0;
        r.c.Start(c, r.now, true, 0.8);
        r.RunUntilEnd();
        Check(!r.defers.empty() && r.defers.front() && r.focusStarts == 0,
              "in a prime spell the sweep is put off, announced once");
    }
    {
        Rig r; r.seeingOpen = false;
        Runner::Config c = Cfg(1, 0); c.holdForSeeing = true; c.seeingHoldSec = 3.0;
        r.c.Start(c, r.now, false, 0.8);
        for (int i = 0; i < 4; ++i) r.Step();
        int waits = 0; for (auto& t : r.toasts) if (t.second == "Sequence is waiting for the seeing") ++waits;
        Check(waits == 1 && r.ui.Saw(": waiting"), "a seeing hold is announced once");
        r.RunUntilEnd();
        Check(r.c.runner.SeeingGaveUp() == 1, "and gives up after its limit, running anyway");
    }

    {
        Rig r;
        std::vector<int> centres, moves;
        int focuserPos = 170923, moveTicks = 0, sweepResult = 181988;
        bool sweepOk = true, recordedWhileMoving = false;
        AutorunHooks h = r.c.Hooks();
        h.focusAround = [&](int c) { centres.push_back(c); r.focusing = true; r.focusTicksLeft = 2; };
        h.moveFocuser = [&](int p) { moves.push_back(p); focuserPos = p; moveTicks = 3; };
        h.focuserMoving = [&] { return moveTicks > 0; };
        h.snapshot = [&](raduk::core::RunsSnapshot& s) {
            s.focusPos = focuserPos; s.sharpPct = 60.0; s.altDeg = 47.0; s.targetSeen = true;
            s.targetX = 321; s.targetY = 240; s.exposureMs = 25.0; s.gain = 228;
        };
        auto rec = h.startRecording;
        h.startRecording = [&, rec] { if (moveTicks > 0) recordedWhileMoving = true; return rec(); };
        r.c.SetHooks(h);
        raduk::core::RunsBaseline b;
        b.set = true; b.target = "Saturn";
        b.at.focusPos = 170923; b.at.sharpPct = 63.6; b.at.altDeg = 43.2; b.at.targetSeen = true;
        b.at.targetX = 320; b.at.targetY = 240; b.at.exposureMs = 25.0; b.at.gain = 228;
        r.c.SetBaseline(b, 2000);
        Check(r.c.Start(Cfg(3, 1), r.now, false, 0.8), "a baseline sequence starts");
        Check(r.ui.Saw("[RUNS] baseline for Saturn: focus 170923"), "the baseline is logged at the press");
        // Each sweep ends with a result the moment it stops focusing.
        for (int i = 0; i < 400 && (r.c.runner.Running() || r.recording); ++i) {
            const bool wasFocusing = r.focusing;
            if (moveTicks > 0) --moveTicks;
            r.Step();
            if (wasFocusing && !r.focusing) {
                r.c.NoteFocusResult(sweepOk, sweepResult);
                sweepOk = !sweepOk; sweepResult = 171100;   // then a failed one, then a good one
            }
        }
        r.Step();
        Check(r.focusStarts == 0, "no plain sweep: never a sweep from wherever the focuser is",
              std::to_string(r.focusStarts));
        Check(!r.ui.Saw("focus before run 1") && r.ui.Saw("no sweep before run 1"), "run 1 starts on the owner's focus");
        Check(centres.size() == 2 && centres[0] == 170923 && centres[1] == 170923,
              "each refocus is centred on the baseline", std::to_string(centres.size()));
        Check(r.ui.Saw("[RUNS] refocus: the sweep ended +11065 steps from the baseline"), "a far result is judged");
        Check(r.ui.Saw("[RUNS] refocus: the sweep failed: back to the baseline"), "a failed sweep is judged");
        Check(moves.size() == 2 && moves[0] == 170923 && moves[1] == 170923, "both go back to the baseline",
              std::to_string(moves.size()));
        Check(!recordedWhileMoving, "a run waits for the focuser to get back");
        Check(r.ui.Saw("[RUNS] run 1 against the baseline: focus +0 steps") &&
              r.ui.Saw("[RUNS] run 3 against the baseline") && r.ui.Saw("altitude +3.8 deg") &&
              r.ui.Saw("target 1 px from its spot"), "every run is logged against the baseline");
        Check(r.c.Results().size() == 3, "all three runs done", std::to_string(r.c.Results().size()));
        Check(!r.c.Baseline().set, "the baseline ends with its sequence");
    }
    if (g_fail) { printf("autorun_controller: %d FAILED\n", g_fail); return 1; }
    printf("autorun_controller: all passed\n");
    return 0;
}
