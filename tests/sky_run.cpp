// ============================================================
//  Phase 156.1 and 156.2: the unattended sky run's rules (core/SkyRun.h)
//
//  The dress rehearsal (156.5) of every decision, on CI, on the owner's
//  terms:
//    1. The limits: the horizon profile interpolated through north, the
//       slew bounds' altitude and azimuth (wrapping too), bad input refused.
//    2. Pre-flight refuses without the site, the horizon or the slew
//       bounds, an aligned mount, the camera, or room on the disk.
//    3. Groups apply to the right targets and devices; out of the limits
//       or setting too soon is a horizon skip, not a not-available.
//    4. The plan: what applies runs first, the target setting soonest
//       first; the rest is kept with its reason.
//    5. The supervisor through a night: a pass, a failure passed over, a
//       lost target, clouds held and re-tried, a stall that stops and
//       parks, the dark window closing, the park at the end.
//    6. results.json, read back, with emulator runs marked.
// ============================================================

#include "core/SkyRun.h"
#include "core/MiniJson.h"

#include <cstdio>
#include <string>

using namespace raduk::core::skyrun;
using raduk::json::JVal;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}
static bool Near(double a, double b, double t = 1e-6) { return std::fabs(a - b) <= t; }

static Limits Lim() {
    Limits l;
    std::string why;
    ParseHorizon("0:15,90:25,180:20,270:15", l.horizon, why);
    ParseSlewBounds("85", "0..360", l, why);
    return l;
}

static Target T(const char* n, const char* type, double alt, double az, double sets = 300) {
    Target t; t.name = n; t.type = type; t.altDeg = alt; t.azDeg = az; t.setsInMin = sets; return t;
}

static const unsigned kAll = kCamera | kFocuser | kMount | kSolver;

