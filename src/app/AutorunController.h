#pragma once
// ============================================================
//  AutorunController -- Phase 153.3: a sequence's wiring, out of main.cpp
//
//  What main.cpp did round AutorunRunner (phase 113), moved here
//  unchanged in behaviour so it can be tested:
//    - Start: reset the sequence's state and the seeing monitor, then the
//      runner.
//    - Tick, from the 500 ms readout: the runner's inputs (the slow ones
//      only when it is about to check), its action carried out -- focus,
//      a run through the RECORD path, a stop, an abort -- each run noted
//      with its file, frames, seeing score and Jupiter note, the seeing
//      hold and the Jupiter deferral announced once a change, the toolbar
//      kept in step, and at the end the summary, the ranked list of runs
//      and the notice.
//    - Pressing: a RECORD press made by the sequence is told apart from a
//      person's, which stops it.
//
//  The runner and the seeing monitor are pure, so the test uses the real
//  ones. Everything outside -- autofocus, recording, the mount's
//  altitude, the disk, Jupiter, the toolbar, the files -- is a hook.
// ============================================================

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "../core/AutorunRunner.h"
#include "../core/RunsBaseline.h"
#include "../core/SeeingMonitor.h"
#include "UiSink.h"

namespace raduk::app {

struct AutorunHooks {
    using Inputs = raduk::core::AutorunRunner::Inputs;
    std::function<void(Inputs&)>            checkInputs;     // target, altitude, seeing, prime, disk
    std::function<void()>                   startFocus;
    std::function<void()>                   abortFocus;
    std::function<bool()>                   startRecording;  // the RECORD path; true if recording
    std::function<void()>                   stopRecording;   // the one stop, and the button
    std::function<uint32_t()>               framesRecorded;
    std::function<std::string()>            runNote;         // what is on Jupiter's disc, or empty
    std::function<void(bool)>               deferChanged;    // phase 144: a sweep put off, or due again
    std::function<void(const std::string&)> session;         // a Capture line in the session log
    std::function<void(ToastLevel, const std::string&)> notify;
    std::function<void(bool, int)>          toolbar;         // running, ordinal
    // Where the ranked list goes: <output dir>\autorun_<stamp>.txt.
    std::function<std::string(const std::string& stamp)> rankingPath;
    // 2026-11-24, the RUNS baseline (core/RunsBaseline.h): the rig's state
    // now, a sweep centred on a position, and a move back to one.
    std::function<void(raduk::core::RunsSnapshot&)> snapshot;
    std::function<void(int centre)>         focusAround;
    std::function<void(int pos)>            moveFocuser;
    std::function<bool()>                   focuserMoving;   // a run waits for the move back
};

class AutorunController {
public:
    using Runner = raduk::core::AutorunRunner;
    Runner runner;

    AutorunController(raduk::core::SeeingMonitor& seeing, IUiSink& ui) : m_seeing(seeing), m_ui(&ui) {}

    void SetHooks(AutorunHooks h) { m_hooks = std::move(h); }
    const AutorunHooks& Hooks() const { return m_hooks; }
    void SetUi(IUiSink& u)        { m_ui = &u; }

    bool Pressing() const          { return m_pressing; }
    void NoteFocusFailed()         { m_focusFailed.store(true); }
    // Every sweep's ending (any thread); judged against the baseline in Tick.
    void NoteFocusResult(bool ok, int pos) {
        m_focusPos.store(ok ? pos : -1);
        m_focusResult.store(true);
    }
    // 2026-11-24: what the owner set up when they pressed RUNS. Given
    // before Start; a sequence started without one behaves as before.
    void SetBaseline(const raduk::core::RunsBaseline& b, int maxDrift) {
        m_baseline = b; m_maxDrift = maxDrift;
    }
    void ClearBaseline()           { m_baseline = {}; }
    const raduk::core::RunsBaseline& Baseline() const { return m_baseline; }
    void SetLastPath(const std::string& p) { m_lastPath = p; }
    const std::vector<raduk::core::RunResult>& Results() const { return m_results; }
    bool Jupiter() const           { return m_jupiter; }

    // A new sequence. False with the runner's reason when it will not start.
    bool Start(const Runner::Config& c, double nowSec, bool jupiter, double seeingOpenFraction) {
        m_jupiter = jupiter;
        m_focusFailed.store(false);
        raduk::core::SeeingMonitor::Config sc;
        sc.openFraction = seeingOpenFraction;
        m_seeing.Reset(sc);
        m_results.clear();
        m_lastPath.clear();
        m_wasHolding = false;
        m_wasHoldingAcq = false;
        m_wasDeferring = false;
        m_runNote.clear();
        m_focusResult.store(false);
        m_awaitResult = false;
        Runner::Config rc = c;
        if (m_baseline.set) rc.focusFirst = false;   // focus is what the owner just made it
        if (!runner.Start(rc, nowSec)) { m_baseline = {}; return false; }
        if (m_baseline.set) {
            Line(raduk::core::BaselineLine(m_baseline) + "\n");
            Line("[RUNS] no sweep before run 1: the owner centred and focused it\n");
        }
        return true;
    }

