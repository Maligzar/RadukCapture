#pragma once
// ============================================================
//  SlewWatch -- Phase 157.0b: a GoTo that never arrives is a fault
//
//  The GoTo panel showed SLEWING for as long as the mount said it was
//  slewing: a mount stuck mid-slew (157.0's slew-stuck fault, or a real
//  one) kept it there for ever, and acquisition waited behind it. The
//  mosaic (90 s) and satellite (120 s) paths already time out; this is
//  the panel's.
//
//  Pure: the caller supplies the time and whether the mount says it is
//  slewing.
// ============================================================

namespace raduk::core {

class SlewWatch {
public:
    // The longest GoTo the 8SE makes is well under this; a slew still
    // going after it is stuck.
    // UNVERIFIED(item 53): time the longest GoTo on the real mount (horizon to horizon); this must stay well above it
    static constexpr double kTimeoutSec = 180.0;

    enum class State { Idle, Slewing, Arrived, TimedOut };

    void Start(double nowSec) { m_start = nowSec; m_state = State::Slewing; }
    void Cancel()             { m_state = State::Idle; }

    // Each tick while a GoTo is in flight.
    State Update(double nowSec, bool mountSaysSlewing) {
        if (m_state != State::Slewing) return m_state;
        if (!mountSaysSlewing) m_state = State::Arrived;
        else if (nowSec - m_start > kTimeoutSec) m_state = State::TimedOut;
        return m_state;
    }

    State Current() const { return m_state; }
    double Elapsed(double nowSec) const { return nowSec - m_start; }

private:
    double m_start = 0.0;
    State  m_state = State::Idle;
};

} // namespace raduk::core
