#pragma once
// ============================================================
//  SkyRunController -- phase 156.2a: the unattended run, carried out
//
//  core/SkyRun.h decides (pre-flight, the plan, the supervisor). This
//  carries the decisions out, the way AutorunController carries out
//  AutorunRunner's: one Tick from the app's readout, every device reached
//  through a hook, so the whole night runs headless in tests/sky_run_controller.cpp
//  against a simulated rig. The owner chose this design on 2026-11-21:
//  the app runs the night; Dash starts it, shows it and can stop it
//  (docs/Phase156-Design-Run.md).
//
//  What a night looks like from here:
//    Start   pre-flight on the live rig; refused with every reason, or
//            the plan made from tonight's targets and devices
//    Wait    for the dark window (the Sun below SkyDarkSunAltDeg)
//    Run     each group in the plan's order, through its steps
//    Watch   every tick: the dark window ending, a stall, clouds (hold,
//            then retry), STOP from Dash or the app
//    Park    through the app's own path, then write results.json
//
//  Groups are built here one at a time. This stage has the TRACKING group
//  (items 48 to 50), whose every check a machine can judge; the others
//  are in the plan, and a group with no steps yet ends "not-available:
//  its steps are not built yet", so nothing is ever faked as run.
//
//  Phase 149: nothing here names a sensor size. The box change (3840,
//  1280, 640 on the owner's camera) is the whole sensor, a third and a
//  sixth of its width, from the camera's own size.
// ============================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "../core/SkyRun.h"

namespace raduk::app {

namespace sr = raduk::core::skyrun;

// The tracker as the tracking group sees it.
enum class LockState { None, Acquiring, Locked, Failed };

struct SkyRunHooks {
    // The night.
    std::function<void(sr::PreFlightIn&)>            preflight;   // the live rig: site, limits, mount, camera, disk
    std::function<double()>                          sunAltDeg;
    std::function<std::vector<sr::Target>()>         targets;     // tonight's planets, the Moon, the owner's stars
    std::function<unsigned()>                        devices;     // sr::Device bits that answer
    std::function<void(int& w, int& h)>              sensorSize;  // the camera's full frame
    // Watching.
    std::function<bool()>                            cloudy;      // the target's signal collapsed (measured, not forecast)
    std::function<std::string()>                     stalled;     // a device past its watchdog, or empty
    // Acting.
    std::function<bool(const sr::Target&, std::string& why)> gotoTarget;   // checks the limits again, then slews
    std::function<bool()>                            slewing;
    std::function<bool(std::string& why)>            startLock;
    std::function<LockState()>                       lockState;
    std::function<bool(int w, int h)>                setBox;
    std::function<bool(double dxPx, double dyPx)>    jog;          // the paddle's path
    std::function<double()>                          offCentrePx;  // the target's distance from the frame's centre
    std::function<void()>                            resetTracker; // stop any acquisition (the RESET path)
    // The tracker's last calibration result, numbered: seq rises by one
    // per result (CentroidTracker's calibration callback). Every
    // auto-acquire calibrates afresh (CentroidTracker.cpp, 2026-11-12).
    std::function<void(int& seq, bool& ok, std::string& msg)> lastCalibration;
    std::function<void(int& w, int& h)>              boxNow;       // the frame as it is now
    std::function<bool(double fracOfHalf)>           nudgeToEdge;  // put the target this far toward an edge
    std::function<void()>                            park;
    std::function<bool()>                            parked;
    // Reporting.
    std::function<void(const std::string&)>          log;          // a [SKYRUN] line
    std::function<void(const std::string& json)>     writeResults; // results.json, at the end
};

class SkyRunController {
public:
    enum class Phase { Idle, Refused, WaitingDark, Running, Holding, Parking, Done };

    // Times for the tracking group's steps (seconds). The 25 s re-centre
    // is the checklist's own figure (item 49).
    static constexpr double kSlewLimitSec    = 200;
    static constexpr double kLockLimitSec    = 300;
    static constexpr double kBoxSettleSec    = 10;
    static constexpr double kRecentreSec     = 25;
    static constexpr double kRecentredPx     = 20;
    static constexpr double kJogPx           = 200;
    static constexpr double kCalibrateLimitSec = 300;   // widen, calibrate, lock

