#pragma once
// ============================================================
// EAFEmulator.h — Software emulator for ZWO EAF focuser
// Implements the same public interface as EAFFocuser.
// Simulates motor movement at realistic step rate.
// Enabled when RADUK_EMULATOR is defined in CMakeLists.txt.
// ============================================================

#ifdef RADUK_EMULATOR
#  include "EAF_focuser_stub.h"
#else
#  include "EAF_focuser.h"
#endif
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "IFocuser.h"
#include "FocusDirection.h"
#include "CoreLog.h"
#include "EmuFault.h"   // phase 157.0

namespace raduk::core {

class EAFFocuser : public IFocuser {
public:
    EAFFocuser()  = default;
    ~EAFFocuser() { Close(); }

    EAFFocuser(const EAFFocuser&) = delete;
    EAFFocuser& operator=(const EAFFocuser&) = delete;

    // ── Lifecycle ──────────────────────────────────────────────

    static int GetCount() {
        raduk::core::CoreLog("[EAFEmulator] GetCount: 1 emulated focuser\n");
        return 1;
    }

    // Phase 157.0e: what the "device" itself holds, apart from any one
    // open handle. With a position record set (SetPositionMemory: the app,
    // and fault_injection's eaf-drop), a real EAF keeps its number while
    // the app closes and re-opens it, so the emulator does too, for the
    // life of the process, starting from the record (the EAF kept its
    // number overnight). Without one, every open starts at kHome, which
    // the bench tests rely on (focuser_safety, af_overshoot,
    // focus_calibration open several focusers in one process).
    // RADUK_EMU_FAULT=eaf-drop@T[+D]: at T s after the first open the
    // focuser is unplugged and every read on the open handle fails, as
    // the SDK's do; D s later it is back, at kDropPosition (owners report
    // an EAF reading about 4761 after a power loss), for a Reconnect to
    // find. Without D it never comes back.
    static constexpr int kDropPosition = 4761;
    // Where this rig's focuser lives. The 8SE with the EAF comes to focus
    // around 150-175k - a measured run landed at 168187 - so the old 50000
    // was nowhere near reality.
    static constexpr int kHome = 166500;
    struct Device {
        bool init = false;
        int  position = 166500;
        bool dropArmed = false;
        EmuFault drop;
        std::chrono::steady_clock::time_point t0{};
        bool dropped = false;   // its number has been lost (power loss)
    };
    static Device& Dev() { static Device d; return d; }
    // Seconds since the first open, for the drop fault's clock.
    static double DevSec() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - Dev().t0).count();
    }
    // Unplugged right now (inside the drop window)?
    static bool DevUnplugged() {
        const Device& d = Dev();
        return d.dropArmed && d.drop.InEffect(DevSec());
    }

    bool Open(int /*index*/) override {
        if (m_isOpen) return false;
        Device& dev = Dev();
        const bool remembers = !m_trust.Path().empty();
        if (remembers && !dev.init) {
            dev.init = true;
            FocuserMemory rec;
            dev.position = m_trust.Saved(rec) ? rec.position : kHome;
        }
        // Armed by the first open that sees it, its clock starting there.
        if (!dev.dropArmed && ActiveEmuFault("eaf-drop", dev.drop)) {
            dev.dropArmed = true;
            dev.dropped = false;
            dev.t0 = std::chrono::steady_clock::now();
            raduk::core::CoreLog("[EAFEmulator] drop injection armed at t+%.0fs\n", dev.drop.startSec);
        }
        if (DevUnplugged()) {
            raduk::core::CoreLog("[EAFEmulator] Open: no focuser (unplugged)\n");
            return false;
        }
        int start = remembers ? dev.position : kHome;
        if (dev.dropArmed && DevSec() >= dev.drop.startSec && !dev.dropped) {
            dev.dropped = true;   // it was unplugged: the number is lost
            dev.position = kDropPosition;
            start = kDropPosition;
        }
        m_position = start;
        m_ledger.Good(start);
        m_excursionFrom = start; m_excursion = 0;
        m_target   = start;
        m_optic    = (double)start;
        m_handleDead = false;
        m_openedSec = DevSec();
        m_isOpen   = true;
        m_moving   = false;
        // Phase 157.0e: the number against the record, before any move.
        const TrustVerdict v = m_trust.OnOpen("", start, GetMaxPosition());
        if (!Trusting(v))
            raduk::core::CoreLog("[EAFEmulator] position NOT TRUSTED: %s\n", m_trust.Problem().c_str());
        // Phase 157.0: RADUK_EMU_FAULT=eaf-stall@T stops the motor at T s.
        m_stallArmed = ActiveEmuFault("eaf-stall", m_stall);
        m_openedAt = std::chrono::steady_clock::now();
        if (m_stallArmed)
            raduk::core::CoreLog("[EAFEmulator] stall injection armed at t+%.0fs\n", m_stall.startSec);
        // Start the motor simulation thread
        m_motorThread = std::thread(&EAFFocuser::MotorLoop, this);
        raduk::core::CoreLog("[EAFEmulator] Opened: id=0, pos=%d, maxStep=600000\n", m_position.load());
        return true;
    }

    void Close() override {
        if (!m_isOpen) return;
        if (!m_handleDead.load() && !DevUnplugged()) {
            int at = 0;
            if (TryGetPosition(at)) m_trust.OnArrived(at);   // 157.0e
        }
        m_isOpen = false;
        m_moving = false;
        if (m_motorThread.joinable()) m_motorThread.join();
        if (!m_handleDead.load() && !m_trust.Path().empty())
            Dev().position = m_position.load();   // the device keeps its number
        raduk::core::CoreLog("[EAFEmulator] Closed\n");
    }

    // ── Phase 157.0e (FocuserTrust.h) ─────────────────────────
    void SetPositionMemory(const std::string& path) override { m_trust.SetPath(path); }
    bool PositionTrusted() const override { return m_trust.Trusted(); }
    std::string TrustProblem() const override { return m_trust.Problem(); }
    bool AcceptReportedPosition() override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        int at = 0;
        if (!TryGetPosition(at)) return false;
        m_trust.Accept(at);
        raduk::core::CoreLog("[EAFEmulator] position %d accepted by the owner\n", at);
        return true;
    }
    // EAFResetPostion: the number changes, the motor does not turn.
    bool RestoreSavedPosition() override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        const int saved = m_trust.SavedPosition();
        if (!m_isOpen || m_handleDead.load() || saved < 0 || IsMoving()) return false;
        {
            // The emulator's mirror follows the number (a real one did not
            // move at all), so putting the number back puts the image back.
            std::lock_guard<std::mutex> lk(m_mutex);
            m_position.store(saved);
            m_target = saved;
            m_optic.store((double)saved);
            m_excursionFrom.store(saved);
        }
        Dev().position = saved;
        m_ledger.Good(saved);
        m_trust.Restored(saved);
        raduk::core::CoreLog("[EAFEmulator] position set back to %d by the owner\n", saved);
        return true;
    }
    // Close the dead handle, open again; no motion.
    bool Reconnect() override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        if (m_isOpen) {
            m_isOpen = false;
            m_moving = false;
            if (m_motorThread.joinable()) m_motorThread.join();
        }
        m_approach.Cancel();
        m_ledger.Idle();
        const bool ok = Open(0);
        raduk::core::CoreLog("[EAFEmulator] Reconnect: %s\n", ok ? "open again" : "no focuser");
        return ok;
    }
    int SavedPosition() const override { return m_trust.SavedPosition(); }

    bool IsOpen() const override { return m_isOpen; }

    // ── Motion ─────────────────────────────────────────────────
    // Phase 161.0a: the same checked path as the EAF wrapper
    // (FocuserGuard.h), so a test of one is a test of the rule.

    bool MoveTo(int position) override       { return Command(position, true,  false); }
    bool MoveToFar(int position) override    { return Command(position, true,  true);  }
    bool MoveToDirect(int position) override { return Command(position, false, false); }
    // Phase 161.0b: stop wins (FocuserGuard.h).
    unsigned StopGeneration() const override { return m_stopGen.load(); }
    bool MoveInSequence(unsigned generation, int position, bool oneSided) override {
        return Command(position, oneSided, true, &generation);
    }
    void Service() override { (void)IsMoving(); }

    void SetMaxTravel(int steps) override { m_maxTravel.store(steps < 0 ? 0 : steps); }
    int  MaxTravel() const override       { return m_maxTravel.load(); }
    MoveRefusal LastRefusalKind() const override { return m_ledger.LastRefusal(); }
    std::string LastRefusal() const override     { return m_ledger.LastRefusalText(); }

    void SetApproachBacklash(int steps) override { m_approach.SetBacklash(steps); }
    void SetApproachDirection(int dir) override  { m_approach.SetDirection(dir); }
    int  ApproachBacklash() const override       { return m_approach.Backlash(); }

    // Phase 139: the knob's slack, simulated. The mirror (what the
    // camera emulator focuses on) trails the step count by up to half of
    // it either way, so the same step number reached from opposite
    // directions is two focus positions, as the owner saw on the rig.
    // 0 (the default) is a perfect focuser.
    void SetSimulatedSlack(int steps) { m_slackHalf = (std::max)(0, steps) / 2; }
    int  GetOpticalPosition() const   { return (int)std::lround(m_optic.load()); }

    // ── Faults (phase 161.0a; phase 157's focuser injection) ───
    // The next n position reads fail, as a USB hiccup does.
    void FailReads(int n)        { m_failReads.store(n < 0 ? 0 : n); }
    // The next n move commands are refused by the "device".
    void RefuseMoves(int n)      { m_refuseMoves.store(n < 0 ? 0 : n); }
    // The motor does not turn while this is on, and says it is moving.
    void SetStalled(bool on)     { m_stalled.store(on); }
    // After a move command the motor waits this long before it starts,
    // and reports "not moving" meanwhile.
    void SetMovingLagMs(int ms)  { m_lagMs.store(ms < 0 ? 0 : ms); }
    // The motor stops this many steps short of every target (a slipping
    // coupling), and reports "not moving" there.
    void SetStopsShortBy(int steps) { m_shortBy.store(steps < 0 ? 0 : steps); }
    // Phase 169.1: refuse a motor command while moving, as the real EAF does.
    void SetRefusesWhileMoving(bool on) { m_refusesWhileMoving.store(on); }
    // How many moves have replaced one in flight.
    int  ReplacedMoves() const { return m_replaced.load(); }
    // The furthest the motor has been from where it was when this was
    // last reset: what a test asserts a fault could not exceed.
    void ResetExcursion()        { m_excursionFrom.store(m_position.load()); m_excursion.store(0); }
    int  MaxExcursion() const    { return m_excursion.load(); }

