// tracking_state: who follows the sky, and what the mount window says.
// 2026-11-22: "TRK: OFF" while PLANET LOCK drove the axes and Saturn held.
#include "core/TrackingState.h"
#include <cstdio>
#include <cwchar>
#include <cstring>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // 1. The night's case: hand control OFF on purpose, the app driving.
    Steering s = WhoSteers(true, true, true, true);
    CHECK(s == Steering::AppLock);
    CHECK(IsFollowingSky(s));
    CHECK(!KnownStill(s));
    CHECK(std::wcscmp(TrkLabel(s, L"TRK: ALT/AZ"), L"TRK: APP (lock)") == 0);

    // 2. The app driving wins even when the hand control cannot be read.
    CHECK(WhoSteers(true, false, true, true) == Steering::AppLock);

    // 3. Hand control tracking, nobody else.
    s = WhoSteers(true, true, false, false);
    CHECK(s == Steering::HandControl);
    CHECK(IsFollowingSky(s));
    CHECK(std::wcscmp(TrkLabel(s, L"TRK: ALT/AZ"), L"TRK: ALT/AZ") == 0);

    // 4. Really off.
    s = WhoSteers(true, true, true, false);
    CHECK(s == Steering::Off);
    CHECK(!IsFollowingSky(s));
    CHECK(KnownStill(s));
    CHECK(std::wcscmp(TrkLabel(s, L"x"), L"TRK: OFF") == 0);

    // 5. Unread is not OFF (Phase 69), and is not refused as still.
    s = WhoSteers(true, false, true, false);
    CHECK(s == Steering::Unknown);
    CHECK(!IsFollowingSky(s));
    CHECK(!KnownStill(s));
    CHECK(std::wcsncmp(TrkLabel(s, L"x"), L"TRK: ?", 6) == 0);

    // 6. No mount: nothing follows, whatever the flags say.
    s = WhoSteers(false, true, false, true);
    CHECK(s == Steering::NotConnected);
    CHECK(!IsFollowingSky(s));

    // 7. Names for the log and the assistant.
    CHECK(std::strcmp(SteeringName(Steering::AppLock), "app (lock)") == 0);

    std::printf("tracking_state: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
