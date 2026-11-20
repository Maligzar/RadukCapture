// ============================================================
//  Phase 165.2: which weather the app uses (core/SiteWeather.h)
//
//    1. WeatherSource=: the four values, case and spaces forgiven; unset
//       is "both" with a LeobenUrl and "internal" without; nonsense falls
//       back to that default and says so.
//    2. From RadukCapture's own fetch: fresh fields pass through, absent
//       stay absent, stale gives a reason; 7Timer's seeing counts while it
//       covers the hour.
//    3. From Leoben: Leoben's units converted, -1 fields absent, stale
//       gives a reason.
//    4. The pick: internal, leoben, off, and "both" (internal first,
//       Leoben standing in, both reasons when neither is fresh).
//    5. The lines: the status row, the AI context line (no coordinates)
//       and the WXCMP parity line.
// ============================================================

#include "core/SiteWeather.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace raduk::core::sky;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}
static bool Has(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

static std::string Data(const char* name) {
    std::filesystem::path p = std::filesystem::path(__FILE__).parent_path() / "data" / "sky_weather" / name;
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static const int64_t T0 = UnixFromUtc(2026, 11, 19, 20, 20);

// A snapshot as the client holds it after one good pass at T0.
static SkySnapshot Snap() {
    SkyWeatherClient c;
    SkyWeatherClient::Options o;
    o.siteSet = true;
    o.latDeg = 51.48;
    o.lonDeg = 0.0;
    c.Configure(o, [](const std::string& h, const std::string&, std::string& body) {
        if (h == "api.open-meteo.com") body = Data("open_meteo_current_greenwich.json");
        else if (h == "www.7timer.info") body = Data("seventimer_astro_layout.json");
        else body = Data("nws_alerts_none.json");
        return FetchResult::Ok;
    });
    c.Step(T0);
    return c.Get();
}

static raduk::net::LeobenConditions Leoben() {
    raduk::net::LeobenConditions c;
    c.valid = true;
    c.fetchedUnix = T0 - 120;
    c.cloudPct = 20;
    c.tempF = 50;            // 10 C
    c.humidityPct = 60;
    c.windMph = 10;
    c.gustMph = 15;
    c.dewPointF = 41;        // 5 C
    c.pressureMslInHg = 30.0;
    c.imagingScore = 70;
    c.dewRisk = "safe";
    c.goNoGo = "CAUTION";
    c.seeing7Timer = -1;     // Leoben never sent it (165 plan, finding 2)
    return c;
}

static void Mode() {
    printf("\n1. WeatherSource=\n");
    bool known = false;
    Check(ParseWeatherSource("internal", false, &known) == WeatherMode::Internal && known, "internal");
    Check(ParseWeatherSource(" Leoben ", false, &known) == WeatherMode::Leoben && known, "Leoben, spaces and case forgiven");
    Check(ParseWeatherSource("BOTH", false) == WeatherMode::Both, "both");
    Check(ParseWeatherSource("off", true) == WeatherMode::Off, "off");
    Check(ParseWeatherSource("", true, &known) == WeatherMode::Both && known, "unset with a LeobenUrl: both, so no one loses weather");
    Check(ParseWeatherSource("", false) == WeatherMode::Internal, "unset without one: internal");
    Check(ParseWeatherSource("leobin", false, &known) == WeatherMode::Internal && !known,
          "nonsense: the default, and flagged for the log");
}

static void Internal() {
    printf("\n2. From RadukCapture's own fetch\n");
    const SkySnapshot s = Snap();
    const SiteWeather w = FromSky(s, T0);
    Check(w.configured && w.fresh && w.source == "internal", "fresh, and named internal");
    Check(w.tempC && *w.tempC == 8.8 && w.cloudPct && *w.cloudPct == 100, "the readings pass through");
    Check(w.dewRisk == "elevated", "dew risk from the spread", w.dewRisk);
    Check(w.imagingScore == -1 && w.verdict.empty(), "no score or verdict until 165.3");
    Check(!w.wind200hPaKmh && !w.wind250hPaKmh, "no upper winds (the hourly parser is owed)");
    auto st = w.StationHpa(1000.0);
    Check(st && *st < 992.3 && *st > 870, "station pressure below sea level at 1000 m", st ? FmtOpt(st, "%.1f") : "absent");

    const SiteWeather late = FromSky(s, T0 + 31 * 60);
    Check(!late.fresh && late.state == "last reading is stale" && !late.tempC,
          "31 minutes on: stale, with the reason, and no values", late.state);

    // 7Timer: the layout file starts at 12:00 + 3 h, seeing class 1.
    const SiteWeather att = FromSky(s, UnixFromUtc(2026, 11, 19, 15, 30));
    Check(att.seeing7Timer && *att.seeing7Timer == 1, "7Timer's seeing for the hour, even with the reading stale");

    SkySnapshot empty;
    empty.status[0].state = SourceState::Offline;
    const SiteWeather off = FromSky(empty, T0);
    Check(!off.fresh && off.state == "offline", "nothing yet, offline: says offline", off.state);
}

static void FromLeobenCase() {
    printf("\n3. From Leoben\n");
    const SiteWeather w = FromLeoben(Leoben(), T0, "ok");
    Check(w.fresh && w.source == "leoben", "fresh, and named leoben");
    Check(w.tempC && std::fabs(*w.tempC - 10.0) < 1e-9, "50 F is 10 C");
    Check(w.windMs && std::fabs(*w.windMs - 10 * kMphToMs) < 1e-9, "mph to m/s");
    Check(w.pressureMslHpa && std::fabs(*w.pressureMslHpa - 30.0 * 33.8639) < 1e-6, "inHg to hPa");
    Check(w.imagingScore == 70 && w.verdict == "CAUTION", "Leoben's score and verdict kept");
    Check(!w.seeing7Timer, "Leoben's -1 seeing is absent");
    auto old = Leoben();
    old.fetchedUnix = T0 - 31 * 60;
    Check(FromLeoben(old, T0, "ok").state == "last reading is stale", "stale: says so");
    Check(FromLeoben(raduk::net::LeobenConditions{}, T0, "Leoben not reachable").state == "Leoben not reachable",
          "never read: the client's own state");
}

static void Pick() {
    printf("\n4. The pick\n");
    const SiteWeather in = FromSky(Snap(), T0);
    const SiteWeather le = FromLeoben(Leoben(), T0, "ok");
    SiteWeather inStale = FromSky(Snap(), T0 + 3600);
    SiteWeather leDown = FromLeoben(raduk::net::LeobenConditions{}, T0, "Leoben not reachable");
    Check(PickWeather(WeatherMode::Internal, in, le).source == "internal", "internal");
    Check(PickWeather(WeatherMode::Leoben, in, le).source == "leoben", "leoben");
    Check(!PickWeather(WeatherMode::Off, in, le).configured, "off: nothing, and not configured");
    Check(PickWeather(WeatherMode::Both, in, le).source == "internal", "both: internal first");
    Check(PickWeather(WeatherMode::Both, inStale, le).source == "leoben", "both: Leoben stands in when internal is stale");
    const SiteWeather none = PickWeather(WeatherMode::Both, inStale, leDown);
    Check(!none.fresh && Has(none.state, "internal last reading is stale") && Has(none.state, "Leoben Leoben not reachable"),
          "both, neither fresh: both reasons", none.state);
}

static void Lines() {
    printf("\n5. The lines\n");
    const SiteWeather in = FromSky(Snap(), T0);
    const StatusRowText row = StatusRow(in, T0 + 240);
    Check(Has(row.bar, "cloud 100%") && Has(row.bar, "dew elevated") && Has(row.bar, "4 min, internal"),
          "status row: readings, age and source", row.bar);
    Check(row.bar.size() < 64, "and it fits the row's 64 characters", std::to_string(row.bar.size()));
    Check(row.air == "48F", "AIR in Fahrenheit, as always", row.air);
    const StatusRowText stale = StatusRow(FromSky(Snap(), T0 + 3600), T0 + 3600);
    Check(Has(stale.bar, "stale") && stale.air == "--", "stale: the reason, no AIR", stale.bar);

    const std::string ai = AiWeatherLine(in, T0 + 60, 50.0);
    Check(Has(ai, "cloud 100%") && Has(ai, "dew risk elevated") && Has(ai, "RadukCapture's own fetch") &&
          Has(ai, "7Timer seeing") == false, "AI line: readings and source", ai.substr(0, 60));
    Check(!Has(ai, "51.4") && !Has(ai, "0.00"), "and no coordinates");
    Check(AiWeatherLine(FromSky(Snap(), T0 + 3600), T0 + 3600, 0).empty(), "stale: no AI line (it says unknown)");

    const SiteWeather le = FromLeoben(Leoben(), T0, "ok");
    const std::string cmp = WxCompareLine(in, le);
    Check(Has(cmp, "cloud 100/20 (+80)") && Has(cmp, "temp 8.8/10.0 (-1.2)") && Has(cmp, "dew risk elevated/safe"),
          "WXCMP: both side by side, with the difference", cmp.substr(0, 60));
    Check(WxCompareLine(in, FromLeoben(raduk::net::LeobenConditions{}, T0, "x")).empty(),
          "no line unless both are fresh");
}

int main() {
    printf("site_weather (phase 165.2)\n");
    Mode();
    Internal();
    FromLeobenCase();
    Pick();
    Lines();
    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
