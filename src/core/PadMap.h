#pragma once
// ============================================================
// PadMap.h -- a game controller's state, turned into the app's actions
// ============================================================
//
// Phase 172.0 (docs/Phase172-Plan-Gamepad-And-Night-Findings.md).
//
// The controller adds no control of its own. Every action it produces is
// one the app already has: a jog goes to the same handler as the arrow
// keys (which suspends PLANET LOCK and re-centres it on release), STOP to
// the same full stop as ESC, and so on. This header is the pure part:
// pad state in, actions out, with a dead zone, hysteresis, edges and the
// safety rules. The Windows glue reads XInput and calls Update(); it
// static_asserts that the Btn values below equal XInput's.
//
// Safety, by construction:
//   - B stops: it releases every jog this mapper started and suppresses
//     new ones while it is held.
//   - A controller that disappears mid-jog (a Bluetooth drop) releases
//     every jog, exactly like letting go of the keys. The mount never
//     runs on.
//   - Only one direction per axis at a time; a change of direction is a
//     release, then a press.
// ============================================================

#include <cstdint>
#include <vector>

namespace raduk::core {

// Button bits. Equal to XInput's XINPUT_GAMEPAD_* (checked in the glue).
namespace Btn {
    constexpr uint16_t DpadUp    = 0x0001;
    constexpr uint16_t DpadDown  = 0x0002;
    constexpr uint16_t DpadLeft  = 0x0004;
    constexpr uint16_t DpadRight = 0x0008;
    constexpr uint16_t Start     = 0x0010;
    constexpr uint16_t Back      = 0x0020;
    constexpr uint16_t LB        = 0x0100;
    constexpr uint16_t RB        = 0x0200;
    constexpr uint16_t A         = 0x1000;
    constexpr uint16_t B         = 0x2000;
    constexpr uint16_t X         = 0x4000;
    constexpr uint16_t Y         = 0x8000;
}

struct PadState {
    bool     connected = false;
    uint16_t buttons   = 0;
    int16_t  lx = 0, ly = 0;   // left stick, -32768..32767, +y is up
};

// The arrow keys' pairing (gui/Window.cpp ArrowAxisDir): axis 0 is
// RA/azimuth, axis 1 is Dec/altitude; dir 0 is right/up, dir 1 left/down.
struct JogChange {
    int  axis;
    int  dir;
    bool pressed;
    bool operator==(const JogChange& o) const {
        return axis == o.axis && dir == o.dir && pressed == o.pressed;
    }
};

struct PadActions {
    std::vector<JogChange> jogs;   // in the order to apply them
    int  rateDelta    = 0;         // LB -1, RB +1 (jog rate preset)
    bool stop         = false;     // B: the full stop, as ESC
    bool lockToggle   = false;     // A: PLANET LOCK
    bool recordToggle = false;     // X: RECORD
    bool autofocus    = false;     // Y: only when allowed (Phase 167)
    bool connectedChanged = false;
    bool Any() const {
        return !jogs.empty() || rateDelta || stop || lockToggle ||
               recordToggle || autofocus || connectedChanged;
    }
};

struct PadConfig {
    // Stick deflection, as a fraction of full: a jog starts past `engage`
    // and ends below `release`, so a stick resting near the edge does not
    // chatter start/stop on a 9600-baud link.
    float engage  = 0.50f;
    float release = 0.35f;
    bool  allowAutofocus = false;   // off until Phase 167 closes
};

class PadMapper {
public:
    explicit PadMapper(PadConfig cfg = {}) : m_cfg(cfg) {}

    PadActions Update(const PadState& s) {
        PadActions a;
        if (s.connected != m_connected) {
            a.connectedChanged = true;
            m_connected = s.connected;
        }
        if (!s.connected) {
            ReleaseAll(a);
            m_prevButtons = 0;
            return a;
        }

        const uint16_t pressed = static_cast<uint16_t>(s.buttons & ~m_prevButtons);
        m_prevButtons = s.buttons;

        // B first: stop, and nothing moves while it is held.
        if (pressed & Btn::B) a.stop = true;
        const bool bHeld = (s.buttons & Btn::B) != 0;
        if (bHeld) {
            ReleaseAll(a);
        } else {
            for (int axis = 0; axis < 2; ++axis)
                SetAxis(a, axis, WantDir(s, axis));
        }

        if (pressed & Btn::LB) a.rateDelta -= 1;
        if (pressed & Btn::RB) a.rateDelta += 1;
        if (pressed & Btn::A)  a.lockToggle = true;
        if (pressed & Btn::X)  a.recordToggle = true;
        if ((pressed & Btn::Y) && m_cfg.allowAutofocus) a.autofocus = true;
        return a;
    }

    // The direction a jog is running on `axis`, or -1.
    int Active(int axis) const { return m_active[axis]; }
    bool Connected() const { return m_connected; }

private:
    // Which way the pad asks `axis` to move: 0, 1, or -1 for none.
    // The D-pad wins over the stick; the stick has hysteresis around the
    // jog already running.
    int WantDir(const PadState& s, int axis) const {
        const uint16_t pos = axis == 0 ? Btn::DpadRight : Btn::DpadUp;
        const uint16_t neg = axis == 0 ? Btn::DpadLeft  : Btn::DpadDown;
        const bool p = (s.buttons & pos) != 0, n = (s.buttons & neg) != 0;
        if (p != n) return p ? 0 : 1;

        const float v = Norm(axis == 0 ? s.lx : s.ly);
        const int running = m_active[axis];
        // Keep the running direction until the stick falls below release.
        if (running == 0 && v >= m_cfg.release)  return 0;
        if (running == 1 && v <= -m_cfg.release) return 1;
        if (v >= m_cfg.engage)  return 0;
        if (v <= -m_cfg.engage) return 1;
        return -1;
    }

    void SetAxis(PadActions& a, int axis, int dir) {
        if (m_active[axis] == dir) return;
        if (m_active[axis] >= 0) a.jogs.push_back({axis, m_active[axis], false});
        if (dir >= 0)            a.jogs.push_back({axis, dir, true});
        m_active[axis] = dir;
    }

    void ReleaseAll(PadActions& a) {
        for (int axis = 0; axis < 2; ++axis) SetAxis(a, axis, -1);
    }

    static float Norm(int16_t v) {
        return v >= 0 ? v / 32767.0f : v / 32768.0f;
    }

    PadConfig m_cfg;
    bool      m_connected = false;
    uint16_t  m_prevButtons = 0;
    int       m_active[2] = { -1, -1 };
};

} // namespace raduk::core
