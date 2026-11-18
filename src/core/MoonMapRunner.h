#pragma once
// ============================================================
//  MoonMapRunner -- section after section, hands off (phase 162)
//
//  For each section still wanted tonight: slew to it, let the tube stop
//  ringing, lock on the surface, focus if a sweep is due, record one
//  run, credit the section, move on.
//
//  Nothing in that list is new. The slew is the GoTo panel's, the lock
//  is PLANET LOCK guiding on the surface itself (phases 109 and 126),
//  the sweep is autofocus's Moon path and the run is RECORD. This only
//  puts them in order, and it is a pure state machine for the reason
//  MosaicRunner and AutorunRunner are: it runs for an hour with nobody
//  watching, and its failures are timing ones. Tick() returns a Step
//  and the caller performs it.
//
//  Two rules carried over, because each was a bug once:
//
//   - A slew is not finished until it has had time to START
//     (MosaicRunner's slewMinSec). A mount asked "still slewing?" the
//     moment after a GoTo says no, and a runner that believes it records
//     the same patch of Moon under every section's name.
//   - Every wait has a limit that is checked first.
//
//  And one of its own: a section is credited only by a run that ended by
//  itself with frames in it. A run cut short by Abort, a run that came
//  back empty, and a section the lock would not hold are all left
//  wanted, so the map never shows green over nothing.
// ============================================================

#include <cstdint>
#include <string>

namespace raduk::core {

class MoonMapRunner {
public:
    enum class Phase {
        Idle, Slewing, Settling, Locking, Focusing, Recording,
        Complete, Aborted, Failed,
    };

    struct Config {
        int    sections       = 0;      // how many are wanted this sitting
        double settleSec      = 8.0;    // also outlasts the GoTo's own 3 s
        double slewMinSec     = 1.5;
        double slewTimeoutSec = 90.0;

        bool   lock           = true;   // guide on the surface during the run
        double lockLimitSec   = 180.0;  // search, calibrate, centre
        // A section that will not lock is recorded all the same while
        // the Moon is plainly in the frame: at the limb half the frame
        // is sky, and over one run the drift is a few arcseconds, which
        // the stacker takes out. With nothing in the frame it is left
        // wanted, and this many of those in a row end the run.
        bool   recordUnlocked = true;
        int    maxLockFailures = 3;

        int    refocusEvery   = 0;      // sections between sweeps; 0 never
        double focusLimitSec  = 300.0;

        double runSec         = 0.0;    // how long one run should take
        int    maxEmptyRuns   = 2;      // in a row: cloud, or the gate is shut
    };

    enum class Action {
        None,
        Slew,        // to section `index`; the caller drops any lock first
        StartLock,
        StartFocus,
        AbortFocus,
        StartRun,    // record section `index`
        StopRun,     // close the file; this run does not count
        Credit,      // section `index` has a good run
        Skipped,     // section `index` is left wanted; Note() says why
    };
    struct Step {
        Action kind  = Action::None;
        int    index = -1;      // 0..sections-1, in the order given at Start
    };

    struct Inputs {
        bool     slewing   = false;   // the mount's own answer
        bool     locked    = false;
        bool     focusing  = false;
        bool     recording = false;
        uint32_t frames    = 0;       // in the current or latest run
        bool     moonSeen  = false;   // something bright is in the frame
    };

    static const char* Refusal(const Config& c) {
        if (c.sections < 1)       return "no section is both wanted and in sunlight";
        if (c.runSec <= 0.0)      return "no run length: set a frame or time limit";
        if (c.settleSec < 0.0)    return "the settle time is negative";
        if (c.refocusEvery < 0)   return "the refocus interval is negative";
        return nullptr;
    }

    // Half again what was expected and a minute more, as Autorun allows.
    static double RunLimitSec(double runSec) { return runSec * 1.5 + 60.0; }

