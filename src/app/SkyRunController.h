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
//  Groups are built here one at a time: the TRACKING group (156.2a, items
//  48 to 50) and the PLANET group (156.2b, items 1, 13, 47, 55, 58). The
//  others are in the plan, and a group with no steps yet ends
//  "not-available: its steps are not built yet", so nothing is ever faked
//  as run. The planet group says how each item came out (Entry::itemStatus):
//  throughput passes or fails on numbers; autofocus's repeatability, the
//  fine pass, the exposure ladder and sharpness are COLLECTED, the evidence
//  gathered and the owner's eye owed.
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

// A finished recording, as the planet group reads it (CaptureManager's
// drop ledger, core/DropLedger.h).
struct RecordingOutcome {
    bool        ok = false;              // it ran its length and stopped normally
    uint64_t    frames = 0;
    uint64_t    unexplained = 0;         // implied missing that nobody accounts for
    uint64_t    droppedSdk = 0, droppedBuffer = 0;
    double      effectiveFps = 0;        // frames over the run's span
    double      expectedFps = 0;         // the profile's rate; 0: not known
    std::string file, why;
};

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
    // 156.2b, the planet group.
    std::function<int()>                             focuserPos;
    std::function<bool(int target, std::string& why)> moveFocuser;  // inside the travel guard, or refused
    std::function<bool()>                            focuserMoving;
    std::function<bool(std::string& why)>            startAutofocus; // AE settles first (AutofocusController)
    // Each sweep's result, numbered like the calibration's: seq rises by
    // one per completed or failed sweep; pos is where it left the focuser.
    std::function<void(int& seq, bool& ok, int& pos, std::string& msg)> lastAutofocus;
    std::function<bool(double seconds, std::string& why)> startRecording;
    std::function<bool()>                            recording;
    std::function<void()>                            stopRecording;
    std::function<void(int& seq, RecordingOutcome& out)> lastRecording;
    std::function<void()>                            saveCamera;     // exposure, gain, AE: put back at the end
    std::function<void()>                            restoreCamera;
    std::function<bool(int us)>                      setExposureUs;
    std::function<bool(int gain)>                    setGain;
    std::function<void(int& lo, int& hi)>            gainRange;      // the camera's own, whatever its units
    std::function<double()>                          highlightFrac;  // the target's highlight, 0..1 of full scale; NaN: not seen
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
    // The planet group (156.2b), from docs/Phase156-Design-Run.md section 3.
    static constexpr int    kDefocusSteps     = 700;    // the owner's answer 2
    static constexpr double kFocuserMoveLimitSec = 120;
    static constexpr double kAutofocusLimitSec = 900;
    static constexpr double kThroughputSec    = 60;     // item 13
    static constexpr double kLadderSec        = 90;     // item 55, each exposure
    static constexpr double kRecordGraceSec   = 60;     // a recording's stop after its length
    static constexpr double kHighlightTarget  = 0.70;   // the owner's answer 3
    static constexpr double kHighlightTol     = 0.07;
    static constexpr double kGainSettleSec    = 2;
    static constexpr int    kGainSteps        = 8;      // bisection over the camera's range
    static constexpr double kFpsReached       = 0.90;   // of the profile's rate

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
        CleanUp();
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
            case sr::Action::Hold:      CleanUp(); m_phase = Phase::Holding; m_stepText = "holding: clouds"; break;
            case sr::Action::Park:
                CleanUp();
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
        m_items.clear();
        m_target = sr::Target{};
        if (!e) return;
        for (const auto& t : m_targets) if (t.name == e->target) m_target = t;
        Log("group " + e->group + " on " + e->target);
    }

    void Finish(sr::Status s, const std::string& detail, double now) {
        const sr::Entry* e = m_sup.Current();
        CleanUp();
        Log("group " + (e ? e->group : std::string("?")) + ": " + sr::StatusText(s) + (detail.empty() ? "" : " (" + detail + ")"));
        Act(m_sup.Result(s, detail, m_measures, now, m_items), now);
    }

    // Leaving a group however it ends (a result, clouds, a park, the
    // emergency stop): a recording it started is stopped, and the camera
    // it changed is put back as it was.
    void CleanUp() {
        if (m_recStarted && m_h.recording && m_h.recording() && m_h.stopRecording) m_h.stopRecording();
        m_recStarted = false;
        if (m_cameraSaved && m_h.restoreCamera) m_h.restoreCamera();
        m_cameraSaved = false;
    }
    void ItemStatus(int item, sr::Status st) {
        for (auto& [n, s] : m_items) if (n == item) { s = st; return; }
        m_items.push_back({item, st});
    }

    void StepGroup(double now) {
        const sr::Entry* e = m_sup.Current();
        if (!e) return;
        if (e->group == "tracking") { StepTracking(now); return; }
        if (e->group == "planet")   { StepPlanet(now); return; }
    }

