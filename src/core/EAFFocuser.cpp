// ============================================================
// EAFFocuser.cpp — ZWO EAF SDK Wrapper for RadukCapture
// ============================================================

#include "EAFFocuser.h"
#include <cstdio> 
#include "CoreLog.h"
#include <chrono>
#include <thread>

// Manual definitions for ZWO EAF types
#ifndef EAF_BOOL
    typedef int EAF_BOOL;
#endif

#ifndef EAF_FALSE
    #define EAF_FALSE 0
#endif

#ifndef EAF_TRUE
    #define EAF_TRUE 1
#endif

namespace raduk::core {

// ── Lifecycle ──────────────────────────────────────────────────────────

bool EAFFocuser::Open(int index) {
    if (m_isOpen) {
        raduk::core::CoreLog("[EAFFocuser] Already open\n");
        return false;
    }

    int id = 0;
    EAF_ERROR_CODE err = EAFGetID(index, &id);
    if (err != EAF_SUCCESS) {
        raduk::core::CoreLog("[EAFFocuser] GetID(%d) failed: %d\n", index, (int)err);
        return false;
    }

    if (!OpenId(id)) return false;
    m_fwBacklashAtOpen = GetBacklash();
    raduk::core::CoreLog("[EAFFocuser] Opened: id=%d, maxStep=%d, firmware backlash=%d\n",
           id, m_info.MaxStep, m_fwBacklashAtOpen);
    return true;
}

std::string EAFFocuser::SerialOf(int id) {
    EAF_SN sn{};
    if (EAFGetSerialNumber(id, &sn) != EAF_SUCCESS) return {};
    return SerialHex(sn.id, (int)sizeof(sn.id));
}

// Phase 157.0e: the part of opening that a reconnect repeats. Under the
// command lock, so no move can slip in between the read and the verdict.
bool EAFFocuser::OpenId(int id) {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    EAF_ERROR_CODE err = EAFOpen(id);
    if (err != EAF_SUCCESS) {
        raduk::core::CoreLog("[EAFFocuser] Open(%d) failed: %d\n", id, (int)err);
        return false;
    }

    m_focuserId = id;

    err = EAFGetProperty(id, &m_info);
    if (err != EAF_SUCCESS) {
        raduk::core::CoreLog("[EAFFocuser] GetProperty failed: %d\n", (int)err);
        EAFClose(id);
        m_focuserId = -1;
        return false;
    }

    m_isOpen = true;
    // Phase 161.0a: a focuser whose position cannot be read is not open.
    int at = 0;
    if (!TryGetPosition(at)) {
        raduk::core::CoreLog("[EAFFocuser] position could not be read on open\n");
        m_isOpen = false;
        EAFClose(id);
        m_focuserId = -1;
        return false;
    }
    // Phase 157.0e: the number against the record, before any move.
    m_serial = SerialOf(id);
    const TrustVerdict v = m_trust.OnOpen(m_serial, at, m_info.MaxStep);
    if (!Trusting(v))
        raduk::core::CoreLog("[EAFFocuser] position NOT TRUSTED: %s\n", m_trust.Problem().c_str());
    return true;
}

// Phase 157.0e. After an unplug the old id is dead, as the camera's
// handle is: close it, look for the same focuser by serial, open that.
// Nothing here moves the motor.
bool EAFFocuser::Reconnect() {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (m_isOpen) {
        EAFClose(m_focuserId);   // the handle may be dead: its error does not matter
        m_isOpen = false;
        m_focuserId = -1;
    }
    m_approach.Cancel();
    m_ledger.Idle();
    const int n = EAFGetNum();
    for (int i = 0; i < n; ++i) {
        int id = 0;
        if (EAFGetID(i, &id) != EAF_SUCCESS) continue;
        if (!m_serial.empty()) {
            if (EAFOpen(id) != EAF_SUCCESS) continue;
            const std::string sn = SerialOf(id);
            EAFClose(id);
            if (sn != m_serial) continue;
        }
        const std::string want = m_serial;
        if (!OpenId(id)) return false;
        raduk::core::CoreLog("[EAFFocuser] Reconnected: id=%d%s%s\n", id,
                             want.empty() ? "" : ", serial ", want.c_str());
        return true;
    }
    raduk::core::CoreLog("[EAFFocuser] Reconnect: no focuser%s found\n",
                         m_serial.empty() ? "" : " with that serial");
    return false;
}

bool EAFFocuser::AcceptReportedPosition() {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    int at = 0;
    if (!TryGetPosition(at)) return false;
    m_trust.Accept(at);
    raduk::core::CoreLog("[EAFFocuser] position %d accepted by the owner\n", at);
    return true;
}

bool EAFFocuser::RestoreSavedPosition() {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    const int saved = m_trust.SavedPosition();
    if (!m_isOpen || saved < 0 || IsMoving()) return false;
    if (EAFResetPostion(m_focuserId, saved) != EAF_SUCCESS) {
        raduk::core::CoreLog("[EAFFocuser] EAFResetPostion(%d) failed\n", saved);
        return false;
    }
    int at = 0;
    if (!TryGetPosition(at) || std::abs(at - saved) > kTrustToleranceSteps) return false;
    m_trust.Restored(at);
    raduk::core::CoreLog("[EAFFocuser] position set back to %d by the owner\n", at);
    return true;
}

// Phase 157.3: the EAF's firmware, which nothing logged before.
DeviceIdentity EAFFocuser::Identity() const {
    DeviceIdentity d;
    d.model = m_info.Name;
    d.sdk   = TidyVersion(EAFGetSDKVersion());
    if (m_isOpen) {
        unsigned char ma = 0, mi = 0, bu = 0;
        if (EAFGetFirmwareVersion(m_focuserId, &ma, &mi, &bu) == EAF_SUCCESS)
            d.firmware = VersionString(ma, mi, bu);
        EAF_SN sn{};
        if (EAFGetSerialNumber(m_focuserId, &sn) == EAF_SUCCESS)
            d.serial = SerialHex(sn.id, (int)sizeof(sn.id));
    }
    return d;
}

void EAFFocuser::Close() {
    if (!m_isOpen) return;
    {   // Phase 157.0e: where it is now (the hand controller may have moved it).
        int at = 0;
        if (!IsMoving() && TryGetPosition(at)) m_trust.OnArrived(at);
    }

    if (m_fwBacklashAtOpen >= 0 && GetBacklash() != m_fwBacklashAtOpen)
        EAFSetBacklash(m_focuserId, m_fwBacklashAtOpen);
    EAFClose(m_focuserId);
    raduk::core::CoreLog("[EAFFocuser] Closed: id=%d\n", m_focuserId);
    
    m_focuserId = -1;
    m_isOpen = false;
}

// ── Motion ─────────────────────────────────────────────────────────────

// Phase 161.0a: every move comes through here. It starts only from a
// position that was read just now, inside the focuser's range, and no
// further than the travel guard unless `pastGuard` overrides it.
// Phase 161.0b: under the command lock, with the caller's stop generation
// checked inside it, so a Stop() that has returned cannot be overtaken.
bool EAFFocuser::Command(int position, bool oneSided, bool pastGuard, const unsigned* sequence,
                         bool relative) {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) { m_ledger.Refuse(MoveRefusal::NotOpen, 0, position, 0); return false; }
    if (sequence && *sequence != m_stopGen.load()) {
        m_ledger.Refuse(MoveRefusal::Stopped, 0, position, 0);
        raduk::core::CoreLog("[EAFFocuser] move to %d dropped: a stop was asked for\n", position);
        return false;
    }
    // Phase 157.0e: an absolute number means nothing until the position is confirmed.
    if (!relative && !m_trust.Trusted()) {
        m_ledger.Refuse(MoveRefusal::Untrusted, 0, position, 0);
        raduk::core::CoreLog("[EAFFocuser] move to %d refused: %s\n", position, m_trust.Problem().c_str());
        return false;
    }
    // Phase 169.1: the EAF refuses a move while it is moving
    // (EAF_ERROR_MOVING, seen at the scope 2026-11-21: "EAFMove(154144)
    // failed: 5"), and the press was dropped without a word. The newest
    // command is the one meant: stop the move in flight and replace it.
    m_lastReplaced.store(-1);
    if (!ReplaceMoveInFlight(position)) return false;
    int from = 0;
    const bool known = TryGetPosition(from);
    const int  guard = m_maxTravel.load();
    const MoveRefusal r = CheckMove(known, from, position, m_info.MaxStep, guard, pastGuard);
    if (r != MoveRefusal::None) {
        m_ledger.Refuse(r, from, position, guard);
        raduk::core::CoreLog("[EAFFocuser] move to %d refused: %s\n", position,
                             m_ledger.LastRefusalText().c_str());
        return false;
    }
    int leg = position;
    if (oneSided) leg = m_approach.Begin(from, position);
    else          m_approach.Direct(from, position);
    EAF_ERROR_CODE err = EAFMove(m_focuserId, leg);
    if (err != EAF_SUCCESS) {
        m_approach.Cancel();
        m_ledger.Idle();
        m_ledger.Refuse(MoveRefusal::Device, from, position, guard);
        raduk::core::CoreLog("[EAFFocuser] EAFMove(%d) failed: %d\n", leg, (int)err);
        return false;
    }
    m_ledger.Commanded(position, leg);
    return true;
}

