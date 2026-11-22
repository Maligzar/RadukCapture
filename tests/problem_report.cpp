// ============================================================
//  Phase 171: what the troubleshooting report holds (core/ProblemReport.h)
//
//  A report leaves the PC (to Claude Code, then GitHub), so:
//    1. rig.ini: the site, the Dash token, keys, secrets and passwords are
//       removed; everything else, comments and sections kept.
//    2. The log's last lines, every line naming the site removed.
//    3. report.json: valid JSON, the owner's words escaped as written.
// ============================================================

#include "core/ProblemReport.h"
#include "core/MiniJson.h"

#include <cstdio>
#include <string>

using namespace raduk::core::report;
using raduk::json::JVal;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("problem_report (phase 171)\n");

    printf("\n1. rig.ini\n");
    {
        const std::string ini =
            "[Rig]\n; the site, set by hand\nSiteLatDeg=40.7\nSiteLonDeg=-74.0\nSiteSource=manual\n"
            "WsToken=0123456789abcdef0123456789abcdef\nAnthropicApiKey=sk-xyz\nLeobenUrl=http://box:5000\n"
            "GmailPassword=hunter2\nAutorunRuns=25\nCameraZenithAngleDeg=180\nSkyParkAltAz=180:20\n";
        const std::string r = RedactIni(ini);
        Check(r.find("40.7") == std::string::npos && r.find("-74.0") == std::string::npos, "the site is gone");
        Check(r.find("0123456789abcdef") == std::string::npos && r.find("sk-xyz") == std::string::npos &&
              r.find("hunter2") == std::string::npos && r.find("http://box") == std::string::npos,
              "the Dash token, an API key, a password and Leoben's address are gone");
        Check(r.find("SiteLatDeg=(removed from the report)") != std::string::npos, "the key stays, saying it was removed");
        Check(r.find("AutorunRuns=25") != std::string::npos && r.find("CameraZenithAngleDeg=180") != std::string::npos &&
              r.find("SkyParkAltAz=180:20") != std::string::npos, "the settings that help are kept");
        Check(r.find("; the site, set by hand") != std::string::npos && r.find("[Rig]") != std::string::npos,
              "comments and sections kept");
    }

    printf("\n2. The log\n");
    {
        std::string log;
        for (int i = 0; i < 50; ++i) log += "[SHARP] line " + std::to_string(i) + "\n";
        log += "[INIT] Site: 40.7N 74.0W (manual)\r\n[NexStarMount] Site -> 40d42'00\" N, 74d00'00\" W : accepted\n"
               "[NexStarMount] Site read from mount: 40.7N 74.0W\n[AF] Fine pass: 168700 (coarse 168000, +700)\n";
        int removed = 0;
        const std::string t = LogTail(log, 10, &removed);
        Check(t.find("40.7") == std::string::npos && t.find("40d42") == std::string::npos, "no line names the site",
              std::to_string(removed) + " removed");
        Check(removed == 3, "exactly the three site lines");
        Check(t.find("[AF] Fine pass: 168700") != std::string::npos, "the evidence is kept");
        Check(t.find("[SHARP] line 0\n") == std::string::npos && t.find("[SHARP] line 49") != std::string::npos,
              "the last lines only");
        Check(t.find('\r') == std::string::npos, "Windows line ends tidied");
    }

    printf("\n3. report.json\n");
    {
        ReportState s;
        s.stamp = "20261122_021500"; s.night = "20261121"; s.version = "3.3.44";
        s.fields = {{"target", "Saturn"}, {"focuser", "168655"}};
        s.files = {"log_tail.txt", "screen_main.png"};
        const std::string desc = "Autofocus took \"Saturn\" to a donut.\nThen the readout said OUT\\31%";
        const std::string j = ReportJson(desc, s);
        JVal root;
        raduk::json::JsonReader rd(j);
        Check(rd.Parse(root) && root.kind == JVal::Obj, "valid JSON");
        Check(root.Get("description") && root.Get("description")->s == desc, "the owner's words, exactly as written");
        Check(root.Get("state") && root.Get("state")->Get("target")->s == "Saturn", "the state snapshot");
        Check(root.Get("format")->s == "raduk-problem-report" && root.Get("files")->a.size() == 2, "its format and its files");
    }

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
