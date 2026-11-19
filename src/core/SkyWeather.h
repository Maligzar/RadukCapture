#pragma once
// ============================================================
//  SkyWeather -- phase 165.0: the sky's weather, read here
//
//  Until now the weather came from Leoben, the assistant on another PC
//  (net/LeobenWeather.h). Phase 165 moves it in, so a night at the scope
//  does not need a second machine awake. This header is the pure half:
//  reading the services' replies, and the tables that turn them into
//  something the app can use. The fetching (165.1) goes through
//  NetPolicy like every other request, and nothing here touches the
//  network, a file or the clock.
//
//  Three sources:
//    - Open-Meteo, current conditions (CC BY 4.0; credit is due wherever
//      its data is shown, open-meteo.com).
//    - 7Timer!, ASTRO product: seeing, transparency, cloud, by 3-hour
//      timepoint. Its classes are from 7Timer's own documentation,
//      www.7timer.info/doc.php, read 2026-11-19 -- not from Leoben's code,
//      which reads a "seeing2" key 7Timer does not send and takes the
//      1-8 wind class for knots (see the 165 plan, findings 6 and 7).
//    - NWS, active alerts (US only), for the dust-storm floors.
//
//  The rule throughout: a value that is missing, null, out of its
//  documented range, or in a unit this file does not know is ABSENT.
//  It is never replaced by a plausible guess, which is what Leoben did
//  (a reply with no cloud figure scored as 15% cloud).
//
//  Units inside are SI and Celsius: m/s, hPa, metres, degrees C.
//
//  Ported from Leoben (maligzar/leoben, modules/weather.py,
//  weather_live.py and dust_storm.py at 164566e): the dew-risk spreads,
//  the wind tiers, the cloud-layer reading and the dust categories,
//  converted exactly from their mph, miles and degrees F. Leoben's
//  rig-specific deductions (an 8SE at f/10, a heavy payload) are left
//  out: the rig is in rig.ini (phase 149), and a verdict built on
//  these is 165.3's job.
//
//  tests/sky_weather.cpp gates it, on replies under tests/data/sky_weather.
// ============================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "MiniJson.h"

