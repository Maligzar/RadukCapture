// ============================================================
//  Phase 156.2a: the unattended run, carried out (app/SkyRunController.h)
//
//  A simulated rig stands in for the mount, the tracker, the camera and
//  the sky, so whole nights run headless:
//    1. Pre-flight on the rig: refused with every reason, nothing moved.
//    2. It waits for the dark window, and starts when the Sun goes below it.
//    3. A good night: the tracking group passes (box change held, re-centred
//       after a jog, calibration stayed in the box and refused at an edge),
//       groups not built yet are not-available (never failed), it parks and
//       writes results.json.
//    4. Each way the tracking group can fail, said with its item.
//    5. Clouds hold the run and the group starts again when it clears; a
//       stall, the dark window's end and STOP each park at once.
//    6. The box sizes come from the camera's own size (phase 149).
// ============================================================

#include "app/SkyRunController.h"
#include "core/MiniJson.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace raduk::app;
namespace sr = raduk::core::skyrun;
using raduk::json::JVal;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

// The simulated rig. Time is advanced by the test; each property is a
// knob a scenario turns.
struct Rig {
    double now = 0;
    // the night
    bool   siteSet = true, horizonSet = true, slewSet = true, aligned = true, parkSet = true;
    double sunAlt = -15;            // degrees; a scenario can make it rise
    double sunriseAt = 1e9;         // the Sun crosses -10 at this time
    int    sensorW = 3840, sensorH = 2160;
    // the mount and tracker
    double slewEnds = -1, slewTakes = 30;
    bool   slewForever = false;
    double lockAt = -1, lockTakes = 40;
    bool   lockNever = false, dropLockAtBox = false;
    std::vector<std::pair<int, int>> boxes;
    double jogAt = -1, recentreTakes = 12;
    bool   neverRecentre = false, abandonAfterJog = false;
    bool   nearEdge = false, edgeCalibrates = false, smallBoxNotBack = false, smallBoxCalFails = false;
    std::string edgeRefusal = "the planet is near the edge of the frame (3100, 300 of 3840x2160) and calibration moves it a few hundred pixels - centre it roughly, then press again";
    int    calSeq = 0;           // one per calibration result
    bool   calOk = false;
    std::string calMsg;
    double calAt = -1;           // when the pending result arrives
    bool   calPendingOk = false;
    std::string calPendingMsg;
    int    boxW = 0, boxH = 0;   // the box last set
    // the sky and the devices
    bool   cloudy = false;
    std::string stalled;
    bool   parkCalled = false;
    double parkedAt = -1;
    std::string results;
    std::vector<std::string> log;

    SkyRunHooks Hooks() {
        SkyRunHooks h;
        h.preflight = [this](sr::PreFlightIn& p) {
            p.siteSet = siteSet;
            std::string w;
            if (horizonSet) sr::ParseHorizon("0:15", p.limits.horizon, w);
            if (slewSet) sr::ParseSlewBounds("85", "0..360", p.limits, w);
            if (parkSet) sr::ParsePark("0:5", p.park, w);
            p.mountConnected = true; p.mountAligned = aligned; p.cameraOk = true;
            p.freeGB = 200; p.plannedGB = 10;
        };
        h.sunAltDeg = [this] { return now >= sunriseAt ? -5.0 : sunAlt; };
        h.targets = [] {
            sr::Target t; t.name = "Saturn"; t.type = "planet"; t.altDeg = 40; t.azDeg = 180; t.setsInMin = 200;
            return std::vector<sr::Target>{t};
        };
        h.devices = [] { return (unsigned)(sr::kCamera | sr::kFocuser | sr::kMount); };
        h.sensorSize = [this](int& w, int& hh) { w = sensorW; hh = sensorH; };
        h.cloudy = [this] { return cloudy; };
        h.stalled = [this] { return stalled; };
        h.gotoTarget = [this](const sr::Target&, std::string&) { slewEnds = now + slewTakes; return true; };
        h.slewing = [this] { return slewForever || (slewEnds >= 0 && now < slewEnds); };
        // Every acquire calibrates afresh; near an edge calibration refuses
        // and the lock never comes.
        h.startLock = [this](std::string&) {
            lockAt = now + lockTakes;
            calAt = now + lockTakes / 2;
            const bool small = boxW > 0 && boxW < sensorW;
            calPendingOk = !(nearEdge && !edgeCalibrates) && !(small && smallBoxCalFails);
            calPendingMsg = calPendingOk ? "calibrated" : (nearEdge ? edgeRefusal : std::string("no stable centroid to calibrate against"));
            if (!calPendingOk) lockAt = 1e18;
            return true;
        };
        h.resetTracker = [this] { lockAt = -1; };
        h.lastCalibration = [this](int& seq, bool& ok, std::string& msg) {
            if (calAt >= 0 && now >= calAt) { calSeq++; calOk = calPendingOk; calMsg = calPendingMsg; calAt = -1; }
            seq = calSeq; ok = calOk; msg = calMsg;
        };
        // Acquiring widens to the whole sensor; once locked the box set
        // before comes back (157.0c).
        h.boxNow = [this](int& w, int& hh) {
            const bool locked = lockAt >= 0 && now >= lockAt;
            if (!locked || smallBoxNotBack) { w = sensorW; hh = sensorH; return; }
            w = boxW; hh = boxH;
        };
        h.lockState = [this] {
            if (lockNever || lockAt < 0 || now < lockAt) return LockState::Acquiring;
            if (dropLockAtBox && boxes.size() >= 3) return LockState::Acquiring;
            if (abandonAfterJog && jogAt >= 0) return LockState::Acquiring;
            return LockState::Locked;
        };
        h.setBox = [this](int w, int hh) { boxes.push_back({w, hh}); boxW = w; boxH = hh; return true; };
        h.jog = [this](double, double) { jogAt = now; return true; };
        h.offCentrePx = [this] {
            if (jogAt < 0) return 5.0;
            if (neverRecentre) return 180.0;
            return now - jogAt >= recentreTakes ? 8.0 : 150.0;
        };
        h.nudgeToEdge = [this](double) { nearEdge = true; return true; };
        h.park = [this] { parkCalled = true; parkedAt = now + 20; };
        h.parked = [this] { return parkedAt >= 0 && now >= parkedAt; };
        h.log = [this](const std::string& s) { log.push_back(s); };
        h.writeResults = [this](const std::string& j) { results = j; };
        return h;
    }
};