    bool Start(const Config& c, double nowSec) {
        if (const char* why = Refusal(c)) { Fail(why); return false; }
        m_cfg = c;
        m_at = 0;
        m_credited = m_skipped = m_empty = m_unlocked = 0;
        m_emptyRow = m_lockRow = 0;
        m_sinceFocus = 0;
        m_focusDue = c.refocusEvery > 0;      // the first section, always
        m_stopPending = m_focusStopPending = false;
        m_reason.clear();
        m_note.clear();
        BeginSection(nowSec);
        return true;
    }

    void Abort(const char* why) {
        if (!Running()) return;
        if (m_phase == Phase::Recording) m_stopPending = true;
        if (m_phase == Phase::Focusing)  m_focusStopPending = true;
        m_phase  = Phase::Aborted;
        m_reason = why ? why : "stopped";
    }

    // The caller could not do what it was told.
    void SlewRefused(const char* why) { if (Running()) Fail(why ? why : "the slew was refused"); }
    void RunRefused()                 { if (Running()) Fail("the recording would not start"); }
    // The acquisition said no outright, without waiting for the limit.
    void LockRefused(double nowSec)   { if (m_phase == Phase::Locking) m_lockDeadline = nowSec; }

    Step Tick(double nowSec, const Inputs& in) {
        Step s;
        if (m_stopPending)      { m_stopPending = false;      s.kind = Action::StopRun;    s.index = m_at; return s; }
        if (m_focusStopPending) { m_focusStopPending = false; s.kind = Action::AbortFocus; s.index = m_at; return s; }
        if (!Running()) return s;

        const double in_ = nowSec - m_entered;
        s.index = m_at;

        switch (m_phase) {
        case Phase::Slewing:
            if (!m_issued) { m_issued = true; s.kind = Action::Slew; return s; }
            if (in_ > m_cfg.slewTimeoutSec) {
                Fail(m_sawSlew ? "a slew did not finish" : "a slew did not start");
                break;
            }
            if (in.slewing) { m_sawSlew = true; break; }
            if (m_sawSlew || in_ >= m_cfg.slewMinSec) Enter(Phase::Settling, nowSec);
            break;

        case Phase::Settling:
            if (in_ < m_cfg.settleSec) break;
            if (m_cfg.lock) {
                Enter(Phase::Locking, nowSec);
                m_lockDeadline = nowSec + m_cfg.lockLimitSec;
                s.kind = Action::StartLock;
                return s;
            }
            return AfterLock(nowSec);

        case Phase::Locking:
            if (in.locked) { m_lockRow = 0; return AfterLock(nowSec); }
            if (nowSec >= m_lockDeadline) {
                if (m_cfg.recordUnlocked && in.moonSeen) {
                    ++m_unlocked;
                    m_lockRow = 0;
                    return AfterLock(nowSec);
                }
                ++m_lockRow;
                if (m_lockRow >= m_cfg.maxLockFailures) {
                    Fail("no lock and nothing in the frame, section after section: is the Moon there?");
                    break;
                }
                return Skip(nowSec, "no lock, and nothing bright in the frame");
            }
            break;

        case Phase::Focusing:
            if (in_ > m_cfg.focusLimitSec) {
                Fail("the focus sweep did not finish");
                s.kind = Action::AbortFocus;
                return s;
            }
            if (in.focusing) { m_sawFocus = true; break; }
            // A sweep takes a moment to report itself, as a slew does.
            if (!m_sawFocus && in_ < 2.0) break;
            m_focusDue = false;
            m_sinceFocus = 0;
            return BeginRun(nowSec);

        case Phase::Recording:
            if (!in.recording) {
                if (in.frames == 0) {
                    ++m_empty; ++m_emptyRow;
                    if (m_emptyRow >= m_cfg.maxEmptyRuns) {
                        Fail("runs are coming back empty: cloud, or the gate is shut");
                        break;
                    }
                    return Skip(nowSec, "the run came back empty");
                }
                m_emptyRow = 0;
                ++m_credited;
                ++m_sinceFocus;
                if (m_cfg.refocusEvery > 0 && m_sinceFocus >= m_cfg.refocusEvery) m_focusDue = true;
                s.kind = Action::Credit;
                Advance(nowSec);
                return s;
            }
            if (in_ > RunLimitSec(m_cfg.runSec)) {
                Fail("a run did not end: is the camera still sending?");
                s.kind = Action::StopRun;
                return s;
            }
            break;

        default: break;
        }
        s.kind = Action::None;
        return s;
    }

