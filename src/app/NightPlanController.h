#pragma once
// ============================================================
//  NightPlanController -- Phase 163: the night plan's wiring
//
//  Holds tonight's plan (core/NightPlan.h) and carries the runner's steps
//  out through hooks, as MoonMapController does for the Moon map:
//
//    Slew       the GoTo panel's GoTo by name, which loads the target's
//               profile and arms the search, as any GoTo does
//    StartLock  PLANET LOCK's full acquisition (wide, then the ROI)
//    StartRuns  the RUNS sequence, as the RUNS button starts it
//    Visited    the runs are done, or the slot ended and they are stopped
//    Skipped    this target is left; the plan goes on
//
//  Every step and skip goes to the session log with its reason, and to
//  raduk_log.txt as a [NIGHTPLAN] line. Everything outside is a hook.
// ============================================================

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "../core/NightPlan.h"
#include "UiSink.h"

namespace raduk::app {

struct NightPlanHooks {
    std::function<double()>                       jdNow;          // UT
    std::function<double(const std::string&)>     altNow;         // computed, not the mount's
    std::function<bool(const std::string& name, std::string& why)> slew;
    std::function<bool()>                         mountSlewing;
    std::function<bool(std::string& why)>         startLock;
    std::function<bool()>                         locked;
    std::function<void()>                         dropLock;
    std::function<bool(std::string& why)>         startRuns;
    std::function<bool()>                         runsRunning;
    std::function<bool()>                         framesFlowing;  // a frame rate at this size
    std::function<void(const char* why)>          stopRuns;
    std::function<bool()>                         runsAborted;    // the last sequence was stopped
    std::function<std::string()>                  runsReason;
    std::function<std::string()>                  mountBusy;      // why, or empty
    std::function<std::string(double jd)>         clock;          // "21:40", local
    std::function<void(const std::string&)>       session;        // a Capture line in the session log
    std::function<void(ToastLevel, const std::string&)> notify;
    std::function<void()>                         changed;        // redraw
    // Phase 163b: CONTINUE plans what is left again, from now.
    std::function<raduk::core::NightPlan(const std::vector<std::string>& names)> replan;
    std::function<std::vector<std::string>()>     ticked;         // the targets ticked now
};

// One line of the plan as shown: what has been done tonight, then what
// is planned, then what was left out.
struct NightRow {
    std::string slew, runs, name, state, why;
};

class NightPlanController {
public:
    using Runner = raduk::core::NightPlanRunner;
    Runner runner;

    explicit NightPlanController(IUiSink& ui) : m_ui(&ui) {}
    void SetHooks(NightPlanHooks h) { m_hooks = std::move(h); }
    void SetUi(IUiSink& u)          { m_ui = &u; }

    // ── The plan ──────────────────────────────────────────────
    void SetPlan(const raduk::core::NightPlan& p) {
        m_plan = p;
        m_havePlan = true;
        m_ran = false;
        m_paused = false;
        m_history.clear();
        LogPlan(p, "planned");
        char b[120];
        std::snprintf(b, sizeof(b), "Night plan: %d visits, %d skipped", (int)p.visits.size(), (int)p.skipped.size());
        Session(b);
        Changed();
    }

    void LogPlan(const raduk::core::NightPlan& p, const char* verb) {
        char b[200];
        std::snprintf(b, sizeof(b), "[NIGHTPLAN] %s %d visit%s, %d skipped, %s to %s: "
                      "%d min of imaging, %d of it PRIME\n",
                      verb, (int)p.visits.size(), p.visits.size() == 1 ? "" : "s", (int)p.skipped.size(),
                      Clock(p.startJd).c_str(), Clock(p.endJd).c_str(),
                      p.TotalImagingMin(), p.TotalPrimeMin());
        Line(b);
        for (size_t i = 0; i < p.visits.size(); ++i) {
            const auto& v = p.visits[i];
            std::snprintf(b, sizeof(b), "[NIGHTPLAN] visit %d: %s, slew %s, runs %s-%s: %s\n",
                          (int)i + 1, v.name.c_str(), Clock(v.slewJd).c_str(), Clock(v.imageJd).c_str(),
                          Clock(v.endJd).c_str(), v.Why().c_str());
            Line(b);
        }
        for (const auto& s : p.skipped)
            Line("[NIGHTPLAN] not planned: " + s.name + ": " + s.why + "\n");
        for (const auto& s : p.busy)
            Line("[NIGHTPLAN] kept clear: the pass of " + s.label + ", " + Clock(s.startJd) +
                 "-" + Clock(s.endJd) + "\n");
    }
    bool HasPlan() const { return m_havePlan; }
    const raduk::core::NightPlan& Plan() const { return m_plan; }
    void ClearPlan() {
        if (runner.Running()) return;
        m_plan = {};
        m_havePlan = false;
        m_ran = false;
        m_paused = false;
        m_history.clear();
        Changed();
    }

