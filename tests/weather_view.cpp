// ============================================================
//  Phase 166.1: the weather, in the app (app/WeatherView.h)
//
//  The owner's rule (2026-11-21): the app is the instrument; Dash is a
//  remote. The app's weather window shows what Dash shows:
//    1. Off: says so; no refresh.
//    2. No fresh reading: says why.
//    3. A reading: every value, in F and mph with C and m/s beside them,
//       "--" for a value not given, the verdict's tone, the source and age.
//    4. Refresh is on only when the app fetches its own and is not offline.
// ============================================================

#include "app/WeatherView.h"

#include <cstdio>
#include <string>

using namespace raduk::app;
namespace wx = raduk::core::sky;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}
static ViewLine::Tone ToneOf(const LinesView& v, const std::string& needle) {
    for (const auto& l : v.lines) if (l.text.find(needle) != std::string::npos) return l.tone;
    return ViewLine::Tone::Normal;
}

int main() {
    printf("weather_view (phase 166.1)\n");

    printf("\n1. Off\n");
    {
        wx::SiteWeather w;
        const auto v = BuildWeatherView(w, wx::WeatherMode::Off, false, 0);
        Check(v.headline == "WEATHER: off" && v.Has("WeatherSource is off"), "says the source is off");
        Check(v.Button("REFRESH") && !v.Button("REFRESH")->enabled, "and refresh is off");
    }

    printf("\n2. No fresh reading\n");
    {
        wx::SiteWeather w; w.configured = true; w.fresh = false; w.state = "last reading is stale";
        const auto v = BuildWeatherView(w, wx::WeatherMode::Internal, false, 0);
        Check(v.headline == "WEATHER: no fresh reading" && v.Has("Why: last reading is stale"), "says why there is none");
        Check(v.Button("REFRESH")->enabled, "refresh is on: the app fetches its own");
    }

    printf("\n3. A reading\n");
    {
        wx::SiteWeather w; w.configured = true; w.fresh = true; w.source = "internal"; w.fetchedUnix = 1000;
        w.cloudPct = 12; w.tempC = 10; w.humidityPct = 40; w.windMs = 4.4704; w.dewPointC = -2.5;
        w.pressureMslHpa = 1016; w.dewRisk = "elevated"; w.seeing7Timer = 3; w.verdict = "GO"; w.imagingScore = 82;
        const auto v = BuildWeatherView(w, wx::WeatherMode::Internal, false, 1000 + 7 * 60);
        Check(v.headline == "WEATHER: GO" && ToneOf(v, "Imaging score 82") == ViewLine::Tone::Good, "the verdict, in its tone");
        Check(v.Has("Temperature  50 F (10.0 C)") && v.Has("Dew point    28 F (-2.5 C)"), "temperatures in F with C beside");
        Check(v.Has("Wind         10 mph (4.5 m/s)"), "wind in mph with m/s beside");
        Check(v.Has("Gusts        --"), "a value not given is --, never a guess");
        Check(v.Has("Cloud        12%") && v.Has("Humidity     40%") && v.Has("Pressure     1016 hPa"), "cloud, humidity, pressure");
        Check(ToneOf(v, "Dew risk") == ViewLine::Tone::Warn && v.Has("Dew risk     elevated"), "dew risk, in its tone");
        Check(v.Has("Seeing       3 of 8 (7Timer, 1 is best)"), "7Timer's seeing, with its scale");
        Check(v.Has("From this PC (Open-Meteo, 7Timer), 7 min old"), "where it came from, and how old");
        w.source = "leoben"; w.verdict = "NO-GO";
        const auto l = BuildWeatherView(w, wx::WeatherMode::Leoben, false, 1000);
        Check(l.headline == "WEATHER: NO-GO" && ToneOf(l, "Imaging score") == ViewLine::Tone::Bad, "NO-GO in the fault tone");
        Check(l.Has("From Leoben") && !l.Button("REFRESH")->enabled, "from Leoben: refresh off (the app fetches nothing)");
    }

    printf("\n4. Offline\n");
    {
        wx::SiteWeather w; w.configured = true; w.fresh = true; w.source = "internal";
        const auto v = BuildWeatherView(w, wx::WeatherMode::Both, true, 0);
        Check(!v.Button("REFRESH")->enabled && v.Has("Offline (rig.ini Offline=1)"), "offline: refresh off, and said why");
    }

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
