// hand_step: a hand step (button, key, Dash) plans from the motor's own
// state (2026-11-24: the owner pressed 10 in and focus went entirely).
#include "core/FocuserGuard.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // At rest, nothing commanded: from the position read.
    HandStep h = PlanHandStep(-10, false, -1, true, 170933);
    CHECK(h.ok && h.base == 170933 && h.to == 170923 && !h.fromTarget && !h.staleTarget);
    // A target left over from an earlier command, with the motor at rest:
    // ignored, whatever else said "moving". 10 in is 10 in.
    h = PlanHandStep(-10, false, 178304, true, 170933);
    CHECK(h.ok && h.to == 170923 && h.staleTarget && !h.fromTarget);
    // The motor really moving: quick presses add up to the move's target.
    h = PlanHandStep(-5000, true, 176988, true, 179000);
    CHECK(h.ok && h.fromTarget && h.to == 171988);
    // The second leg of a one-sided move still to come counts as in flight.
    h = PlanHandStep(10, true, 170923, true, 170723);
    CHECK(h.ok && h.fromTarget && h.to == 170933);
    // Arrived (the target is where it is): not called stale.
    CHECK(!PlanHandStep(10, false, 170934, true, 170933).staleTarget);
    // Where it is cannot be read and nothing is in flight: no move.
    CHECK(!PlanHandStep(10, false, 170000, false, 0).ok);
    // In flight with the position unreadable: the move's target still holds.
    h = PlanHandStep(10, true, 170000, false, 0);
    CHECK(h.ok && h.to == 170010);

    std::printf("hand_step: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
