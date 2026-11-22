#pragma once
// ============================================================
// TrackingConfirm.h -- a tracking command, confirmed by the poll
// ============================================================
//
// 2026-11-21, all night on the 8SE's hand control: every `T 1` (ALT-AZ)
// was ACKed, and every 't' readback 250 ms later failed. `T 0` read back
// fine every time. The app reported "tracking=REFUSED" while the mount
// tracked: PLANET LOCK calibrated on it (which needs both axes moving)
// and held Saturn for two hours. Worse, the T key then read a cache it
// could not trust and turned tracking OFF when the owner meant ON.
//
// The position poll's own 't' read (every ~0.5 s) never failed that
// night. So a tracking command that is ACKed but not read back is
// "sent, confirming", and the poll settles it: the mode it reads is the
// answer, and a mount that reads OFF after an ACKed `T 1` is reported
// as not running (the Phase 64 case: an unaligned mount ACKs and sits).
//
// Pure: the caller supplies the clock in ms and the poll's reading.
// ============================================================

namespace raduk::core {

enum class ConfirmResult {
    None,         // nothing pending
    Waiting,      // pending, no verdict yet
    Confirmed,    // the poll read the mode that was sent
    NotRunning,   // the poll read a different mode (usually OFF)
    Unconfirmed,  // no readable poll within the window: say so, once
};

class TrackingConfirm {
public:
    static constexpr long long kWindowMs = 5000;

    // A tracking command was ACKed but not read back.
    void Sent(int mode, long long nowMs) {
        m_pending = true;
        m_mode    = mode;
        m_since   = nowMs;
    }
    // A readback (or anything else) settled it.
    void Clear() { m_pending = false; }
    bool Pending() const { return m_pending; }
    int  Mode() const { return m_mode; }

    // Each poll: modeRead says whether its 't' read succeeded.
    ConfirmResult OnPoll(bool modeRead, int mode, long long nowMs) {
        if (!m_pending) return ConfirmResult::None;
        if (modeRead) {
            m_pending = false;
            m_got = mode;
            return mode == m_mode ? ConfirmResult::Confirmed : ConfirmResult::NotRunning;
        }
        if (nowMs - m_since >= kWindowMs) {
            m_pending = false;
            return ConfirmResult::Unconfirmed;
        }
        return ConfirmResult::Waiting;
    }
    int Got() const { return m_got; }

private:
    bool      m_pending = false;
    int       m_mode    = 0;
    int       m_got     = 0;
    long long m_since   = 0;
};

// What the T key (and the TRACKING button) should ask for. With the mode
// not known, ask for ON: turning on a mount that already tracks costs
// nothing, while turning off one the owner meant to start loses the target
// (2026-11-21). Modes as IMount: 0 is OFF, 1 is ALT-AZ.
inline int ToggleTarget(bool modeKnown, int mode, int onMode = 1) {
    if (!modeKnown) return onMode;
    return mode == 0 ? onMode : 0;
}

} // namespace raduk::core