    void SetHooks(SkyRunHooks h) { m_h = std::move(h); }

    // Starts the night. False, with every reason in Refusals(), when the
    // pre-flight does not pass.
    bool Start(const std::vector<std::string>& groups, double darkSunAltDeg, bool emulator,
               const std::string& night, const std::string& appVersion, double now) {
        m_refusals.clear();
        m_night = night; m_version = appVersion; m_dark = darkSunAltDeg; m_emulator = emulator;
        sr::PreFlightIn pf;
        if (m_h.preflight) m_h.preflight(pf);
        m_limits = pf.limits;
        m_refusals = sr::PreFlight(pf);
        if (!m_refusals.empty()) {
            m_phase = Phase::Refused;
            Log("refused: " + Join(m_refusals));
            return false;
        }
        const unsigned dev = m_h.devices ? m_h.devices() : 0;
        m_targets = m_h.targets ? m_h.targets() : std::vector<sr::Target>{};
        auto plan = sr::MakePlan(groups, m_targets, dev, m_limits);
        // A group whose steps are not built yet is said to be, never run as
        // a failure (which would mark the checklist FAILED for nothing).
        for (auto& e : plan)
            if (e.status == sr::Status::Pending && !Built(e.group)) {
                e.status = sr::Status::NotAvailable;
                e.detail = "its steps are not built yet";
            }
        m_sup.Begin(std::move(plan), emulator);
        m_phase = Phase::WaitingDark;
        Log("started: " + std::to_string(m_sup.Plan().size()) + " groups planned");
        (void)now;
        return true;
    }

    // STOP from Dash or the app: park now; what is left is not reached.
    void Stop(double now) {
        if (m_phase == Phase::Idle || m_phase == Phase::Done || m_phase == Phase::Refused || m_phase == Phase::Parking) return;
        m_stoppedByOwner = true;
        Act(m_sup.Stopped("STOP pressed", now), now);
    }

    // The emergency stop (ESC, the mount window's STOP): everything halts
    // and nothing moves again, so unlike STOP there is no park slew. What
    // is left is not reached, and results.json is still written.
    void Abort(const std::string& why, double now) {
        if (m_phase == Phase::Idle || m_phase == Phase::Done || m_phase == Phase::Refused) return;
        m_stoppedByOwner = true;
        m_sup.Stopped(why, now);
        m_phase = Phase::Done;
        m_stepText = "stopped, not parked";
        Log("aborted, not parked: " + why);
        if (m_h.writeResults) m_h.writeResults(sr::ResultsJson(m_sup, m_night, m_version));
    }

    // For Dash: "group|target|status|detail" rows joined by ';', the
    // separators taken out of the text.
    std::string PlanRows() const {
        auto clean = [](std::string t) {
            for (auto& c : t) if (c == '|' || c == ';') c = ',';
            return t;
        };
        std::string out;
        for (const auto& e : m_sup.Plan()) {
            if (!out.empty()) out += ';';
            out += clean(e.group) + "|" + clean(e.target) + "|" + sr::StatusText(e.status) + "|" + clean(e.detail);
        }
        return out;
    }

    static const char* PhaseName(Phase p) {
        switch (p) {
            case Phase::Idle: return "idle";        case Phase::Refused: return "refused";
            case Phase::WaitingDark: return "waiting-dark"; case Phase::Running: return "running";
            case Phase::Holding: return "holding";  case Phase::Parking: return "parking";
            case Phase::Done: return "done";
        }
        return "?";
    }

