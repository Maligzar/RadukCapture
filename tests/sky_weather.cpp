// ============================================================
//  Phase 165.0: the sky's weather, read here (core/SkyWeather.h)
//
//  On replies under tests/data/sky_weather (see its README):
//    1. Open-Meteo current conditions, captured 2026-11-19 for the Royal
//       Observatory, Greenwich: every reading lands, in SI; the visibility
//       a clear night gives (16 km and more) is a number, not ">10 mi".
//    2. Open-Meteo's own error reply (its daily limit) is refused with its
//       reason; a missing, null or unknown-unit value is absent, never a
//       guess; a local-time reply is refused.
//    3. 7Timer ASTRO, in the captured reply's layout: "seeing" is read
//       (Leoben read "seeing2"); the wind is a class, not knots; -9999 and
//       out-of-range classes are absent; the documented tables.
//    4. NWS alerts, captured 2026-11-19: none at a point, thirteen in
//       Alaska, each with its event.
//    5. Dew risk, the wind tiers, the cloud layers and the dust categories,
//       against Leoben's own values converted from mph, miles and deg F.
// ============================================================

#include "core/SkyWeather.h"

#include <cmath>
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
static std::string F(double v, int d = 3) { char b[48]; snprintf(b, sizeof(b), "%.*f", d, v); return b; }
static bool Near(Opt v, double want, double tol = 1e-6) { return v && std::fabs(*v - want) <= tol; }

