// ============================================================
//  emulator_targets -- 2026-11-15
//
//  The owner, watching a soak: a GoTo to Kochab drew Jupiter, and Mars
//  and Venus were tinted discs. Checked here:
//    1. a star the emulator's table does not name is drawn as a star
//       when the caller says it is one, and not otherwise
//    2. Mercury, Uranus and Neptune resolve to themselves
//    3. PlanetPhaseAt against dates whose geometry is known
//    4. Venus at a crescent is lit on the side of its bright limb only,
//       with about the lit fraction the phase angle gives
//    5. Mars has markings (contrast across the disc) and a south cap
//       (blue pixels whiten near the bottom limb)
// ============================================================

#include "core/CameraEmulator.h"
#include "sky/SkyObject.h"

#include <cmath>
#include <cstdio>
#include <vector>

using raduk::core::ASICamera;

static int g_fail = 0;

static void Check(bool ok, const char* what) {
    if (ok) printf("  ok    %s\n", what);
    else  { printf("  FAIL  %s\n", what); ++g_fail; }
}

static bool Grab(const char* target, float radiusAs, float phaseDeg, float paDeg,
                 std::vector<uint8_t>& out, int& W, int& H) {
    ASICamera cam;
    if (!cam.Open(0)) return false;
    W = 640; H = 480;
    cam.SetROI(W, H, 1, ASI_IMG_RAW8);
    int aB = 1; ASI_IMG_TYPE aT = ASI_IMG_RAW8;
    cam.GetROI(W, H, aB, aT);
    cam.SetTargetName(target);
    cam.SetTargetAngularRadiusArcsec(radiusAs);
    cam.SetPlanetPhase(phaseDeg, paDeg);
    cam.SetExposure(5.0);
    cam.SetPeakTargetDn(200);
    cam.SetNoiseAmplitude(0);
    cam.SetFreezeSurface(true);
    if (!cam.StartVideoCapture()) { cam.Close(); return false; }
    out.assign((size_t)W * H, 0);
    bool got = false;
    for (int i = 0; i < 6; i++)
        got = cam.GetVideoFrame(out.data(), (int)out.size(), 8000) || got;
    cam.StopVideoCapture();
    cam.Close();
    return got;
}

