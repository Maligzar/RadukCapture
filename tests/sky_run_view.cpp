// ============================================================
//  Phase 156.2c: what the app's SKY RUN window says (app/SkyRunView.h)
//
//  The owner's rule (2026-11-21): the app is the instrument, Dash a remote.
//  The window shows the same night as Dash's card, from the controller:
//    1. Idle: what START does, START on, STOP off.
//    2. Refused: every reason, START on again.
//    3. Running: the step, the plan with each group's status and its
//       items' statuses, START off, STOP on.
//    4. Parking: STOP off (it is already ending). Done: why it ended.
// ============================================================

#include "app/SkyRunView.h"

#include <cstdio>
#include <string>

using namespace raduk::app;
namespace sr = raduk::core::skyrun;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}
static bool Has(const SkyRunViewModel& v, const std::string& needle, SkyRunLine::Tone* tone = nullptr) {
    for (const auto& l : v.lines)
        if (l.text.find(needle) != std::string::npos) { if (tone) *tone = l.tone; return true; }
    return false;
}

// A rig good enough to start, with the run stepped by hand.
struct Rig {
    double now = 0, sunAlt = -15;
    bool aligned = true, parked = false;
    SkyRunHooks Hooks() {
        SkyRunHooks h;
        h.preflight = [this](sr::PreFlightIn& p) {
            std::string w;
            p.siteSet = true;
            sr::ParseHorizon("0:15", p.limits.horizon, w);
            sr::ParseSlewBounds("85", "0..360", p.limits, w);
            sr::ParsePark("0:5", p.park, w);
            p.mountConnected = true; p.mountAligned = aligned; p.cameraOk = true; p.freeGB = 100;
        };
        h.sunAltDeg = [this] { return sunAlt; };
        h.targets = [] {
            sr::Target t; t.name = "Saturn"; t.type = "planet"; t.altDeg = 40; t.azDeg = 180; t.setsInMin = 300;
            return std::vector<sr::Target>{t};
        };
        h.devices = [] { return (unsigned)(sr::kCamera | sr::kMount | sr::kFocuser); };
        h.gotoTarget = [](const sr::Target&, std::string&) { return true; };
        h.slewing = [] { return true; };   // holds the group at its slew, so its state can be read
        h.park = [this] { parked = false; };
        h.parked = [this] { return parked; };
        return h;
    }
};

int main() {
    printf("sky_run_view (phase 156.2c)\n");

    printf("\n1. Idle\n");
    {
        SkyRunController c;
        const auto v = BuildSkyRunView(c);
        Check(v.state == "not started" && v.canStart && !v.canStop, "not started: START on, STOP off");
        const auto w = SkyRunLines(v);
        Check(w.headline == "SKY RUN: not started" && w.Button("START") && w.Button("START")->enabled &&
              w.Button("STOP") && !w.Button("STOP")->enabled, "the window's headline and buttons follow it");
        Check(Has(v, "START checks the rig") && Has(v, "without a park") && Has(v, "results.json"),
              "says what START and STOP do, and where results go");
    }

    printf("\n2. Refused\n");
    {
        Rig r; r.aligned = false;
        SkyRunController c; c.SetHooks(r.Hooks());
        c.Start({"tracking"}, -10, false, "20261121", "x", 0);
        const auto v = BuildSkyRunView(c);
        SkyRunLine::Tone t{};
        Check(v.state == "refused" && v.canStart && !v.canStop, "refused: START on again, STOP off");
        Check(Has(v, "Every reason") && Has(v, "aligned", &t) && t == SkyRunLine::Tone::Bad, "every reason, in the fault tone");
    }

    printf("\n3. Running\n");
    {
        Rig r;
        SkyRunController c; c.SetHooks(r.Hooks());
        c.Start({"tracking", "moon"}, -10, false, "20261121", "x", 0);
        Check(BuildSkyRunView(c).state == "waiting for the dark window" && BuildSkyRunView(c).canStop,
              "waiting for the dark: STOP on");
        c.Tick(1); c.Tick(2);
        const auto v = BuildSkyRunView(c);
        SkyRunLine::Tone t{};
        Check(v.state == "running" && !v.canStart && v.canStop, "running: START off, STOP on");
        Check(Has(v, "Now: slewing to Saturn", &t) && t == SkyRunLine::Tone::Accent, "the step now running");
        Check(Has(v, "PLAN") && Has(v, "tracking on Saturn: running"), "the plan, with each group's status");
        Check(Has(v, "moon on") || Has(v, "moon"), "and the groups that cannot run tonight, with why");
    }

    printf("\n4. Parking, done, tones\n");
    {
        Rig r;
        SkyRunController c; c.SetHooks(r.Hooks());
        c.Start({"tracking"}, -10, false, "20261121", "x", 0);
        c.Tick(1);
        c.Stop(2);
        auto v = BuildSkyRunView(c);
        Check(v.state == "parking" && !v.canStop && !v.canStart, "parking: STOP off (already ending), START off");
        r.parked = true;
        c.Tick(3);
        v = BuildSkyRunView(c);
        Check(v.state == "done: STOP pressed" && v.canStart && !v.canStop, "done: why it ended, START on", v.state);
    }
    {
        Check(SkyRunStatusTone(sr::Status::Failed) == SkyRunLine::Tone::Bad &&
              SkyRunStatusTone(sr::Status::Passed) == SkyRunLine::Tone::Good &&
              SkyRunStatusTone(sr::Status::Collected) == SkyRunLine::Tone::Accent, "each status has its tone");
    }

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
