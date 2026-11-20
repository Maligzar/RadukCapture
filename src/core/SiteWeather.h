#pragma once
// ============================================================
//  SiteWeather -- phase 165.2: which weather the app uses
//
//  Two sources can now answer "what is the air doing at the scope":
//  RadukCapture's own fetch (core/SkyWeatherClient.h, 165.1) and Leoben
//  on the LAN (net/LeobenClient.h). rig.ini WeatherSource= chooses:
//
//    internal   RadukCapture's own fetch (the default for a new rig)
//    leoben     Leoben only, as before 165
//    both       internal first, Leoben when internal has no fresh
//               reading; and every 10 minutes a WXCMP line in the
//               session log with both side by side (the parity nights)
//    off        no weather
//
//  With WeatherSource unset, a rig.ini that has a LeobenUrl starts on
//  "both", so nobody loses weather on upgrade, and one without starts on
//  "internal".
//
//  Every consumer in main.cpp (the ADC's live air, the dew risk on a
//  fade, the hourly SEEING line, the observation record, the status
//  window and the AI context) reads one SiteWeather, so a consumer never
//  needs to know where the weather came from. A field is absent unless
//  the chosen source gave it fresh: nothing is filled in.
//
//  Pure: no windows.h. tests/site_weather.cpp gates it.
// ============================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "SkyWeatherClient.h"
#include "../net/LeobenWeather.h"

