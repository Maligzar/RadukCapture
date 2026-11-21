#pragma once
// ============================================================
//  SkyRun -- phase 156.1 and 156.2: the unattended sky run's rules
//
//  The owner's terms (2026-11-13), which this file encodes and nothing
//  else may widen:
//    - it refuses to start unless the site, the horizon limits and the
//      slew bounds are set (and the devices answer, and the disk has room);
//    - a target outside the horizon limits or the slew bounds is skipped,
//      not failed;
//    - a test that fails is recorded and passed over;
//    - it stops on clouds (holding, then parking if the dark window ends),
//      on a lost target (that test fails, the next starts), and on a
//      stalled device (the run stops and parks);
//    - it parks at the end.
//
//  Tests come in GROUPS (the owner, 2026-11-13), each tagged with what it
//  needs and the clear-night items it settles, so a group can run on its
//  own for whatever is in view, or the whole list can run unattended,
//  ordered by which target sets first.
//
//  This header is pure: it decides, and the app (156.2's wiring, driving
//  the app through Dash, docs/TESTING.md) acts. Times are seconds passed
//  in. tests/sky_run.cpp is the dress rehearsal (156.5) of every decision,
//  on CI.
//
//  Results go to results.json (156.3) through ResultsJson(); an emulator
//  run says so, and tools/sky_checklist_update.py never marks the
//  checklist from one (only the sky settles an UNVERIFIED value).
// ============================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace raduk::core::skyrun {

// ── Limits: the horizon and the slew bounds (rig.ini) ────────

struct HorizonPoint { double azDeg, minAltDeg; };

// rig.ini SkyHorizon=az:alt,az:alt,...  e.g. "0:15,90:25,180:20,270:15".
// Points are sorted by azimuth; between them the limit is interpolated,
// wrapping through north. One point is a flat horizon. Empty is unset.
inline bool ParseHorizon(const std::string& text, std::vector<HorizonPoint>& out, std::string& why) {
    out.clear();
    why.clear();
    size_t p = 0;
    while (p < text.size()) {
        size_t c = text.find(',', p);
        if (c == std::string::npos) c = text.size();
        const std::string item = text.substr(p, c - p);
        p = c + 1;
        if (item.find_first_not_of(" \t") == std::string::npos) continue;
        double az = 0, alt = 0;
        char extra = 0;
        if (std::sscanf(item.c_str(), " %lf : %lf %c", &az, &alt, &extra) != 2) {
            why = "\"" + item + "\" is not az:alt";
            out.clear();
            return false;
        }
        if (az < 0 || az >= 360 || alt < 0 || alt >= 90) {
            why = "\"" + item + "\" is outside az 0-360, alt 0-90";
            out.clear();
            return false;
        }
        out.push_back({az, alt});
    }
    std::sort(out.begin(), out.end(), [](const HorizonPoint& a, const HorizonPoint& b) { return a.azDeg < b.azDeg; });
    if (out.empty()) why = "no horizon points";
    return !out.empty();
}

// The lowest altitude allowed at an azimuth.
inline double MinAltAt(const std::vector<HorizonPoint>& h, double azDeg) {
    if (h.empty()) return 90.0;   // unset: nothing is allowed
    if (h.size() == 1) return h[0].minAltDeg;
    azDeg = std::fmod(std::fmod(azDeg, 360.0) + 360.0, 360.0);
    for (size_t i = 0; i < h.size(); i++) {
        const HorizonPoint& a = h[i];
        const HorizonPoint& b = h[(i + 1) % h.size()];
        const double span = i + 1 < h.size() ? b.azDeg - a.azDeg : b.azDeg + 360.0 - a.azDeg;
        double d = azDeg - a.azDeg;
        if (d < 0) d += 360.0;
        if (d <= span) return a.minAltDeg + (b.minAltDeg - a.minAltDeg) * (span > 0 ? d / span : 0.0);
    }
    return h.back().minAltDeg;
}

struct Limits {
    std::vector<HorizonPoint> horizon;   // empty: unset
    bool   slewSet = false;
    double altMaxDeg = 90;               // the slew bounds: highest altitude,
    double azMinDeg = 0, azMaxDeg = 360; // and the azimuth range (min > max wraps north)
};

