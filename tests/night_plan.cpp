// ============================================================
//  A night of planets and stars (phase 163)
//
//  src/core/NightPlan.h places each ticked target while it is in GOOD
//  (45-60 deg) or PRIME (60 deg and up), around the ticked passes, and
//  runs the plan: wait, slew, settle, lock, RUNS, move on.
//
//  The planner is driven with altitude curves made up for the purpose,
//  so each rule can be seen on its own, and with two real stars from two
//  latitudes. The runner is driven on a fake clock.
//
//  THE ONES THAT MATTER:
//
//   1. A target outside GOOD and PRIME is never scheduled.
//   2. PRIME is preferred; GOOD is the fallback.
//   4. Visits never overlap, and the overhead sits between them.
//   5. A ticked pass is never touched.
//  10. Placing the targets in a better order schedules more.
//  13. The runner does not believe a slew is over before it has started.
// ============================================================

#include "core/NightPlan.h"
#include "app/NightPlanController.h"   // phase 163b: pause and continue

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-70s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) ++g_fail;
}

static const double J0 = 2461000.5;          // any start; only differences matter
static double Jd(double minutes) { return J0 + minutes / 1440.0; }
static double Min(double jd) { return (jd - J0) * 1440.0; }

// An altitude curve: peak at minute `at`, falling `fall` deg per hour
// either side of it, roughly the shape of a transit.
struct Curve { double peak, at, fallPerHour; };
static double AltOf(const Curve& c, double minutes) {
    const double h = (minutes - c.at) / 60.0;
    return c.peak - c.fallPerHour * h * h;
}

static NightSampler Sampler(const std::vector<Curve>& cs, double sunAlt = -30.0, double sunSep = 120.0) {
    return [cs, sunAlt, sunSep](int t, double jd) {
        NightSample s;
        s.altDeg = AltOf(cs[(size_t)t], Min(jd));
        s.sunAltDeg = sunAlt;
        s.sunSepDeg = sunSep;
        return s;
    };
}

static std::vector<NightTarget> Names(std::initializer_list<const char*> n) {
    std::vector<NightTarget> v;
    for (const char* s : n) v.push_back({s, true, ""});
    return v;
}

static const NightVisit* Find(const NightPlan& p, const std::string& name) {
    for (const auto& v : p.visits) if (v.name == name) return &v;
    return nullptr;
}
static const NightSkip* Skipped(const NightPlan& p, const std::string& name) {
    for (const auto& s : p.skipped) if (s.name == name) return &s;
    return nullptr;
}
static std::string Show(const NightVisit* v) {
    if (!v) return "not placed";
    char b[200];
    snprintf(b, sizeof(b), "slew %.0f, image %.0f-%.0f, %s", Min(v->slewJd), Min(v->imageJd),
             Min(v->endJd), v->Why().c_str());
    return b;
}

// The altitude everywhere in [a, b], sampled each minute.
static double LowestIn(const Curve& c, double a, double b) {
    double lo = 90.0;
    for (double m = a; m <= b + 1e-9; m += 1.0) lo = std::fmin(lo, AltOf(c, m));
    return lo;
}