// Phase 169.1: true when the motor is free for a new move (it was idle,
// or the move in flight was stopped and the motor came to rest).
bool EAFFocuser::ReplaceMoveInFlight(int position) {
    bool moving = false, hand = false;
    if (EAFIsMoving(m_focuserId, &moving, &hand) != EAF_SUCCESS || !moving) return true;
    const int was = m_ledger.Target();
    m_approach.Cancel();
    EAFStop(m_focuserId);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReplaceWaitMs);
    for (;;) {
        moving = false;
        if (EAFIsMoving(m_focuserId, &moving, &hand) == EAF_SUCCESS && !moving) break;
        if (std::chrono::steady_clock::now() >= until) {
            m_ledger.Idle();
            m_ledger.Refuse(MoveRefusal::Device, 0, position, 0);
            raduk::core::CoreLog("[EAFFocuser] move to %d refused: the move in flight did not stop within %d ms\n",
                                 position, kReplaceWaitMs);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    m_ledger.Idle();
    m_lastReplaced.store(was);
    int at = 0;
    const bool known = TryGetPosition(at);
    if (was >= 0)
        raduk::core::CoreLog("[EAFFocuser] move to %d replaces the move in flight to %d: stopped at %s\n",
                             position, was, known ? std::to_string(at).c_str() : "an unread position");
    else
        raduk::core::CoreLog("[EAFFocuser] move to %d: the motor was moving (another controller?), stopped first\n",
                             position);
    return true;
}

bool EAFFocuser::MoveTo(int position)       { return Command(position, true,  false); }
bool EAFFocuser::MoveToFar(int position)    { return Command(position, true,  true);  }
bool EAFFocuser::MoveToDirect(int position) { return Command(position, false, false); }
bool EAFFocuser::MoveInSequence(unsigned generation, int position, bool oneSided) {
    return Command(position, oneSided, true, &generation);
}

bool EAFFocuser::MoveRelative(int steps) {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) { m_ledger.Refuse(MoveRefusal::NotOpen, 0, 0, 0); return false; }
    // While a move is in flight a step adds to where that move ends, not
    // to wherever the motor is (which may be the overshoot leg).
    int target = m_ledger.Target();
    if (target >= 0 && !IsMoving()) target = -1;   // it has finished
    int position = 0;
    if (target < 0 && !TryGetPosition(position)) {
        m_ledger.Refuse(MoveRefusal::PositionUnknown, 0, 0, 0);
        raduk::core::CoreLog("[EAFFocuser] relative move of %d refused: %s\n", steps,
                             m_ledger.LastRefusalText().c_str());
        return false;
    }
    return Command(RelativeBase(target, position) + steps, true, false, nullptr, true);
}

