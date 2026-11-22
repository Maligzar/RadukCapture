// ============================================================
//  Phase 168: RGB ALIGN corrects the dispersion it computes, no more
//  (core/RgbAlign.h)
//
//  The owner at the scope, 2026-11-21: "RGB align does not seem to be
//  working. The image looks like I am seeing double." It moved blue and
//  red apart by 1.5 times the dispersion, along rows only.
//    1. Blue's share of the separation, from the refractive index.
//    2. A planet's blue and red, displaced by a known dispersion toward
//       the zenith, land back on green within half a pixel, at any
//       zenith angle in the frame.
//    3. The old correction left a fringe of half the separation.
//    4. With an ADC fitted, only the manual offset applies.
//    5. The default angle (180) keeps the old direction: blue up, red down.
// ============================================================

#include "core/RgbAlign.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

// Where a channel's spot appears on the display: it sits at
// green + d * u (u toward the zenith) in the frame, and the display
// samples it from (x + sx, y + sy), so it shows at that minus the shift.
static void Shown(double d, double angleDeg, int sx, int sy, double& x, double& y) {
    const double a = angleDeg * 3.14159265358979 / 180.0;
    x = d * std::sin(a) - sx;
    y = -d * std::cos(a) - sy;
}

int main() {
    printf("rgb_align (phase 168)\n");

    printf("\n1. Blue's share of the separation\n");
    const double share = BlueShareOfDispersion(400, 700);
    Check(share > 0.55 && share < 0.75, "400-700 nm: blue is spread further from green than red is",
          std::to_string(share));
    Check(BlueShareOfDispersion(700, 400) == 0.5, "a band given backwards: an even split, not nonsense");

    printf("\n2. Blue and red land on green, at any angle\n");
    bool all = true;
    std::string worst;
    for (double sep : {3.0, 9.0, 17.5}) {
        for (double ang = 0; ang < 360; ang += 15) {
            const RgbAlignShift s = ComputeRgbAlign(sep, 0, ang, share, false);
            double bx, by, rx, ry;
            Shown(sep * share, ang, s.bx, s.by, bx, by);           // blue: toward the zenith
            Shown(-sep * (1 - share), ang, s.rx, s.ry, rx, ry);    // red: away from it
            const double eb = std::hypot(bx, by), er = std::hypot(rx, ry);
            if (eb > 0.71 || er > 0.71) { all = false; worst = std::to_string(sep) + "px at " + std::to_string(ang); }
        }
    }
    Check(all, "3, 9 and 17.5 px, every 15 degrees: within rounding of green", worst);
    {
        const RgbAlignShift s = ComputeRgbAlign(9, 0, 180, share, false);
        Check(std::abs((s.by - s.ry) - 9) <= 1 && s.bx == 0 && s.rx == 0,
              "blue and red move apart by the separation, not 1.5 times it",
              "blue " + std::to_string(s.by) + ", red " + std::to_string(s.ry));
    }

    printf("\n3. The old correction, for the record\n");
    {
        // Phase 23: blue sampled from +shift rows, red from -shift/2, at the
        // default 180 degrees (zenith toward the frame's bottom).
        const int shift = 9;
        double bx, by, rx, ry;
        Shown(9 * share, 180, 0, shift, bx, by);
        Shown(-9 * (1 - share), 180, 0, -shift / 2, rx, ry);
        Check(std::abs(by - ry) >= 3.5, "the old one left blue and red about 4 px apart at +9 (the double image)",
              std::to_string(by - ry));
    }

    printf("\n4. An ADC fitted\n");
    {
        const RgbAlignShift s = ComputeRgbAlign(9, 0, 180, share, true);
        Check(s.bx == 0 && s.by == 0 && s.rx == 0 && s.ry == 0 && s.totalPx == 0,
              "the ADC corrects: nothing automatic is added");
        const RgbAlignShift m = ComputeRgbAlign(9, 3, 180, share, true);
        Check(m.totalPx == 3 && m.by != 0, "the manual offset still applies");
    }

    printf("\n5. Directions\n");
    {
        const RgbAlignShift s = ComputeRgbAlign(9, 0, 180, share, false);
        Check(s.by > 0 && s.ry < 0, "180 (the default): blue moved up, red down, as before");
        const RgbAlignShift t = ComputeRgbAlign(9, 0, 0, share, false);
        Check(t.by < 0 && t.ry > 0, "0: the other way");
        const RgbAlignShift u = ComputeRgbAlign(9, 0, 90, share, false);
        Check(u.bx > 0 && u.rx < 0 && u.by == 0 && u.ry == 0, "90: along the rows");
        const RgbAlignShift z = ComputeRgbAlign(9, -9, 180, share, false);
        Check(z.bx == 0 && z.by == 0 && z.rx == 0 && z.ry == 0, "a manual offset can cancel it entirely");
    }

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
