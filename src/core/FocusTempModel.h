#pragma once
// ============================================================
// FocusTempModel.h -- where focus settles, against focuser temperature
// ============================================================
//
// Phase 174.1. A tube shrinks as it cools and focus walks with it. The
// history (core/FocusHistory.h) holds, for every sweep, the focuser's
// temperature and, in its followup, where the owner actually settled. This
// fits settled position against temperature, per target and filter:
//   - robustly: the slope is the median of pairwise slopes (Theil-Sen),
//     so one bad night (cloud, a wrong sweep) does not bend it;
//   - honestly: it reports the points, nights and temperature span it
//     rests on, the scatter of the points about the line, and says it is
//     not confident until there is enough of each.
//
// It only predicts and suggests. Nothing here moves the focuser: a
// prediction is logged beside each sweep's result so the history itself
// shows whether the model would have helped, before it is ever used to
// start a sweep (the 174 principle: the model proposes, replay judges).
//
// Pure. Reads the history's text with core/FocusReplay.h's JSON reader.
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "FocusReplay.h"

namespace raduk::core::focustemp {

// One ground-truth point: where focus settled, and the temperature then.
struct Point {
    std::string key;          // "<target>|<filter>"
    std::string sweepId;
    double tempC = NAN;
    int settled = 0;
    int night = 0;            // the evening it belongs to: days since 1970, shifted 12 h
};

// The evening a time belongs to: a sweep at 02:00 is the same night as one at 22:00.
inline int NightOf(int64_t unixSec) {
    return (int)((unixSec - 12 * 3600) / 86400);
}

inline std::string KeyOf(const std::string& target, const std::string& filter) {
    return target + "|" + filter;
}

// Sweeps joined to their followups. A sweep with no followup, or no
// temperature, gives no point: settled focus is the label, never a guess.
inline std::vector<Point> PointsFromHistory(const std::string& text) {
    using replay::J;
    std::map<std::string, int> settled;
    std::vector<J> sweeps;
    size_t p = 0;
    while (p < text.size()) {
        size_t nl = text.find('\n', p);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = text.substr(p, nl - p);
        p = nl + 1;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        J j;
        if (!replay::Reader(line).Parse(j) || j.t != J::Obj) continue;
        if (j["kind"].s == "sweep") sweeps.push_back(std::move(j));
        else if (j["kind"].s == "followup") settled[j["sweep"].s] = j["settled"].integer();
    }
    std::vector<Point> out;
    for (const auto& s : sweeps) {
        auto it = settled.find(s["id"].s);
        if (it == settled.end()) continue;
        const double t = s["at"]["focuser_temp_c"].num();
        if (!std::isfinite(t) || t < -60.0 || t > 80.0) continue;
        Point pt;
        pt.key = KeyOf(s["at"]["target"].s, s["at"]["filter"].s);
        pt.sweepId = s["id"].s;
        pt.tempC = t;
        pt.settled = it->second;
        pt.night = NightOf((int64_t)s["unix"].num(0));
        out.push_back(pt);
    }
    return out;
}

inline double Median(std::vector<double> v) {
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// What it takes to be trusted. Below any of these it still fits and
// reports, but Confident() is false and nothing should act on it.
struct Minimums {
    int points = 6;
    int nights = 2;
    double spanC = 2.0;        // a slope from 0.5 C of range is noise
};

struct Fit {
    std::string key;
    int points = 0, nights = 0;
    double tMin = NAN, tMax = NAN;
    double slope = NAN;        // steps per degree C
    double intercept = NAN;    // settled position at 0 C
    double scatter = NAN;      // robust sigma of the points about the line, steps
    bool ok = false;           // a line could be drawn at all
    bool confident = false;    // ...and it rests on enough to use
    std::string why;           // what it lacks, when not confident

    double Predict(double tempC) const { return ok ? intercept + slope * tempC : NAN; }
};

inline Fit FitPoints(const std::string& key, const std::vector<Point>& pts, const Minimums& m = {}) {
    Fit f;
    f.key = key;
    std::set<int> nights;
    for (const auto& p : pts) {
        if (p.key != key) continue;
        ++f.points;
        nights.insert(p.night);
        f.tMin = std::isfinite(f.tMin) ? std::min(f.tMin, p.tempC) : p.tempC;
        f.tMax = std::isfinite(f.tMax) ? std::max(f.tMax, p.tempC) : p.tempC;
    }
    f.nights = (int)nights.size();
    std::vector<const Point*> mine;
    for (const auto& p : pts) if (p.key == key) mine.push_back(&p);

    // Pairwise slopes, only across a real temperature difference.
    std::vector<double> slopes;
    for (size_t i = 0; i < mine.size(); ++i)
        for (size_t j = i + 1; j < mine.size(); ++j) {
            const double dt = mine[j]->tempC - mine[i]->tempC;
            if (std::fabs(dt) >= 0.5) slopes.push_back((mine[j]->settled - mine[i]->settled) / dt);
        }
    if (!slopes.empty()) {
        f.slope = Median(slopes);
        std::vector<double> icpt;
        for (const auto* p : mine) icpt.push_back(p->settled - f.slope * p->tempC);
        f.intercept = Median(icpt);
        std::vector<double> res;
        for (const auto* p : mine) res.push_back(std::fabs(p->settled - (f.intercept + f.slope * p->tempC)));
        f.scatter = 1.4826 * Median(res);
        f.ok = std::isfinite(f.slope) && std::isfinite(f.intercept);
    }

    const double span = f.points ? f.tMax - f.tMin : 0.0;
    char b[160];
    if (!f.ok) {
        snprintf(b, sizeof(b), "%d point(s), no temperature spread to fit", f.points);
        f.why = b;
    } else if (f.points < m.points) {
        snprintf(b, sizeof(b), "%d of %d points", f.points, m.points); f.why = b;
    } else if (f.nights < m.nights) {
        snprintf(b, sizeof(b), "%d of %d nights", f.nights, m.nights); f.why = b;
    } else if (span < m.spanC) {
        snprintf(b, sizeof(b), "%.1f of %.1f C of temperature range", span, m.spanC); f.why = b;
    } else {
        f.confident = true;
    }
    return f;
}

// Every key in the history, fitted.
inline std::vector<Fit> FitAll(const std::vector<Point>& pts, const Minimums& m = {}) {
    std::set<std::string> keys;
    for (const auto& p : pts) keys.insert(p.key);
    std::vector<Fit> out;
    for (const auto& k : keys) out.push_back(FitPoints(k, pts, m));
    return out;
}

// Whether the temperature has moved focus by more than `zoneSteps` (the
// critical focus zone, in focuser steps) since the last focus. Needs a
// confident fit and a known zone; otherwise it never suggests.
inline bool RefocusSuggested(const Fit& f, double tempAtFocus, double tempNow, double zoneSteps) {
    if (!f.confident || !(zoneSteps > 0.0)) return false;
    if (!std::isfinite(tempAtFocus) || !std::isfinite(tempNow)) return false;
    return std::fabs(f.slope * (tempNow - tempAtFocus)) > zoneSteps;
}

} // namespace raduk::core::focustemp