int main() {
    printf("Emulator targets: stars as stars, planets as themselves\n");

    printf("\n1. A star not in the table\n");
    {
        ASICamera cam;
        cam.SetTargetName("Kochab", true);
        Check(strcmp(cam.TargetName(), "Star") == 0, "Kochab, said to be a star, is the white star");
        cam.SetTargetName("Pherkad", true);
        Check(strcmp(cam.TargetName(), "Star") == 0, "and so is Pherkad");
        cam.SetTargetName("Vega", true);
        Check(strcmp(cam.TargetName(), "Vega") == 0, "a star the table names keeps its own colour");
        cam.SetTargetName("Star");
        Check(strcmp(cam.TargetName(), "Star") == 0, "the Star profile's name is a star");
        cam.SetTargetName("NGC 7000");
        Check(strcmp(cam.TargetName(), "Jupiter") == 0, "anything else unknown stays on Jupiter");
    }

    printf("\n2. The planets that used to fall back to Jupiter\n");
    {
        ASICamera cam;
        for (const char* n : { "Mercury", "Uranus", "Neptune", "Mars", "Venus" }) {
            cam.SetTargetName(n);
            char what[64];
            snprintf(what, sizeof(what), "%s is %s", n, n);
            Check(strcmp(cam.TargetName(), n) == 0, what);
        }
    }

    printf("\n3. Phases on dates with known geometry\n");
    {
        using raduk::sky::PlanetPhaseAt;
        using raduk::sky::CalcJD;
        // Venus: inferior conjunction 2025-03-23, greatest eastern
        // elongation 2025-01-10 (about half lit), superior conjunction
        // 2026-01-06 (full). Mars at opposition 2025-01-16.
        const auto vInf = PlanetPhaseAt("Venus", CalcJD(2025, 3, 23.0));
        const auto vEl  = PlanetPhaseAt("Venus", CalcJD(2025, 1, 10.0));
        const auto vSup = PlanetPhaseAt("Venus", CalcJD(2026, 1, 6.0));
        const auto mOpp = PlanetPhaseAt("Mars",  CalcJD(2025, 1, 16.0));
        printf("        Venus %.1f / %.1f / %.1f deg, Mars at opposition %.1f deg\n",
               vInf.phaseAngleDeg, vEl.phaseAngleDeg, vSup.phaseAngleDeg, mOpp.phaseAngleDeg);
        Check(vInf.valid && vInf.phaseAngleDeg > 165.0, "Venus at inferior conjunction is nearly new");
        Check(vEl.valid && vEl.illuminated > 0.40 && vEl.illuminated < 0.60,
              "Venus at greatest elongation is about half lit");
        Check(vSup.valid && vSup.illuminated > 0.98, "Venus at superior conjunction is full");
        Check(mOpp.valid && mOpp.phaseAngleDeg < 5.0, "Mars at opposition is full");
        // Evening Venus (east of the Sun) has its bright limb toward the
        // Sun, to the west: a position angle in the western half.
        Check(vEl.brightLimbPADeg > 180.0 && vEl.brightLimbPADeg < 360.0,
              "evening Venus is lit on its west (Sun) side");
        Check(!PlanetPhaseAt("Kochab", CalcJD(2025, 1, 1.0)).valid, "not a planet, not valid");
    }

    printf("\n4. Venus as a crescent\n");
    {
        std::vector<uint8_t> f; int W = 0, H = 0;
        // Phase angle 150: 6.7% lit. Bright limb at PA 90, east, which
        // is the frame's left.
        if (!Grab("Venus", 20.0f, 150.0f, 90.0f, f, W, H)) { Check(false, "frame"); }
        else {
            const float R = 20.0f / ASICamera::EMU_PLATE_SCALE_AS_PX;
            long left = 0, right = 0, disc = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    const float dx = x - W * 0.5f, dy = y - H * 0.5f;
                    if (dx * dx + dy * dy > R * R * 0.95f) continue;
                    ++disc;
                    if (f[(size_t)y * W + x] > 40) (dx < 0 ? left : right)++;
                }
            const double lit = (double)(left + right) / (double)disc;
            printf("        lit pixels: %ld left, %ld right, %.1f%% of the disc\n", left, right, lit * 100);
            Check(left > 20 * (right + 1), "lit on the bright-limb side only");
            Check(lit > 0.01 && lit < 0.15, "about the lit fraction a 150-degree phase gives");
        }
        // Full: lit across.
        if (Grab("Venus", 20.0f, 0.0f, 90.0f, f, W, H)) {
            long left = 0, right = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                    if (f[(size_t)y * W + x] > 40) (x < W / 2 ? left : right)++;
            Check(left > 0 && right > 0 && left < right * 1.2 && right < left * 1.2,
                  "full Venus is lit evenly");
        }
    }

    printf("\n5. Mars has a surface\n");
    {
        // The same statistic on full Venus, a flat tint under the same
        // limb darkening, is the bar Mars's markings have to clear.
        auto middleSpread = [](const std::vector<uint8_t>& f, int W, int H, float R) {
            double s = 0, s2 = 0; long n = 0;
            for (int y = 0; y < H; y += 2)
                for (int x = 0; x < W; x += 2) {
                    const float dx = (x - W * 0.5f) / R, dy = (y - H * 0.5f) / R;
                    if (dx * dx + dy * dy >= 0.36f) continue;
                    const double v = f[(size_t)y * W + x];
                    s += v; s2 += v * v; ++n;
                }
            const double m = n ? s / n : 0;
            return m > 0 ? std::sqrt(s2 / n - m * m) / m : 0.0;
        };
        std::vector<uint8_t> f; int W = 0, H = 0;
        double flat = 0;
        if (Grab("Venus", 20.0f, 0.0f, 90.0f, f, W, H))
            flat = middleSpread(f, W, H, 20.0f / ASICamera::EMU_PLATE_SCALE_AS_PX);
        if (!Grab("Mars", 20.0f, 0.0f, 90.0f, f, W, H)) { Check(false, "frame"); }
        else {
            const float R = 20.0f / ASICamera::EMU_PLATE_SCALE_AS_PX;
            // Red pixels (even row, even column) over the middle of the
            // disc: markings make them vary far more than a tinted disc.
            double s = 0, s2 = 0; long n = 0;
            double bCentre = 0, bCap = 0; long nC = 0, nP = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    const float dx = (x - W * 0.5f) / R, dy = (y - H * 0.5f) / R;
                    const float r2 = dx * dx + dy * dy;
                    const uint8_t v = f[(size_t)y * W + x];
                    if (y % 2 == 0 && x % 2 == 0 && r2 < 0.36f) { s += v; s2 += (double)v * v; ++n; }
                    if (y % 2 == 1 && x % 2 == 1) {        // blue
                        if (r2 < 0.04f) { bCentre += v; ++nC; }
                        if (fabsf(dx) < 0.15f && dy > 0.80f && r2 < 0.95f) { bCap += v; ++nP; }
                    }
                }
            const double mean = n ? s / n : 0, sd = n ? std::sqrt(s2 / n - mean * mean) : 0;
            bCentre = nC ? bCentre / nC : 0; bCap = nP ? bCap / nP : 0;
            printf("        red across the middle: mean %.1f, sd %.1f; blue at centre %.1f, at the south limb %.1f\n",
                   mean, sd, bCentre, bCap);
            printf("        spread across the middle: Mars %.3f, flat Venus %.3f\n", sd / mean, flat);
            Check(mean > 0 && flat > 0 && sd / mean > 2.0 * flat,
                  "markings: twice the spread of a flat disc under the same lighting");
            Check(bCap > bCentre * 1.3, "a white south cap near the bottom limb");
        }
    }

    printf("\n6. A star out of focus is a doughnut you can still see\n");
    {
        // 1500 steps out at the emulator's 0.0228 px/step: the 34 px the
        // first short soak had, where the star vanished under one DN.
        ASICamera cam;
        int W = 640, H = 480;
        if (cam.Open(0)) {
            cam.SetROI(W, H, 1, ASI_IMG_RAW8);
            int aB = 1; ASI_IMG_TYPE aT = ASI_IMG_RAW8;
            cam.GetROI(W, H, aB, aT);
            cam.SetTargetName("Kochab", true);
            cam.SetExposure(1.0);
            cam.SetNoiseAmplitude(0);
            cam.SetFreezeSurface(true);
            cam.SetTrueFocusPosition(168000);
            cam.SetBlurPxPerStep(0.0228f);
            cam.SetFocusPositionProvider([](int& p) { p = 166500; return true; });
            std::vector<uint8_t> f((size_t)W * H, 0);
            bool got = false;
            if (cam.StartVideoCapture()) {
                for (int i = 0; i < 6; i++)
                    got = cam.GetVideoFrame(f.data(), (int)f.size(), 8000) || got;
                cam.StopVideoCapture();
            }
            cam.Close();
            int peak = 0, centre = 0;
            for (uint8_t v : f) if (v > peak) peak = v;
            for (int y = H / 2 - 1; y <= H / 2 + 1; y++)
                for (int x = W / 2 - 1; x <= W / 2 + 1; x++)
                    centre = (std::max)(centre, (int)f[(size_t)y * W + x]);
            printf("        peak %d DN, centre %d DN\n", peak, centre);
            Check(got && peak >= 30, "still well over the sky, so it can be found and focused");
            Check(centre < peak / 2, "dark in the middle: the secondary's shadow");
        } else {
            Check(false, "camera opened");
        }
    }

    printf("\n%s\n", g_fail ? "FAILED" : "all passed");
    return g_fail ? 1 : 0;
}
