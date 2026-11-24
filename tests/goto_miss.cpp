// goto_miss: a GoTo that "completed" with nothing in the frame says why
// (2026-11-24: a stale hand-control alignment sent four GoTos to the horizon).
#include "core/GoToMiss.h"
#include "core/Readiness.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    GoToMissWatch w;
    // The night: Saturn, then the Moon, both missing.
    w.GoToComplete("Saturn", 100.0);
    CHECK(w.Tick(105.0, false).empty());              // still within the wait
    const std::string m1 = w.Tick(121.0, false);
    CHECK(m1.find("Saturn") != std::string::npos && m1.find("Quick Align") != std::string::npos);
    CHECK(w.Tick(130.0, false).empty());              // once per GoTo
    w.GoToComplete("Moon", 200.0);
    const std::string m2 = w.Tick(221.0, false);
    CHECK(m2.find("second GoTo in a row") != std::string::npos && m2.find("stale") != std::string::npos);
    CHECK(w.MissesInRow() == 2);
    // A GoTo that lands resets the count, and says nothing.
    w.GoToComplete("Saturn", 300.0);
    CHECK(w.Tick(304.0, true).empty());
    CHECK(w.Tick(330.0, false).empty() && w.MissesInRow() == 0);
    // A star or a deep-sky object may be too faint to find: no claim made.
    w.GoToComplete("Deneb", 400.0);
    CHECK(w.Tick(430.0, false).empty());
    w.GoToComplete("M31", 500.0);
    CHECK(w.Tick(530.0, false).empty());
    CHECK(GoToMissWatch::IsBright("JUPITER") && !GoToMissWatch::IsBright("Sun"));
    // The readiness check: an alignment carried over from before the app
    // started is something to confirm by hand, never a silent pass.
    {
        ReadyInputs in;
        in.camOpen = true; in.framesSeen = 10; in.focuserOpen = true; in.mountConnected = true;
        in.mountAligned = true; in.trackingKnown = true; in.trackingOn = true; in.siteSet = true;
        in.profileLoaded = true; in.diskFreeGB = 500;
        auto findAlign = [](const ReadyReport& r) -> const ReadyItem* {
            for (const auto& i : r.items) if (i.name == "Alignment") return &i;
            return nullptr;
        };
        const ReadyReport r1 = EvaluateReadiness(in);
        const ReadyItem* a = findAlign(r1);
        CHECK(a && a->state == ReadyState::Pass);                 // aligned during this session
        in.alignedBeforeStart = true;
        const ReadyReport r2 = EvaluateReadiness(in);
        a = findAlign(r2);
        CHECK(a && a->state == ReadyState::Manual && a->detail.find("Quick Align") != std::string::npos);
        in.mountAligned = false;
        const ReadyReport r3 = EvaluateReadiness(in);
        a = findAlign(r3);
        CHECK(a && a->state == ReadyState::Fail);                 // not aligned stays a failure
    }
    std::printf("goto_miss: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