// rig.ini SkySlewAltMax=<deg> and SkySlewAz=<min>..<max> ("0..360" for
// no azimuth limit, set on purpose). Both must be set.
inline bool ParseSlewBounds(const std::string& altMax, const std::string& azRange, Limits& l, std::string& why) {
    why.clear();
    l.slewSet = false;
    double a = 0, lo = 0, hi = 0;
    char extra = 0;
    if (altMax.empty() || azRange.empty()) { why = "SkySlewAltMax and SkySlewAz are not both set"; return false; }
    if (std::sscanf(altMax.c_str(), " %lf %c", &a, &extra) != 1 || a <= 0 || a > 90) {
        why = "SkySlewAltMax \"" + altMax + "\" is not an altitude in 0-90"; return false;
    }
    // Split on ".." first: sscanf's %lf would take "300." as a number and
    // swallow the first dot.
    const size_t dots = azRange.find("..");
    bool azOk = dots != std::string::npos;
    if (azOk) {
        const std::string a1 = azRange.substr(0, dots), a2 = azRange.substr(dots + 2);
        azOk = std::sscanf(a1.c_str(), " %lf %c", &lo, &extra) == 1 &&
               std::sscanf(a2.c_str(), " %lf %c", &hi, &extra) == 1;
    }
    if (!azOk || lo < 0 || lo > 360 || hi < 0 || hi > 360) {
        why = "SkySlewAz \"" + azRange + "\" is not min..max in 0-360"; return false;
    }
    l.altMaxDeg = a; l.azMinDeg = lo; l.azMaxDeg = hi; l.slewSet = true;
    return true;
}

// Inside the limits? `why` says which limit, for the skipped-horizon line.
inline bool WithinLimits(const Limits& l, double altDeg, double azDeg, std::string* why = nullptr) {
    char b[160];
    const double minAlt = MinAltAt(l.horizon, azDeg);
    if (altDeg < minAlt) {
        if (why) { std::snprintf(b, sizeof(b), "at %.0f deg, below the horizon limit of %.0f deg at azimuth %.0f", altDeg, minAlt, azDeg); *why = b; }
        return false;
    }
    if (altDeg > l.altMaxDeg) {
        if (why) { std::snprintf(b, sizeof(b), "at %.0f deg, above the slew bound of %.0f deg", altDeg, l.altMaxDeg); *why = b; }
        return false;
    }
    const double az = std::fmod(std::fmod(azDeg, 360.0) + 360.0, 360.0);
    const bool inAz = l.azMinDeg <= l.azMaxDeg ? (az >= l.azMinDeg && az <= l.azMaxDeg)
                                               : (az >= l.azMinDeg || az <= l.azMaxDeg);
    if (!inAz) {
        if (why) { std::snprintf(b, sizeof(b), "at azimuth %.0f, outside the slew bounds %.0f-%.0f", az, l.azMinDeg, l.azMaxDeg); *why = b; }
        return false;
    }
    return true;
}

// rig.ini SkyStars=Vega,Arcturus,Capella  -- the star-field group's
// stars, from the owner's own list (2026-11-21), in his order of
// preference. Names are trimmed; empty entries dropped.
inline std::vector<std::string> ParseStarList(const std::string& text) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p <= text.size()) {
        size_t c = text.find(',', p);
        if (c == std::string::npos) c = text.size();
        std::string n = text.substr(p, c - p);
        const size_t a = n.find_first_not_of(" \t"), b = n.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(n.substr(a, b - a + 1));
        p = c + 1;
    }
    return out;
}

// rig.ini SkyDarkSunAltDeg (default -10, the owner, 2026-11-21): the run
// works while the Sun is below it, waits for it before starting, and
// parks when the Sun rises past it.
inline constexpr double kDefaultDarkSunAltDeg = -10.0;
inline bool IsDark(double sunAltDeg, double thresholdDeg = kDefaultDarkSunAltDeg) {
    return sunAltDeg < thresholdDeg;
}

// rig.ini SkyParkAltAz=az:alt (the owner, 2026-11-21): where the run parks
// the tube, at the end, on a stall, at dawn or on STOP. The app converts it
// to RA/Dec for the moment, GoTos it with the ordinary slew and turns
// tracking off (the app had no park before 156.2a). No default.
struct ParkPosition { bool set = false; double azDeg = 0, altDeg = 0; };