private:
    bool Command(int position, bool oneSided, bool pastGuard, const unsigned* sequence = nullptr,
                 bool relative = false) {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        if (!m_isOpen) { m_ledger.Refuse(MoveRefusal::NotOpen, 0, position, 0); return false; }
        if (sequence && *sequence != m_stopGen.load()) {
            m_ledger.Refuse(MoveRefusal::Stopped, 0, position, 0);
            raduk::core::CoreLog("[EAFEmulator] move to %d dropped: a stop was asked for\n", position);
            return false;
        }
        // Phase 157.0e: an absolute number means nothing until the position is confirmed.
        if (!relative && !m_trust.Trusted()) {
            m_ledger.Refuse(MoveRefusal::Untrusted, 0, position, 0);
            raduk::core::CoreLog("[EAFEmulator] move to %d refused: %s\n", position, m_trust.Problem().c_str());
            return false;
        }
        // Phase 169.1: as EAFFocuser.cpp, a new move replaces the one in flight.
        if (!ReplaceMoveInFlight(position)) return false;
        int from = 0;
        const bool known = TryGetPosition(from);
        const int  guard = m_maxTravel.load();
        const MoveRefusal r = CheckMove(known, from, position, GetMaxPosition(), guard, pastGuard);
        if (r != MoveRefusal::None) {
            m_ledger.Refuse(r, from, position, guard);
            raduk::core::CoreLog("[EAFEmulator] move to %d refused: %s\n", position,
                                 m_ledger.LastRefusalText().c_str());
            return false;
        }
        int leg = position;
        if (oneSided) leg = m_approach.Begin(from, position);
        else          m_approach.Direct(from, position);
        if (!Leg(leg)) {
            m_approach.Cancel();
            m_ledger.Idle();
            m_ledger.Refuse(MoveRefusal::Device, from, position, guard);
            return false;
        }
        m_ledger.Commanded(position, leg);
        return true;
    }

    // Phase 169.1: the motor is free for a new move, stopping the one in
    // flight first. The emulator's motor stops at once.
    bool MotorBusy() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_target != m_position.load();
    }
    bool ReplaceMoveInFlight(int position) {
        if (!MotorBusy()) return true;
        const int was = m_ledger.Target();
        m_approach.Cancel();
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_target = m_position.load();
            m_moving = false;
        }
        m_ledger.Idle();
        ++m_replaced;
        raduk::core::CoreLog("[EAFEmulator] move to %d replaces the move in flight to %d: stopped at %d\n",
                             position, was, m_position.load());
        return true;
    }

    // The "device": one motor command.
    bool Leg(int position) {
        if (HandleDead()) {   // phase 157.0e: unplugged
            raduk::core::CoreLog("[EAFEmulator] MoveTo %d failed: the focuser is gone\n", position);
            return false;
        }
        // Phase 169.1: the real EAF answers EAF_ERROR_MOVING to a move
        // while it moves. Off unless a test asks for it.
        if (m_refusesWhileMoving.load() && MotorBusy()) {
            raduk::core::CoreLog("[EAFEmulator] MoveTo %d failed: 5 (moving)\n", position);
            return false;
        }
        int left = m_refuseMoves.load();
        while (left > 0 && !m_refuseMoves.compare_exchange_weak(left, left - 1)) {}
        if (left > 0) {
            raduk::core::CoreLog("[EAFEmulator] MoveTo %d REFUSED (injected)\n", position);
            return false;
        }
        std::lock_guard<std::mutex> lk(m_mutex);
        const int by = m_shortBy.load(), pos = m_position.load();
        m_target = position;
        if (by > 0 && std::abs(position - pos) > by) m_target = position + (position > pos ? -by : by);
        m_startAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_lagMs.load());
        m_moving = (m_lagMs.load() == 0) && (m_target != pos);
        raduk::core::CoreLog("[EAFEmulator] MoveTo %d (from %d)\n", position, pos);
        return true;
    }