    // From the 500 ms readout.
    void Tick(double nowSec, bool recording, bool focusing, uint32_t frames) {
        if (!runner.Running() && !m_shown) return;
        if (m_focusFailed.exchange(false)) runner.NoteFocusFailed();
        // A refocus under a baseline holds the next run until its result is
        // judged (and any move back is under way). A sweep that never
        // reports is given up on after kAwaitResultSec without focusing.
        if (m_awaitResult) {
            if (focusing) m_awaitSince = nowSec;
            else if (nowSec - m_awaitSince > kAwaitResultSec) {
                m_awaitResult = false;
                Line("[RUNS] the sweep never reported: going on from where the focuser is\n");
            }
        }
        if (m_focusResult.exchange(false) && runner.Running() && m_baseline.set) {
            m_awaitResult = false;
            const int pos = m_focusPos.load();
            const auto v = raduk::core::JudgeRefocus(m_baseline, pos >= 0, pos, m_maxDrift);
            Line("[RUNS] refocus: " + v.why + "\n");
            if (v.backToBaseline && m_hooks.moveFocuser) m_hooks.moveFocuser(m_baseline.at.focusPos);
        }

        Runner::Inputs ai;
        ai.recording = recording;
        ai.focusing  = focusing || m_awaitResult ||
                       (m_baseline.set && m_hooks.focuserMoving && m_hooks.focuserMoving());
        ai.frames    = frames;
        if (runner.AboutToCheck(nowSec) && m_hooks.checkInputs) m_hooks.checkInputs(ai);

        // A run that ended by itself shows up as the count moving; one
        // ended here shows up as StopRun, and never counts.
        const int ordinalBefore = runner.Ordinal();
        auto noteRun = [&]() {
            m_results.push_back({ordinalBefore, m_lastPath,
                                 m_hooks.framesRecorded ? m_hooks.framesRecorded() : 0u,
                                 m_seeing.EndRun(), m_runNote});
        };
        const int doneBefore = runner.Done();
        // A sequence refused at its first check ended before it was ever
        // shown; ending is judged from before this tick as well.
        const bool runningBefore = runner.Running();

        char b[256];
        switch (runner.Tick(nowSec, ai)) {
        case Runner::Action::StartFocus:
            std::snprintf(b, sizeof(b), "[AUTORUN] focus before run %d\n", runner.Ordinal());
            Line(b);
            if (m_baseline.set && m_baseline.at.focusPos >= 0 && m_hooks.focusAround) {
                m_focusResult.store(false);
                m_awaitResult = true; m_awaitSince = nowSec;
                m_hooks.focusAround(m_baseline.at.focusPos);
            }
            else if (m_hooks.startFocus)
                m_hooks.startFocus();
            break;
        case Runner::Action::StartRun: {
            m_pressing = true;
            const bool ok = m_hooks.startRecording && m_hooks.startRecording();
            m_pressing = false;
            if (ok) {
                m_seeing.BeginRun();
                std::snprintf(b, sizeof(b), "[AUTORUN] run %d of %d\n", runner.Ordinal(), runner.Total());
                Line(b);
                if (m_baseline.set && m_hooks.snapshot) {
                    raduk::core::RunsSnapshot now;
                    m_hooks.snapshot(now);
                    Line(raduk::core::DeltaLine(m_baseline, now, runner.Ordinal()) + "\n");
                }
                m_runNote.clear();
                if (m_jupiter && m_hooks.runNote) {
                    m_runNote = m_hooks.runNote();
                    if (!m_runNote.empty()) Line("[AUTORUN] on the disc: " + m_runNote + "\n");
                }
                std::snprintf(b, sizeof(b), "Autorun: run %d of %d", runner.Ordinal(), runner.Total());
                Session(b);
            } else {
                runner.RunRefused();
            }
            break;
        }
        case Runner::Action::StopRun:
            if (m_hooks.stopRecording) m_hooks.stopRecording();
            noteRun();
            break;
        case Runner::Action::AbortFocus:
            m_awaitResult = false;
            if (m_hooks.abortFocus) m_hooks.abortFocus();
            break;
        default:
            break;
        }

        if (runner.Done() > doneBefore) noteRun();

        if (runner.HoldingForSeeing() != m_wasHolding) {
            m_wasHolding = runner.HoldingForSeeing();
            std::snprintf(b, sizeof(b), "[AUTORUN] seeing %.1f against tonight's best %.1f: %s\n",
                          m_seeing.Current(nowSec), m_seeing.Best(), m_wasHolding ? "waiting" : "going");
            Line(b);
            if (m_wasHolding) Notify(ToastLevel::Info, "Sequence is waiting for the seeing");
        }

        // Phase 157.0c: say once when the sequence waits for acquisition.
        if (runner.HoldingForAcquisition() != m_wasHoldingAcq) {
            m_wasHoldingAcq = runner.HoldingForAcquisition();
            Line(m_wasHoldingAcq ? "[AUTORUN] waiting for acquisition to end before focus or a run\n"
                                 : "[AUTORUN] acquisition has ended: going on\n");
        }

        // Phase 144: say once when a sweep is put off, and when it is due.
        if (runner.DeferringFocus() != m_wasDeferring) {
            m_wasDeferring = runner.DeferringFocus();
            if (m_hooks.deferChanged) m_hooks.deferChanged(m_wasDeferring);
        }

        if (m_hooks.toolbar) m_hooks.toolbar(runner.Running(), runner.Ordinal());
        const bool ended = (m_shown || runningBefore) && !runner.Running();
        m_shown = runner.Running();
        if (ended) { Ended(); m_baseline = {}; }
    }

private:
    void Ended() {
        const bool ok = runner.State() == Runner::Phase::Complete;
        char b[320];
        std::snprintf(b, sizeof(b), "[AUTORUN] end: %d of %d runs, %d empty, %d focus failed, "
                      "%d without the seeing, %d with a sweep put off: %s\n",
                      runner.Done(), runner.Total(), runner.Empty(), runner.FocusFailures(),
                      runner.SeeingGaveUp(), runner.FocusDeferred(), runner.Reason().c_str());
        Line(b);
        // Phase 114: which runs to stack first.
        if (!m_results.empty()) {
            const auto ranked = raduk::core::RankRuns(m_results);
            std::time_t tt = std::time(nullptr);
            std::tm lt{};
#ifdef _WIN32
            localtime_s(&lt, &tt);
#else
            localtime_r(&tt, &lt);
#endif
            char stamp[32];
            std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &lt);
            const std::string text = raduk::core::FormatRunRanking(ranked, runner.Reason());
            const std::string path = m_hooks.rankingPath ? m_hooks.rankingPath(stamp) : std::string();
            FILE* lf = nullptr;
            if (!path.empty()) {
#ifdef _WIN32
                if (fopen_s(&lf, path.c_str(), "w") != 0) lf = nullptr;
#else
                lf = std::fopen(path.c_str(), "w");
#endif
            }
            if (lf) {
                std::fputs(text.c_str(), lf);
                std::fclose(lf);
                std::snprintf(b, sizeof(b), "[AUTORUN] runs ranked in %s, best is run %d\n",
                              path.c_str(), ranked.front().ordinal);
                Line(b);
            } else {
                Line("[AUTORUN] could not write " + path + "\n");
            }
        }
        std::snprintf(b, sizeof(b), "Autorun ended: %d of %d runs: %s",
                      runner.Done(), runner.Total(), runner.Reason().c_str());
        Session(b);
        std::snprintf(b, sizeof(b), "Sequence: %d of %d runs - %s",
                      runner.Done(), runner.Total(), runner.Reason().c_str());
        Notify(ok ? ToastLevel::Info : ToastLevel::Warn, b);
    }

    void Line(const std::string& s)    { m_ui->Line(s); }
    void Session(const std::string& s) { if (m_hooks.session) m_hooks.session(s); }
    void Notify(ToastLevel l, const std::string& s) { if (m_hooks.notify) m_hooks.notify(l, s); }

    raduk::core::SeeingMonitor& m_seeing;
    IUiSink*          m_ui;
    AutorunHooks      m_hooks;
    bool              m_pressing = false;
    bool              m_shown = false;
    bool              m_wasHolding = false;
    bool m_wasHoldingAcq = false;   // phase 157.0c
    bool              m_wasDeferring = false;
    bool              m_jupiter = false;
    std::atomic<bool> m_focusFailed{false};
    std::atomic<bool> m_focusResult{false};
    std::atomic<int>  m_focusPos{-1};
    raduk::core::RunsBaseline m_baseline;
    int               m_maxDrift = raduk::core::kRunsFocusMaxDriftDefault;
    static constexpr double kAwaitResultSec = 10.0;
    bool              m_awaitResult = false;
    double            m_awaitSince = 0.0;
    std::vector<raduk::core::RunResult> m_results;
    std::string       m_lastPath;
    std::string       m_runNote;
};

} // namespace raduk::app