inline bool ParsePark(const std::string& text, ParkPosition& p, std::string& why) {
    p = ParkPosition{};
    why.clear();
    double az = 0, alt = 0;
    char extra = 0;
    if (text.find_first_not_of(" \t") == std::string::npos) { why = "SkyParkAltAz is not set"; return false; }
    if (std::sscanf(text.c_str(), " %lf : %lf %c", &az, &alt, &extra) != 2) {
        why = "SkyParkAltAz \"" + text + "\" is not az:alt"; return false;
    }
    if (az < 0 || az >= 360 || alt < 0 || alt > 90) {
        why = "SkyParkAltAz \"" + text + "\" is outside az 0-360, alt 0-90"; return false;
    }
    p.set = true; p.azDeg = az; p.altDeg = alt;
    return true;
}

// The park slew keeps to the slew bounds (the altitude ceiling, the
// azimuth range for the cables), but not to the horizon limits: those are
// for imaging over the trees, and a low park is deliberate.
inline bool ParkWithinBounds(const ParkPosition& p, const Limits& l, std::string* why = nullptr) {
    if (!p.set) { if (why) *why = "no park position"; return false; }
    Limits noHorizon = l;
    noHorizon.horizon = {{0.0, 0.0}};
    return WithinLimits(noHorizon, p.altDeg, p.azDeg, why);
}

// ── Pre-flight (156.1) ───────────────────────────────────────

struct PreFlightIn {
    bool   siteSet = false;
    Limits limits;
    ParkPosition park;
    bool   mountConnected = false, mountAligned = false;
    bool   cameraOk = false, focuserOk = false, solverOk = false;
    double freeGB = 0, plannedGB = 0;
};

// Every reason not to start, in the order a person would fix them. Empty
// means go. The solver and focuser are needed by some groups only, so
// their absence is listed for the groups to read, not a refusal here.
inline std::vector<std::string> PreFlight(const PreFlightIn& in) {
    std::vector<std::string> no;
    if (!in.siteSet) no.push_back("the site is not set in rig.ini");
    if (in.limits.horizon.empty()) no.push_back("the horizon limits are not set (rig.ini SkyHorizon)");
    if (!in.limits.slewSet) no.push_back("the slew bounds are not set (rig.ini SkySlewAltMax and SkySlewAz)");
    if (!in.park.set) no.push_back("the park position is not set (rig.ini SkyParkAltAz)");
    else if (in.limits.slewSet) {
        std::string why;
        if (!ParkWithinBounds(in.park, in.limits, &why)) no.push_back("the park position is outside the slew bounds: it is " + why);
    }
    if (!in.mountConnected) no.push_back("the mount is not connected");
    else if (!in.mountAligned) no.push_back("the mount is not aligned");
    if (!in.cameraOk) no.push_back("the camera does not answer");
    if (in.plannedGB > 0 && in.freeGB < in.plannedGB) {
        char b[96];
        std::snprintf(b, sizeof(b), "the disk has %.1f GB free, the run plans %.1f GB", in.freeGB, in.plannedGB);
        no.push_back(b);
    }
    return no;
}

// ── Groups ───────────────────────────────────────────────────

enum class Needs { AnyPlanet, Jupiter, Moon, StarField, BrightTarget };
enum Device : unsigned { kCamera = 1, kFocuser = 2, kMount = 4, kSolver = 8 };

struct Group {
    const char* id;
    const char* name;
    Needs       needs;
    unsigned    devices;
    std::vector<int> items;   // clear-night checklist items it settles
    int         estMinutes;
};

// The plan's table (docs/Phase156-Plan-Live-Sky-Test-Run.md).
inline const std::vector<Group>& Groups() {
    static const std::vector<Group> g = {
        {"planet",   "Any planet: sharpness, autofocus and the fine pass, the exposure ladder, throughput",
                     Needs::AnyPlanet, kCamera | kFocuser | kMount, {1, 13, 47, 55, 58}, 25},
        {"jupiter",  "Jupiter: Autorun round an event", Needs::Jupiter, kCamera | kFocuser | kMount, {57}, 40},
        {"moon",     "The Moon: the surface fine pass, the mosaic", Needs::Moon, kCamera | kFocuser | kMount, {18, 58}, 30},
        {"solve",    "A star field: plate scale, row order, pointing", Needs::StarField, kCamera | kMount | kSolver, {3, 15, 53}, 10},
        {"tracking", "Tracking: lock, hand jog, box change", Needs::BrightTarget, kCamera | kMount, {48, 49, 50}, 15},
    };
    return g;
}

inline const Group* FindGroup(const std::string& id) {
    for (const auto& g : Groups()) if (id == g.id) return &g;
    return nullptr;
}

struct Target {
    std::string name;          // "Jupiter", "Moon", "Vega"
    std::string type;          // "planet" | "moon" | "star"
    double altDeg = 0, azDeg = 0;
    double setsInMin = 1e9;    // minutes until it falls below the horizon limit
};