static const std::vector<std::string> kAll = {"planet", "jupiter", "moon", "solve", "tracking"};

// Runs the controller until it is done or `until` seconds pass, at 2 Hz.
static void RunNight(SkyRunController& c, Rig& r, double until, const std::function<void()>& each = {}) {
    for (; r.now < until && c.phase() != SkyRunController::Phase::Done; r.now += 0.5) {
        if (each) each();
        c.Tick(r.now);
    }
}

static const sr::Entry* Find(const SkyRunController& c, const std::string& g) {
    for (const auto& e : c.Supervisor().Plan()) if (e.group == g) return &e;
    return nullptr;
}

static bool Start(SkyRunController& c, Rig& r) {
    c.SetHooks(r.Hooks());
    return c.Start(kAll, -10, false, "20261121", "3.3.39", r.now);
}

int main() {
    printf("sky_run_controller (phase 156.2a)\n");

    printf("\n1. Pre-flight on the rig\n");
    {
        Rig r; r.horizonSet = false; r.aligned = false; r.parkSet = false;
        SkyRunController c;
        Check(!Start(c, r) && c.phase() == SkyRunController::Phase::Refused, "refused");
        Check(c.Refusals().size() == 3, "with every reason: horizon, park, alignment", std::to_string(c.Refusals().size()));
        c.Tick(1);
        Check(r.boxes.empty() && !r.parkCalled && r.slewEnds < 0, "and nothing moved");
    }

    printf("\n2. The dark window\n");
    {
        Rig r; r.sunAlt = -5;
        SkyRunController c;
        Check(Start(c, r) && c.phase() == SkyRunController::Phase::WaitingDark, "started: waiting for the dark");
        for (int i = 0; i < 20; i++) { r.now += 0.5; c.Tick(r.now); }
        Check(r.slewEnds < 0, "no slew in twilight");
        r.sunAlt = -11;
        r.now += 0.5; c.Tick(r.now);   // the group begins
        r.now += 0.5; c.Tick(r.now);   // and its GoTo goes on the next tick
        Check(c.phase() == SkyRunController::Phase::Running && r.slewEnds > 0, "below -10: the first group starts");
    }

    printf("\n3. A good night\n");
    Rig good;
    SkyRunController gc;
    Start(gc, good);
    RunNight(gc, good, 3000);
    const sr::Entry* tr = Find(gc, "tracking");
    Check(gc.phase() == SkyRunController::Phase::Done && good.parkCalled, "it parked and finished");
    Check(tr && tr->status == sr::Status::Passed, "the tracking group passed", tr ? tr->detail : "");
    Check(tr && tr->measures.size() == 4, "with its measures: the box change, the re-centre, calibration in the box, the edge refusal",
          tr ? std::to_string(tr->measures.size()) : "");
    bool recentre = false;
    if (tr) for (const auto& m : tr->measures) if (m.item == 49 && m.value >= 12 && m.value <= 13 && m.hasExpected && m.expected == 25) recentre = true;
    Check(recentre, "item 49: re-centred in about 12 s, against 25");
    const sr::Entry* pl = Find(gc, "planet");
    Check(pl && pl->status == sr::Status::NotAvailable && pl->detail == "its steps are not built yet",
          "a group not built yet: not-available, never failed");
    JVal root;
    raduk::json::JsonReader rd(good.results);
    Check(!good.results.empty() && rd.Parse(root) && root.Get("parked")->b && root.Get("stopped")->s == "the end of the list",
          "results.json written: parked, at the end of the list");
    bool logged = false;
    for (const auto& l : good.log) if (l.find("[SKYRUN] group tracking: passed") != std::string::npos) logged = true;
    Check(logged, "and the log says so");

    printf("\n4. How the tracking group fails\n");
    auto failWith = [](std::function<void(Rig&)> setup, const std::string& want, const char* what) {
        Rig r;
        setup(r);
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000);
        const sr::Entry* e = Find(c, "tracking");
        Check(e && e->status == sr::Status::Failed && e->detail.find(want) != std::string::npos && r.parkCalled,
              what, e ? e->detail : "");
    };
    failWith([](Rig& r) { r.slewForever = true; }, "the slew did not finish", "a slew that never ends");
    failWith([](Rig& r) { r.lockNever = true; }, "lock not made in 5 minutes", "a lock never made");
    failWith([](Rig& r) { r.dropLockAtBox = true; }, "the lock dropped at box a sixth (item 48)", "the lock dropped in the box change");
    failWith([](Rig& r) { r.neverRecentre = true; }, "not re-centred in 25 s", "no re-centre after the jog");
    failWith([](Rig& r) { r.abandonAfterJog = true; }, "abandoned after the jog (item 49)", "the lock abandoned after the jog");
    failWith([](Rig& r) { r.smallBoxNotBack = true; }, "not the small box back (item 50)", "the small box not put back after the lock");
    failWith([](Rig& r) { r.smallBoxCalFails = true; }, "calibration from the small box failed", "calibration failing from the small box");
    failWith([](Rig& r) { r.edgeCalibrates = true; }, "it should refuse (item 50)", "calibration going ahead at the edge");
    failWith([](Rig& r) { r.edgeRefusal = "no"; }, "not with its message", "a refusal without its message");

    printf("\n5. Clouds, a stall, the dawn, STOP\n");
    {
        Rig r;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000, [&] {
            if (r.now == 50) r.cloudy = true;    // mid-group
            if (r.now == 700) r.cloudy = false;  // clears after a hold
        });
        int clouded = 0, passed = 0;
        for (const auto& e : c.Supervisor().Plan()) {
            if (e.group == "tracking" && e.status == sr::Status::Clouded) clouded++;
            if (e.group == "tracking" && e.status == sr::Status::Passed) passed++;
        }
        Check(clouded == 1 && passed == 1, "clouds: held, then the group ran again from its start and passed");
    }
    {
        Rig r;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000, [&] { if (r.now == 60) r.stalled = "the mount"; });
        const sr::Entry* e = Find(c, "tracking");
        Check(e && e->status == sr::Status::AbortedStall && r.parkCalled && c.Supervisor().StopReason() == "the mount stalled",
              "a stall: aborted, parked at once", c.Supervisor().StopReason());
    }
    {
        Rig r; r.sunriseAt = 80;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000);
        const sr::Entry* e = Find(c, "tracking");
        Check(e && e->status == sr::Status::NotReached && c.Supervisor().StopReason() == "the dark window closed" && r.parkCalled,
              "the dawn: not reached, parked");
    }
    {
        Rig r;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000, [&] { if (r.now == 90) c.Stop(r.now); });
        const sr::Entry* e = Find(c, "tracking");
        Check(e && e->status == sr::Status::NotReached && e->detail == "stopped: STOP pressed" && c.StoppedByOwner() && r.parkCalled,
              "STOP: not reached, parked, and said to be the owner's", e ? e->detail : "");
    }

    {
        Rig r;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000, [&] { if (r.now == 90) c.Abort("emergency stop", r.now); });
        const sr::Entry* e = Find(c, "tracking");
        Check(e && e->status == sr::Status::NotReached && !r.parkCalled && c.phase() == SkyRunController::Phase::Done,
              "emergency stop: not reached, and NOT parked (nothing moves after ESC)", e ? e->detail : "");
        Check(!r.results.empty(), "and results.json is still written");
        const std::string rows = c.PlanRows();
        Check(rows.find("tracking|") != std::string::npos && rows.find("|not-reached|") != std::string::npos,
              "the plan rows Dash shows", rows.substr(0, 60));
        Check(std::string(SkyRunController::PhaseName(c.phase())) == "done", "the phase's name");
    }

    printf("\n6. Box sizes from the camera\n");
    Check(good.boxes.size() >= 5, "the boxes were set", std::to_string(good.boxes.size()));
    if (good.boxes.size() >= 5) {
        Check(good.boxes[0] == std::make_pair(3840, 2160) && good.boxes[1] == std::make_pair(1280, 720) &&
              good.boxes[2] == std::make_pair(640, 480) && good.boxes[3] == std::make_pair(3840, 2160),
              "full, a third, a sixth, full: the owner's 3840, 1280, 640");
    }
    {
        Rig r; r.sensorW = 1920; r.sensorH = 1080;
        SkyRunController c;
        Start(c, r);
        RunNight(c, r, 3000);
        Check(r.boxes.size() >= 3 && r.boxes[1] == std::make_pair(640, 360) && r.boxes[2] == std::make_pair(320, 240),
              "another camera, its own sizes");
    }

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