public:

    bool MoveRelative(int steps) override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        if (!m_isOpen) { m_ledger.Refuse(MoveRefusal::NotOpen, 0, 0, 0); return false; }
        int target = m_ledger.Target();
        if (target >= 0 && !IsMoving()) target = -1;   // it has finished
        int position = 0;
        if (target < 0 && !TryGetPosition(position)) {
            m_ledger.Refuse(MoveRefusal::PositionUnknown, 0, 0, 0);
            raduk::core::CoreLog("[EAFEmulator] relative move of %d refused: %s\n", steps,
                                 m_ledger.LastRefusalText().c_str());
            return false;
        }
        return Command(RelativeBase(target, position) + steps, true, false, nullptr, true);
    }

    bool Stop() override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        m_stopGen.fetch_add(1);
        if (!m_isOpen) return false;
        m_approach.Cancel();
        m_ledger.Idle();
        std::lock_guard<std::mutex> lk(m_mutex);
        m_target = m_position.load();  // Cancel move — target = current
        m_moving = false;
        raduk::core::CoreLog("[EAFEmulator] Stop at %d\n", m_position.load());
        return true;
    }

    bool IsMoving() const override {
        std::lock_guard<std::recursive_mutex> cmd(m_cmd);
        if (!m_isOpen) return false;
        if (m_stalled.load()) {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_target != m_position.load()) return true;
        }
        if (m_moving.load()) return true;
        int at = 0;
        if (!TryGetPosition(at)) return true;   // unknown counts as moving
        switch (m_ledger.StoppedAt(at, MoveLedger::Clock::now())) {
            case MoveLedger::LegState::Waiting: return true;
            case MoveLedger::LegState::Short: {
                const int target = m_ledger.Target();
                m_approach.Cancel();
                m_ledger.Idle();
                m_ledger.Refuse(MoveRefusal::StoppedShort, at, target, 0);
                raduk::core::CoreLog("[EAFEmulator] %s\n", m_ledger.LastRefusalText().c_str());
                return false;
            }
            case MoveLedger::LegState::Arrived: break;
        }
        // Phase 139.2: the first leg is done; command the second.
        const int next = m_approach.NextLeg(at);
        if (next >= 0) {
            if (!const_cast<EAFFocuser*>(this)->Leg(next)) {
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

    bool TryGetPosition(int& position) const override {
        if (!m_isOpen) return false;
        if (HandleDead()) { m_ledger.ReadFailed(); return false; }   // phase 157.0e
        int left = m_failReads.load();
        while (left > 0 && !m_failReads.compare_exchange_weak(left, left - 1)) {}
        if (left > 0) { m_ledger.ReadFailed(); return false; }
        position = m_position.load();
        m_ledger.Good(position);
        return true;
    }

    // Never an invented number: after a failed read, the last good one.
    int GetPosition() const override {
        int pos = 0;
        if (TryGetPosition(pos)) return pos;
        return m_ledger.Last();
    }

    int GetMaxPosition() const override { return 600000; }

    // ── Configuration ──────────────────────────────────────────

    void SetReverse(bool reversed) override {
        m_reversed = reversed;
        raduk::core::CoreLog("[EAFEmulator] Reverse: %s\n", reversed ? "on" : "off");
    }

    void SetBacklash(int steps) override {
        m_backlash = steps;
        raduk::core::CoreLog("[EAFEmulator] Backlash: %d steps\n", steps);
    }
    int GetBacklash() const override { return m_backlash; }

    // ── Temperature ────────────────────────────────────────────

    float GetTemperature() const override {
        // Stable temperature with minor noise — realistic for a focuser
        float noise = ((m_tempTick.load() % 10) - 5) * 0.02f;
        return 18.3f + noise;
    }

    // ── Info ───────────────────────────────────────────────────

    int FocuserId() const override { return 0; }
    DeviceIdentity Identity() const override {   // phase 157.3
        DeviceIdentity d;
        d.model = "ZWO EAF [EMU]";
        d.firmware = d.sdk = "emulated";
        return d;
    }

private:
    // Motor simulation: moves position toward target at ~1000 steps/sec
    void MotorLoop() {
        const int STEPS_PER_TICK = 10;   // steps moved per millisecond tick
        while (m_isOpen) {
            if (HandleDead()) {   // phase 157.0e: no device, no motor
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                int pos = m_position.load();
                // Phase 157.0: RADUK_EMU_FAULT=eaf-stall@T freezes it the
                // same way SetStalled does (phase 161's test hook).
                const bool faultStalled = m_stallArmed && m_stall.InEffect(
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - m_openedAt).count());
                if (pos != m_target && !m_stalled.load() && !faultStalled &&
                    std::chrono::steady_clock::now() >= m_startAt) {
                    int delta = m_target - pos;
                    int step = std::min(std::abs(delta), STEPS_PER_TICK);
                    if (delta < 0) step = -step;
                    m_position.store(pos + step);
                    const double p = pos + step, o = m_optic.load();
                    if (p > o + m_slackHalf) m_optic.store(p - m_slackHalf);
                    if (p < o - m_slackHalf) m_optic.store(p + m_slackHalf);
                    m_moving = (m_position.load() != m_target);
                    const int ex = std::abs(m_position.load() - m_excursionFrom.load());
                    if (ex > m_excursion.load()) m_excursion.store(ex);
                } else if (pos == m_target) {
                    m_moving = false;
                }
            }
            m_tempTick++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    bool                m_isOpen   = false;
    std::atomic<int>    m_position{166500};
    int                 m_target   = 166500;
    std::atomic<bool>   m_moving{false};
    std::atomic<bool>   m_refusesWhileMoving{false};
    std::atomic<int>    m_replaced{0};
    bool                m_reversed = false;
    int                 m_backlash = 0;
    mutable ApproachSequencer m_approach;
    std::atomic<double> m_optic{166500.0};
    int                 m_slackHalf = 0;
    mutable std::mutex  m_mutex;
    // Phase 161.0a.
    mutable MoveLedger  m_ledger;
    mutable std::recursive_mutex m_cmd;       // phase 161.0b
    std::atomic<unsigned> m_stopGen{0};
    std::atomic<int>    m_maxTravel{kDefaultMaxTravelSteps};
    mutable std::atomic<int> m_failReads{0};
    std::atomic<int>    m_refuseMoves{0};
    std::atomic<bool>   m_stalled{false};
    std::atomic<int>    m_lagMs{0};
    std::atomic<int>    m_shortBy{0};
    std::atomic<int>    m_excursionFrom{166500};
    std::atomic<int>    m_excursion{0};
    std::chrono::steady_clock::time_point m_startAt{};
    std::thread         m_motorThread;
    std::atomic<int>    m_tempTick{0};
    // Phase 157.0e: the record, and an unplugged handle that stays dead.
    mutable PositionTrust m_trust;
    mutable std::atomic<bool> m_handleDead{false};
    double m_openedSec = 0.0;
    bool HandleDead() const {
        if (m_handleDead.load()) return true;
        const Device& d = Dev();
        if (d.dropArmed && m_openedSec < d.drop.startSec && DevSec() >= d.drop.startSec) {   // unplugged since this handle opened
            m_handleDead.store(true);
            raduk::core::CoreLog("[EAFEmulator] the focuser was unplugged (injected)\n");
            return true;
        }
        return false;
    }
    // Phase 157.0: fault injection.
    bool     m_stallArmed = false;
    EmuFault m_stall;
    std::chrono::steady_clock::time_point m_openedAt{};
};

} // namespace raduk::core