struct Applies {
    bool ok = false;
    bool fits = false;         // the target is the kind the group needs
    bool horizon = false;      // not ok because of the limits: a skipped-horizon, not a not-available
    std::string why;
};

// Whether a group can run on this target now, with these devices.
inline Applies Applicable(const Group& g, const Target& t, unsigned devices, const Limits& l) {
    Applies a;
    auto lower = [](std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; };
    const std::string name = lower(t.name), type = lower(t.type);
    bool fits = false;
    switch (g.needs) {
        case Needs::AnyPlanet:    fits = type == "planet"; break;
        case Needs::Jupiter:      fits = name == "jupiter"; break;
        case Needs::Moon:         fits = type == "moon" || name == "moon"; break;
        case Needs::StarField:    fits = type == "star"; break;
        case Needs::BrightTarget: fits = !t.name.empty(); break;
    }
    if (!fits) { a.why = "needs " + std::string(g.needs == Needs::AnyPlanet ? "a planet" : g.needs == Needs::Jupiter ? "Jupiter"
                         : g.needs == Needs::Moon ? "the Moon" : g.needs == Needs::StarField ? "a star field" : "a target")
                         + ", not " + (t.name.empty() ? std::string("nothing") : t.name); return a; }
    a.fits = true;
    const unsigned missing = g.devices & ~devices;
    if (missing) {
        a.why = std::string("needs ") + ((missing & kCamera) ? "the camera " : "") + ((missing & kFocuser) ? "the focuser " : "")
                + ((missing & kMount) ? "the mount " : "") + ((missing & kSolver) ? "the plate solver " : "");
        a.why.pop_back();
        return a;
    }
    std::string why;
    if (!WithinLimits(l, t.altDeg, t.azDeg, &why)) { a.horizon = true; a.why = t.name + " " + why; return a; }
    if (t.setsInMin * 60.0 < g.estMinutes * 60.0) {
        a.horizon = true;
        char b[120];
        std::snprintf(b, sizeof(b), "%s sets in %.0f min, the group needs about %d", t.name.c_str(), t.setsInMin, g.estMinutes);
        a.why = b;
        return a;
    }
    a.ok = true;
    return a;
}

// ── The plan: what runs, in what order ───────────────────────

// Collected (the owner, 2026-11-21): the evidence is gathered and a
// person's judgment is owed. Passed is kept for items with a criterion a
// machine can check, so an item judged by eye is never marked passed just
// because its recording happened.
enum class Status { Pending, Running, Passed, Collected, Failed, SkippedHorizon, Clouded, AbortedStall, NotReached, NotAvailable };

inline const char* StatusText(Status s) {
    switch (s) {
        case Status::Pending:        return "pending";
        case Status::Running:        return "running";
        case Status::Passed:         return "passed";
        case Status::Collected:      return "collected";
        case Status::Failed:         return "failed";
        case Status::SkippedHorizon: return "skipped-horizon";
        case Status::Clouded:        return "clouded";
        case Status::AbortedStall:   return "aborted-stall";
        case Status::NotReached:     return "not-reached";
        case Status::NotAvailable:   return "not-available";
    }
    return "?";
}

struct Measure { int item; std::string what; double value; double expected; bool hasExpected; std::string unit; };

struct Entry {
    std::string group, target;
    std::vector<int> items;
    Status status = Status::Pending;
    std::string detail;
    std::vector<Measure> measures;
    double startSec = -1, endSec = -1;
};