namespace raduk::core::sky {

using Opt = std::optional<double>;

// ── Units ────────────────────────────────────────────────────

inline constexpr double kMphToMs  = 0.44704;      // exact
inline constexpr double kKnotToMs = 1852.0 / 3600.0;
inline constexpr double kMileToM  = 1609.344;     // exact

inline double FahrenheitToC(double f) { return (f - 32.0) * 5.0 / 9.0; }

// Converts v from the named unit to m/s. False for a unit not known.
inline bool SpeedToMs(const std::string& unit, double v, double& out) {
    if (unit == "m/s")  { out = v;              return true; }
    if (unit == "km/h") { out = v / 3.6;        return true; }
    if (unit == "mp/h" || unit == "mph") { out = v * kMphToMs; return true; }
    if (unit == "kn")   { out = v * kKnotToMs;  return true; }
    return false;
}
inline bool TempToC(const std::string& unit, double v, double& out) {
    if (unit == "\xC2\xB0" "C") { out = v;                return true; }   // "°C"
    if (unit == "\xC2\xB0" "F") { out = FahrenheitToC(v); return true; }   // "°F"
    return false;
}
inline bool LengthToM(const std::string& unit, double v, double& out) {
    if (unit == "m")  { out = v;          return true; }
    if (unit == "ft") { out = v * 0.3048; return true; }
    return false;
}

// UTC seconds from a civil date (Howard Hinnant's days_from_civil).
inline int64_t UnixFromUtc(int y, int mo, int d, int h, int mi, int s = 0) {
    y -= mo <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (unsigned)(mo + (mo > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    const int64_t days = era * 146097 + (int64_t)doe - 719468;
    return days * 86400 + h * 3600 + mi * 60 + s;
}

// "2026-11-19T20:15" or "2026-11-19T20:15:00" (UTC) to UTC seconds; -1
// when it does not read.
inline int64_t UnixFromIsoMinute(const std::string& t) {
    int y, mo, d, h, mi, s = 0;
    const int n = std::sscanf(t.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s);
    if (n < 5 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return -1;
    return UnixFromUtc(y, mo, d, h, mi, s);
}

namespace detail {
using raduk::json::JVal;

inline Opt Num(const JVal* o, const char* key) {
    if (!o) return std::nullopt;
    const JVal* v = o->Get(key);
    if (!v || v->kind != JVal::Num || !std::isfinite(v->n)) return std::nullopt;
    return v->n;
}
inline std::string Str(const JVal* o, const char* key) {
    if (!o) return {};
    const JVal* v = o->Get(key);
    return v && v->kind == JVal::Str ? v->s : std::string();
}
inline Opt InRange(Opt v, double lo, double hi) {
    if (!v || *v < lo || *v > hi) return std::nullopt;
    return v;
}
} // namespace detail

// ── Open-Meteo: current conditions ───────────────────────────

struct OmCurrent {
    bool        valid = false;
    std::string error;            // the service's own reason, when it gave one
    int64_t     timeUtc = -1;     // the reading's time; the request asks for UTC
    int         intervalSec = 0;
    Opt tempC, dewPointC, humidityPct, windMs, gustMs, windDirDeg,
        cloudPct, pressureMslHpa, visibilityM;
    // Fields read but set aside, each "name: why", for the log.
    std::vector<std::string> setAside;
};

// The body of /v1/forecast?current=... . valid needs the current block
// and its time; each reading is then present only if its value and unit
// both read. timezone must be UTC (or GMT): the request asks for it,
// and a local time read as UTC would put the reading hours off.
inline bool ParseOpenMeteoCurrent(const std::string& body, OmCurrent& out) {
    using namespace detail;
    out = OmCurrent{};
    JVal root;
    raduk::json::JsonReader r(body);
    if (!r.Parse(root) || root.kind != JVal::Obj) { out.error = "the reply is not JSON"; return false; }
    if (const JVal* e = root.Get("error"); e && e->kind == JVal::Bool && e->b) {
        out.error = Str(&root, "reason");
        if (out.error.empty()) out.error = "the service reported an error";
        return false;
    }
    const std::string tz = Str(&root, "timezone");
    if (tz != "GMT" && tz != "UTC") { out.error = "the times are not UTC (timezone \"" + tz + "\")"; return false; }
    const JVal* cur = root.Get("current");
    const JVal* units = root.Get("current_units");
    if (!cur || cur->kind != JVal::Obj) { out.error = "no current block"; return false; }
    out.timeUtc = UnixFromIsoMinute(Str(cur, "time"));
    if (out.timeUtc < 0) { out.error = "no time on the reading"; return false; }
    if (Opt i = Num(cur, "interval")) out.intervalSec = (int)*i;

    auto unitOf = [&](const char* k) { return Str(units, k); };
    auto aside = [&](const char* k, const char* why) {
        out.setAside.push_back(std::string(k) + ": " + why);
    };
    // One reading: present, its unit known, inside [lo, hi] after conversion.
    auto read = [&](const char* key, Opt& dst, double lo, double hi,
                    bool (*conv)(const std::string&, double, double&)) {
        const Opt v = Num(cur, key);
        if (!v) { if (cur->Get(key)) aside(key, "null or not a number"); return; }
        double si = *v;
        if (conv && !conv(unitOf(key), *v, si)) { aside(key, "unit not known"); return; }
        if (si < lo || si > hi) { aside(key, "out of range"); return; }
        dst = si;
    };
    auto pct = [](const std::string& u, double v, double& o) { o = v; return u == "%"; };
    auto deg = [](const std::string& u, double v, double& o) { o = v; return u == "\xC2\xB0"; };
    auto hpa = [](const std::string& u, double v, double& o) { o = v; return u == "hPa"; };

    read("temperature_2m",       out.tempC,          -90, 60,   TempToC);
    read("dew_point_2m",         out.dewPointC,      -100, 40,  TempToC);
    read("relative_humidity_2m", out.humidityPct,    0, 100,    pct);
    read("wind_speed_10m",       out.windMs,         0, 120,    SpeedToMs);
    read("wind_gusts_10m",       out.gustMs,         0, 150,    SpeedToMs);
    read("wind_direction_10m",   out.windDirDeg,     0, 360,    deg);
    read("cloud_cover",          out.cloudPct,       0, 100,    pct);
    read("pressure_msl",         out.pressureMslHpa, 850, 1090, hpa);
    read("visibility",           out.visibilityM,    0, 1.0e6,  LengthToM);
    out.valid = true;
    return true;
}

// ── 7Timer!, ASTRO ───────────────────────────────────────────

// A closed or open range, from 7Timer's documentation. An open end is
// -inf or +inf: class 1 seeing is "<0.5\"", class 8 is ">2.5\"".
struct Range { double lo, hi; };
inline constexpr double kInf = std::numeric_limits<double>::infinity();

// Seeing, classes 1 to 8, in arcseconds.
inline std::optional<Range> SevenTimerSeeingArcsec(int cls) {
    static const Range t[8] = {{-kInf, 0.5}, {0.5, 0.75}, {0.75, 1.0}, {1.0, 1.25},
                               {1.25, 1.5}, {1.5, 2.0}, {2.0, 2.5}, {2.5, kInf}};
    if (cls < 1 || cls > 8) return std::nullopt;
    return t[cls - 1];
}
// Transparency, classes 1 to 8, in magnitudes per air mass.
inline std::optional<Range> SevenTimerTransparencyMag(int cls) {
    static const Range t[8] = {{-kInf, 0.3}, {0.3, 0.4}, {0.4, 0.5}, {0.5, 0.6},
                               {0.6, 0.7}, {0.7, 0.85}, {0.85, 1.0}, {1.0, kInf}};
    if (cls < 1 || cls > 8) return std::nullopt;
    return t[cls - 1];
}
// Cloud cover, classes 1 to 9, in percent.
inline std::optional<Range> SevenTimerCloudPct(int cls) {
    static const Range t[9] = {{0, 6}, {6, 19}, {19, 31}, {31, 44}, {44, 56},
                               {56, 69}, {69, 81}, {81, 94}, {94, 100}};
    if (cls < 1 || cls > 9) return std::nullopt;
    return t[cls - 1];
}
// Relative humidity, classes -4 to 16: 5% steps, then 95-99, then 100.
inline std::optional<Range> SevenTimerHumidityPct(int cls) {
    if (cls < -4 || cls > 16) return std::nullopt;
    if (cls == 16) return Range{100, 100};
    if (cls == 15) return Range{95, 99};
    const double lo = (cls + 4) * 5.0;
    return Range{lo, lo + 5.0};
}
// 10 m wind, classes 1 to 8, in m/s. A class, not a speed.
inline std::optional<Range> SevenTimerWindMs(int cls) {
    static const Range t[8] = {{-kInf, 0.3}, {0.3, 3.4}, {3.4, 8.0}, {8.0, 10.8},
                               {10.8, 17.2}, {17.2, 24.5}, {24.5, 32.6}, {32.6, kInf}};
    if (cls < 1 || cls > 8) return std::nullopt;
    return t[cls - 1];
}
// Lifted index: 7Timer sends one of eight values, each standing for a band.
inline bool SevenTimerLiftedIndexKnown(int v) {
    for (int k : {-10, -6, -4, -1, 2, 6, 10, 15}) if (v == k) return true;
    return false;
}

struct T7Point {
    int64_t timeUtc = 0;   // init + timepoint hours
    std::optional<int> cloud, seeing, transparency, liftedIndex, humidity, windClass;
    Opt         temp2mC;
    std::string windDir, precType;
};
struct T7Forecast {
    bool        valid = false;
    std::string error;
    int64_t     initUtc = 0;
    std::vector<T7Point> points;
    int         setAside = 0;   // values out of their documented range, or -9999
};

// The JSON body of astro.php?...&output=json. valid needs product
// "astro", an init time and at least one timepoint.
inline bool ParseSevenTimerAstro(const std::string& body, T7Forecast& out) {
    using namespace detail;
    out = T7Forecast{};
    JVal root;
    raduk::json::JsonReader r(body);
    if (!r.Parse(root) || root.kind != JVal::Obj) { out.error = "the reply is not JSON"; return false; }
    if (Str(&root, "product") != "astro") { out.error = "not the ASTRO product"; return false; }
    const std::string init = Str(&root, "init");
    int y, mo, d, h;
    if (init.size() != 10 || std::sscanf(init.c_str(), "%4d%2d%2d%2d", &y, &mo, &d, &h) != 4
        || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23) {
        out.error = "no init time";
        return false;
    }
    out.initUtc = UnixFromUtc(y, mo, d, h, 0);
    const JVal* ds = root.Get("dataseries");
    if (!ds || ds->kind != JVal::Arr) { out.error = "no dataseries"; return false; }

    // An integer class inside [lo, hi]; -9999 ("undefined") and anything
    // outside are absent and counted.
    auto cls = [&](const JVal& p, const char* key, int lo, int hi) -> std::optional<int> {
        const Opt v = Num(&p, key);
        if (!v) return std::nullopt;
        const int i = (int)std::lround(*v);
        if ((double)i != *v || i < lo || i > hi) { out.setAside++; return std::nullopt; }
        return i;
    };
    for (const JVal& p : ds->a) {
        const Opt tp = Num(&p, "timepoint");
        if (!tp || *tp < 0 || *tp > 24 * 16) continue;
        T7Point pt;
        pt.timeUtc      = out.initUtc + (int64_t)std::lround(*tp) * 3600;
        pt.cloud        = cls(p, "cloudcover", 1, 9);
        pt.seeing       = cls(p, "seeing", 1, 8);
        pt.transparency = cls(p, "transparency", 1, 8);
        pt.humidity     = cls(p, "rh2m", -4, 16);
        pt.liftedIndex  = cls(p, "lifted_index", -10, 15);
        if (pt.liftedIndex && !SevenTimerLiftedIndexKnown(*pt.liftedIndex)) {
            pt.liftedIndex.reset();
            out.setAside++;
        }
        if (const JVal* w = p.Get("wind10m")) {
            pt.windClass = cls(*w, "speed", 1, 8);
            pt.windDir   = Str(w, "direction");
        }
        if (Opt t = Num(&p, "temp2m")) {
            if (*t >= -76 && *t <= 60) pt.temp2mC = t; else out.setAside++;
        }
        pt.precType = Str(&p, "prec_type");
        out.points.push_back(pt);
    }
    if (out.points.empty()) { out.error = "no timepoints"; return false; }
    out.valid = true;
    return true;
}

// The timepoint nearest t, if one is within 90 minutes (7Timer is
// 3-hourly); else none.
inline const T7Point* SevenTimerAt(const T7Forecast& f, int64_t t) {
    const T7Point* best = nullptr;
    int64_t bestD = 90 * 60 + 1;
    for (const auto& p : f.points) {
        const int64_t d = p.timeUtc > t ? p.timeUtc - t : t - p.timeUtc;
        if (d < bestD) { bestD = d; best = &p; }
    }
    return best;
}

// ── NWS: active alerts ───────────────────────────────────────

struct NwsAlert { std::string event, headline, severity, onset, ends; };

// The GeoJSON body of api.weather.gov/alerts/active?... . True when it
// is a FeatureCollection, with its alerts (possibly none) in out.
inline bool ParseNwsAlerts(const std::string& body, std::vector<NwsAlert>& out,
                           std::string& error) {
    using namespace detail;
    out.clear();
    error.clear();
    JVal root;
    raduk::json::JsonReader r(body);
    if (!r.Parse(root) || root.kind != JVal::Obj) { error = "the reply is not JSON"; return false; }
    if (Str(&root, "type") != "FeatureCollection") { error = "not a FeatureCollection"; return false; }
    const JVal* fs = root.Get("features");
    if (!fs || fs->kind != JVal::Arr) { error = "no features"; return false; }
    for (const JVal& f : fs->a) {
        const JVal* pr = f.Get("properties");
        if (!pr) continue;
        NwsAlert a;
        a.event    = Str(pr, "event");
        a.headline = Str(pr, "headline");
        a.severity = Str(pr, "severity");
        a.onset    = Str(pr, "onset");
        a.ends     = Str(pr, "ends");
        if (!a.event.empty()) out.push_back(std::move(a));
    }
    return true;
}

// ── Dew ──────────────────────────────────────────────────────

enum class DewRisk { Unknown, Safe, Elevated, Warning };
inline const char* DewRiskText(DewRisk r) {
    switch (r) {
        case DewRisk::Safe:     return "safe";
        case DewRisk::Elevated: return "elevated";
        case DewRisk::Warning:  return "warning";
        default:                return "unknown";
    }
}
// Leoben's spreads, 4 and 8 degrees F, as Celsius differences.
// UNVERIFIED(item 62): dew on the corrector against the spread on a humid night
inline constexpr double kDewWarnSpreadC = 4.0 * 5.0 / 9.0;
// UNVERIFIED(item 62): dew on the corrector against the spread on a humid night
inline constexpr double kDewElevatedSpreadC = 8.0 * 5.0 / 9.0;

inline DewRisk DewRiskFromSpread(Opt tempC, Opt dewPointC) {
    if (!tempC || !dewPointC) return DewRisk::Unknown;
    const double spread = *tempC - *dewPointC;
    if (spread <= kDewWarnSpreadC + 1e-9)     return DewRisk::Warning;
    if (spread <= kDewElevatedSpreadC + 1e-9) return DewRisk::Elevated;
    return DewRisk::Safe;
}

// ── Wind on the tube ─────────────────────────────────────────

// Leoben's tiers: the lowest band that the sustained wind AND the gust
// both satisfy. Bands in mph: 5/8, 10/14, 15/22, 20/28. Leoben's
// deductions for its own rig (f/10, a heavy payload) are not here.
struct WindTier { int base = -1; const char* label = "UNKNOWN"; bool gustKnown = false; };

// UNVERIFIED(item 63): the tracking error and star shake at each band, on the real mount
inline constexpr double kWindBandMph[4][2] = {{5, 8}, {10, 14}, {15, 22}, {20, 28}};

// Without a gust only the sustained limit is tested, and the result
// says so. Leoben invented a gust (1.4 x the wind) instead.
inline WindTier WindTierFrom(Opt windMs, Opt gustMs) {
    WindTier t;
    if (!windMs) return t;
    t.gustKnown = gustMs.has_value();
    static const int  base[5]  = {100, 97, 72, 36, 0};
    static const char* name[5] = {"NEGLIGIBLE", "NEGLIGIBLE", "MARGINAL", "POOR", "DANGER"};
    int band = 4;
    for (int i = 0; i < 4; i++) {
        const bool w = *windMs <= kWindBandMph[i][0] * kMphToMs + 1e-9;
        const bool g = !gustMs || *gustMs <= kWindBandMph[i][1] * kMphToMs + 1e-9;
        if (w && g) { band = i; break; }
    }
    t.base  = base[band];
    t.label = name[band];
    return t;
}

// ── Cloud by layer ───────────────────────────────────────────

struct CloudReading { const char* impact = "UNKNOWN"; const char* dominant = "unknown"; };

// Leoben's reading of low, mid and high cloud for imaging, unchanged
// except that a missing layer gives UNKNOWN rather than counting as 0%.
inline CloudReading ReadCloudLayers(Opt low, Opt mid, Opt high, Opt total) {
    if (!low || !mid || !high) return {};
    const double lo = *low, mi = *mid, hi = *high;
    const double tot = total ? *total : std::fmax(lo, std::fmax(mi, hi));
    if (tot < 10) return {"CLEAR", "none"};
    if (lo > 40)  return {"BLOCKED", "low"};
    if (lo > 15)  return {"POOR", "low"};
    if (mi > 50)  return {"POOR", "mid"};
    if (mi > 20)  return {"MARGINAL", "mid"};
    if (hi > 70)  return {"MARGINAL", "high"};
    if (hi > 30)  return {"FAIR", "high"};
    if (lo <= 10 && mi <= 10) return {"GOOD", "high"};
    return {"FAIR", "mixed"};
}

// ── Dust ─────────────────────────────────────────────────────

struct DustReading {
    int         category = 0;          // 0 clear .. 5 haboob
    const char* label = "Clear";
    bool        observed = false;      // visibility and gust were both known
    bool        fromAlert = false;     // an NWS alert set the category
    std::string alertEvent;
};
inline const char* DustLabel(int c) {
    static const char* t[6] = {"Clear", "Dusty", "Blowing Dust", "Dust Storm",
                               "Severe Dust Storm", "Haboob"};
    return t[c < 0 ? 0 : c > 5 ? 5 : c];
}

namespace detail {
inline std::string Lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}
}

// Leoben's thresholds: visibility at or under, and gust at or over,
// checked from the worst down. In miles and mph: 1/16 & 50, 1/8 & 45,
// 1/4 & 35, 1 & 25, 3 & 20. Missing visibility or gust gives no
// observed category (Leoben read a missing visibility as 10 miles).
// An NWS dust alert sets a floor: "haboob" in the event or headline 5,
// a Dust Storm Warning 4, any other dust advisory 1.
inline DustReading DustFrom(Opt visibilityM, Opt gustMs, const std::vector<NwsAlert>& alerts) {
    DustReading d;
    static const struct { int cat; double visMi, gustMph; } th[5] = {
        {5, 1.0 / 16, 50}, {4, 0.125, 45}, {3, 0.25, 35}, {2, 1.0, 25}, {1, 3.0, 20}};
    if (visibilityM && gustMs) {
        d.observed = true;
        for (const auto& t : th) {
            if (*visibilityM <= t.visMi * kMileToM + 1e-6 && *gustMs >= t.gustMph * kMphToMs - 1e-9) {
                d.category = t.cat;
                break;
            }
        }
    }
    int best = 0;
    const NwsAlert* bestA = nullptr;
    for (const auto& a : alerts) {
        const std::string ev = detail::Lower(a.event), hd = detail::Lower(a.headline);
        int floor = 0;
        if (ev.find("haboob") != std::string::npos || hd.find("haboob") != std::string::npos) floor = 5;
        else if (ev == "dust storm warning") floor = 4;
        else if (ev.find("dust") != std::string::npos) floor = 1;
        if (floor > best) { best = floor; bestA = &a; }
    }
    if (bestA && best >= d.category) {
        d.category  = best;
        d.fromAlert = true;
        d.alertEvent = bestA->event;
    }
    d.label = DustLabel(d.category);
    return d;
}

} // namespace raduk::core::sky
