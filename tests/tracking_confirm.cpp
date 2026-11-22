// tracking_confirm: a tracking command ACKed but not read back is settled
// by the position poll (2026-11-21: every `T 1` readback failed while the
// mount tracked; the app said REFUSED and the T key then turned it off).
#include "core/TrackingConfirm.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // 1. Nothing sent: polls say nothing.
    {
        TrackingConfirm c;
        CHECK(c.OnPoll(true, 1, 0) == ConfirmResult::None);
        CHECK(!c.Pending());
    }
    // 2. The night's case: T 1 ACKed, readback failed, the poll reads 1.
    {
        TrackingConfirm c;
        c.Sent(1, 1000);
        CHECK(c.Pending());
        CHECK(c.OnPoll(false, 0, 1400) == ConfirmResult::Waiting);   // a dropped poll read
        CHECK(c.OnPoll(true, 1, 1900) == ConfirmResult::Confirmed);
        CHECK(!c.Pending());
        CHECK(c.OnPoll(true, 1, 2400) == ConfirmResult::None);       // said once
    }
    // 3. The Phase 64 case: ACKed, and the poll reads OFF - not running.
    {
        TrackingConfirm c;
        c.Sent(1, 0);
        CHECK(c.OnPoll(true, 0, 500) == ConfirmResult::NotRunning);
        CHECK(c.Got() == 0);
    }
    // 4. No readable poll for 5 s: unconfirmed, said once, not "refused".
    {
        TrackingConfirm c;
        c.Sent(1, 0);
        CHECK(c.OnPoll(false, 0, 4999) == ConfirmResult::Waiting);
        CHECK(c.OnPoll(false, 0, 5000) == ConfirmResult::Unconfirmed);
        CHECK(c.OnPoll(false, 0, 9000) == ConfirmResult::None);
    }
    // 5. A readback that did work clears it; a later poll says nothing.
    {
        TrackingConfirm c;
        c.Sent(1, 0);
        c.Clear();
        CHECK(c.OnPoll(true, 1, 100) == ConfirmResult::None);
    }
    // 6. A second command replaces the first.
    {
        TrackingConfirm c;
        c.Sent(1, 0);
        c.Sent(0, 100);
        CHECK(c.Mode() == 0);
        CHECK(c.OnPoll(true, 0, 300) == ConfirmResult::Confirmed);
    }
    // 7. The T key: unknown asks for ON; known toggles.
    CHECK(ToggleTarget(false, 0) == 1);   // unknown, cache says OFF: ON
    CHECK(ToggleTarget(false, 1) == 1);   // unknown, cache says ON: still ON
    CHECK(ToggleTarget(true, 0) == 1);
    CHECK(ToggleTarget(true, 1) == 0);
    CHECK(ToggleTarget(true, 2, 2) == 0); // EQ modes switch off like ALT-AZ
    CHECK(ToggleTarget(false, 2, 2) == 2);

    std::printf("tracking_confirm: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
