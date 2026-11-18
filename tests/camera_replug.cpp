// ============================================================
//  A replugged camera is told again what it was told before
//
//  After a USB unplug the SDK handle is dead and the camera comes back
//  at its power-on defaults. ICamera::Reconnect re-opens it and replays
//  CameraSettingsCache onto it. Checked here: what is replayed, in what
//  order, and that a start position the SDK has since thrown away (a
//  new format re-centres the window) is not put back.
//
//  The whole sequence, through the capture loop and the emulator whose
//  handle now dies like the real one: fault_injection camera-blip.
// ============================================================

#include "core/CameraSettingsCache.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

// Records what a replay told the camera, in order.
struct Recorder {
    std::vector<std::string> calls;
    int  refuseId = -1;
    bool Replay(const CameraSettingsSnapshot& s) {
        return ReplaySettings(s,
            [&](int w, int h, int b, int t) {
                calls.push_back("region " + std::to_string(w) + "x" + std::to_string(h) +
                                " bin" + std::to_string(b) + " type" + std::to_string(t));
                return true; },
            [&](int x, int y) {
                calls.push_back("start " + std::to_string(x) + "," + std::to_string(y));
                return true; },
            [&](int id, long v, bool a) {
                calls.push_back("control " + std::to_string(id) + "=" + std::to_string(v) +
                                (a ? " auto" : ""));
                return id != refuseId; });
    }
};

int main() {
    printf("1. Nothing set, nothing replayed\n");
    {
        CameraSettingsCache c;
        Recorder r;
        Check(r.Replay(c.Get()) && r.calls.empty(), "an untouched camera is left at its defaults");
    }

    printf("\n2. Region, then start, then controls, each once with its last value\n");
    {
        CameraSettingsCache c;
        c.Control(1, 100, false);   // gain
        c.Region(640, 480, 1, 0);
        c.Control(0, 8000, false);  // exposure
        c.Start(1600, 840);
        c.Control(1, 250, false);   // gain again
        Recorder r;
        Check(r.Replay(c.Get()), "every setting accepted");
        const std::vector<std::string> want = {
            "region 640x480 bin1 type0", "start 1600,840", "control 1=250", "control 0=8000" };
        Check(r.calls == want, "region first, start next, controls in first-set order",
              std::to_string(r.calls.size()) + " calls");
    }

    printf("\n3. A new format throws the old start position away\n");
    {
        CameraSettingsCache c;
        c.Region(640, 480, 1, 0);
        c.Start(1600, 840);
        c.Region(1280, 960, 1, 0);  // the SDK re-centres here
        Recorder r;
        r.Replay(c.Get());
        bool start = false;
        for (auto& s : r.calls) start |= s.rfind("start", 0) == 0;
        Check(!start, "no start position from before the format change");
        Check(!r.calls.empty() && r.calls[0] == "region 1280x960 bin1 type0", "the newest format");
        c.Start(1280, 600);
        Recorder r2;
        r2.Replay(c.Get());
        Check(r2.calls.size() >= 2 && r2.calls[1] == "start 1280,600",
              "a start position set after it is kept");
    }

    printf("\n4. Auto mode survives, and a refusal does not stop the rest\n");
    {
        CameraSettingsCache c;
        c.Control(0, 5000, true);
        c.Control(1, 200, false);
        c.Control(2, 50, false);
        Recorder r;
        r.refuseId = 1;
        Check(!r.Replay(c.Get()), "a refused control is reported");
        Check(r.calls.size() == 3 && r.calls[0] == "control 0=5000 auto" && r.calls[2] == "control 2=50",
              "auto exposure stays auto, and the control after the refusal is still set");
    }

    printf("\n5. Clear and Restore\n");
    {
        CameraSettingsCache a;
        a.Region(640, 480, 2, 1);
        a.Control(1, 300, false);
        CameraSettingsCache b;
        b.Restore(a.Get());
        a.Clear();
        Recorder ra, rb;
        ra.Replay(a.Get()); rb.Replay(b.Get());
        Check(ra.calls.empty(), "a closed camera forgets what it was told");
        Check(rb.calls.size() == 2 && rb.calls[0] == "region 640x480 bin2 type1",
              "a moved camera keeps it");
    }

    printf("\n%s\n", g_fail ? "FAILED" : "ALL PASSED");
    return g_fail ? 1 : 0;
}
