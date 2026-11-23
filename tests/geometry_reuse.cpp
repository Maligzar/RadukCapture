// geometry_reuse: Phase 172 finding 11. After ALIGN (a sync) or a GoTo, a
// lock keeps the last calibration's response and re-measures only the
// rates - when nothing about the response can have changed.
#include "core/GuideSolve.h"
#include <cstdio>
#include <cstring>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // The 2026-11-22 22:52 case: calibrated at 22:41, ALIGN, lock again on
    // the same 640x480 box with both axes turning the same way.
    auto ok = CanReuseGeometry(true, 38.0f, 640, 480, 1.0f, -1.0f, 640, 480, 1.0f, -1.0f, 11 * 60.0);
    CHECK(ok.ok);
    // What sends it back to a full calibration.
    CHECK(!CanReuseGeometry(false, 38.0f, 640, 480, 1, -1, 640, 480, 1, -1, 60).ok);          // none
    CHECK(!CanReuseGeometry(true, -1.0f, 640, 480, 1, -1, 640, 480, 1, -1, 60).ok);           // no altitude
    CHECK(!CanReuseGeometry(true, 38.0f, 0, 0, 1, -1, 640, 480, 1, -1, 60).ok);               // old record
    auto sz = CanReuseGeometry(true, 38.0f, 640, 480, 1, -1, 1280, 960, 1, -1, 60);           // new frame
    CHECK(!sz.ok && std::strstr(sz.why, "frame size"));
    auto sg = CanReuseGeometry(true, 38.0f, 640, 480, 1, -1, 640, 480, 1, 1, 60);             // a sign flipped
    CHECK(!sg.ok && std::strstr(sg.why, "other way"));
    CHECK(!CanReuseGeometry(true, 38.0f, 640, 480, 0, 0, 640, 480, 1, -1, 60).ok);            // signs unknown
    CHECK(!CanReuseGeometry(true, 38.0f, 640, 480, 1, -1, 640, 480, 1, -1, 5 * 3600.0).ok);   // too old
    CHECK(!CanReuseGeometry(true, 38.0f, 640, 480, 1, -1, 640, 480, 1, -1, -1.0).ok);         // no age
    CHECK(CanReuseGeometry(true, 38.0f, 640, 480, 1, -1, 640, 480, 1, -1, 3.9 * 3600.0).ok);

    // The kept response, rescaled for a new altitude, still solves: the
    // drift taken out of the rates is the drift measured.
    GuideResponse cal{0.010f, 0.001f, -0.0008f, 0.012f};    // px/ms at 30 deg
    GuideResponse now = RescaleForAltitude(cal, 30.0f, 60.0f);
    CHECK(now.nyPerMs == cal.nyPerMs && now.exPerMs < cal.exPerMs);   // azimuth shrinks, altitude not
    const GuidePulses d = SolveGuidePulses(now, -2.0f, 1.0f);
    CHECK(d.valid);
    const float rx = now.exPerMs * d.eastMs + now.nxPerMs * d.northMs;
    const float ry = now.eyPerMs * d.eastMs + now.nyPerMs * d.northMs;
    CHECK(std::fabs(rx + 2.0f) < 1e-3f && std::fabs(ry - 1.0f) < 1e-3f);

    std::printf("geometry_reuse: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
