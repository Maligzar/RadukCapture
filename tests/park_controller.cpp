// ============================================================
//  Phase 156.2a: park as the owner chose (app/ParkController.h)
//
//  A slew to rig.ini SkyParkAltAz, then tracking off. Simulated mount:
//    1. Each refusal says why and moves nothing: unset or malformed key,
//       outside the slew bounds, mount not connected or not aligned, no site.
//    2. A good park: GoTo the converted RA/Dec, wait, tracking off, parked.
//    3. A slew that never ends is cancelled at 200 s and reported.
//    4. Tracking off refused: failed, not parked.
//    5. Unpark: a jog/GoTo/STOP leaves parked, and cancels a slew under way.
//    6. Moved more than a degree off the park: no longer parked.
//    7. A low park below the horizon limits is allowed (horizon is for
//       imaging over the trees, not for parking).
// ============================================================

#include "app/ParkController.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace raduk::app;
namespace sr = raduk::core::skyrun;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

struct Mount {
    std::string park = "180:20";
    std::string slewAlt = "85", slewAz = "0..360";
    std::string horizon = "0:30,180:30";
    bool connected = true, aligned = true, siteSet = true;
    bool acceptGoto = true, acceptTrackingOff = true;
    int  slewTicks = 3;          // how many Ticks the slew lasts; <0 never ends
    // what happened
    int  gotos = 0, cancels = 0, trackingOffs = 0, beforeSlews = 0;
    double lastRa = -1, lastDec = -99, lastAlt = -1, lastAz = -1;
    bool moving = false;
    std::vector<std::string> log;

    ParkHooks Hooks() {
        ParkHooks h;
        h.parkText  = [this] { return park; };
        h.limits    = [this](sr::Limits& l, std::string& why) {
            if (!sr::ParseSlewBounds(slewAlt, slewAz, l, why)) return false;
            return sr::ParseHorizon(horizon, l.horizon, why);
        };
        h.connected = [this] { return connected; };
        h.aligned   = [this] { return aligned; };
        h.toRADec   = [this](double alt, double az, double& ra, double& dec) {
            if (!siteSet) return false;
            lastAlt = alt; lastAz = az;
            ra = az / 15.0; dec = alt - 30.0;   // any fixed map will do
            return true;
        };
        h.beforeSlew = [this] { beforeSlews++; };
        h.gotoRADec = [this](double ra, double dec) {
            if (!acceptGoto) return false;
            gotos++; lastRa = ra; lastDec = dec; moving = slewTicks != 0;
            return true;
        };
        h.slewing    = [this] {
            if (!moving) return false;
            if (slewTicks > 0 && --slewTicks == 0) moving = false;
            return moving || slewTicks < 0;
        };
        h.cancelSlew  = [this] { cancels++; moving = false; };
        h.trackingOff = [this] { trackingOffs++; return acceptTrackingOff; };
        h.log = [this](const std::string& s) { log.push_back(s); };
        return h;
    }
};

static bool Refused(Mount& m, const std::string& needle, std::string* got = nullptr) {
    ParkController p(m.Hooks());
    std::string why;
    const bool ok = p.Request(0, why);
    if (got) *got = why;
    return !ok && why.find(needle) != std::string::npos && m.gotos == 0
           && p.state() == ParkController::State::Idle;
}

