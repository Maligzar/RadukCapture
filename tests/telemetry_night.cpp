// ============================================================
//  Phase 132.10d: the dashboard follows the app's night vision
//
//  The telemetry message carries night_vision, additively, so the
//  dashboard can switch with the app's toggle. The page applies it only
//  when the field is there (checked in Chromium for the PR); here, that
//  the field is in the message and says what it should.
// ============================================================

#include "net/APEXTelemetry.h"

#include <cstdio>
#include <string>

using namespace raduk::net;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-58s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("Telemetry night vision (phase 132.10d)\n\n");
    APEXTelemetry t;
    const std::string off = t.ToJSON();
    Check(off.find("\"night_vision\":false") != std::string::npos, "off by default, and said so");
    t.night_vision = true;
    const std::string on = t.ToJSON();
    Check(on.find("\"night_vision\":true") != std::string::npos, "on when the app is in night vision");
    Check(on.find("\"planet_lock\":") != std::string::npos, "the existing fields are still there");
    Check(!on.empty() && on.front() == '{' && on.back() == '}', "still one JSON object");

    // Phase 157.0e: Dash's focuser-trust card reads these.
    Check(off.find("\"eaf_trusted\":true") != std::string::npos &&
          off.find("\"eaf_saved_position\":-1") != std::string::npos &&
          off.find("\"eaf_trust_problem\":\"\"") != std::string::npos, "focuser trusted by default, nothing saved");
    t.eaf_trusted = false;
    t.eaf_saved_position = 166700;
    t.eaf_trust_problem = "the focuser reports 4761 but was last at \"166700\"";
    const std::string doubt = t.ToJSON();
    Check(doubt.find("\"eaf_trusted\":false") != std::string::npos &&
          doubt.find("\"eaf_saved_position\":166700") != std::string::npos, "in doubt: said, with the saved number");
    Check(doubt.find("was last at \\\"166700\\\"") != std::string::npos, "and the problem's text escaped as JSON");

    // Phase 165.4: the weather Dash shows instead of fetching its own.
    Check(off.find("\"wx_configured\":false") != std::string::npos &&
          off.find("\"wx_fresh\":false") != std::string::npos &&
          off.find("\"wx_cloud_pct\":null") != std::string::npos &&
          off.find("\"wx_temp_c\":null") != std::string::npos &&
          off.find("\"net_offline\":false") != std::string::npos, "no weather: said, and the readings null");
    APEXTelemetry w;
    w.wx_configured = true;
    w.wx_fresh = true;
    w.wx_source = "internal";
    w.wx_age_min = 4;
    w.wx_cloud_pct = 100;
    w.wx_temp_c = 8.8;
    w.wx_wind_ms = 1.194;
    w.wx_dew_risk = "elevated";
    w.net_offline = true;
    const std::string wj = w.ToJSON();
    Check(wj.find("\"wx_source\":\"internal\"") != std::string::npos &&
          wj.find("\"wx_cloud_pct\":100,") != std::string::npos &&
          wj.find("\"wx_temp_c\":8.8,") != std::string::npos &&
          wj.find("\"wx_wind_ms\":1.19,") != std::string::npos, "a reading goes out in SI, at its precision");
    Check(wj.find("\"wx_gust_ms\":null") != std::string::npos, "a reading not given stays null");
    Check(wj.find("\"net_offline\":true") != std::string::npos, "offline is said, so Dash asks nothing");
    Check(wj.find("\"histogram\":[0,0,") != std::string::npos, "the fields after it are unchanged");
    Check(wj.find(":nan") == std::string::npos && wj.find(":inf") == std::string::npos &&
          wj.find(":-nan") == std::string::npos, "never a bare nan or inf");
    printf("\n%s\n", g_fail == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return g_fail == 0 ? 0 : 1;
}