int main() {
    printf("Night plan for planets and stars (phase 163)\n\n");
    NightPlanOptions o;            // 20 min visits, 6.5 min overhead -> 7 minutes
    const double overheadMin = std::ceil(o.OverheadSec() / 60.0);

    printf("1. A target outside GOOD and PRIME is never scheduled\n");
    {
        const std::vector<Curve> cs = {{40.0, 300.0, 3.0}};
        const auto p = BuildNightPlan(Names({"Low"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        Check(p.visits.empty(), "a target peaking at 40 deg has no visit");
        const auto* s = Skipped(p, "Low");
        Check(s && s->why.find("never reaches 45") != std::string::npos, "and the plan says why",
              s ? s->why : "");
    }

    printf("\n2. PRIME is preferred\n");
    {
        // Above 60 from minute 300-155 to 300+155: about five hours of PRIME.
        const std::vector<Curve> cs = {{70.0, 300.0, 1.5}};
        const auto p = BuildNightPlan(Names({"High"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        const auto* v = Find(p, "High");
        Check(v != nullptr, "the target is placed", Show(v));
        Check(v && v->primeMin == 20 && v->goodMin == 0, "all 20 minutes are PRIME");
        Check(v && v->Window() == SkyWindow::Prime, "its window is PRIME");
        Check(v && LowestIn(cs[0], Min(v->imageJd), Min(v->endJd)) >= 60.0,
              "and the curve is at 60 deg or more all through the imaging");
        Check(v && std::fabs((v->imageJd - v->slewJd) * 1440.0 - overheadMin) < 0.01,
              "the slew starts the overhead ahead of the first run");
        Check(v && v->Why().find("PRIME 20 min") == 0, "Why() says the window and how long", v ? v->Why() : "");
    }

    printf("\n3. GOOD is the fallback\n");
    {
        const std::vector<Curve> cs = {{55.0, 300.0, 2.0}};
        const auto p = BuildNightPlan(Names({"Mid"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        const auto* v = Find(p, "Mid");
        Check(v && v->primeMin == 0 && v->goodMin == 20, "a target peaking at 55 deg gets 20 GOOD minutes", Show(v));
        Check(v && v->Window() == SkyWindow::Good, "its window is GOOD");
        Check(v && LowestIn(cs[0], Min(v->imageJd), Min(v->endJd)) >= 45.0, "at 45 deg or more all through");
    }

    printf("\n4. Visits never overlap, and the overhead sits between them\n");
    {
        // Three targets with the same PRIME window: only one can have its
        // middle, and none may share a minute.
        const std::vector<Curve> cs = {{75.0, 300.0, 1.0}, {75.0, 300.0, 1.0}, {75.0, 300.0, 1.0}};
        const auto p = BuildNightPlan(Names({"A", "B", "C"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        Check(p.visits.size() == 3, "all three are placed", std::to_string(p.visits.size()));
        bool apart = true;
        for (size_t i = 1; i < p.visits.size(); ++i)
            if (p.visits[i].slewJd < p.visits[i - 1].endJd - 1e-9) apart = false;
        Check(apart, "each slew starts after the visit before it ends");
        bool gap = true;
        for (size_t i = 1; i < p.visits.size(); ++i)
            if ((p.visits[i].imageJd - p.visits[i - 1].endJd) * 1440.0 < overheadMin - 0.01) gap = false;
        Check(gap, "and the gap before each first run holds the whole overhead");
        Check(p.TotalPrimeMin() == 60, "all 60 minutes are PRIME", std::to_string(p.TotalPrimeMin()));
    }

    printf("\n5. A ticked pass is never touched\n");
    {
        // One target in PRIME only between about minute 280 and 320; a pass
        // sits at 330-336. With the 2-minute margin, 328 onwards is taken.
        const std::vector<Curve> cs = {{62.0, 300.0, 5.0}};
        std::vector<NightBusy> busy = {{Jd(330), Jd(336), "ISS"}};
        const auto p = BuildNightPlan(Names({"T"}), Sampler(cs), Jd(0), Jd(600), busy, o);
        const auto* v = Find(p, "T");
        Check(v != nullptr, "placed", Show(v));
        Check(v && (v->endJd <= Jd(328) + 1e-9 || v->slewJd >= Jd(338) - 1e-9),
              "the visit, overhead and all, stays clear of the pass and its margin");
        Check(p.busy.size() == 1 && std::fabs(Min(p.busy[0].startJd) - 328.0) < 0.01,
              "the plan keeps the pass with its margin added");

        // A pass right across the only stretch: no visit.
        const std::vector<Curve> narrow = {{48.0, 300.0, 3.0}};   // 45+ for 120 min
        std::vector<NightBusy> across = {{Jd(230), Jd(370), "Starlink"}};
        const auto q = BuildNightPlan(Names({"N"}), Sampler(narrow), Jd(0), Jd(600), across, o);
        Check(q.visits.empty(), "a pass over the whole window leaves no visit");
        const auto* s = Skipped(q, "N");
        Check(s && s->why.find("other visits or passes") != std::string::npos, "and says so", s ? s->why : "");
    }

    printf("\n6. Shorten before dropping\n");
    {
        // 45 deg or more for 15 minutes: 47 at the top, 2 deg down 7.5 min either side.
        const std::vector<Curve> cs = {{47.0, 300.0, 128.0}};
        const auto p = BuildNightPlan(Names({"Brief"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        const auto* v = Find(p, "Brief");
        Check(v && v->shortened && v->ImagingMin() >= 5 && v->ImagingMin() < 20,
              "a 15-minute window gets a shortened visit", Show(v));
        Check(v && v->Why().find("shortened from 20 min") != std::string::npos, "and says it was shortened");
    }

    printf("\n7. Too short to be worth a slew\n");
    {
        const std::vector<Curve> cs = {{45.3, 300.0, 270.0}};   // about 4 min
        const auto p = BuildNightPlan(Names({"Blip"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        Check(p.visits.empty(), "under 5 usable minutes: not placed");
        Check(Skipped(p, "Blip") != nullptr, "and listed as skipped", Skipped(p, "Blip") ? Skipped(p, "Blip")->why : "");
    }

    printf("\n8. The Sun rules\n");
    {
        const std::vector<Curve> cs = {{70.0, 300.0, 1.5}};
        const auto near = BuildNightPlan(Names({"Venus"}), Sampler(cs, 20.0, 18.0), Jd(0), Jd(600), {}, o);
        Check(near.visits.empty(), "18 deg from a risen Sun: never scheduled");
        const auto* s = Skipped(near, "Venus");
        Check(s && s->why.find("Sun") != std::string::npos, "and the reason names the Sun", s ? s->why : "");

        const auto night = BuildNightPlan(Names({"Venus"}), Sampler(cs, -5.0, 18.0), Jd(0), Jd(600), {}, o);
        Check(night.visits.size() == 1, "the same distance with the Sun set is allowed");

        NightPlanOptions dark = o;
        dark.sunMaxAltDeg = -12.0;
        const auto day = BuildNightPlan(Names({"Star"}), Sampler(cs, 10.0, 120.0), Jd(0), Jd(600), {}, dark);
        Check(day.visits.empty(), "with a dark-sky limit, a Sun at 10 deg rules it out");
        const auto dflt = BuildNightPlan(Names({"Star"}), Sampler(cs, 10.0, 120.0), Jd(0), Jd(600), {}, o);
        Check(dflt.visits.size() == 1, "by default daylight is allowed");
    }

    printf("\n9. Not a planet or a star\n");
    {
        std::vector<NightTarget> t = {{"M13", false, "not a planet or a star"}, {"Vega", true, ""}};
        const std::vector<Curve> cs = {{80.0, 300.0, 1.0}, {80.0, 300.0, 1.0}};
        const auto p = BuildNightPlan(t, Sampler(cs), Jd(0), Jd(600), {}, o);
        Check(!Find(p, "M13") && Skipped(p, "M13"), "a cluster is listed, not planned");
        Check(Find(p, "Vega") != nullptr, "the star beside it is planned");
    }

    printf("\n10. A better order schedules more\n");
    {
        // A: PRIME from minute 0 to 100. B: PRIME only from 10 to 30. Placed
        // in the order given, A takes 0-20 and leaves B 3 minutes, which is
        // not a visit. B first gets B all 20 and A 20 after it.
        NightSampler sm = [&](int t, double jd) {
            NightSample s;
            s.sunAltDeg = -30.0;
            const double m = Min(jd);
            if (t == 0) s.altDeg = (m >= 0.0 && m <= 100.0) ? 65.0 : 30.0;
            else        s.altDeg = (m >= 10.0 && m <= 30.0) ? 65.0 : 30.0;
            return s;
        };
        const auto p = BuildNightPlan(Names({"A", "B"}), sm, Jd(-7), Jd(200), {}, o);
        const auto* a = Find(p, "A");
        const auto* b = Find(p, "B");
        Check(a && b, "both are placed", Show(a) + " | " + Show(b));
        Check(a && b && a->ImagingMin() == 20 && b->ImagingMin() == 20, "each gets its full 20 minutes");
        NightPlanOptions scarce = o;
        scarce.exhaustiveMax = 0;           // the rule used beyond six targets
        const auto g = BuildNightPlan(Names({"A", "B"}), sm, Jd(-7), Jd(200), {}, scarce);
        Check(g.visits.size() == 2, "fewest usable minutes first also places both");
        const auto again = BuildNightPlan(Names({"A", "B"}), sm, Jd(-7), Jd(200), {}, o);
        bool same = again.visits.size() == p.visits.size();
        for (size_t i = 0; same && i < p.visits.size(); ++i)
            same = again.visits[i].name == p.visits[i].name && again.visits[i].slewJd == p.visits[i].slewJd;
        Check(same, "the same inputs give the same plan");
    }

    printf("\n11. Two real stars, from two latitudes\n");
    {
        // Kochab and Pherkad. Altitude from the hour angle on a sidereal
        // clock that starts with Kochab on the meridian.
        struct Star { const char* name; double raH, decDeg; };
        const Star stars[] = {{"Kochab", 14.845, 74.155}, {"Pherkad", 15.345, 71.834}};
        auto sampler = [&](double latDeg) {
            return NightSampler([=](int t, double jd) {
                const double d2r = 3.14159265358979 / 180.0;
                const double lst = 14.845 + Min(jd) / 60.0 * 1.00273791;
                const double ha = (lst - stars[t].raH) * 15.0 * d2r;
                const double dec = stars[t].decDeg * d2r, lat = latDeg * d2r;
                NightSample s;
                s.altDeg = std::asin(std::sin(lat) * std::sin(dec) +
                                     std::cos(lat) * std::cos(dec) * std::cos(ha)) / d2r;
                s.sunAltDeg = -30.0;
                return s;
            });
        };
        const auto north = BuildNightPlan(Names({"Kochab", "Pherkad"}), sampler(65.0), Jd(0), Jd(720), {}, o);
        Check(north.visits.size() == 2, "at 65 N both are placed", std::to_string(north.visits.size()));
        Check(north.TotalPrimeMin() == 40, "both in PRIME", std::to_string(north.TotalPrimeMin()));
        const auto home = BuildNightPlan(Names({"Kochab", "Pherkad"}), sampler(33.4), Jd(0), Jd(720), {}, o);
        const auto* k = Find(home, "Kochab");
        Check(k && k->primeMin == 0, "at 33 N Kochab is GOOD only", Show(k));
        Check(k && k->peakAltDeg < 50.0, "peaking under 50 deg", k ? std::to_string(k->peakAltDeg) : "");
    }

    printf("\n12. The plan never runs past its end\n");
    {
        const std::vector<Curve> cs = {{70.0, 590.0, 1.0}};   // best right at the end
        const auto p = BuildNightPlan(Names({"Late"}), Sampler(cs), Jd(0), Jd(600), {}, o);
        const auto* v = Find(p, "Late");
        Check(v && v->endJd <= Jd(600) + 1e-9, "the visit ends by dawn", Show(v));
    }

    // ── The runner ──────────────────────────────────────────
    using R = NightPlanRunner;
    using A = R::Action;
    auto twoVisits = []() {
        NightPlan p;
        NightVisit a; a.name = "Kochab";  a.slewJd = Jd(10); a.imageJd = Jd(17); a.endJd = Jd(37);
        NightVisit b; b.name = "Pherkad"; b.slewJd = Jd(60); b.imageJd = Jd(67); b.endJd = Jd(87);
        p.visits = {a, b};
        return p;
    };
    auto sec = [](double minutes, double s) { return Jd(minutes) + s / 86400.0; };

    printf("\n13. The runner: wait, slew, settle, lock, runs, move on\n");
    {
        R r;
        R::Config c; c.afterLockSec = 0.0;
        Check(r.Start(twoVisits(), c, Jd(0)), "starts");
        R::Inputs in;
        in.nowJd = Jd(5);
        Check(r.Tick(in).kind == A::None && r.State() == R::Phase::Waiting, "waits before the slew time");
        in.nowJd = Jd(10);
        auto s = r.Tick(in);
        Check(s.kind == A::Slew && s.index == 0, "slews at the slew time");
        in.nowJd = sec(10, 0.5);
        in.slewing = false;             // the mount has not admitted to it yet
        r.Tick(in);
        Check(r.State() == R::Phase::Slewing, "a mount that says 'not slewing' at once is not believed");
        in.nowJd = sec(10, 2.0); in.slewing = true;  r.Tick(in);
        in.nowJd = sec(10, 40.0); in.slewing = false; r.Tick(in);
        Check(r.State() == R::Phase::Settling, "settles when the slew ends");
        in.nowJd = sec(10, 46.0);
        s = r.Tick(in);
        Check(s.kind == A::StartLock, "then locks");
        in.nowJd = sec(10, 120.0); in.locked = true;
        s = r.Tick(in);
        Check(s.kind == A::StartRuns && r.State() == R::Phase::Running, "then RUNS");
        in.runsRunning = true;
        in.nowJd = sec(20, 0); r.Tick(in);
        in.nowJd = sec(30, 0); in.runsRunning = false;
        s = r.Tick(in);
        Check(s.kind == A::Visited && !s.stopRuns && r.Note() == "its runs are done",
              "moves on when the runs are done", r.Note());

        // Pherkad is due at 60 but is already in its window: it may start early.
        in.locked = false;
        in.inWindow = true;
        in.nowJd = sec(31, 0);
        s = r.Tick(in);
        Check(s.kind == A::Slew && s.index == 1, "the next starts early when already in its window", r.Note());
        in.nowJd = sec(31, 2); in.slewing = true; r.Tick(in);
        in.nowJd = sec(31, 30); in.slewing = false; r.Tick(in);
        in.nowJd = sec(31, 40); r.Tick(in);
        in.locked = true; in.nowJd = sec(32, 0);
        Check(r.Tick(in).kind == A::StartRuns, "locks and runs");
        in.runsRunning = true;
        in.nowJd = Jd(87);
        s = r.Tick(in);
        Check(s.kind == A::Visited && s.stopRuns && r.Note() == "its slot ended",
              "at the planned end the runs are stopped", r.Note());
        Check(r.State() == R::Phase::Complete && r.Visited() == 2 && r.Skipped() == 0,
              "and the plan is complete", r.Reason());
    }

    printf("\n14. No lock: this target is skipped, the plan goes on\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in;
        in.nowJd = Jd(10); r.Tick(in);
        in.nowJd = sec(10, 2); in.slewing = true; r.Tick(in);
        in.nowJd = sec(10, 20); in.slewing = false; r.Tick(in);
        in.nowJd = sec(10, 30); Check(r.Tick(in).kind == A::StartLock, "locking");
        in.nowJd = sec(10, 30 + 299); Check(r.Tick(in).kind == A::None, "still trying at 299 s");
        in.nowJd = sec(10, 30 + 301);
        const auto s = r.Tick(in);
        Check(s.kind == A::Skipped && s.dropLock && r.Note() == "no lock in 300 s",
              "skipped at the limit, with the acquisition to drop", r.Note());
        Check(r.Running() && r.Index() == 1, "and the plan waits for the next");
        Check(r.OutcomeOf(0) == R::Outcome::Skipped && r.NoteOf(0) == "no lock in 300 s", "the visit keeps its reason");
    }

    printf("\n15. A slew that never ends stops the plan\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.nowJd = Jd(10); r.Tick(in);
        in.slewing = true;
        in.nowJd = sec(10, 181); r.Tick(in);
        Check(r.State() == R::Phase::Failed, "failed", r.Reason());
        Check(r.Reason().find("did not finish") != std::string::npos, "and says why");
    }

    printf("\n16. A person stopping the runs stops the plan\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.nowJd = Jd(10); r.Tick(in);
        in.nowJd = sec(10, 2); in.slewing = true; r.Tick(in);
        in.nowJd = sec(10, 9); in.slewing = false; r.Tick(in);
        in.nowJd = sec(10, 20); r.Tick(in);
        in.locked = true; in.nowJd = sec(10, 30); r.Tick(in);
        in.runsRunning = true; in.nowJd = sec(12, 0); r.Tick(in);
        in.runsRunning = false; in.runsByPerson = true; in.nowJd = sec(13, 0); r.Tick(in);
        Check(r.State() == R::Phase::Aborted, "stopped", r.Reason());
    }

    printf("\n17. STOP during the runs closes them\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.nowJd = Jd(10); r.Tick(in);
        in.nowJd = sec(10, 2); in.slewing = true; r.Tick(in);
        in.nowJd = sec(10, 9); in.slewing = false; r.Tick(in);
        in.nowJd = sec(10, 20); r.Tick(in);
        in.locked = true; in.nowJd = sec(10, 30); r.Tick(in);
        in.runsRunning = true;
        r.Abort("stopped by the operator");
        const auto s = r.Tick(in);
        Check(s.kind == A::Stop && s.stopRuns, "the next step stops the runs");
        Check(!r.Running() && r.Reason() == "stopped by the operator", "and the plan is stopped");
        Check(r.Tick(in).kind == A::None, "then nothing more");
    }

    printf("\n18. A busy mount: wait, and skip when the slot passes\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.mountBusy = true;
        in.nowJd = Jd(12); Check(r.Tick(in).kind == A::None, "waits while a pass has the mount");
        in.nowJd = Jd(33);
        const auto s = r.Tick(in);
        Check(s.kind == A::Skipped && !s.dropLock && r.Note().find("busy") != std::string::npos,
              "skipped once less than 5 minutes are left", r.Note());
    }

    printf("\n19. No early start across a pass\n");
    {
        NightPlan p = twoVisits();
        p.visits.erase(p.visits.begin());
        p.busy = {{Jd(40), Jd(50), "ISS"}};
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(p, c, Jd(30));
        R::Inputs in; in.inWindow = true; in.nowJd = Jd(31);
        Check(r.Tick(in).kind == A::None, "in its window, but a pass comes before the slot: waits");
        in.nowJd = Jd(60);
        Check(r.Tick(in).kind == A::Slew, "starts at its slot");
    }

    printf("\n20. A refused slew skips one target\n");
    {
        R r; R::Config c; c.afterLockSec = 0.0;
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.nowJd = Jd(10);
        Check(r.Tick(in).kind == A::Slew, "slew asked for");
        r.SlewRefused("cord wrap", Jd(10));
        Check(r.Running() && r.Index() == 1 && r.Skipped() == 1, "skipped; the next waits", r.Note());
        R empty; NightPlan none;
        Check(!empty.Start(none, c, Jd(0)) && empty.Reason() == "the plan has no visits", "an empty plan will not start");
    }

    printf("\n21. After the lock, RUNS waits for frames at the new size\n");
    {
        R r; R::Config c;              // afterLockSec 3 s, as the app has it
        r.Start(twoVisits(), c, Jd(10));
        R::Inputs in; in.nowJd = Jd(10); r.Tick(in);
        in.nowJd = sec(10, 2); in.slewing = true; r.Tick(in);
        in.nowJd = sec(10, 9); in.slewing = false; r.Tick(in);
        in.nowJd = sec(10, 20); r.Tick(in);
        in.locked = true; in.framesFlowing = false;
        in.nowJd = sec(10, 30); Check(r.Tick(in).kind == A::None, "locked: no RUNS at once");
        in.nowJd = sec(10, 40); Check(r.Tick(in).kind == A::None, "still no frame rate at the ROI: waits");
        in.framesFlowing = true;
        in.nowJd = sec(10, 41); Check(r.Tick(in).kind == A::StartRuns, "frames flowing, locked 3 s or more: RUNS");
    }

    // ── Phase 163b: pause, stay, change the ticks, continue ──
    printf("\n22. PAUSE stays on a target; CONTINUE plans the rest again from now\n");
    {
        raduk::app::RecordingUiSink ui;
        raduk::app::NightPlanController ctl(ui);
        double now = Jd(10);
        bool runs = false;
        int stops = 0;
        std::vector<std::string> ticks = {"Kochab", "Pherkad"};
        std::vector<std::string> asked;
        raduk::app::NightPlanHooks h;
        h.jdNow = [&]() { return now; };
        h.altNow = [](const std::string&) { return 50.0; };
        h.slew = [](const std::string&, std::string&) { return true; };
        h.mountSlewing = []() { return false; };
        h.startLock = [](std::string&) { return true; };
        h.locked = []() { return true; };
        h.startRuns = [&](std::string&) { runs = true; return true; };
        h.runsRunning = [&]() { return runs; };
        h.stopRuns = [&](const char*) { runs = false; ++stops; };
        h.runsAborted = []() { return false; };
        h.ticked = [&]() { return ticks; };
        // A re-plan from now: one visit a name, back to back, as the
        // planner would place them on a clear sky.
        h.replan = [&](const std::vector<std::string>& names) {
            asked = names;
            NightPlan p;
            p.startJd = now;
            double t = now;
            for (const auto& n : names) {
                NightVisit v; v.name = n; v.slewJd = t; v.imageJd = t + 7 / 1440.0;
                v.endJd = t + 27 / 1440.0; v.goodMin = 20;
                p.visits.push_back(v);
                t = v.endJd;
            }
            return p;
        };
        ctl.SetHooks(h);
        ctl.SetPlan(twoVisits());
        R::Config c; c.afterLockSec = 0.0;
        std::string why;
        Check(ctl.Start(c, why), "the plan runs", why);
        // Kochab: slew, settle, lock, runs.
        for (int s = 0; s <= 60 && !runs; s += 1) { now = sec(10, s); ctl.Tick(); }
        Check(runs && ctl.CurrentTarget() == "Kochab", "Kochab's runs are going");

        Check(ctl.Pause(why), "PAUSE", why);
        Check(ctl.StateLine().rfind("PAUSED on Kochab", 0) == 0, "the state says paused on Kochab", ctl.StateLine());
        Check(!ctl.Pause(why), "a second PAUSE is refused", why);
        now = Jd(45);                      // Kochab's slot ended at 37
        ctl.Tick();
        Check(runs && stops == 0, "past its slot, the runs are left going: the clock is stopped");

        ticks.push_back("Vega");           // the plan changed during the pause
        now = Jd(50);
        Check(ctl.Continue(why), "CONTINUE", why);
        Check(stops == 1 && !runs, "Kochab's runs are stopped as the plan moves on");
        Check(asked.size() == 2 && asked[0] == "Pherkad" && asked[1] == "Vega",
              "re-planned: every ticked target not yet visited, Vega included",
              asked.size() > 1 ? asked[0] + ", " + asked[1] : "");
        Check(ctl.runner.Running() && ctl.runner.Total() == 2, "and the plan runs on with two visits");
        Check(ctl.runner.Visits()[0].slewJd >= Jd(50) - 1e-9, "the new visits start from now, not from the old plan");
        const auto rows = ctl.Rows();
        Check(rows.size() == 3 && rows[0].name == "Kochab" && rows[0].state == "done: stayed on through a pause",
              "Kochab stays in the list, done", rows.empty() ? "" : rows[0].state);
        Check(ui.Saw("[NIGHTPLAN] re-planned 2 visits"), "the re-plan is in the log");

        // Paused before a visit started: nothing to stop, nothing counted.
        for (int s = 0; s < 5; ++s) { now = Jd(50) + s / 86400.0; ctl.Tick(); }
        now = Jd(51);
        ctl.Tick();
        // Run Pherkad and Vega to the end.
        for (int m = 0; m < 70 && ctl.runner.Running(); ++m) {
            for (int s = 0; s < 60; s += 2) { now = Jd(51 + m) + s / 86400.0; ctl.Tick(); }
            if (runs && m % 10 == 9) runs = false;   // the runs finish by themselves
        }
        Check(!ctl.runner.Running() && ui.Saw("[NIGHTPLAN] plan finished: 3 visited, 0 skipped"),
              "the night ends with all three counted");
        Check(ctl.StateLine() == "DONE: 3 visited, 0 skipped", "and the state line counts the whole night",
              ctl.StateLine());
    }

    printf("\n23. CONTINUE with nothing left ends the plan\n");
    {
        raduk::app::RecordingUiSink ui;
        raduk::app::NightPlanController ctl(ui);
        double now = Jd(5);
        raduk::app::NightPlanHooks h;
        h.jdNow = [&]() { return now; };
        h.ticked = []() { return std::vector<std::string>{}; };   // everything unticked
        h.replan = [](const std::vector<std::string>&) { return NightPlan{}; };
        ctl.SetHooks(h);
        ctl.SetPlan(twoVisits());
        R::Config c;
        std::string why;
        ctl.Start(c, why);
        Check(ctl.Pause(why) && ctl.StateLine().rfind("PAUSED before Kochab", 0) == 0,
              "paused while waiting for the first", ctl.StateLine());
        Check(ctl.Continue(why) && !ctl.runner.Running(), "CONTINUE with nothing ticked ends it");
        Check(ui.Saw("plan finished: 0 visited, 0 skipped (every ticked target is visited)") ||
              ui.Saw("plan finished: 0 visited, 0 skipped"), "and says so");
        Check(!ctl.Continue(why), "CONTINUE when not paused is refused", why);
    }

    printf("\n%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