bool EAFFocuser::Stop() {
    // Phase 161.0b: the generation moves first and under the lock, so a
    // sequence's next move is refused from this instant, and the second
    // leg of a one-sided move cannot be commanded behind the stop.
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    m_stopGen.fetch_add(1);
    if (!m_isOpen) return false;
    m_approach.Cancel();
    m_ledger.Idle();
    return EAFStop(m_focuserId) == EAF_SUCCESS;
}

bool EAFFocuser::IsMoving() const {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return false;

    // signature: EAF_ERROR_CODE EAFIsMoving(int, bool*, bool*)
    bool bMoving = false;
    bool bHandControl = false;

    // Phase 161.0a: a query that failed is not "stopped". Unknown counts
    // as moving; a caller waiting on it stalls out and stops the motor.
    if (EAFIsMoving(m_focuserId, &bMoving, &bHandControl) != EAF_SUCCESS) return true;
    if (bMoving) return true;
    int at = 0;
    if (!TryGetPosition(at)) return true;
    // "Not moving" short of the leg: the motor may not have started.
    switch (m_ledger.StoppedAt(at, MoveLedger::Clock::now())) {
        case MoveLedger::LegState::Waiting: return true;
        case MoveLedger::LegState::Short: {
            const int target = m_ledger.Target();
            m_approach.Cancel();
            m_ledger.Idle();
            m_ledger.Refuse(MoveRefusal::StoppedShort, at, target, 0);
            raduk::core::CoreLog("[EAFFocuser] %s\n", m_ledger.LastRefusalText().c_str());
            return false;
        }
        case MoveLedger::LegState::Arrived: break;
    }
    // Phase 139.2: the first leg is done; command the second.
    const int next = m_approach.NextLeg(at);
    if (next >= 0) {
        if (EAFMove(m_focuserId, next) != EAF_SUCCESS) {
            m_approach.Cancel();
            m_ledger.Idle();
            m_ledger.Refuse(MoveRefusal::Device, at, next, 0);
            return false;
        }
        m_ledger.Leg(next);
        return true;
    }
    const bool hadMove = m_ledger.Target() >= 0;
    m_ledger.Idle();
    if (hadMove) m_trust.OnArrived(at);   // phase 157.0e: the record follows every completed move
    return false;
}

