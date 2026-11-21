#pragma once
// ============================================================
//  SkyRunView -- phase 156.2c: what the app's SKY RUN window says
//
//  The owner's rule (2026-11-21): the app is the instrument; Dash is a
//  remote for an iPhone or iPad and a test harness. 156.2a gave the sky
//  run its controls only in Dash, which is the debt this pays: the run is
//  started, watched and stopped from the app's own window
//  (gui/SkyRunWindow.h, Shift+R), and Dash's card mirrors it.
//
//  Pure: the controller's state in, the lines and the buttons' states
//  out, so the wording is tested on Linux (tests/sky_run_view.cpp) and the
//  window only paints what it is given.
// ============================================================

#include <string>
#include <vector>

#include "SkyRunController.h"

namespace raduk::app {

struct SkyRunLine {
    enum class Tone { Normal, Dim, Good, Accent, Warn, Bad };
    std::string text;
    Tone tone = Tone::Normal;
    bool heading = false;
};

struct SkyRunViewModel {
    std::string state;                 // the one-line headline
    std::vector<SkyRunLine> lines;
    bool canStart = false, canStop = false;
};

inline const char* SkyRunPhaseText(SkyRunController::Phase p) {
    using P = SkyRunController::Phase;
    switch (p) {
        case P::Idle:        return "not started";
        case P::Refused:     return "refused";
        case P::WaitingDark: return "waiting for the dark window";
        case P::Running:     return "running";
        case P::Holding:     return "holding";
        case P::Parking:     return "parking";
        case P::Done:        return "done";
    }
    return "?";
}

inline SkyRunLine::Tone SkyRunStatusTone(sr::Status s) {
    using T = SkyRunLine::Tone;
    switch (s) {
        case sr::Status::Passed:       return T::Good;
        case sr::Status::Collected:    return T::Accent;
        case sr::Status::Running:      return T::Accent;
        case sr::Status::Failed:
        case sr::Status::AbortedStall: return T::Bad;
        case sr::Status::Clouded:      return T::Warn;
        default:                       return T::Dim;
    }
}

inline bool SkyRunActive(SkyRunController::Phase p) {
    using P = SkyRunController::Phase;
    return p == P::WaitingDark || p == P::Running || p == P::Holding || p == P::Parking;
}

inline SkyRunViewModel BuildSkyRunView(const SkyRunController& c) {
    using T = SkyRunLine::Tone;
    using P = SkyRunController::Phase;
    SkyRunViewModel v;
    const P phase = c.phase();
    const bool active = SkyRunActive(phase);
    v.canStart = !active;
    v.canStop  = active && phase != P::Parking;

    v.state = SkyRunPhaseText(phase);
    if (phase == P::Done && !c.Supervisor().StopReason().empty()) v.state += ": " + c.Supervisor().StopReason();

    if (phase == P::Idle) {
        v.lines.push_back({"START checks the rig, waits for the dark window, runs each test group that applies "
                           "to what is in view, and parks at the end.", T::Dim});
        v.lines.push_back({"STOP ends the run and parks. Esc or Space (the emergency stop) ends it without a park.", T::Dim});
        v.lines.push_back({"Results go to field-results\\<night>\\results.json.", T::Dim});
        return v;
    }
    if (active && !c.StepText().empty()) v.lines.push_back({"Now: " + c.StepText(), T::Accent});
    if (phase == P::Refused) {
        v.lines.push_back({"Not started. Every reason:", T::Bad, true});
        for (const auto& r : c.Refusals()) v.lines.push_back({"  " + r, T::Bad});
        return v;
    }

    const auto& plan = c.Supervisor().Plan();
    if (!plan.empty()) v.lines.push_back({"PLAN", T::Normal, true});
    for (const auto& e : plan) {
        std::string t = e.group + " on " + e.target + ": " + sr::StatusText(e.status);
        if (!e.detail.empty()) t += " (" + e.detail + ")";
        v.lines.push_back({t, SkyRunStatusTone(e.status)});
        if (!e.itemStatus.empty()) {
            std::string items = "    items:";
            for (size_t i = 0; i < e.itemStatus.size(); i++)
                items += (i ? ", " : " ") + std::to_string(e.itemStatus[i].first) + " " +
                         sr::StatusText(e.itemStatus[i].second);
            v.lines.push_back({items, T::Dim});
        }
    }
    return v;
}

} // namespace raduk::app