// One entry per (group, target) pair the night offers. Those that apply
// come first, the target that sets soonest first; a skipped-horizon or a
// not-available is decided now and kept in the list, with its reason, so
// the results say what was not run and why. A group runs on one target:
// the first that applies.
inline std::vector<Entry> MakePlan(const std::vector<std::string>& groupIds, const std::vector<Target>& targets,
                                   unsigned devices, const Limits& l) {
    struct Cand { Entry e; double sets; };
    std::vector<Cand> run, rest;
    for (const std::string& id : groupIds) {
        const Group* g = FindGroup(id);
        if (!g) continue;
        bool placed = false;
        Entry best;
        double bestSets = 1e18;
        // Reasons, most telling first: a target that fits but lacks a
        // device; one that fits but is outside the limits; any other.
        std::string deviceWhy, horizonWhy, otherWhy;
        for (const Target& t : targets) {
            const Applies a = Applicable(*g, t, devices, l);
            if (a.ok) {
                if (t.setsInMin < bestSets) {
                    best = Entry{};
                    best.group = g->id; best.target = t.name; best.items = g->items;
                    bestSets = t.setsInMin;
                }
                placed = true;
            } else if (a.horizon) {
                if (horizonWhy.empty()) horizonWhy = a.why;
            } else if (a.fits) {
                if (deviceWhy.empty()) deviceWhy = a.why;
            } else if (otherWhy.empty()) {
                otherWhy = a.why;
            }
        }
        if (placed) { run.push_back({best, bestSets}); continue; }
        Entry e;
        e.group = g->id; e.items = g->items;
        // A missing device makes it not available tonight, whatever the
        // limits; only a group that could run but for them is a horizon skip.
        e.status = deviceWhy.empty() && !horizonWhy.empty() ? Status::SkippedHorizon : Status::NotAvailable;
        e.detail = !deviceWhy.empty() ? deviceWhy : !horizonWhy.empty() ? horizonWhy
                 : !otherWhy.empty() ? otherWhy : "no target tonight";
        rest.push_back({e, 0});
    }
    std::stable_sort(run.begin(), run.end(), [](const Cand& a, const Cand& b) { return a.sets < b.sets; });
    std::vector<Entry> out;
    for (auto& c : run) out.push_back(c.e);
    for (auto& c : rest) out.push_back(c.e);
    return out;
}

// ── The supervisor (156.2) ───────────────────────────────────

enum class Action { None, StartNext, Hold, Park, Done };

inline const char* ActionText(Action a) {
    switch (a) {
        case Action::None:      return "none";
        case Action::StartNext: return "start-next";
        case Action::Hold:      return "hold";
        case Action::Park:      return "park";
        case Action::Done:      return "done";
    }
    return "?";
}

// Drives the plan through the night's events. The app does what each call
// returns: StartNext (run Current()), Hold (wait, then call Tick), Park
// (park the mount, close the camera, then call Parked), Done.
class Supervisor {
public:
    static constexpr double kRecheckCloudSec = 300;   // check the sky again every 5 minutes

    void Begin(std::vector<Entry> plan, bool emulator) {
        m_plan = std::move(plan);
        m_emulator = emulator;
        m_idx = -1;
        m_parked = false;
        m_holding = false;
        m_stopReason.clear();
    }

    // The first step, or the next after a result.
    Action Next(double now) {
        if (m_parked) return Action::Done;
        for (size_t i = (size_t)(m_idx + 1); i < m_plan.size(); i++) {
            if (m_plan[i].status == Status::Pending) {
                m_idx = (int)i;
                m_plan[i].status = Status::Running;
                m_plan[i].startSec = now;
                return Action::StartNext;
            }
        }
        if (m_stopReason.empty()) m_stopReason = "the end of the list";
        return Action::Park;
    }

    // The running group's own result: Passed, Collected or Failed.
    Action Result(Status outcome, const std::string& detail, std::vector<Measure> measures, double now) {
        Entry* e = Running();
        if (!e) return Action::None;
        if (outcome != Status::Passed && outcome != Status::Collected) outcome = Status::Failed;
        e->status = outcome;
        e->detail = detail;
        e->measures = std::move(measures);
        e->endSec = now;
        return Next(now);
    }

    // The target was lost: that test fails, and the next starts.
    Action TargetLost(const std::string& why, double now) {
        Entry* e = Running();
        if (!e) return Action::None;
        e->status = Status::Failed;
        e->detail = "target lost: " + why;
        e->endSec = now;
        return Next(now);
    }

    // The sky closed: the test stops as clouded and is offered again once
    // the sky clears; the run holds and looks again every few minutes.
    Action Clouds(const std::string& why, double now) {
        Entry* e = Running();
        if (e) {
            Entry again = *e;
            e->status = Status::Clouded;
            e->detail = "clouds: " + why;
            e->endSec = now;
            again.status = Status::Pending;   // offered again after the hold
            again.startSec = again.endSec = -1;
            m_plan.insert(m_plan.begin() + m_idx + 1, again);
        }
        m_holding = true;
        m_holdUntil = now + kRecheckCloudSec;
        return Action::Hold;
    }

    // During a hold: still clouded, or clear again.
    Action Tick(bool clear, double now) {
        if (!m_holding) return Action::None;
        if (now < m_holdUntil) return Action::Hold;
        if (!clear) { m_holdUntil = now + kRecheckCloudSec; return Action::Hold; }
        m_holding = false;
        return Next(now);
    }