    // The readout's tick (about 2 Hz).
    void Tick(double now) {
        switch (m_phase) {
            case Phase::Idle: case Phase::Refused: case Phase::Done: return;
            case Phase::Parking:
                if (!m_h.parked || m_h.parked()) {
                    m_sup.Parked();
                    m_phase = Phase::Done;
                    Log("parked; " + m_sup.StopReason());
                    if (m_h.writeResults) m_h.writeResults(sr::ResultsJson(m_sup, m_night, m_version));
                }
                return;
            case Phase::WaitingDark:
                if (Dark()) Act(m_sup.Next(now), now);
                return;
            default: break;
        }
        // Running or holding: the night's watches first.
        if (!Dark()) { Act(m_sup.DarkEnds(now), now); return; }
        const std::string st = m_h.stalled ? m_h.stalled() : std::string();
        if (!st.empty()) { Act(m_sup.Stall(st, now), now); return; }
        if (m_phase == Phase::Holding) {
            const bool clear = !(m_h.cloudy && m_h.cloudy());
            Act(m_sup.Tick(clear, now), now);
            return;
        }
        if (m_h.cloudy && m_h.cloudy()) { Act(m_sup.Clouds("the target's signal collapsed", now), now); return; }
        StepGroup(now);
    }

    Phase phase() const { return m_phase; }
    const std::vector<std::string>& Refusals() const { return m_refusals; }
    const sr::Supervisor& Supervisor() const { return m_sup; }
    std::string StepText() const { return m_stepText; }
    bool StoppedByOwner() const { return m_stoppedByOwner; }

private:
    // ── The supervisor's actions ─────────────────────────────
    void Act(sr::Action a, double now) {
        switch (a) {
            case sr::Action::StartNext: BeginGroup(now); m_phase = Phase::Running; break;
            case sr::Action::Hold:      m_phase = Phase::Holding; m_stepText = "holding: clouds"; break;
            case sr::Action::Park:
                m_phase = Phase::Parking;
                m_stepText = "parking";
                Log("parking: " + m_sup.StopReason());
                if (m_h.park) m_h.park();
                break;
            case sr::Action::Done:      m_phase = Phase::Done; break;
            case sr::Action::None:      break;
        }
    }

    void BeginGroup(double now) {
        const sr::Entry* e = m_sup.Current();
        m_step = 0;
        m_stepStart = now;
        m_measures.clear();
        m_target = sr::Target{};
        if (!e) return;
        for (const auto& t : m_targets) if (t.name == e->target) m_target = t;
        Log("group " + e->group + " on " + e->target);
    }

    void Finish(sr::Status s, const std::string& detail, double now) {
        const sr::Entry* e = m_sup.Current();
        Log("group " + (e ? e->group : std::string("?")) + ": " + sr::StatusText(s) + (detail.empty() ? "" : " (" + detail + ")"));
        Act(m_sup.Result(s, detail, m_measures, now), now);
    }

    void StepGroup(double now) {
        const sr::Entry* e = m_sup.Current();
        if (!e) return;
        if (e->group == "tracking") { StepTracking(now); return; }
    }

public:
    // The groups this build can carry out. The plan marks the rest
    // not-available, with the reason.
    static bool Built(const std::string& group) { return group == "tracking"; }

private:

    // ── The tracking group: items 48, 49, 50 ─────────────────
    void Next(double now, const std::string& text) { m_step++; m_stepStart = now; m_stepText = text; }
    double Elapsed(double now) const { return now - m_stepStart; }