public:
    // The groups this build can carry out. The plan marks the rest
    // not-available, with the reason.
    static bool Built(const std::string& group) { return group == "tracking" || group == "planet"; }

    // The exposure ladder's gain search, by bisection over the camera's own
    // range, so it holds for any make whatever its gain units (phase 149).
    // Returns the next gain to try, and narrows [lo, hi]; `done` when the
    // highlight is within tolerance or the range is spent.
    static int NextGain(int gain, double highlight, int& lo, int& hi, bool& done) {
        done = false;
        if (std::isfinite(highlight) && std::fabs(highlight - kHighlightTarget) <= kHighlightTol) { done = true; return gain; }
        if (!std::isfinite(highlight) || highlight < kHighlightTarget) lo = gain; else hi = gain;
        if (hi - lo <= 1) { done = true; return (!std::isfinite(highlight) || highlight < kHighlightTarget) ? hi : lo; }
        return lo + (hi - lo) / 2;
    }

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

    // ── The planet group: items 1, 13, 47, 55, 58 (156.2b) ───
    //   0 GoTo           1 the slew ends, lock      2 locked: save the camera,
    //   3 to +700        4 sweep from +700          5 to -700
    //   6 sweep from -700 10 focuser lands          7 throughput, 60 s
    //   8, 9 each rung of the exposure ladder
    static constexpr int kLadderUs[4] = {2000, 5000, 10000, 15000};

    int AfSeq() const {
        int seq = 0, pos = 0; bool ok = false; std::string msg;
        if (m_h.lastAutofocus) m_h.lastAutofocus(seq, ok, pos, msg);
        return seq;
    }
    int RecSeq() const {
        int seq = 0; RecordingOutcome o;
        if (m_h.lastRecording) m_h.lastRecording(seq, o);
        return seq;
    }
    bool StartRec(double sec, std::string& why) {
        m_recSeq0 = RecSeq();
        if (!m_h.startRecording || !m_h.startRecording(sec, why)) return false;
        m_recStarted = true;
        return true;
    }
    // A recording has ended: true, with its outcome. A recording that ran
    // past its length and grace is stopped and reported as not ended well.
    bool RecDone(double now, double sec, RecordingOutcome& out, bool& overdue) {
        overdue = false;
        int seq = 0;
        if (m_h.lastRecording) m_h.lastRecording(seq, out);
        if (seq > m_recSeq0 && !(m_h.recording && m_h.recording())) { m_recStarted = false; return true; }
        if (Elapsed(now) > sec + kRecordGraceSec) {
            if (m_h.stopRecording) m_h.stopRecording();
            m_recStarted = false;
            overdue = true;
            return true;
        }
        return false;
    }
    // Move to the next defocus start, or skip autofocus if the move is refused.
    void DefocusTo(int pass, double now) {
        const int target = m_focus0 + (pass == 0 ? kDefocusSteps : -kDefocusSteps);
        std::string why;
        if (!m_h.moveFocuser || !m_h.moveFocuser(target, why)) {
            m_afOk[pass] = false;
            m_afMsg[pass] = "the focuser refused the move to " + std::to_string(target) + ": " + why;
            if (pass == 0) { DefocusTo(1, now); return; }
            AfterFocus(now);
            return;
        }
        m_afPass = pass;
        m_step = pass == 0 ? 3 : 5;
        m_stepStart = now;
        m_stepText = std::string("focuser to ") + (pass == 0 ? "+" : "-") + std::to_string(kDefocusSteps) + " (item 47)";
    }
    void AfterFocus(double now) {
        if (m_afOk[0] && m_afOk[1]) {
            ItemStatus(47, sr::Status::Collected);   // both completed; what repeatability is good is the owner's call
            ItemStatus(58, sr::Status::Collected);   // each fine pass's figures are in the validation log
            Measure(47, "autofocus repeatability, from +700 and -700", std::fabs((double)(m_afPos[0] - m_afPos[1])), " steps");
            Measure(47, "best focus from +700", m_afPos[0], "");
            Measure(47, "best focus from -700", m_afPos[1], "");
        } else {
            ItemStatus(47, sr::Status::Failed);
            ItemStatus(58, sr::Status::Failed);
            m_afNote = !m_afOk[0] ? "sweep from +700: " + m_afMsg[0] : "sweep from -700: " + m_afMsg[1];
        }
        // A failed last sweep leaves the focuser 700 steps out; put it back
        // at the best focus found, or where it started, before anything else
        // is recorded (and for every group after this one).
        if (!m_afOk[1] && m_h.moveFocuser) {
            const int back = m_afOk[0] ? m_afPos[0] : m_focus0;
            std::string w;
            if (m_h.moveFocuser(back, w)) Log("focuser back to " + std::to_string(back) + " after a failed sweep");
            else Log("focuser could not go back to " + std::to_string(back) + ": " + w);
        }
        m_step = 10; m_stepStart = now; m_stepText = "focuser settling before throughput";
    }
    void StartThroughput(double now) {
        std::string why;
        if (!StartRec(kThroughputSec, why)) {
            ItemStatus(13, sr::Status::Failed);
            m_recNote = "the throughput recording would not start: " + why;
            BeginLadder(now);
            return;
        }
        m_step = 7; m_stepStart = now; m_stepText = "throughput: 60 s at the profile's frame (item 13)";
    }
    void BeginLadder(double now) {
        m_rung = 0;
        m_ladderDone = 0;
        StartRung(now);
    }
    void StartRung(double now) {
        if (m_rung >= 4) { EndPlanet(now); return; }
        if (m_h.setExposureUs) m_h.setExposureUs(kLadderUs[m_rung]);
        m_gainLo = 0; m_gainHi = 0;
        if (m_h.gainRange) m_h.gainRange(m_gainLo, m_gainHi);
        m_gain = m_gainLo + (m_gainHi - m_gainLo) / 2;
        if (m_h.setGain) m_h.setGain(m_gain);
        m_gainTries = 0;
        m_step = 8; m_stepStart = now;
        char b[96];
        std::snprintf(b, sizeof(b), "exposure ladder: %d ms, gain to bring the highlight near 70%% (item 55)", kLadderUs[m_rung] / 1000);
        m_stepText = b;
    }
    void EndPlanet(double now) {
        ItemStatus(55, m_ladderDone == 4 ? sr::Status::Collected : sr::Status::Failed);
        // Sharpness is measured from every frame by the validation log (155.4).
        ItemStatus(1, sr::Status::Collected);
        std::string detail;
        auto add = [&](const std::string& t) { if (!t.empty()) detail += (detail.empty() ? "" : "; ") + t; };
        add(m_afNote);
        add(m_recNote);
        char b[64];
        std::snprintf(b, sizeof(b), "%d of 4 ladder recordings", m_ladderDone);
        add(b);
        add("sharpness and the fine passes in the validation log");
        Finish(sr::OverallOf(m_items), detail, now);
    }

    void StepPlanet(double now) {
        auto lockHeld = [&] { return m_h.lockState && m_h.lockState() == LockState::Locked; };
        switch (m_step) {
            case 0: {
                m_afNote.clear(); m_recNote.clear();
                std::string why;
                if (!m_h.gotoTarget || !m_h.gotoTarget(m_target, why)) { Finish(sr::Status::Failed, "GoTo refused: " + why, now); return; }
                Next(now, "slewing to " + m_target.name);
                return;
            }
            case 1:
                if (m_h.slewing && m_h.slewing()) {
                    if (Elapsed(now) > kSlewLimitSec) Finish(sr::Status::Failed, "the slew did not finish", now);
                    return;
                }
                { std::string why;
                  if (!m_h.startLock || !m_h.startLock(why)) { Finish(sr::Status::Failed, "lock refused: " + why, now); return; } }
                Next(now, "PLANET LOCK");
                return;
            case 2:
                if (m_h.lockState && m_h.lockState() == LockState::Failed) { Finish(sr::Status::Failed, "lock failed", now); return; }
                if (!lockHeld()) {
                    if (Elapsed(now) > kLockLimitSec) Finish(sr::Status::Failed, "lock not made in 5 minutes", now);
                    return;
                }
                if (m_h.saveCamera) { m_h.saveCamera(); m_cameraSaved = true; }
                m_focus0 = m_h.focuserPos ? m_h.focuserPos() : 0;
                m_afOk[0] = m_afOk[1] = false;
                DefocusTo(0, now);
                return;
            case 3: case 5: {   // the defocus move lands, then the sweep
                if (m_h.focuserMoving && m_h.focuserMoving()) {
                    if (Elapsed(now) > kFocuserMoveLimitSec) {
                        m_afMsg[m_afPass] = "the focuser did not arrive in 2 minutes";
                        if (m_afPass == 0) DefocusTo(1, now); else AfterFocus(now);
                    }
                    return;
                }
                m_afSeq0 = AfSeq();
                std::string why;
                if (!m_h.startAutofocus || !m_h.startAutofocus(why)) {
                    m_afMsg[m_afPass] = "autofocus would not start: " + why;
                    if (m_afPass == 0) DefocusTo(1, now); else AfterFocus(now);
                    return;
                }
                m_step++; m_stepStart = now;
                m_stepText = std::string("autofocus from ") + (m_afPass == 0 ? "+" : "-") + std::to_string(kDefocusSteps) + " (items 47, 58)";
                return;
            }
            case 4: case 6: {   // the sweep's result
                int seq = 0, pos = 0; bool ok = false; std::string msg;
                if (m_h.lastAutofocus) m_h.lastAutofocus(seq, ok, pos, msg);
                if (seq <= m_afSeq0) {
                    if (Elapsed(now) > kAutofocusLimitSec) {
                        m_afMsg[m_afPass] = "no result in 15 minutes";
                        if (m_afPass == 0) DefocusTo(1, now); else AfterFocus(now);
                    }
                    return;
                }
                m_afOk[m_afPass] = ok;
                m_afPos[m_afPass] = pos;
                m_afMsg[m_afPass] = msg;
                if (m_afPass == 0) DefocusTo(1, now); else AfterFocus(now);
                return;
            }
            case 7: {   // throughput
                RecordingOutcome o; bool overdue = false;
                if (!RecDone(now, kThroughputSec, o, overdue)) return;
                if (overdue || !o.ok) {
                    ItemStatus(13, sr::Status::Failed);
                    m_recNote = overdue ? "the throughput recording did not stop" : "the throughput recording ended early: " + o.why;
                } else {
                    const bool rate = o.expectedFps <= 0 || o.effectiveFps >= kFpsReached * o.expectedFps;
                    const bool clean = o.unexplained == 0;
                    ItemStatus(13, clean && rate ? sr::Status::Passed : sr::Status::Failed);
                    Measure(13, "unexplained drops", (double)o.unexplained, "", 0);
                    Measure(13, "frame rate", o.effectiveFps, " fps", o.expectedFps > 0 ? o.expectedFps : NAN);
                    if (!clean) m_recNote = std::to_string(o.unexplained) + " unexplained drops in 60 s (item 13)";
                    else if (!rate) {
                        char b[96];
                        std::snprintf(b, sizeof(b), "%.1f fps against the profile's %.1f (item 13)", o.effectiveFps, o.expectedFps);
                        m_recNote = b;
                    }
                }
                BeginLadder(now);
                return;
            }
            case 8: {   // a rung: the gain, by bisection, until the highlight sits near 70%
                if (Elapsed(now) < kGainSettleSec) return;
                const double h = m_h.highlightFrac ? m_h.highlightFrac() : NAN;
                m_lastHighlight = h;
                bool done = false;
                const int g = NextGain(m_gain, h, m_gainLo, m_gainHi, done);
                if (!done && ++m_gainTries < kGainSteps) {
                    m_gain = g;
                    if (m_h.setGain) m_h.setGain(m_gain);
                    m_stepStart = now;
                    return;
                }
                if (g != m_gain) { m_gain = g; if (m_h.setGain) m_h.setGain(m_gain); }
                std::string why;
                if (!StartRec(kLadderSec, why)) {
                    Log("ladder rung " + std::to_string(kLadderUs[m_rung] / 1000) + " ms: the recording would not start: " + why);
                    m_rung++; StartRung(now);
                    return;
                }
                m_step = 9; m_stepStart = now;
                return;
            }
            case 10:    // the focuser has landed (after a failed sweep it is moving back)
                if (m_h.focuserMoving && m_h.focuserMoving() && Elapsed(now) < kFocuserMoveLimitSec) return;
                StartThroughput(now);
                return;
            case 9: {   // the rung's 90 s recording
                RecordingOutcome o; bool overdue = false;
                if (!RecDone(now, kLadderSec, o, overdue)) return;
                const double ms = kLadderUs[m_rung] / 1000.0;
                if (!overdue && o.ok) {
                    m_ladderDone++;
                    Measure(55, "ladder " + std::to_string((int)ms) + " ms: gain", m_gain, "");
                    if (std::isfinite(m_lastHighlight))
                        Measure(55, "ladder " + std::to_string((int)ms) + " ms: highlight", m_lastHighlight * 100.0, "%", kHighlightTarget * 100.0);
                } else {
                    Log("ladder rung " + std::to_string((int)ms) + " ms: " + (overdue ? "did not stop" : "ended early: " + o.why));
                }
                m_rung++;
                StartRung(now);
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
    std::vector<std::pair<int, sr::Status>> m_items;
    // The planet group's state.
    bool   m_cameraSaved = false, m_recStarted = false;
    int    m_focus0 = 0, m_afSeq0 = 0, m_recSeq0 = 0, m_afPass = 0;
    int    m_afPos[2] = {0, 0};
    bool   m_afOk[2] = {false, false};
    std::string m_afMsg[2];
    int    m_rung = 0, m_gain = 0, m_gainLo = 0, m_gainHi = 0, m_gainTries = 0, m_ladderDone = 0;
    double m_lastHighlight = NAN;
    std::string m_afNote, m_recNote;
};

} // namespace raduk::app