    // A device stopped answering past its watchdog: stop the run, park.
    Action Stall(const std::string& device, double now) {
        Entry* e = Running();
        if (e) {
            e->status = Status::AbortedStall;
            e->detail = device + " stalled";
            e->endSec = now;
        }
        m_stopReason = device + " stalled";
        MarkRestNotReached();
        return Action::Park;
    }

    // STOP, from Dash or the app (156.2a): the running test and the rest
    // are not reached, the reason is the owner's, and the run parks.
    Action Stopped(const std::string& reason, double now) {
        Entry* e = Running();
        if (e) { e->status = Status::NotReached; e->detail = "stopped: " + reason; e->endSec = now; }
        m_holding = false;
        m_stopReason = reason;
        MarkRestNotReached();
        return Action::Park;
    }

    // The dark window closed: whatever is left is not reached; park.
    Action DarkEnds(double now) {
        Entry* e = Running();
        if (e) { e->status = Status::NotReached; e->detail = "the dark window closed during it"; e->endSec = now; }
        m_holding = false;
        m_stopReason = "the dark window closed";
        MarkRestNotReached();
        return Action::Park;
    }

    Action Parked() { m_parked = true; return Action::Done; }

    const Entry* Current() const { return m_idx >= 0 && m_idx < (int)m_plan.size() ? &m_plan[(size_t)m_idx] : nullptr; }
    const std::vector<Entry>& Plan() const { return m_plan; }
    bool Holding() const { return m_holding; }
    bool ParkedDone() const { return m_parked; }
    const std::string& StopReason() const { return m_stopReason; }
    bool Emulator() const { return m_emulator; }

private:
    Entry* Running() {
        if (m_idx < 0 || m_idx >= (int)m_plan.size()) return nullptr;
        Entry& e = m_plan[(size_t)m_idx];
        return e.status == Status::Running ? &e : nullptr;
    }
    void MarkRestNotReached() {
        for (auto& e : m_plan)
            if (e.status == Status::Pending) { e.status = Status::NotReached; e.detail = "not reached: " + m_stopReason; }
    }

    std::vector<Entry> m_plan;
    bool   m_emulator = false, m_parked = false, m_holding = false;
    int    m_idx = -1;
    double m_holdUntil = 0;
    std::string m_stopReason;
};

// ── results.json (156.3) ─────────────────────────────────────

inline std::string JsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}
inline std::string JsonNum(double v, int prec = 4) {
    if (!std::isfinite(v)) return "null";
    char b[48];
    std::snprintf(b, sizeof(b), "%.*f", prec, v);
    return b;
}

inline std::string ResultsJson(const Supervisor& s, const std::string& night, const std::string& appVersion) {
    std::string j = "{\n\"format\":\"raduk-sky-results\",\"version\":1,\"night\":" + JsonStr(night) +
                    ",\"app\":" + JsonStr(appVersion) + ",\"emulator\":" + (s.Emulator() ? "true" : "false") +
                    ",\"stopped\":" + JsonStr(s.StopReason()) + ",\"parked\":" + (s.ParkedDone() ? "true" : "false") +
                    ",\n\"tests\":[";
    const auto& plan = s.Plan();
    for (size_t i = 0; i < plan.size(); i++) {
        const Entry& e = plan[i];
        j += (i ? ",\n " : "\n ");
        j += "{\"group\":" + JsonStr(e.group) + ",\"target\":" + JsonStr(e.target) +
             ",\"status\":" + JsonStr(StatusText(e.status)) + ",\"detail\":" + JsonStr(e.detail) + ",\"items\":[";
        for (size_t k = 0; k < e.items.size(); k++) j += (k ? "," : "") + std::to_string(e.items[k]);
        j += "],\"start\":" + JsonNum(e.startSec, 1) + ",\"end\":" + JsonNum(e.endSec, 1) + ",\"measures\":[";
        for (size_t k = 0; k < e.measures.size(); k++) {
            const Measure& m = e.measures[k];
            j += (k ? "," : "") + std::string("{\"item\":") + std::to_string(m.item) + ",\"what\":" + JsonStr(m.what) +
                 ",\"value\":" + JsonNum(m.value) + ",\"expected\":" + (m.hasExpected ? JsonNum(m.expected) : "null") +
                 ",\"unit\":" + JsonStr(m.unit) + "}";
        }
        j += "]}";
    }
    j += "\n]\n}\n";
    return j;
}

} // namespace raduk::core::skyrun