namespace raduk::core::sky {

enum class WeatherMode { Internal, Leoben, Both, Off };

inline const char* WeatherModeText(WeatherMode m) {
    switch (m) {
        case WeatherMode::Internal: return "internal";
        case WeatherMode::Leoben:   return "leoben";
        case WeatherMode::Both:     return "both";
        case WeatherMode::Off:      return "off";
    }
    return "?";
}

// rig.ini WeatherSource=. Unknown text falls back to the default, and
// `known` says so, for one log line.
inline WeatherMode ParseWeatherSource(const std::string& v, bool hasLeobenUrl, bool* known = nullptr) {
    std::string s;
    for (char c : v) if (c != ' ' && c != '\t') s += (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    if (known) *known = true;
    if (s == "internal") return WeatherMode::Internal;
    if (s == "leoben")   return WeatherMode::Leoben;
    if (s == "both")     return WeatherMode::Both;
    if (s == "off")      return WeatherMode::Off;
    if (known) *known = s.empty();
    return hasLeobenUrl ? WeatherMode::Both : WeatherMode::Internal;
}

inline bool ModeUsesInternal(WeatherMode m) { return m == WeatherMode::Internal || m == WeatherMode::Both; }
inline bool ModeUsesLeoben(WeatherMode m)   { return m == WeatherMode::Leoben || m == WeatherMode::Both; }

struct SiteWeather {
    bool        configured = false;   // a source is chosen (WeatherSource is not off)
    bool        fresh = false;        // a reading under 30 minutes old
    std::string source;               // "internal" | "leoben", when fresh
    std::string state;                // why there is no fresh reading
    int64_t     fetchedUnix = 0;

    Opt cloudPct, tempC, humidityPct, windMs, gustMs, dewPointC, pressureMslHpa;
    std::string dewRisk;              // "safe" | "elevated" | "warning", or empty
    int         imagingScore = -1;    // Leoben's 0-100; ours waits for 165.3
    std::string verdict;              // Leoben's GO / CAUTION / NO-GO; ours waits for 165.3
    Opt         seeing7Timer;         // 7Timer's class 1-8 for the hour
    Opt         wind200hPaKmh, wind250hPaKmh;

    // The air above the scope, for the ADC: sea level reduced to the site.
    Opt StationHpa(double elevM) const {
        if (!pressureMslHpa || !tempC) return std::nullopt;
        return raduk::net::StationPressureHpa(*pressureMslHpa, elevM, *tempC);
    }
};

// RadukCapture's own reading.
inline SiteWeather FromSky(const SkySnapshot& s, int64_t now) {
    SiteWeather w;
    w.configured = true;
    if (s.CurrentFresh(now)) {
        const auto& c = s.current;
        w.fresh = true;
        w.source = "internal";
        w.fetchedUnix = s.Of(Source::OpenMeteo).lastGoodUnix;
        w.cloudPct = c.cloudPct;  w.tempC = c.tempC;  w.humidityPct = c.humidityPct;
        w.windMs = c.windMs;      w.gustMs = c.gustMs; w.dewPointC = c.dewPointC;
        w.pressureMslHpa = c.pressureMslHpa;
        const DewRisk d = s.Dew(now);
        if (d != DewRisk::Unknown) w.dewRisk = DewRiskText(d);
    } else {
        const auto& st = s.Of(Source::OpenMeteo);
        w.state = st.lastGoodUnix > 0 && s.current.valid ? "last reading is stale"
                                                         : SourceStateText(st.state);
    }
    // The forecast is a forecast: it counts while it covers the hour,
    // whether or not the current reading is fresh.
    if (const T7Point* p = s.SeeingNow(now); p && p->seeing) w.seeing7Timer = (double)*p->seeing;
    return w;
}

// Leoben's reading, through the same struct. Its "absent" fields are
// -1 (seeing, upper winds) or simply not sent; a reading that is not
// fresh gives nothing.
inline SiteWeather FromLeoben(const raduk::net::LeobenConditions& c, int64_t now,
                              const std::string& clientState) {
    SiteWeather w;
    w.configured = true;
    if (!raduk::net::LeobenFresh(c, now)) {
        w.state = c.valid ? "last reading is stale" : clientState;
        return w;
    }
    w.fresh = true;
    w.source = "leoben";
    w.fetchedUnix = c.fetchedUnix;
    w.cloudPct = c.cloudPct;
    w.tempC = c.TempC();
    w.humidityPct = c.humidityPct;
    w.windMs = c.WindMs();
    w.gustMs = c.GustMs();
    w.dewPointC = c.DewPointC();
    if (c.pressureMslInHg > 0) w.pressureMslHpa = c.PressureMslHpa();
    w.dewRisk = c.dewRisk;
    w.imagingScore = c.imagingScore;
    w.verdict = c.goNoGo;
    if (c.seeing7Timer >= 0)  w.seeing7Timer = c.seeing7Timer;
    if (c.wind200hPaKmh >= 0) w.wind200hPaKmh = c.wind200hPaKmh;
    if (c.wind250hPaKmh >= 0) w.wind250hPaKmh = c.wind250hPaKmh;
    return w;
}

// The one the app uses. In "both", internal wins when fresh; Leoben
// stands in when it is not; with neither fresh, internal's reason is
// the one shown, since internal is what is meant to answer.
inline SiteWeather PickWeather(WeatherMode m, const SiteWeather& internal, const SiteWeather& leoben) {
    switch (m) {
        case WeatherMode::Off:      return SiteWeather{};
        case WeatherMode::Internal: return internal;
        case WeatherMode::Leoben:   return leoben;
        case WeatherMode::Both: {
            if (internal.fresh) return internal;
            if (leoben.fresh)   return leoben;
            SiteWeather w = internal;
            w.state = "internal " + internal.state + ", Leoben " + leoben.state;
            return w;
        }
    }
    return SiteWeather{};
}

// ── Lines for people ─────────────────────────────────────────

inline std::string FmtOpt(Opt v, const char* fmt, const char* none = "?") {
    if (!v) return none;
    char b[32];
    std::snprintf(b, sizeof(b), fmt, *v);
    return b;
}

// The status window's WX row: the bar text and the AIR value. Miles per
// hour and Fahrenheit, as the row always showed.
struct StatusRowText { std::string bar, air; };
inline StatusRowText StatusRow(const SiteWeather& w, int64_t now) {
    StatusRowText t;
    if (!w.fresh) {
        t.bar = std::string(w.source.empty() ? "weather" : w.source.c_str()) + ": " +
                (w.state.empty() ? "no reading" : w.state);
        t.air = "--";
        return t;
    }
    const int age = (int)((now - w.fetchedUnix) / 60);
    const Opt windMph = w.windMs ? Opt(*w.windMs / kMphToMs) : std::nullopt;
    const Opt tempF = w.tempC ? Opt(*w.tempC * 9.0 / 5.0 + 32.0) : std::nullopt;
    t.bar = (w.verdict.empty() ? std::string("--") : w.verdict) +
            "  cloud " + FmtOpt(w.cloudPct, "%.0f%%") +
            "  wind " + FmtOpt(windMph, "%.0f mph") +
            "  dew " + (w.dewRisk.empty() ? "?" : w.dewRisk) +
            "  " + std::to_string(age) + " min, " + w.source;
    t.air = FmtOpt(tempF, "%.0fF", "--");
    return t;
}

// The AI context's weather line. No coordinates: the site-precision rule
// covers it. Empty when there is no fresh reading, and the context then
// says the weather is unknown.
inline std::string AiWeatherLine(const SiteWeather& w, int64_t now, double elevM) {
    if (!w.fresh) return {};
    std::string s = w.verdict.empty() ? std::string("no verdict") : w.verdict;
    if (w.imagingScore >= 0) s += ", imaging score " + std::to_string(w.imagingScore) + "/100";
    s += "; cloud " + FmtOpt(w.cloudPct, "%.0f%%") +
         ", air " + FmtOpt(w.tempC, "%.1f C") +
         ", humidity " + FmtOpt(w.humidityPct, "%.0f%%") +
         ", wind " + FmtOpt(w.windMs, "%.1f m/s") +
         " gusting " + FmtOpt(w.gustMs, "%.1f") +
         ", dew point " + FmtOpt(w.dewPointC, "%.1f C") +
         " (dew risk " + (w.dewRisk.empty() ? "unknown" : w.dewRisk) + ")" +
         ", pressure " + FmtOpt(w.StationHpa(elevM), "%.0f hPa") + " at the site";
    if (w.seeing7Timer) s += "; 7Timer seeing class " + FmtOpt(w.seeing7Timer, "%.0f") + " of 8";
    s += "; reading " + std::to_string((int)((now - w.fetchedUnix) / 60)) + " min old, from " +
         (w.source == "internal" ? "RadukCapture's own fetch" : "Leoben");
    return s;
}

// The parity line, in "both": the two readings side by side, when both
// are fresh. Empty otherwise (nothing to compare).
inline std::string WxCompareLine(const SiteWeather& in, const SiteWeather& le) {
    if (!in.fresh || !le.fresh) return {};
    auto pair = [](const char* name, Opt a, Opt b, const char* fmt) {
        std::string s = std::string(name) + " " + FmtOpt(a, fmt, "-") + "/" + FmtOpt(b, fmt, "-");
        if (a && b) {
            char d[32];
            std::snprintf(d, sizeof(d), fmt, *a - *b);
            s += std::string(" (") + (*a - *b >= 0 ? "+" : "") + d + ")";
        }
        return s;
    };
    return "internal/leoben: " + pair("cloud", in.cloudPct, le.cloudPct, "%.0f") +
           ", " + pair("temp", in.tempC, le.tempC, "%.1f") +
           ", " + pair("dew", in.dewPointC, le.dewPointC, "%.1f") +
           ", " + pair("msl", in.pressureMslHpa, le.pressureMslHpa, "%.1f") +
           ", " + pair("wind", in.windMs, le.windMs, "%.1f") +
           ", " + pair("gust", in.gustMs, le.gustMs, "%.1f") +
           ", dew risk " + (in.dewRisk.empty() ? "-" : in.dewRisk) + "/" +
           (le.dewRisk.empty() ? "-" : le.dewRisk) +
           ", seeing " + FmtOpt(in.seeing7Timer, "%.0f", "-") + "/" + FmtOpt(le.seeing7Timer, "%.0f", "-");
}

} // namespace raduk::core::sky
