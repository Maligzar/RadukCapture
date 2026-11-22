// focus_replace: Phase 169.1. A move sent while the focuser is moving used
// to be dropped: the EAF answers EAF_ERROR_MOVING (seen at the scope,
// 2026-11-21: "EAFMove(154144) failed: 5") and only the log said so. Now
// the newest command stops the move in flight and replaces it. Driven on
// the emulated EAF with the real device's refusal switched on.
#define RADUK_EMULATOR 1
#include "core/EAFEmulator.h"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool WaitStopped(EAFFocuser& f, int limitMs = 20000) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(limitMs);
    while (std::chrono::steady_clock::now() < until) {
        if (!f.IsMoving()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
static int Pos(EAFFocuser& f) { int p = 0; f.TryGetPosition(p); return p; }

int main() {
    // 1. An absolute move while moving: replaced, not dropped.
    {
        EAFFocuser f; f.Open(0);
        f.SetRefusesWhileMoving(true);
        f.SetMaxTravel(0);
        f.SetApproachBacklash(0);
        const int start = Pos(f);
        CHECK(f.MoveToDirect(start + 8000));
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        CHECK(f.IsMoving());
        const int mid = Pos(f);
        CHECK(mid != start && mid < start + 8000);
        CHECK(f.MoveToDirect(start - 3000));          // the press that used to be dropped
        CHECK(f.ReplacedMoves() == 1);
        CHECK(WaitStopped(f));
        std::printf("  absolute: %d -> (moving, at %d) -> ended %d, wanted %d\n", start, mid, Pos(f), start - 3000);
        CHECK(Pos(f) == start - 3000);
        f.Close();
    }
    // 2. Relative steps while moving still add to where the move was going.
    {
        EAFFocuser f; f.Open(0);
        f.SetRefusesWhileMoving(true);
        f.SetMaxTravel(0);
        f.SetApproachBacklash(0);
        const int start = Pos(f);
        CHECK(f.MoveRelative(5000));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(f.IsMoving());
        CHECK(f.MoveRelative(1000));                  // a second key press mid-move
        CHECK(WaitStopped(f));
        std::printf("  relative: two steps of 5000 and 1000 from %d ended %d\n", start, Pos(f));
        CHECK(Pos(f) == start + 6000);
        f.Close();
    }
    // 3. A one-sided move replaced mid-way still finishes from the outward side.
    {
        EAFFocuser f; f.Open(0);
        f.SetRefusesWhileMoving(true);
        f.SetMaxTravel(0);
        f.SetApproachBacklash(400);
        f.SetApproachDirection(+1);
        const int start = Pos(f);
        CHECK(f.MoveTo(start - 6000));                // overshoots below, then comes up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(f.MoveTo(start - 2000));
        CHECK(WaitStopped(f));
        std::printf("  one-sided: replaced mid-way, ended %d, wanted %d\n", Pos(f), start - 2000);
        CHECK(Pos(f) == start - 2000);
        f.Close();
    }
    // 4. Idle: no stop, nothing replaced.
    {
        EAFFocuser f; f.Open(0);
        f.SetRefusesWhileMoving(true);
        f.SetMaxTravel(0);
        const int start = Pos(f);
        CHECK(f.MoveToDirect(start + 500));
        CHECK(WaitStopped(f));
        CHECK(f.MoveToDirect(start));
        CHECK(WaitStopped(f));
        CHECK(f.ReplacedMoves() == 0 && Pos(f) == start);
        f.Close();
    }

    std::printf("focus_replace: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