bool EAFFocuser::TryGetPosition(int& position) const {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return false;
    int pos = 0;
    if (EAFGetPosition(m_focuserId, &pos) != EAF_SUCCESS) {
        m_ledger.ReadFailed();
        return false;
    }
    m_ledger.Good(pos);
    position = pos;
    return true;
}

// Never an invented number: after a failed read, the last good one.
int EAFFocuser::GetPosition() const {
    int pos = 0;
    if (TryGetPosition(pos)) return pos;
    return m_ledger.Last();
}

// ── Configuration & Sensors ───────────────────────────────────────────

float EAFFocuser::GetTemperature() const {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return 0.0f;
    float temp = 0.0f;
    EAFGetTemp(m_focuserId, &temp);
    return temp;
}

void EAFFocuser::SetReverse(bool reversed) {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return;
    EAFSetReverse(m_focuserId, reversed ? EAF_TRUE : EAF_FALSE);
}

void EAFFocuser::SetBacklash(int steps) {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return;
    EAFSetBacklash(m_focuserId, steps);
}

int EAFFocuser::GetBacklash() const {
    std::lock_guard<std::recursive_mutex> cmd(m_cmd);
    if (!m_isOpen) return -1;
    int v = -1;
    if (EAFGetBacklash(m_focuserId, &v) != EAF_SUCCESS) return -1;
    return v;
}

} // namespace raduk::core