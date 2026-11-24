// runs_baseline: RUNS starts from what the owner set up (2026-11-24): the
// press is the baseline, every refocus is judged against it, every run is
// logged against it.
#include "core/RunsBaseline.h"
#include "core/AutorunRunner.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    RunsBaseline b;
    b.set = true; b.target = "Saturn";
    b.at.focusPos = 170923; b.at.sharpPct = 63.6; b.at.focuserTempC = 19.0; b.at.altDeg = 43.2;
    b.at.targetSeen = true; b.at.targetX = 320; b.at.targetY = 240; b.at.exposureMs = 25.0; b.at.gain = 228;
    const std::string bl = BaselineLine(b);
    std::printf("  %s\n", bl.c_str());
    CHECK(bl.find("Saturn") != std::string::npos && bl.find("170923") != std::string::npos);
    CHECK(bl.find("(320, 240)") != std::string::npos);

    // A run's telemetry against it.
    RunsSnapshot n = b.at;
    n.focusPos = 170963; n.sharpPct = 58.5; n.focuserTempC = 18.2; n.altDeg = 46.3;
    n.targetX = 322; n.targetY = 241; n.gain = 236;
    const std::string dl = DeltaLine(b, n, 3);
    std::printf("  %s\n", dl.c_str());
    CHECK(dl.find("run 3") != std::string::npos && dl.find("focus +40 steps") != std::string::npos);
    CHECK(dl.find("sharpness 92% of it") != std::string::npos);
    CHECK(dl.find("focuser -0.8 C") != std::string::npos && dl.find("altitude +3.1 deg") != std::string::npos);
    CHECK(dl.find("target 2 px from its spot") != std::string::npos);
    CHECK(dl.find("gain 236 (+8)") != std::string::npos && dl.find("(+0.00)") == std::string::npos);
    n.targetSeen = false;
    CHECK(DeltaLine(b, n, 4).find("target not in the frame") != std::string::npos);

    // Refocus verdicts. Tonight's sweep: a "success" 11,065 steps away.
    RefocusVerdict v = JudgeRefocus(b, true, 181988);
    CHECK(v.backToBaseline && v.why.find("+11065") != std::string::npos);
    v = JudgeRefocus(b, true, 171050);
    CHECK(!v.backToBaseline && v.why.find("kept") != std::string::npos);
    CHECK(JudgeRefocus(b, false, 171050).backToBaseline);           // a failed sweep
    CHECK(JudgeRefocus(b, true, -1).backToBaseline);
    CHECK(!JudgeRefocus(b, true, 172800, 2000).backToBaseline);     // 1,877 away, inside 2,000
    CHECK(JudgeRefocus(b, true, 173000, 2000).backToBaseline);      // 2,077 away
    RunsBaseline none;
    CHECK(!JudgeRefocus(none, false, 5).backToBaseline);           // no baseline: not this rule's call

    // The runner: with focusFirst off, run 1 starts without a sweep, and
    // the refocus still comes every N runs.
    AutorunRunner r;
    AutorunRunner::Config c;
    c.runs = 4; c.runSec = 10; c.gapSec = 0; c.refocusEvery = 2; c.focusFirst = false;
    CHECK(r.Start(c, 0.0));
    AutorunRunner::Inputs in;
    in.targetSeen = true; in.altDeg = 40; in.diskHasRoom = true; in.seeingOpen = true;
    CHECK(r.Tick(0.0, in) == AutorunRunner::Action::StartRun);     // no focus first
    AutorunRunner::Config c2 = c; c2.focusFirst = true;
    AutorunRunner r2;
    CHECK(r2.Start(c2, 0.0));
    CHECK(r2.Tick(0.0, in) == AutorunRunner::Action::StartFocus);  // the old behaviour, still there

    std::printf("runs_baseline: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