int main() {
    printf("=== park_controller (phase 156.2a) ===\n");

    printf("-- refusals: nothing moves --\n");
    { Mount m; m.park = "";        std::string w; Check(Refused(m, "not set", &w), "no SkyParkAltAz: refused, no default position", w); }
    { Mount m; m.park = "north";   std::string w; Check(Refused(m, "not az:alt", &w), "malformed key: refused", w); }
    { Mount m; m.park = "180:89";  std::string w; Check(Refused(m, "outside the slew bounds", &w), "above the altitude ceiling: refused", w); }
    { Mount m; m.park = "10:20"; m.slewAz = "90..270"; std::string w;
      Check(Refused(m, "outside the slew bounds", &w), "outside the azimuth range: refused", w); }
    { Mount m; m.slewAlt = "";     std::string w; Check(Refused(m, "slew bounds", &w), "slew bounds unset: refused", w); }
    { Mount m; m.connected = false; std::string w; Check(Refused(m, "not connected", &w), "mount not connected: refused", w); }
    { Mount m; m.aligned = false;  std::string w; Check(Refused(m, "not aligned", &w), "mount not aligned: refused (a park is a GoTo)", w); }
    { Mount m; m.siteSet = false;  std::string w; Check(Refused(m, "site", &w), "no site: refused", w); }
    { Mount m; m.acceptGoto = false; std::string w; Check(Refused(m, "refused the GoTo", &w), "GoTo refused: said so", w); }
    { Mount m; m.park = "";  ParkController p(m.Hooks()); std::string w; p.Request(0, w);
      Check(m.beforeSlews == 0, "a refusal does not stop the tracker either"); }

    printf("-- a good park --\n");
    {
        Mount m;
        ParkController p(m.Hooks());
        std::string why;
        Check(p.Request(10, why), "park starts", why);
        Check(m.gotos == 1 && m.beforeSlews == 1, "the tracker stopped, then one GoTo");
        Check(m.lastAz == 180 && m.lastAlt == 20, "converted from the configured az:alt");
        Check(m.lastRa == 12.0 && m.lastDec == -10.0, "the GoTo carries the converted RA/Dec");
        Check(p.state() == ParkController::State::Slewing && !p.parked(), "slewing, not yet parked");
        std::string again;
        Check(!p.Request(11, again) && again.find("under way") != std::string::npos,
              "a second press while slewing is refused", again);
        for (int i = 0; i < 2; ++i) p.Tick(11 + i);
        Check(m.trackingOffs == 0, "tracking left on while it slews");
        p.Tick(14); p.Tick(15);
        Check(p.parked() && m.trackingOffs == 1, "slew ended: tracking off, parked");
        Check(!m.log.empty() && m.log.back().rfind("[PARK]", 0) == 0, "logged as [PARK]", m.log.back());
    }

    printf("-- a low park, below the horizon limits --\n");
    {
        Mount m; m.park = "90:5";
        ParkController p(m.Hooks()); std::string why;
        Check(p.Request(0, why), "alt 5 under a 30-degree horizon: allowed", why);
    }

    printf("-- a slew that never ends --\n");
    {
        Mount m; m.slewTicks = -1;
        ParkController p(m.Hooks()); std::string why;
        p.Request(0, why);
        p.Tick(100);
        Check(p.state() == ParkController::State::Slewing, "100 s: still waiting");
        p.Tick(201);
        Check(p.state() == ParkController::State::Failed && m.cancels == 1,
              "201 s: cancelled and failed", p.reason());
        Check(m.trackingOffs == 0 && !p.parked(), "not parked, tracking not touched");
    }

    printf("-- tracking off refused --\n");
    {
        Mount m; m.slewTicks = 1; m.acceptTrackingOff = false;
        ParkController p(m.Hooks()); std::string why;
        p.Request(0, why); p.Tick(1); p.Tick(2);
        Check(p.state() == ParkController::State::Failed && !p.parked(),
              "failed, not parked", p.reason());
    }

    printf("-- unpark --\n");
    {
        Mount m; m.slewTicks = 1;
        ParkController p(m.Hooks()); std::string why;
        p.Request(0, why); p.Tick(1); p.Tick(2);
        p.Unpark("jog");
        Check(p.state() == ParkController::State::Idle && m.cancels == 0,
              "parked then jogged: idle, nothing to cancel");
        Mount m2; m2.slewTicks = -1;
        ParkController p2(m2.Hooks());
        p2.Request(0, why);
        p2.Unpark("STOP");
        Check(p2.state() == ParkController::State::Idle && m2.cancels == 1,
              "STOP during the park slew: slew cancelled");
        Check(p2.Request(5, why), "and it can park again", why);
    }

    printf("-- moved off the park --\n");
    {
        Mount m; m.slewTicks = 1; m.park = "359.5:20";
        ParkController p(m.Hooks()); std::string why;
        p.Request(0, why); p.Tick(1); p.Tick(2);
        p.WatchPosition(20.4, 0.2);
        Check(p.parked(), "within a degree, across north: still parked");
        p.WatchPosition(24.0, 0.2);
        Check(p.state() == ParkController::State::Idle, "moved 4 degrees: no longer parked");
    }

    printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