    // ── Running it ────────────────────────────────────────────
    bool Start(const Runner::Config& c, std::string& why) {
        if (runner.Running())  { why = "the plan is already running"; return false; }
        if (!m_havePlan)       { why = "no plan: press PLAN TONIGHT first"; return false; }
        if (m_plan.visits.empty()) { why = "the plan has no visits"; return false; }
        const double now = Jd();
        if (now >= m_plan.visits.back().endJd) { why = "every visit in the plan is over: plan again"; return false; }
        if (!runner.Start(m_plan, c, now)) { why = runner.Reason(); return false; }
        m_cfg = c;
        m_shown = true;
        m_ran = true;
        m_paused = false;
        m_history.clear();
        m_doneBefore = m_skippedBefore = 0;
        char b[200];
        std::snprintf(b, sizeof(b), "[NIGHTPLAN] running: %d visits, lock limit %.0f s\n",
                      runner.Total(), c.lockLimitSec);
        Line(b);
        std::snprintf(b, sizeof(b), "Night plan running: %d visits", runner.Total());
        Session(b);
        Changed();
        return true;
    }

    void Stop(const char* why) {
        if (!runner.Running()) return;
        m_paused = false;
        runner.Abort(why);
        Tick();     // carry out the stop now: close the runs, drop the lock
    }

    // The target the plan is on, or "" between visits.
    std::string CurrentTarget() const {
        if (!runner.Running()) return std::string();
        const auto p = runner.State();
        if (p == Runner::Phase::Waiting) return std::string();
        return runner.Visits()[(size_t)runner.Index()].name;
    }

    // One line for the panel and Dash.
    std::string StateLine() const {
        if (m_paused) {
            const auto& v = runner.Visits()[(size_t)runner.Index()];
            return std::string(m_pausedOn ? "PAUSED on " : "PAUSED before ") + v.name +
                   ": CONTINUE plans the rest from the sky then";
        }
        if (runner.Running()) {
            const auto& v = runner.Visits()[(size_t)runner.Index()];
            char b[200];
            if (runner.State() == Runner::Phase::Waiting)
                std::snprintf(b, sizeof(b), "RUNNING %d/%d: %s next, slew at %s", runner.Index() + 1,
                              runner.Total(), v.name.c_str(), Clock(v.slewJd).c_str());
            else
                std::snprintf(b, sizeof(b), "RUNNING %d/%d: %s %s, until %s", runner.Index() + 1,
                              runner.Total(), v.name.c_str(), Runner::PhaseName(runner.State()),
                              Clock(v.endJd).c_str());
            return b;
        }
        if (m_ran) {
            const auto p = runner.State();
            if (p == Runner::Phase::Complete) {
                // The whole night, across any CONTINUE, not the last stretch.
                char b[96];
                std::snprintf(b, sizeof(b), "DONE: %d visited, %d skipped",
                              m_doneBefore + runner.Visited(), m_skippedBefore + runner.Skipped());
                return b;
            }
            const char* how = p == Runner::Phase::Aborted ? "STOPPED" : "FAILED";
            return std::string(how) + ": " + runner.Reason();
        }
        if (!m_havePlan) return "NO PLAN";
        char b[96];
        std::snprintf(b, sizeof(b), "PLANNED: %d visits, %d skipped", (int)m_plan.visits.size(),
                      (int)m_plan.skipped.size());
        return b;
    }

