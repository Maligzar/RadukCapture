// ============================================================
//  Phase 157.0b: a GoTo that never arrives (src/core/SlewWatch.h)
//    1. Arriving inside the limit is arrival, however long it took.
//    2. Still slewing past the limit is a timeout, once.
//    3. Idle until started; a cancel returns it to idle.
// ============================================================

#include "core/SlewWatch.h"

#include <cstdio>
#include <string>

using raduk::core::SlewWatch;

static int g_fail = 0;
static void Check(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("Slew watch (phase 157.0b)\n");
    const double T = SlewWatch::kTimeoutSec;
    {
        SlewWatch w;
        Check(w.Update(5, true) == SlewWatch::State::Idle, "idle until a GoTo starts it");
        w.Start(100);
        Check(w.Update(101, true) == SlewWatch::State::Slewing, "slewing");
        Check(w.Update(100 + T - 1, true) == SlewWatch::State::Slewing, "still slewing just inside the limit");
        Check(w.Update(100 + T - 0.5, false) == SlewWatch::State::Arrived, "arriving inside it is arrival");
        Check(w.Update(100 + T + 50, true) == SlewWatch::State::Arrived, "and stays arrival");
    }
    {
        SlewWatch w; w.Start(0);
        Check(w.Update(T + 0.1, true) == SlewWatch::State::TimedOut, "past the limit, still slewing: timed out");
        Check(w.Update(T + 1, false) == SlewWatch::State::TimedOut, "once timed out, it stays so");
        Check(w.Elapsed(T + 1) > T, "and knows how long it waited");
        w.Cancel();
        Check(w.Current() == SlewWatch::State::Idle, "a cancel returns it to idle");
    }
    Check(T >= 120.0, "the limit is longer than the satellite path's 120 s");
    if (g_fail) { printf("slew_watch: %d FAILED\n", g_fail); return 1; }
    printf("slew_watch: all passed\n");
    return 0;
}