    void StepTracking(double now) {
        auto lockHeld = [&] { return m_h.lockState && m_h.lockState() == LockState::Locked; };
        int sw = 0, sh = 0;
        if (m_h.sensorSize) m_h.sensorSize(sw, sh);
        switch (m_step) {
            case 0: {   // GoTo
                std::string why;
                if (!m_h.gotoTarget || !m_h.gotoTarget(m_target, why)) { Finish(sr::Status::Failed, "GoTo refused: " + why, now); return; }
                Next(now, "slewing to " + m_target.name);
                return;
            }
            case 1:     // the slew ends
                if (m_h.slewing && m_h.slewing()) {
                    if (Elapsed(now) > kSlewLimitSec) Finish(sr::Status::Failed, "the slew did not finish", now);
                    return;
                }
                { std::string why;
                  if (!m_h.startLock || !m_h.startLock(why)) { Finish(sr::Status::Failed, "lock refused: " + why, now); return; } }
                Next(now, "locking");
                return;
            case 2:     // locked
                if (m_h.lockState && m_h.lockState() == LockState::Failed) { Finish(sr::Status::Failed, "lock failed", now); return; }
                if (!lockHeld()) {
                    if (Elapsed(now) > kLockLimitSec) Finish(sr::Status::Failed, "lock not made in 5 minutes", now);
                    return;
                }
                m_box = 0;
                BoxTo(0, sw, sh);
                Next(now, "box change (item 48)");
                return;
            case 3:     // box change: full, a third, a sixth, full; lock held throughout
                if (!lockHeld()) { Finish(sr::Status::Failed, "the lock dropped at box " + BoxName(m_box, sw, sh) + " (item 48)", now); return; }
                if (Elapsed(now) < kBoxSettleSec) return;
                if (m_box < 3) { m_box++; BoxTo(m_box, sw, sh); m_stepStart = now; return; }
                Measure(48, "box change held the lock", 1, "");
                if (!m_h.jog || !m_h.jog(kJogPx, 0)) { Finish(sr::Status::Failed, "the jog was refused (item 49)", now); return; }
                Next(now, "hand jog (item 49)");
                return;
            case 4: {   // re-centred within 25 s, lock held
                if (!lockHeld()) { Finish(sr::Status::Failed, "the lock was abandoned after the jog (item 49)", now); return; }
                const double off = m_h.offCentrePx ? m_h.offCentrePx() : 1e9;
                if (off <= kRecentredPx) {
                    Measure(49, "re-centred after a 200 px jog", Elapsed(now), " s", kRecentreSec);
                    // Item 50, as the app behaves since 157.0c: an acquire
                    // from a small box widens, calibrates, locks, and puts
                    // the small box back round the target.
                    BoxTo(2, sw, sh);
                    if (m_h.resetTracker) m_h.resetTracker();
                    m_calSeq0 = CalSeq();
                    std::string why;
                    if (!m_h.startLock || !m_h.startLock(why)) { Finish(sr::Status::Failed, "acquire refused from the small box: " + why + " (item 50)", now); return; }
                    Next(now, "acquire from a small box (item 50)");
                    return;
                }
                if (Elapsed(now) > kRecentreSec) {
                    char b[96];
                    std::snprintf(b, sizeof(b), "not re-centred in %.0f s (still %.0f px off; item 49)", kRecentreSec, off);
                    Finish(sr::Status::Failed, b, now);
                }
                return;
            }
            case 5: {   // calibrated afresh, locked, the small box back
                int seq = 0; bool ok = false; std::string msg;
                if (m_h.lastCalibration) m_h.lastCalibration(seq, ok, msg);
                if (seq > m_calSeq0 && !ok) { Finish(sr::Status::Failed, "calibration from the small box failed: " + msg + " (item 50)", now); return; }
                if (!(seq > m_calSeq0 && lockHeld())) {
                    if (Elapsed(now) > kCalibrateLimitSec) Finish(sr::Status::Failed, "no fresh calibration and lock from the small box in 5 minutes (item 50)", now);
                    return;
                }
                int bw = 0, bh = 0, ew = sw / 6;
                if (m_h.boxNow) m_h.boxNow(bw, bh);
                ew &= ~7;
                if (bw != ew) {
                    if (Elapsed(now) > kCalibrateLimitSec) {
                        char b[128];
                        std::snprintf(b, sizeof(b), "locked, but the frame is %dx%d, not the small box back (item 50)", bw, bh);
                        Finish(sr::Status::Failed, b, now);
                    }
                    return;
                }
                Measure(50, "from a small box: widened, calibrated, locked, small box back", Elapsed(now), " s");
                // Item 50's edge: on the whole frame, near an edge, an
                // acquire must refuse with its message.
                BoxTo(0, sw, sh);
                if (m_h.resetTracker) m_h.resetTracker();
                if (!m_h.nudgeToEdge || !m_h.nudgeToEdge(0.85)) { Finish(sr::Status::Failed, "could not put the target near an edge (item 50)", now); return; }
                Next(now, "near an edge: calibration must refuse (item 50)");
                return;
            }
            case 6: {   // start an acquire near the edge
                if (Elapsed(now) < 3) return;   // let the nudge land
                m_calSeq0 = CalSeq();
                std::string why;
                if (!m_h.startLock || !m_h.startLock(why)) { Finish(sr::Status::Failed, "acquire refused at the edge before calibrating: " + why + " (item 50)", now); return; }
                Next(now, "waiting for calibration to refuse at the edge (item 50)");
                return;
            }
            case 7: {   // it refuses, with its message; then put the target back
                int seq = 0; bool ok = false; std::string msg;
                if (m_h.lastCalibration) m_h.lastCalibration(seq, ok, msg);
                if (seq <= m_calSeq0) {
                    if (Elapsed(now) > kCalibrateLimitSec) Finish(sr::Status::Failed, "no calibration result at the edge (item 50)", now);
                    return;
                }
                if (ok) { Finish(sr::Status::Failed, "calibration went ahead near an edge; it should refuse (item 50)", now); return; }
                if (msg.find("centre it roughly") == std::string::npos) {
                    Finish(sr::Status::Failed, "refused near the edge, but not with its message: \"" + msg + "\" (item 50)", now);
                    return;
                }
                Measure(50, "refused near an edge with its message", 1, "");
                if (m_h.resetTracker) m_h.resetTracker();
                std::string why;
                if (m_h.gotoTarget) m_h.gotoTarget(m_target, why);   // put it back in the middle for whatever runs next
                Finish(sr::Status::Passed, "lock held through the box change, re-centred after the jog, re-acquired from a small box, refused at the edge", now);
                return;
            }
        }
    }