    // What a visit is doing, for the panel's list and Dash.
    std::string VisitState(int i) const {
        if (!m_ran) return "planned";
        using O = Runner::Outcome;
        switch (runner.OutcomeOf(i)) {
        case O::Active:  return Runner::PhaseName(runner.State());
        case O::Done:    return "done: " + runner.NoteOf(i);
        case O::Skipped: return "skipped: " + runner.NoteOf(i);
        default:         return runner.Running() ? "waiting" : "not reached";
        }
    }

    std::string Clock(double jd) const {
        if (m_hooks.clock) return m_hooks.clock(jd);
        char b[16];
        std::snprintf(b, sizeof(b), "%.4f", jd);
        return b;
    }

    // ── Phase 163b: pause, and continue with the rest planned again ──
    //
    // PAUSE stops the plan's clock and nothing else: the runs, the lock and
    // the mount are left as they are and are the operator's, to stay longer
    // on a target or do something else. The ticks can be changed meanwhile.
    //
    // CONTINUE keeps what is done, counts the visit it was paused on as
    // visited if its runs had started, and plans every ticked target not
    // yet visited again from now: the windows from the sky as it is then,
    // the slew, lock and focus before each visit, and the ticked passes.
    bool Paused() const { return m_paused; }

    bool Pause(std::string& why) {
        if (!runner.Running()) { why = "the plan is not running"; return false; }
        if (m_paused)          { why = "the plan is already paused"; return false; }
        m_paused = true;
        m_pausedPhase = runner.State();
        m_pausedOn = m_pausedPhase != Runner::Phase::Waiting;
        const auto& v = runner.Visits()[(size_t)runner.Index()];
        Line(std::string("[NIGHTPLAN] paused ") + (m_pausedOn ? "on " : "before ") + v.name + " (" +
             Runner::PhaseName(m_pausedPhase) + "): the plan's clock is stopped; the runs, the lock and "
             "the mount are left as they are. CONTINUE plans the rest from the sky then\n");
        Session("Night plan paused " + std::string(m_pausedOn ? "on " : "before ") + v.name);
        Changed();
        return true;
    }

    bool Continue(std::string& why) {
        if (!m_paused) { why = "the plan is not paused"; return false; }
        const auto& visits = runner.Visits();
        const int at = runner.Index();
        // What this stretch of the plan did stays in the history.
        for (int i = 0; i < at && i < (int)visits.size(); ++i)
            m_history.push_back(RowOf(visits[(size_t)i], VisitState(i)));
        if (at < (int)visits.size()) {
            const auto& v = visits[(size_t)at];
            if (m_pausedPhase == Runner::Phase::Running) {
                // Its runs started: it was visited, for as long as wanted.
                if (m_hooks.runsRunning && m_hooks.runsRunning() && m_hooks.stopRuns)
                    m_hooks.stopRuns("the night plan continues");
                m_history.push_back(RowOf(v, "done: stayed on through a pause"));
                ++m_doneBefore;
            } else if (m_pausedPhase != Runner::Phase::Waiting) {
                if (m_hooks.dropLock) m_hooks.dropLock();
            }
        }
        m_doneBefore += runner.Visited();
        m_skippedBefore += runner.Skipped();

        // Every ticked target not yet visited tonight.
        std::vector<std::string> pending;
        const std::vector<std::string> ticks = m_hooks.ticked ? m_hooks.ticked() : std::vector<std::string>();
        for (const auto& n : ticks) {
            bool done = false;
            for (const auto& h : m_history)
                if (h.name == n && h.state.rfind("done", 0) == 0) { done = true; break; }
            if (!done) pending.push_back(n);
        }
        m_paused = false;
        Line("[NIGHTPLAN] continuing: " + std::to_string(pending.size()) + " ticked target" +
             (pending.size() == 1 ? "" : "s") + " not yet visited, planned again from now\n");
        raduk::core::NightPlan p;
        if (!pending.empty() && m_hooks.replan) p = m_hooks.replan(pending);
        m_plan = p;
        LogPlan(p, "re-planned");
        if (p.visits.empty()) {
            runner.Close(pending.empty() ? "every ticked target is visited"
                                         : "nothing left fits a GOOD or PRIME window");
        } else if (!runner.Start(p, m_cfg, Jd())) {
            runner.Close(runner.Reason().c_str());
        }
        char b[120];
        std::snprintf(b, sizeof(b), "Night plan continued: %d visits re-planned", (int)p.visits.size());
        Session(b);
        Changed();
        if (!runner.Running()) { m_shown = false; Ended(); }
        return true;
    }

