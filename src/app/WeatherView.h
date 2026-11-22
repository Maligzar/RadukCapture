#pragma once
// ============================================================
//  WeatherView -- phase 166.1: the weather, in the app (Shift+W)
//
//  The owner's rule (2026-11-21): the app is the instrument; Dash is a
//  remote. Until now the app showed one weather row in the Status window
//  while Dash showed the whole reading and had the refresh. This is the
//  whole reading in the app's own window, from the same core/SiteWeather.h
//  the row and Dash use, with the refresh as its button.
//
//  Units as Dash shows them, Fahrenheit and mph, with Celsius and m/s
//  beside them, so the two read the same. A value the reading did not give
//  is "--", never a guess. Pure, tested by tests/weather_view.cpp.
// ============================================================

#include <cstdint>
#include <cstdio>
#include <string>

#include "LinesView.h"
#include "../core/SiteWeather.h"

namespace raduk::app {

namespace wxv {
namespace wx = raduk::core::sky;
using Opt = std::optional<double>;

inline std::string Fmt(const char* f, double a) { char b[64]; std::snprintf(b, sizeof b, f, a); return b; }
inline std::string Fmt2(const char* f, double a, double c) { char b[64]; std::snprintf(b, sizeof b, f, a, c); return b; }

inline std::string Temp(const Opt& c) { return c ? Fmt2("%.0f F (%.1f C)", *c * 9.0 / 5.0 + 32.0, *c) : "--"; }
inline std::string Speed(const Opt& ms) { return ms ? Fmt2("%.0f mph (%.1f m/s)", *ms * 2.2369363, *ms) : "--"; }
inline std::string Pct(const Opt& p) { return p ? Fmt("%.0f%%", *p) : "--"; }
inline std::string Hpa(const Opt& p) { return p ? Fmt("%.0f hPa", *p) : "--"; }
}  // namespace wxv

// refreshable: the app fetches its own weather (WeatherSource internal or
// both) and is not offline, so the refresh button can do something.
inline LinesView BuildWeatherView(const raduk::core::sky::SiteWeather& w, raduk::core::sky::WeatherMode mode,
                                  bool offline, int64_t now) {
    using T = ViewLine::Tone;
    using namespace wxv;
    LinesView v;
    const bool refreshable = wx::ModeUsesInternal(mode) && !offline;
    v.buttons.push_back({"REFRESH", "\xE2\x86\xBB  REFRESH", refreshable, T::Accent});

    if (!w.configured || mode == wx::WeatherMode::Off) {
        v.headline = "WEATHER: off";
        v.lines.push_back({"No weather source: rig.ini WeatherSource is off.", T::Dim});
        return v;
    }
    if (offline) v.lines.push_back({"Offline (rig.ini Offline=1): nothing is fetched, so the reading only ages.", T::Warn});
    if (!w.fresh) {
        v.headline = "WEATHER: no fresh reading";
        v.lines.push_back({"Why: " + (w.state.empty() ? std::string("not known") : w.state), T::Warn});
        v.lines.push_back({std::string("Source setting: ") + wx::WeatherModeText(mode), T::Dim});
        return v;
    }

    const std::string verdict = w.verdict.empty() ? "" : w.verdict;
    v.headline = "WEATHER" + (verdict.empty() ? std::string("") : ": " + verdict);
    const T vt = verdict == "GO" ? T::Good : verdict == "CAUTION" ? T::Warn : verdict.empty() ? T::Normal : T::Bad;
    if (w.imagingScore >= 0) v.lines.push_back({"Imaging score " + std::to_string(w.imagingScore) + " of 100", vt});

    v.lines.push_back({"Cloud        " + Pct(w.cloudPct), T::Normal});
    v.lines.push_back({"Temperature  " + Temp(w.tempC), T::Normal});
    v.lines.push_back({"Humidity     " + Pct(w.humidityPct), T::Normal});
    v.lines.push_back({"Wind         " + Speed(w.windMs), T::Normal});
    v.lines.push_back({"Gusts        " + Speed(w.gustMs), T::Normal});
    v.lines.push_back({"Dew point    " + Temp(w.dewPointC), T::Normal});
    const T dt = w.dewRisk == "warning" ? T::Bad : w.dewRisk == "elevated" ? T::Warn : T::Normal;
    v.lines.push_back({"Dew risk     " + (w.dewRisk.empty() ? std::string("--") : w.dewRisk), dt});
    v.lines.push_back({"Pressure     " + Hpa(w.pressureMslHpa) + " (sea level)", T::Normal});
    v.lines.push_back({"Seeing       " + (w.seeing7Timer ? Fmt("%.0f of 8 (7Timer, 1 is best)", *w.seeing7Timer) : std::string("--")),
                       T::Normal});

    std::string src = "From " + (w.source == "internal" ? std::string("this PC (Open-Meteo, 7Timer)")
                                 : w.source == "leoben" ? std::string("Leoben") : w.source);
    if (w.fetchedUnix > 0 && now >= w.fetchedUnix) src += ", " + std::to_string((now - w.fetchedUnix) / 60) + " min old";
    v.lines.push_back({src, T::Dim});
    return v;
}

} // namespace raduk::app