    int CalSeq() const {
        int seq = 0; bool ok = false; std::string msg;
        if (m_h.lastCalibration) m_h.lastCalibration(seq, ok, msg);
        return seq;
    }

    void BoxTo(int i, int sw, int sh) {
        int w = sw, h = sh;
        if (i == 1) { w = sw / 3; h = sh / 3; }
        if (i == 2) { w = sw / 6; h = w * 3 / 4; }
        w &= ~7; h &= ~1;   // the camera's own alignment rules are its backend's; keep it even
        if (m_h.setBox && w > 0 && h > 0) m_h.setBox(w, h);
    }
    static std::string BoxName(int i, int sw, int sh) {
        const char* n[4] = {"full", "a third", "a sixth", "full"};
        (void)sw; (void)sh;
        return n[i < 0 ? 0 : i > 3 ? 3 : i];
    }

    void Measure(int item, const std::string& what, double v, const std::string& unit, double expected = NAN) {
        sr::Measure m;
        m.item = item; m.what = what; m.value = v; m.unit = unit;
        m.hasExpected = std::isfinite(expected); m.expected = m.hasExpected ? expected : 0;
        m_measures.push_back(m);
    }

    bool Dark() const { return !m_h.sunAltDeg || sr::IsDark(m_h.sunAltDeg(), m_dark); }
    void Log(const std::string& s) const { if (m_h.log) m_h.log("[SKYRUN] " + s); }
    static std::string Join(const std::vector<std::string>& v) {
        std::string o;
        for (size_t i = 0; i < v.size(); i++) o += (i ? "; " : "") + v[i];
        return o;
    }

    SkyRunHooks m_h;
    sr::Supervisor m_sup;
    sr::Limits m_limits;
    std::vector<sr::Target> m_targets;
    sr::Target m_target;
    std::vector<sr::Measure> m_measures;
    std::vector<std::string> m_refusals;
    std::string m_night, m_version, m_stepText;
    Phase  m_phase = Phase::Idle;
    double m_dark = sr::kDefaultDarkSunAltDeg;
    bool   m_emulator = false, m_stoppedByOwner = false;
    int    m_step = 0, m_box = 0, m_calSeq0 = 0;
    double m_stepStart = 0;
};

} // namespace raduk::app