    // What has been done tonight, then the plan, then what it left out.
    std::vector<NightRow> Rows() const {
        std::vector<NightRow> out = m_history;
        if (!m_havePlan) return out;
        for (int i = 0; i < (int)m_plan.visits.size(); ++i)
            out.push_back(RowOf(m_plan.visits[(size_t)i], VisitState(i)));
        for (const auto& s : m_plan.skipped)
            out.push_back({"--", "--", s.name, "not planned", s.why});
        return out;
    }

    // From the 500 ms readout.
    void Tick() {
        if (m_paused) return;          // the clock is stopped
        if (!runner.Running() && !m_shown) return;

        Runner::Inputs in;
        in.nowJd = Jd();
        const auto before = runner.State();
        const int at = runner.Index();
        std::string busy;
        if (runner.Running()) {
            const auto& v = runner.Visits()[(size_t)at];
            if (before == Runner::Phase::Waiting) {
                if (m_hooks.mountBusy) busy = m_hooks.mountBusy();
                in.mountBusy = !busy.empty();
                // Early only when it is already as high as the plan puts
                // it: a PRIME visit does not start early in GOOD.
                const double need = v.primeMin > 0 ? raduk::core::NIGHT_PRIME_DEG
                                                   : raduk::core::NIGHT_GOOD_DEG;
                in.inWindow = m_hooks.altNow && m_hooks.altNow(v.name) >= need;
            }
            // Asked only while a slew is awaited: it is a query down the
            // mount's cable.
            in.slewing = before == Runner::Phase::Slewing && m_hooks.mountSlewing && m_hooks.mountSlewing();
            in.locked  = before == Runner::Phase::Locking && m_hooks.locked && m_hooks.locked();
            in.framesFlowing = !m_hooks.framesFlowing || m_hooks.framesFlowing();
            in.runsRunning = m_hooks.runsRunning && m_hooks.runsRunning();
            in.runsByPerson = before == Runner::Phase::Running && !in.runsRunning &&
                              m_hooks.runsAborted && m_hooks.runsAborted();
        }
        if (in.mountBusy && busy != m_busyShown) {
            Line("[NIGHTPLAN] waiting: " + busy + "\n");
            m_busyShown = busy;
        } else if (!in.mountBusy) {
            m_busyShown.clear();
        }

        const auto s = runner.Tick(in);
        const std::string name = (s.index >= 0 && s.index < runner.Total())
                                     ? runner.Visits()[(size_t)s.index].name : std::string();
        char b[300];
        std::string why;
        using A = Runner::Action;
        switch (s.kind) {
        case A::Slew: {
            const auto& v = runner.Visits()[(size_t)s.index];
            std::snprintf(b, sizeof(b), "[NIGHTPLAN] %s (%d of %d): slewing, %s; %s\n", name.c_str(),
                          s.index + 1, runner.Total(), runner.Note().c_str(), v.Why().c_str());
            Line(b);
            Session("Night plan: GoTo " + name + " (" + runner.Note() + ")");
            if (m_hooks.dropLock) m_hooks.dropLock();
            if (!m_hooks.slew || !m_hooks.slew(name, why)) {
                if (why.empty()) why = "the slew was refused";
                runner.SlewRefused(why.c_str(), in.nowJd);
                Skipped(name, why);
            }
            break;
        }
        case A::StartLock:
            Line("[NIGHTPLAN] " + name + ": locking\n");
            if (!m_hooks.startLock || !m_hooks.startLock(why)) {
                if (why.empty()) why = "acquisition refused";
                runner.LockRefused(why.c_str(), in.nowJd);
                if (m_hooks.dropLock) m_hooks.dropLock();
                Skipped(name, "no lock: " + why);
            }
            break;
        case A::StartRuns:
            Line("[NIGHTPLAN] " + name + ": locked, starting the runs\n");
            Session("Night plan: " + name + " locked, runs started");
            if (!m_hooks.startRuns || !m_hooks.startRuns(why)) {
                if (why.empty()) why = "refused";
                runner.RunsRefused(why.c_str(), in.nowJd);
                Skipped(name, "the runs would not start: " + why);
            }
            break;
        case A::Visited: {
            if (s.stopRuns && m_hooks.stopRuns) m_hooks.stopRuns("the night plan's slot ended");
            std::string note = runner.Note();
            if (!s.stopRuns && m_hooks.runsReason) {
                const std::string r = m_hooks.runsReason();
                if (!r.empty()) note += " (" + r + ")";
            }
            Line("[NIGHTPLAN] " + name + " visited: " + note + "\n");
            Session("Night plan: " + name + " done: " + note);
            break;
        }
        case A::Skipped:
            if (s.dropLock && m_hooks.dropLock) m_hooks.dropLock();
            Skipped(name, runner.Note());
            break;
        case A::Stop:
            if (s.stopRuns && m_hooks.stopRuns) m_hooks.stopRuns(runner.Reason().c_str());
            if (s.dropLock && m_hooks.dropLock) m_hooks.dropLock();
            break;
        default:
            break;
        }

        if (runner.State() != before || s.kind != A::None) Changed();
        if (m_shown && !runner.Running()) { m_shown = false; Ended(); }
    }

private:
    void Skipped(const std::string& name, const std::string& why) {
        Line("[NIGHTPLAN] " + name + " skipped: " + why + "\n");
        Session("Night plan: " + name + " skipped: " + why);
    }