int main() {
    printf("sky_run (phase 156.1, 156.2)\n");

    printf("\n1. The limits\n");
    std::vector<HorizonPoint> h;
    std::string why;
    Check(ParseHorizon("90:25, 0:15 ,180:20,270:15", h, why) && h.size() == 4 && h[0].azDeg == 0, "a profile parses, sorted by azimuth");
    Check(Near(MinAltAt(h, 45), 20) && Near(MinAltAt(h, 90), 25), "interpolated between points");
    Check(Near(MinAltAt(h, 315), 15) && Near(MinAltAt(h, 359), 15), "and through north");
    std::vector<HorizonPoint> one;
    ParseHorizon("18", one, why);
    Check(one.empty() && !why.empty(), "a bare number is refused (az:alt)", why);
    ParseHorizon("0:18", one, why);
    Check(one.size() == 1 && Near(MinAltAt(one, 200), 18), "one point is a flat horizon");
    Check(!ParseHorizon("0:15,400:10", h, why) && h.empty(), "an azimuth past 360 is refused", why);
    Check(!ParseHorizon("", h, why), "empty is unset");
    Check(MinAltAt({}, 100) == 90, "unset allows nothing");
    Limits l;
    Check(!ParseSlewBounds("", "0..360", l, why) && !l.slewSet, "both slew keys are needed", why);
    Check(!ParseSlewBounds("95", "0..360", l, why), "an altitude past 90 is refused");
    Check(!ParseSlewBounds("85", "300-60", l, why), "an azimuth range without .. is refused", why);
    Check(ParseSlewBounds("85", " 0 .. 360 ", l, why) && l.azMinDeg == 0 && l.azMaxDeg == 360, "0..360 with spaces: set, no limit");
    Check(ParseSlewBounds("85", "300..60", l, why) && l.slewSet, "an azimuth range wrapping north");
    l.horizon = one;
    Check(WithinLimits(l, 40, 10) && WithinLimits(l, 40, 330), "inside a wrapped range");
    Check(!WithinLimits(l, 40, 180, &why) && why.find("outside the slew bounds") != std::string::npos, "outside it, and said", why);
    Check(!WithinLimits(l, 88, 10, &why) && why.find("above the slew bound") != std::string::npos, "above the altitude bound", why);
    Check(!WithinLimits(l, 10, 10, &why) && why.find("below the horizon limit") != std::string::npos, "below the horizon", why);

    printf("\n2. Pre-flight\n");
    PreFlightIn pf;
    auto no = PreFlight(pf);
    Check(no.size() >= 4, "nothing set: refused, every reason listed", std::to_string(no.size()));
    pf.siteSet = true; pf.limits = Lim(); pf.mountConnected = true; pf.mountAligned = true; pf.cameraOk = true;
    ParsePark("0:5", pf.park, why);
    pf.freeGB = 200; pf.plannedGB = 60;
    Check(PreFlight(pf).empty(), "all set: go");
    pf.limits.horizon.clear();
    no = PreFlight(pf);
    Check(no.size() == 1 && no[0].find("horizon") != std::string::npos, "no horizon: refused, and that is the reason", no.empty() ? "" : no[0]);
    pf.limits = Lim(); pf.limits.slewSet = false;
    Check(PreFlight(pf).size() == 1, "no slew bounds: refused");
    pf.limits = Lim(); pf.mountAligned = false;
    Check(PreFlight(pf)[0].find("not aligned") != std::string::npos, "an unaligned mount: refused");
    pf.mountAligned = true;
    {
        PreFlightIn np = pf; np.park = ParkPosition{};
        auto r = PreFlight(np);
        Check(r.size() == 1 && r[0].find("SkyParkAltAz") != std::string::npos, "no park position: refused (the owner, 2026-11-21)", r.empty() ? "" : r[0]);
        PreFlightIn hi = pf; ParsePark("180:89", hi.park, why);
        r = PreFlight(hi);
        Check(r.size() == 1 && r[0].find("above the slew bound") != std::string::npos, "a park above the slew bound: refused", r.empty() ? "" : r[0]);
        PreFlightIn low = pf; ParsePark("0:5", low.park, why);
        Check(PreFlight(low).empty(), "a park low in the north, under the horizon limit: allowed (it is deliberate)");
        ParkPosition pp;
        Check(!ParsePark("", pp, why) && !ParsePark("5", pp, why) && !ParsePark("0:95", pp, why) && ParsePark(" 350 : 10 ", pp, why) &&
              pp.azDeg == 350 && pp.altDeg == 10, "SkyParkAltAz parses az:alt, and refuses the rest");
    }
    pf.freeGB = 20;
    Check(PreFlight(pf)[0].find("GB free") != std::string::npos, "too little disk: refused", PreFlight(pf)[0]);

    printf("\n3. Groups\n");
    const Limits L = Lim();
    const Group* planet = FindGroup("planet");
    const Group* jup = FindGroup("jupiter");
    const Group* moon = FindGroup("moon");
    const Group* solve = FindGroup("solve");
    Check(planet && jup && moon && solve && FindGroup("tracking"), "the plan's five groups");
    Check(Applicable(*planet, T("Saturn", "planet", 40, 180), kAll, L).ok, "a planet: the planet group applies");
    Check(!Applicable(*jup, T("Saturn", "planet", 40, 180), kAll, L).ok, "Saturn is not Jupiter");
    auto a = Applicable(*planet, T("Saturn", "planet", 40, 180), kCamera | kMount, L);
    Check(!a.ok && !a.horizon && a.why == "needs the focuser", "no focuser: not available, and why", a.why);
    a = Applicable(*solve, T("Vega", "star", 50, 90), kCamera | kMount, L);
    Check(!a.ok && a.why == "needs the plate solver", "no solver: the solve group says so", a.why);
    a = Applicable(*planet, T("Saturn", "planet", 18, 90), kAll, L);
    Check(!a.ok && a.horizon, "below the horizon at its azimuth: a horizon skip", a.why);
    a = Applicable(*jup, T("Jupiter", "planet", 40, 180, 20), kAll, L);
    Check(!a.ok && a.horizon && a.why.find("sets in 20 min") != std::string::npos, "setting before the group could finish: a horizon skip", a.why);
    Check(Applicable(*moon, T("Moon", "moon", 35, 120), kAll, L).ok, "the Moon: the moon group");

    printf("\n4. The plan\n");
    const std::vector<Target> sky = { T("Jupiter", "planet", 50, 120, 400), T("Saturn", "planet", 30, 220, 90),
                                      T("Moon", "moon", 12, 100, 200), T("Vega", "star", 60, 300, 600) };
    auto plan = MakePlan({"planet", "jupiter", "moon", "solve", "tracking"}, sky, kCamera | kFocuser | kMount, L);
    Check(plan.size() == 5, "one entry a group", std::to_string(plan.size()));
    Check(plan[0].target == "Saturn" && plan[0].group == "planet", "the target setting soonest goes first", plan[0].group + "/" + plan[0].target);
    Check(plan[0].status == Status::Pending && plan[1].status == Status::Pending && plan[2].status == Status::Pending,
          "what applies is pending");
    const Entry* moonE = nullptr; const Entry* solveE = nullptr;
    for (const auto& e : plan) { if (e.group == "moon") moonE = &e; if (e.group == "solve") solveE = &e; }
    Check(moonE && moonE->status == Status::SkippedHorizon && moonE->detail.find("below the horizon") != std::string::npos,
          "the Moon too low: kept, skipped-horizon, with why", moonE ? moonE->detail : "");
    Check(solveE && solveE->status == Status::NotAvailable && solveE->detail == "needs the plate solver",
          "no solver: kept, not-available, with why");

    printf("\n5. The supervisor through a night\n");
    Supervisor s;
    s.Begin(plan, false);
    Check(s.Next(0) == Action::StartNext && s.Current()->group == "planet", "it starts the first pending");
    Check(s.Result(Status::Passed, "fine pass +12", {{1, "sharpness p99", 61, 0, false, "%"}}, 600) == Action::StartNext &&
          s.Current()->group == "tracking" && s.Current()->target == "Saturn",
          "a pass, then the next: tracking, on the same setting target");
    Check(s.Result(Status::Failed, "Autorun did not start", {}, 900) == Action::StartNext, "a failure is recorded and passed over");
    const std::string third = s.Current()->group;
    Check(s.Clouds("target signal collapsed", 1000) == Action::Hold && s.Holding(), "clouds: hold");
    Check(s.Tick(false, 1100) == Action::Hold, "still held before the recheck");
    Check(s.Tick(false, 1300) == Action::Hold, "still cloudy at the recheck: held again");
    Check(s.Tick(true, 1650) == Action::StartNext && s.Current()->group == third, "clear: the clouded test is tried again");
    Check(s.TargetLost("no disc for 30 s", 1800) == Action::Park, "a lost target fails that test; at the end of the list, park");
    Check(s.Parked() == Action::Done && s.ParkedDone(), "parked: done");
    int clouded = 0, failed = 0, passed = 0;
    for (const auto& e : s.Plan()) {
        clouded += e.status == Status::Clouded; failed += e.status == Status::Failed; passed += e.status == Status::Passed;
    }
    Check(passed == 1 && failed == 2 && clouded == 1, "the night's tally: 1 passed, 2 failed, 1 clouded",
          std::to_string(passed) + "/" + std::to_string(failed) + "/" + std::to_string(clouded));
    Check(s.StopReason() == "the end of the list", "it stopped at the end of the list", s.StopReason());

    Supervisor st;
    st.Begin(plan, false);
    st.Next(0);
    Check(st.Stall("the focuser", 300) == Action::Park, "a stalled device: park at once");
    Check(st.Plan()[0].status == Status::AbortedStall, "the running test: aborted-stall");
    bool rest = st.Plan()[1].status == Status::NotReached && st.Plan()[2].status == Status::NotReached;
    Check(rest && st.Plan()[1].detail.find("the focuser stalled") != std::string::npos, "the rest: not reached, and why", st.Plan()[1].detail);

    Supervisor dk;
    dk.Begin(plan, false);
    dk.Next(0);
    dk.Clouds("cloud", 100);
    Check(dk.DarkEnds(400) == Action::Park && !dk.Holding(), "the dark window closes in a hold: park");
    Check(dk.StopReason() == "the dark window closed", "and says so");

    printf("\n5b. Collected, the star list and the dark window (the owner, 2026-11-21)\n");
    Supervisor co;
    co.Begin(plan, false);
    co.Next(0);
    co.Result(Status::Collected, "4 ladder recordings", {}, 100);
    Check(co.Plan()[0].status == Status::Collected && std::string(StatusText(Status::Collected)) == "collected",
          "a group can end collected: the evidence gathered, your eye owed");
    co.Result(Status::NotReached, "nonsense", {}, 200);
    Check(co.Plan()[1].status == Status::Failed, "a result that is not passed or collected counts as failed");
    const auto stars = ParseStarList(" Vega, Arcturus ,,Capella ");
    Check(stars.size() == 3 && stars[0] == "Vega" && stars[1] == "Arcturus" && stars[2] == "Capella",
          "the owner's star list, in his order, trimmed");
    Check(ParseStarList("").empty(), "an empty list is no stars");
    Check(IsDark(-10.5) && !IsDark(-9.5) && kDefaultDarkSunAltDeg == -10.0, "dark below -10 degrees by default");
    Check(IsDark(-13, -12) && !IsDark(-11, -12), "and the threshold is a setting");

    printf("\n6. results.json\n");
    const std::string j = ResultsJson(s, "20261121", "3.3.38");
    JVal root;
    raduk::json::JsonReader rd(j);
    Check(rd.Parse(root) && root.kind == JVal::Obj, "valid JSON");
    Check(root.Get("format")->s == "raduk-sky-results" && !root.Get("emulator")->b && root.Get("parked")->b,
          "its format; a sky run, parked");
    const JVal* tests = root.Get("tests");
    Check(tests && tests->a.size() == s.Plan().size(), "every entry, run or not");
    Check(tests && tests->a[0].Get("status")->s == "passed" && tests->a[0].Get("measures")->a.size() == 1 &&
          tests->a[0].Get("measures")->a[0].Get("expected")->kind == JVal::Null, "a passed test with its measure; no expected is null");
    bool items = tests && tests->a[0].Get("items")->a.size() == 5;
    Check(items, "the checklist items it settles");
    Supervisor em;
    em.Begin(plan, true);
    JVal er;
    const std::string ej = ResultsJson(em, "20261121", "x");   // the reader keeps a reference: not a temporary
    raduk::json::JsonReader rd2(ej);
    Check(rd2.Parse(er) && er.Get("emulator")->b, "an emulator run says so");

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