    // ── Readout ────────────────────────────────────────────────
    Phase State() const { return m_phase; }
    bool Running() const {
        return m_phase == Phase::Slewing || m_phase == Phase::Settling || m_phase == Phase::Locking ||
               m_phase == Phase::Focusing || m_phase == Phase::Recording;
    }
    int  Index()    const { return m_at; }            // the section in hand
    int  Total()    const { return m_cfg.sections; }
    int  Credited() const { return m_credited; }
    int  Skipped()  const { return m_skipped; }
    int  Empty()    const { return m_empty; }
    int  Unlocked() const { return m_unlocked; }   // recorded without a lock
    double RunSec() const { return m_cfg.runSec; }
    const std::string& Reason() const { return m_reason; }
    const std::string& Note()   const { return m_note; }   // why the last section was skipped

    static const char* PhaseName(Phase p) {
        switch (p) {
            case Phase::Idle:      return "idle";
            case Phase::Slewing:   return "slewing";
            case Phase::Settling:  return "settling";
            case Phase::Locking:   return "locking";
            case Phase::Focusing:  return "focusing";
            case Phase::Recording: return "recording";
            case Phase::Complete:  return "complete";
            case Phase::Aborted:   return "stopped";
            case Phase::Failed:    return "failed";
        }
        return "idle";
    }

private:
    Step AfterLock(double nowSec) {
        Step s; s.index = m_at;
        if (m_focusDue) {
            Enter(Phase::Focusing, nowSec);
            m_sawFocus = false;
            s.kind = Action::StartFocus;
            return s;
        }
        return BeginRun(nowSec);
    }
    Step BeginRun(double nowSec) {
        Step s; s.index = m_at;
        Enter(Phase::Recording, nowSec);
        s.kind = Action::StartRun;
        return s;
    }
    Step Skip(double nowSec, const char* why) {
        Step s; s.index = m_at; s.kind = Action::Skipped;
        m_note = why;
        ++m_skipped;
        Advance(nowSec);
        return s;
    }
    void Advance(double nowSec) {
        ++m_at;
        if (m_at >= m_cfg.sections) {
            m_phase = Phase::Complete;
            m_reason = m_skipped == 0 ? "every section wanted tonight is captured"
                                      : "done, with sections left for another try";
            return;
        }
        BeginSection(nowSec);
    }
    void BeginSection(double nowSec) {
        Enter(Phase::Slewing, nowSec);
        m_issued = false;
        m_sawSlew = false;
    }
    void Enter(Phase p, double nowSec) { m_phase = p; m_entered = nowSec; }
    void Fail(const char* why) { m_phase = Phase::Failed; m_reason = why ? why : "failed"; }

    Config m_cfg;
    Phase  m_phase = Phase::Idle;
    double m_entered = 0.0, m_lockDeadline = 0.0;
    int    m_at = 0;
    int    m_credited = 0, m_skipped = 0, m_empty = 0, m_unlocked = 0;
    int    m_emptyRow = 0, m_lockRow = 0, m_sinceFocus = 0;
    bool   m_issued = false, m_sawSlew = false, m_sawFocus = false, m_focusDue = false;
    bool   m_stopPending = false, m_focusStopPending = false;
    std::string m_reason, m_note;
};

// Every path is exercised by tests/moon_map_run.cpp on a fake clock and a
// fake rig. Nothing here has stepped a real mount across the Moon.
inline constexpr bool MOON_MAP_RUN_PROVEN_ON_SKY = false;

} // namespace raduk::core