    NightRow RowOf(const raduk::core::NightVisit& v, const std::string& state) const {
        return {Clock(v.slewJd), Clock(v.imageJd) + "-" + Clock(v.endJd), v.name, state, v.Why()};
    }

    void Ended() {
        const auto p = runner.State();
        const char* how = p == Runner::Phase::Complete ? "finished" : p == Runner::Phase::Aborted ? "stopped" : "FAILED";
        // A complete plan's reason is only the count again; any other
        // ending says why.
        const bool done = p == Runner::Phase::Complete;
        const std::string why = done ? std::string() : " (" + runner.Reason() + ")";
        char b[300];
        std::snprintf(b, sizeof(b), "Night plan %s: %d visited, %d skipped", how,
                      m_doneBefore + runner.Visited(), m_skippedBefore + runner.Skipped());
        Line("[NIGHTPLAN] plan " + std::string(b).substr(11) + why + "\n");
        Session(std::string(b) + why);
        Notify(p == Runner::Phase::Complete ? ToastLevel::Info : ToastLevel::Warn,
               p == Runner::Phase::Complete ? std::string(b) : std::string(b) + " (" + runner.Reason() + ")");
        Changed();
    }

    double Jd() const { return m_hooks.jdNow ? m_hooks.jdNow() : 0.0; }
    void Line(const std::string& s)    { m_ui->Line(s); }
    void Session(const std::string& s) { if (m_hooks.session) m_hooks.session(s); }
    void Notify(ToastLevel l, const std::string& s) { if (m_hooks.notify) m_hooks.notify(l, s); }
    void Changed()                     { if (m_hooks.changed) m_hooks.changed(); }

    IUiSink*       m_ui;
    NightPlanHooks m_hooks;
    raduk::core::NightPlan m_plan;
    bool           m_havePlan = false;
    bool           m_ran = false;
    bool           m_shown = false;
    std::string    m_busyShown;
    Runner::Config m_cfg;
    bool           m_paused = false;
    bool           m_pausedOn = false;          // on a target, not between two
    Runner::Phase  m_pausedPhase = Runner::Phase::Idle;
    std::vector<NightRow> m_history;            // before the last CONTINUE
    int            m_doneBefore = 0, m_skippedBefore = 0;
};

} // namespace raduk::app
