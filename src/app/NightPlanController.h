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
    std::function<void(const char* why)>          stopRuns;
    std::function<bool()>                         runsAborted;    // the last sequence was stopped
    std::function<std::string()>                  runsReason;
    std::function<std::string()>                  mountBusy;      // why, or empty
    std::function<std::string(double jd)>         clock;          // "21:40", local
    std::function<void(const std::string&)>       session;        // a Capture line in the session log
    std::function<void(ToastLevel, const std::string&)> notify;
    std::function<void()>                         changed;        // redraw
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
        char b[200];
        std::snprintf(b, sizeof(b), "[NIGHTPLAN] planned %d visit%s, %d skipped, %s to %s: "
                      "%d min of imaging, %d of it PRIME\n",
                      (int)p.visits.size(), p.visits.size() == 1 ? "" : "s", (int)p.skipped.size(),
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
        std::snprintf(b, sizeof(b), "Night plan: %d visits, %d skipped", (int)p.visits.size(), (int)p.skipped.size());
        Session(b);
        Changed();
    }
    bool HasPlan() const { return m_havePlan; }
    const raduk::core::NightPlan& Plan() const { return m_plan; }
    void ClearPlan() {
        if (runner.Running()) return;
        m_plan = {};
        m_havePlan = false;
        m_ran = false;
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
        m_shown = true;
        m_ran = true;
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
            const char* how = p == Runner::Phase::Complete ? "DONE" : p == Runner::Phase::Aborted ? "STOPPED" : "FAILED";
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

    // From the 500 ms readout.
    void Tick() {
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

    void Ended() {
        const auto p = runner.State();
        const char* how = p == Runner::Phase::Complete ? "finished" : p == Runner::Phase::Aborted ? "stopped" : "FAILED";
        char b[300];
        std::snprintf(b, sizeof(b), "[NIGHTPLAN] plan %s: %d visited, %d skipped: %s\n", how,
                      runner.Visited(), runner.Skipped(), runner.Reason().c_str());
        Line(b);
        std::snprintf(b, sizeof(b), "Night plan %s: %d visited, %d skipped", how, runner.Visited(), runner.Skipped());
        Session(std::string(b) + ": " + runner.Reason());
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
};

} // namespace raduk::app