// The data folder beside this file. run-tests.ps1 compiles with an
// absolute source path, so __FILE__ is absolute.
static std::string Data(const char* name) {
    std::filesystem::path p = std::filesystem::path(__FILE__).parent_path() / "data" / "sky_weather" / name;
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void OpenMeteoCurrent() {
    printf("\n1. Open-Meteo, current conditions (captured)\n");
    const std::string body = Data("open_meteo_current_greenwich.json");
    Check(!body.empty(), "the captured reply is there");
    OmCurrent c;
    Check(ParseOpenMeteoCurrent(body, c), "it reads", c.error);
    Check(c.timeUtc == UnixFromUtc(2026, 11, 19, 20, 15), "the reading's time, UTC");
    Check(c.intervalSec == 900, "a 15-minute reading");
    Check(Near(c.tempC, 8.8), "temperature in C", c.tempC ? F(*c.tempC, 1) : "absent");
    Check(Near(c.dewPointC, 5.9), "dew point in C");
    Check(Near(c.humidityPct, 82), "humidity");
    Check(Near(c.windMs, 4.3 / 3.6), "wind km/h to m/s", c.windMs ? F(*c.windMs) : "absent");
    Check(Near(c.gustMs, 9.4 / 3.6), "gust km/h to m/s");
    Check(Near(c.windDirDeg, 342), "wind direction");
    Check(Near(c.cloudPct, 100), "cloud");
    Check(Near(c.pressureMslHpa, 992.3), "sea-level pressure in hPa");
    Check(Near(c.visibilityM, 10000), "visibility in metres, as a number");
    Check(c.setAside.empty(), "nothing set aside");

    // Leoben's 503: 16 km and more became the string ">10 mi", then a float.
    std::string clear = body;
    const size_t p = clear.find("\"visibility\":10000.00");
    clear.replace(p, 21, "\"visibility\":24140.00");
    Check(ParseOpenMeteoCurrent(clear, c) && Near(c.visibilityM, 24140),
          "a clear desert night's visibility reads (Leoben answered 503)");
}

static void OpenMeteoRefusals() {
    printf("\n2. Open-Meteo, what is refused or left absent\n");
    OmCurrent c;
    const bool ok = ParseOpenMeteoCurrent(Data("open_meteo_daily_limit.json"), c);
    Check(!ok && c.error.find("Daily API request limit") != std::string::npos,
          "the service's own error is refused, with its reason", c.error);

    const std::string base = Data("open_meteo_current_greenwich.json");
    auto with = [&](const std::string& from, const std::string& to) {
        std::string s = base;
        const size_t p = s.find(from);
        if (p == std::string::npos) return std::string();
        s.replace(p, from.size(), to);
        return s;
    };
    ParseOpenMeteoCurrent(with("\"cloud_cover\":100,", ""), c);
    Check(c.valid && !c.cloudPct, "no cloud figure: cloud is absent, not 15%");
    ParseOpenMeteoCurrent(with("\"cloud_cover\":100,", "\"cloud_cover\":null,"), c);
    Check(c.valid && !c.cloudPct && c.setAside.size() == 1, "a null cloud is absent, and noted",
          c.setAside.empty() ? "" : c.setAside[0]);
    ParseOpenMeteoCurrent(with("\"wind_speed_10m\":\"km/h\"", "\"wind_speed_10m\":\"furlongs\""), c);
    Check(c.valid && !c.windMs && c.gustMs, "an unknown unit leaves that reading absent");
    ParseOpenMeteoCurrent(with("\"relative_humidity_2m\":82", "\"relative_humidity_2m\":182"), c);
    Check(c.valid && !c.humidityPct, "an out-of-range humidity is absent");
    ParseOpenMeteoCurrent(with("\"wind_speed_10m\":\"km/h\"", "\"wind_speed_10m\":\"mp/h\""), c);
    Check(Near(c.windMs, 4.3 * kMphToMs), "mph is converted when the reply says mph");
    const std::string local = with("\"timezone\":\"GMT\"", "\"timezone\":\"America/Denver\"");
    Check(!ParseOpenMeteoCurrent(local, c), "a reply in local time is refused", c.error);
    Check(!ParseOpenMeteoCurrent("not json", c), "a reply that is not JSON is refused");
}

static void SevenTimer() {
    printf("\n3. 7Timer ASTRO\n");
    T7Forecast f;
    const std::string body = Data("seventimer_astro_layout.json");
    Check(ParseSevenTimerAstro(body, f), "it reads", f.error);
    Check(f.initUtc == UnixFromUtc(2026, 11, 19, 12, 0), "init 2026111912 is 12:00 UTC");
    Check(f.points.size() == 5, "five timepoints", std::to_string(f.points.size()));
    if (f.points.size() != 5) return;
    const auto& a = f.points[0];
    Check(a.timeUtc == f.initUtc + 3 * 3600, "timepoint 3 is init + 3 h");
    Check(a.seeing == 1 && a.transparency == 1 && a.cloud == 1, "the key is \"seeing\", and it is read");
    Check(a.humidity == -4 && a.liftedIndex == -10 && a.windClass == 1 && Near(a.temp2mC, -76),
          "the low ends of every class");
    const auto& b = f.points[1];
    Check(b.seeing == 8 && b.transparency == 8 && b.cloud == 9 && b.humidity == 16 &&
          b.liftedIndex == 15 && b.windClass == 8 && Near(b.temp2mC, 60) && b.precType == "rain",
          "the high ends of every class");
    Check(!f.points[2].seeing, "-9999 (undefined) seeing is absent");
    const auto& d = f.points[3];
    Check(!d.cloud && !d.seeing && !d.transparency && !d.humidity && !d.windClass &&
          !d.liftedIndex && !d.temp2mC, "values outside the documented classes are absent");
    Check(!f.points[4].transparency && f.points[4].seeing == 3, "a missing key is absent, the rest read");
    Check(f.setAside == 8, "and each one set aside is counted", std::to_string(f.setAside));

    // The class is not a speed. Leoben: class 6 x 1.15078 = about 7 mph.
    auto w = SevenTimerWindMs(6);
    Check(w && w->lo == 17.2 && w->hi == 24.5, "wind class 6 is a gale, 17.2 to 24.5 m/s");
    auto s = SevenTimerSeeingArcsec(4);
    Check(s && s->lo == 1.0 && s->hi == 1.25, "seeing class 4 is 1\" to 1.25\" (Leoben's every night)");
    Check(SevenTimerSeeingArcsec(1)->lo == -kInf && SevenTimerSeeingArcsec(8)->hi == kInf,
          "the end classes are open");
    Check(!SevenTimerSeeingArcsec(0) && !SevenTimerSeeingArcsec(9), "no class 0 or 9 seeing");
    auto t = SevenTimerTransparencyMag(6);
    Check(t && t->lo == 0.7 && t->hi == 0.85, "transparency 6 is 0.7 to 0.85 mag per air mass");
    auto c = SevenTimerCloudPct(9);
    Check(c && c->lo == 94 && c->hi == 100, "cloud 9 is 94% to 100%");
    auto h13 = SevenTimerHumidityPct(13), h15 = SevenTimerHumidityPct(15), h16 = SevenTimerHumidityPct(16);
    Check(h13 && h13->lo == 85 && h13->hi == 90 && h15->lo == 95 && h15->hi == 99 &&
          h16->lo == 100, "humidity 13 is 85-90%, 15 is 95-99%, 16 is 100%");

    Check(SevenTimerAt(f, f.initUtc + 4 * 3600) == &f.points[0], "the nearest timepoint to a time");
    Check(SevenTimerAt(f, f.initUtc + 40 * 3600) == nullptr, "none when nothing is within 90 minutes");

    std::string other = body;
    other.replace(other.find("\"astro\""), 7, "\"civil\"");
    Check(!ParseSevenTimerAstro(other, f), "another product is refused", f.error);
}

static void Nws() {
    printf("\n4. NWS active alerts (captured)\n");
    std::vector<NwsAlert> al;
    std::string err;
    Check(ParseNwsAlerts(Data("nws_alerts_none.json"), al, err) && al.empty(),
          "a point with no alerts: read, and empty", err);
    Check(ParseNwsAlerts(Data("nws_alerts_alaska.json"), al, err), "thirteen Alaska alerts read", err);
    Check(al.size() == 13, "all thirteen", std::to_string(al.size()));
    bool events = true;
    for (const auto& a : al) events = events && !a.event.empty() && !a.severity.empty();
    Check(events, "each with its event and severity", al.empty() ? "" : al[0].event);
    Check(!ParseNwsAlerts("{\"type\":\"Feature\"}", al, err), "not a FeatureCollection: refused", err);
}

static void Tables() {
    printf("\n5. Dew, wind, cloud layers and dust\n");
    // Dew: Leoben's 4 and 8 degrees F of spread.
    Check(DewRiskFromSpread(10.0, 10.0 - 4.0 * 5 / 9) == DewRisk::Warning, "a 4 F spread is a warning");
    Check(DewRiskFromSpread(10.0, 10.0 - 4.1 * 5 / 9) == DewRisk::Elevated, "just over 4 F is elevated");
    Check(DewRiskFromSpread(10.0, 10.0 - 8.0 * 5 / 9) == DewRisk::Elevated, "8 F is still elevated");
    Check(DewRiskFromSpread(10.0, 10.0 - 8.1 * 5 / 9) == DewRisk::Safe, "past 8 F is safe");
    Check(DewRiskFromSpread(10.0, std::nullopt) == DewRisk::Unknown, "no dew point: unknown, not safe");

    // Wind: both limits must hold for a band.
    auto mph = [](double v) { return v * kMphToMs; };
    Check(WindTierFrom(mph(5), mph(8)).base == 100, "5 mph gusting 8: 100");
    Check(WindTierFrom(mph(5), mph(9)).base == 97, "a gust over 8 drops a band");
    Check(WindTierFrom(mph(15), mph(22)).base == 72, "15 gusting 22: 72, MARGINAL");
    Check(WindTierFrom(mph(20), mph(28)).base == 36, "20 gusting 28: 36, POOR");
    auto danger = WindTierFrom(mph(21), mph(22));
    Check(danger.base == 0 && std::string(danger.label) == "DANGER", "over 20 sustained: DANGER");
    auto noGust = WindTierFrom(mph(5), std::nullopt);
    Check(noGust.base == 100 && !noGust.gustKnown, "no gust: the sustained limit only, and said so");
    Check(WindTierFrom(std::nullopt, mph(5)).base == -1, "no wind: unknown");

    // Cloud layers.
    auto rd = [](double l, double m, double h) { return std::string(ReadCloudLayers(l, m, h, std::nullopt).impact); };
    Check(rd(2, 3, 4) == "CLEAR", "under 10% in all: CLEAR");
    Check(rd(45, 0, 0) == "BLOCKED", "a low deck over 40%: BLOCKED");
    Check(rd(5, 30, 0) == "MARGINAL", "mid cloud 20-50%: MARGINAL");
    Check(rd(5, 5, 40) == "FAIR", "thin cirrus: FAIR");
    Check(rd(5, 5, 20) == "GOOD", "scattered high cloud only: GOOD");
    Check(std::string(ReadCloudLayers(5, std::nullopt, 20, std::nullopt).impact) == "UNKNOWN",
          "a missing layer: UNKNOWN, not 0%");

    // Dust: visibility at or under, gust at or over.
    std::vector<NwsAlert> none;
    auto mi = [](double v) { return v * kMileToM; };
    Check(DustFrom(mi(3), mph(20), none).category == 1, "3 mi and 20 mph: Dusty");
    Check(DustFrom(mi(3.1), mph(60), none).category == 0, "over 3 mi: clear, whatever the gust");
    Check(DustFrom(mi(0.2), mph(36), none).category == 3, "1/5 mi and 36 mph: Dust Storm");
    Check(DustFrom(mi(0.05), mph(50), none).category == 5, "1/20 mi and 50 mph: Haboob");
    auto unseen = DustFrom(std::nullopt, mph(60), none);
    Check(unseen.category == 0 && !unseen.observed, "no visibility: no observed category (Leoben read 10 mi)");
    std::vector<NwsAlert> warn = {{"Dust Storm Warning", "", "Severe", "", ""}};
    auto w = DustFrom(mi(10), mph(5), warn);
    Check(w.category == 4 && w.fromAlert && w.alertEvent == "Dust Storm Warning", "a Dust Storm Warning floors it at 4");
    std::vector<NwsAlert> hab = {{"Special Weather Statement", "A HABOOB is approaching", "Moderate", "", ""}};
    Check(DustFrom(std::nullopt, std::nullopt, hab).category == 5, "\"haboob\" in a headline: 5");
    std::vector<NwsAlert> adv = {{"Blowing Dust Advisory", "", "Moderate", "", ""}};
    auto a = DustFrom(mi(0.2), mph(36), adv);
    Check(a.category == 3 && !a.fromAlert, "an advisory does not lower what was observed");
}

int main() {
    printf("sky_weather (phase 165.0)\n");
    OpenMeteoCurrent();
    OpenMeteoRefusals();
    SevenTimer();
    Nws();
    Tables();
    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
