// pad_map: Phase 172.0's controller mapping. Pure: no Windows, no XInput.
#include "core/PadMap.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static PadState Pad(uint16_t b = 0, int16_t lx = 0, int16_t ly = 0) {
    PadState s; s.connected = true; s.buttons = b; s.lx = lx; s.ly = ly; return s;
}

int main() {
    // 1. Connect, then idle: one connectedChanged, no jog.
    {
        PadMapper m;
        auto a = m.Update(Pad());
        CHECK(a.connectedChanged && a.jogs.empty() && !a.stop);
        CHECK(!m.Update(Pad()).Any());
    }
    // 2. D-pad up: the arrow-up pairing (axis 1, dir 0), held without repeats,
    //    released on let-go.
    {
        PadMapper m; m.Update(Pad());
        auto a = m.Update(Pad(Btn::DpadUp));
        CHECK(a.jogs.size() == 1 && (a.jogs[0] == JogChange{1, 0, true}));
        CHECK(m.Update(Pad(Btn::DpadUp)).jogs.empty());   // no repeat
        a = m.Update(Pad());
        CHECK(a.jogs.size() == 1 && (a.jogs[0] == JogChange{1, 0, false}));
        // left is axis 0 dir 1, right axis 0 dir 0, down axis 1 dir 1
        CHECK((m.Update(Pad(Btn::DpadLeft)).jogs[0] == JogChange{0, 1, true}));
        m.Update(Pad());
        CHECK((m.Update(Pad(Btn::DpadRight)).jogs[0] == JogChange{0, 0, true}));
        m.Update(Pad());
        CHECK((m.Update(Pad(Btn::DpadDown)).jogs[0] == JogChange{1, 1, true}));
    }
    // 3. Stick: dead zone, engage at 50%, hold down to 35%, release below.
    {
        PadMapper m; m.Update(Pad());
        CHECK(m.Update(Pad(0, 13000)).jogs.empty());            // 40%: nothing
        auto a = m.Update(Pad(0, 20000));                        // 61%: right
        CHECK(a.jogs.size() == 1 && (a.jogs[0] == JogChange{0, 0, true}));
        CHECK(m.Update(Pad(0, 13000)).jogs.empty());            // 40%: still held
        a = m.Update(Pad(0, 9000));                              // 27%: released
        CHECK(a.jogs.size() == 1 && (a.jogs[0] == JogChange{0, 0, false}));
        CHECK(m.Update(Pad(0, 0, -32768)).jogs[0].dir == 1);     // full down
    }
    // 4. Direction change on one axis: release, then press, in that order.
    {
        PadMapper m; m.Update(Pad());
        m.Update(Pad(0, 30000));
        auto a = m.Update(Pad(0, -30000));
        CHECK(a.jogs.size() == 2);
        CHECK((a.jogs[0] == JogChange{0, 0, false}) && (a.jogs[1] == JogChange{0, 1, true}));
    }
    // 5. Diagonal: both axes, independently.
    {
        PadMapper m; m.Update(Pad());
        auto a = m.Update(Pad(Btn::DpadUp | Btn::DpadRight));
        CHECK(a.jogs.size() == 2 && m.Active(0) == 0 && m.Active(1) == 0);
        // Both D-pad directions on one axis cancel: no jog on it.
        a = m.Update(Pad(Btn::DpadLeft | Btn::DpadRight | Btn::DpadUp));
        CHECK(m.Active(0) == -1 && m.Active(1) == 0);
    }
    // 6. B stops: every jog released, stop raised once, and nothing moves
    //    while B is held, even with the stick pushed.
    {
        PadMapper m; m.Update(Pad());
        m.Update(Pad(Btn::DpadUp, 30000));
        auto a = m.Update(Pad(Btn::B | Btn::DpadUp, 30000));
        CHECK(a.stop);
        CHECK(a.jogs.size() == 2 && !a.jogs[0].pressed && !a.jogs[1].pressed);
        a = m.Update(Pad(Btn::B | Btn::DpadUp, 30000));
        CHECK(!a.stop && a.jogs.empty());                        // held: no repeat, no jog
        a = m.Update(Pad(Btn::DpadUp));                          // B let go: jog may start again
        CHECK(a.jogs.size() == 1 && a.jogs[0].pressed);
    }
    // 7. A Bluetooth drop mid-jog releases every jog; reconnect starts clean.
    {
        PadMapper m; m.Update(Pad());
        m.Update(Pad(Btn::DpadUp | Btn::DpadLeft));
        PadState gone;  // connected = false
        auto a = m.Update(gone);
        CHECK(a.connectedChanged);
        CHECK(a.jogs.size() == 2 && !a.jogs[0].pressed && !a.jogs[1].pressed);
        CHECK(m.Active(0) == -1 && m.Active(1) == -1);
        CHECK(!m.Update(gone).Any());
        // Reconnect with A already down: not a fresh press? It is: the
        // mapper forgot the buttons, and a held button seen first counts.
        a = m.Update(Pad(Btn::A));
        CHECK(a.connectedChanged && a.lockToggle);
    }
    // 8. Edges: A, X, LB, RB act once per press.
    {
        PadMapper m; m.Update(Pad());
        auto a = m.Update(Pad(Btn::A | Btn::X | Btn::RB));
        CHECK(a.lockToggle && a.recordToggle && a.rateDelta == 1);
        a = m.Update(Pad(Btn::A | Btn::X | Btn::RB));
        CHECK(!a.lockToggle && !a.recordToggle && a.rateDelta == 0);
        a = m.Update(Pad(Btn::LB));
        CHECK(a.rateDelta == -1);
    }
    // 9. Y is autofocus only when allowed (Phase 167).
    {
        PadMapper off; off.Update(Pad());
        CHECK(!off.Update(Pad(Btn::Y)).autofocus);
        PadConfig c; c.allowAutofocus = true;
        PadMapper on(c); on.Update(Pad());
        CHECK(on.Update(Pad(Btn::Y)).autofocus);
    }

    std::printf("pad_map: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
