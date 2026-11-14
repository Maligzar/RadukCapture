#pragma once
// ============================================================
//  FocusCalibration.h -- phase 161.3: what one focuser step does
//
//  Autofocus was tuned on a figure nobody measured: "about 2,000 steps
//  per mm", a focus zone of "+/-500 to 1,000 steps". The mechanical
//  figures that can be assembled (5,760 steps a turn, a 0.75 to 1.06 mm
//  thread, a moving primary's 26x) say seven to ten times otherwise. One
//  run on the sky settles it, and this is the arithmetic of that run.
//
//  All pure. The routine that moves the focuser and takes the readings
//  is AutofocusRoutine::StartCalibration; it hands its readings to the
//  functions here.
//
//    FitWings          the growth of half-flux diameter with steps far
//                      from focus, from both sides together.
//    UmPerStep         that slope as focal-plane travel per step.
//    CriticalZoneUm    the depth of focus the result is measured against.
//    SlackFromProbes   the knob's slack, from a reversal.
//    FormatCalibration the block appended to focus_calibration.ini.
//
//  The optics, for the record. Far from focus the image of a point is
//  the telescope's pupil: an annulus of outer diameter D = defocus / N
//  (N the focal ratio) with a hole of eps * D (eps the central
//  obstruction). Half of an annulus's light lies inside radius
//  R * sqrt((1 + eps^2) / 2), so its half-flux diameter is
//  D * sqrt((1 + eps^2) / 2). A planet's disc convolved with that annulus
//  approaches the same slope once the blur is larger than the disc, which
//  is why only the outer points of each wing are fitted.
//
//  Gated by tests/focus_calibration.cpp.
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace raduk::core {

// A wing point counts when its HFD is at least this many times the
// smallest: by then the blur, not the disc, sets the diameter.
inline constexpr double kWingMinGrowth = 1.5;

struct WingFit {
    bool   ok = false;
    double hfdPxPerStep = 0.0;   // the asymptotic slope, from a parabola through HFD^2
    double leftSlope = 0.0, rightSlope = 0.0;   // each wing's chord, as magnitudes (a check)
    double crossing = 0.0;       // the parabola's vertex: focus, in steps
    int    nLeft = 0, nRight = 0;
    const char* why = "";        // when !ok
};

// A least-squares line through (x, y): y = a + b x. False if degenerate.
inline bool FitLine(const std::vector<double>& x, const std::vector<double>& y,
                    double& a, double& b) {
    const size_t n = (std::min)(x.size(), y.size());
    if (n < 2) return false;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; ++i) { sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; }
    const double d = (double)n * sxx - sx * sx;
    if (std::fabs(d) < 1e-9) return false;
    b = ((double)n * sxy - sx * sy) / d;
    a = (sy - b * sx) / (double)n;
    return true;
}

// A least-squares parabola through (x, y): y = A x^2 + B x + C. False if
// degenerate. x should be centred (the caller's job) for conditioning.
inline bool FitParabola(const std::vector<double>& x, const std::vector<double>& y,
                        double& A, double& B, double& C) {
    const size_t n = (std::min)(x.size(), y.size());
    if (n < 3) return false;
    double scale = 0.0;
    for (size_t i = 0; i < n; ++i) scale = (std::max)(scale, std::fabs(x[i]));
    if (!(scale > 0.0)) return false;
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, t0 = 0, t1 = 0, t2 = 0;
    for (size_t i = 0; i < n; ++i) {
        const double u = x[i] / scale, v = y[i];
        s0 += 1; s1 += u; s2 += u * u; s3 += u * u * u; s4 += u * u * u * u;
        t0 += v; t1 += u * v; t2 += u * u * v;
    }
    auto det3 = [](double a, double b, double c, double d, double e, double f, double g, double h, double i) {
        return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    };
    const double D = det3(s4, s3, s2, s3, s2, s1, s2, s1, s0);
    if (std::fabs(D) < 1e-12) return false;
    const double a = det3(t2, s3, s2, t1, s2, s1, t0, s1, s0) / D;
    const double b = det3(s4, t2, s2, s3, t1, s1, s2, t0, s0) / D;
    const double c = det3(s4, s3, t2, s3, s2, t1, s2, s1, t0) / D;
    A = a / (scale * scale); B = b / scale; C = c;
    return true;
}

// pos[i]: a focuser step. hfd[i]: the half-flux diameter measured there,
// in pixels (<= 0: nothing measured). The points need not be sorted.
inline WingFit FitWings(const std::vector<int>& pos, const std::vector<float>& hfd) {
    WingFit f;
    const size_t n = (std::min)(pos.size(), hfd.size());
    size_t best = n;
    for (size_t i = 0; i < n; ++i)
        if (hfd[i] > 0.0f && (best == n || hfd[i] < hfd[best])) best = i;
    if (best == n) { f.why = "no half-flux diameter was measured"; return f; }
    const double h0 = hfd[best], x0 = pos[best];
    std::vector<double> lx, ly, rx, ry;
    for (size_t i = 0; i < n; ++i) {
        if (!(hfd[i] >= kWingMinGrowth * h0)) continue;
        if (pos[i] < x0)      { lx.push_back(pos[i]); ly.push_back(hfd[i]); }
        else if (pos[i] > x0) { rx.push_back(pos[i]); ry.push_back(hfd[i]); }
    }
    f.nLeft = (int)lx.size(); f.nRight = (int)rx.size();
    if (f.nLeft < 2 || f.nRight < 2) {
        f.why = "fewer than two points on a wing where the blur had outgrown the target";
        return f;
    }
    double al, bl, ar, br;
    if (!FitLine(lx, ly, al, bl) || !FitLine(rx, ry, ar, br)) { f.why = "a wing's points coincide"; return f; }
    if (!(bl < 0.0) || !(br > 0.0)) { f.why = "the wings do not slope away from focus"; return f; }
    f.leftSlope = -bl; f.rightSlope = br;
    // Two wings of one optical system have one slope. Far apart, something
    // else moved (the target left the aperture, a cloud, a slipping motor).
    const double ratio = f.leftSlope / f.rightSlope;
    if (ratio < 0.6 || ratio > 1.0 / 0.6) { f.why = "the two wings' slopes disagree by more than 40%"; return f; }
    // The figure itself: a parabola through HFD SQUARED on the wing points
    // of both sides, y = A (x - c)^2 + C, and sqrt(A). A blurred disc's
    // diameter grows as a hyperbola, sqrt(h0^2 + (k x)^2) to a good
    // approximation, whose square is exactly that parabola with A = k^2;
    // a straight line through the same points reads the hyperbola's chord
    // and comes out low. Measured on a simulated disc convolved with a
    // 34%-obstructed annulus (Mars, Saturn and Jupiter sizes): the line 8
    // to 12% low, this 3 to 4% low. C is left free, because near focus
    // the real curve is flatter than the hyperbola.
    std::vector<double> qx, qy;
    for (size_t i = 0; i < lx.size(); ++i) { qx.push_back(lx[i] - x0); qy.push_back(ly[i] * ly[i]); }
    for (size_t i = 0; i < rx.size(); ++i) { qx.push_back(rx[i] - x0); qy.push_back(ry[i] * ry[i]); }
    double A = 0, B = 0, C = 0;
    if (!FitParabola(qx, qy, A, B, C) || !(A > 0.0)) { f.why = "the wings do not curve away from focus"; return f; }
    f.hfdPxPerStep = std::sqrt(A);
    f.crossing = x0 - B / (2.0 * A);
    f.ok = true;
    return f;
}

// The annulus's half-flux diameter over its outer diameter.
inline double AnnulusHfdFactor(double obstruction) {
    const double e = (std::max)(0.0, (std::min)(0.95, obstruction));
    return std::sqrt((1.0 + e * e) / 2.0);
}

// Focal-plane travel per focuser step, in micrometres.
inline double UmPerStep(double hfdPxPerStep, double pixelUm, double focalRatio, double obstruction) {
    if (!(hfdPxPerStep > 0) || !(pixelUm > 0) || !(focalRatio > 0)) return 0.0;
    const double blurPxPerStep = hfdPxPerStep / AnnulusHfdFactor(obstruction);
    return blurPxPerStep * pixelUm * focalRatio;
}

// The whole depth of focus for a quarter-wave of defocus, 4.88 lambda N^2
// (twice the +/-2.44 lambda N^2 usually quoted), in micrometres.
inline double CriticalZoneUm(double focalRatio, double wavelengthUm = 0.55) {
    return focalRatio > 0 ? 4.88 * wavelengthUm * focalRatio * focalRatio : 0.0;
}

// ── The knob's slack, from a reversal ───────────────────────────
//
// The focuser is brought the final way to a step on the SLOPE of the
// edge score (not its top, where the score does not answer to a small
// move), giving `base`. It then reverses in small moves. While the slack
// is being taken up the mirror does not move and the score stays at
// base; once it is taken up the mirror heads back toward focus and the
// score climbs. travel[i] is how far it has reversed, score[i] what it
// read there.
//
// The slack is the travel at which the score first leaves base by more
// than `threshold` and stays out at the next probe, less half a probe
// step (it happened somewhere in that step), less the mirror travel the
// score needed to cross the threshold (`scorePerStep`, the slope measured
// on the way out; 0: not known, no such correction).
struct SlackFit {
    bool   found = false;
    double steps = 0.0;
    double probedTo = 0.0;   // when !found: the slack is at least this
};

inline SlackFit SlackFromProbes(const std::vector<int>& travel, const std::vector<float>& score,
                                float base, float threshold, double scorePerStep = 0.0) {
    SlackFit s;
    const size_t n = (std::min)(travel.size(), score.size());
    if (n == 0 || !(threshold > 0.0f)) return s;
    s.probedTo = travel[n - 1];
    for (size_t i = 0; i < n; ++i) {
        if (!(std::fabs(score[i] - base) > threshold)) continue;
        if (i + 1 < n && !(std::fabs(score[i + 1] - base) > threshold)) continue;   // one reading: seeing
        const double step = (i == 0) ? travel[0] : (travel[i] - travel[i - 1]);
        double at = travel[i] - 0.5 * step;
        if (scorePerStep > 0.0) at -= threshold / scorePerStep;
        s.found = true;
        s.steps = (std::max)(0.0, at);
        return s;
    }
    return s;
}

// ── The result ──────────────────────────────────────────────────
struct FocusCalibration {
    bool   ok = false;
    std::string message;          // what was and was not measured

    int    position = 0;          // the step it was run at (in focus)
    float  hfdAtFocusPx = 0.0f;

    bool   haveScale = false;
    double hfdPxPerStep = 0.0;
    double leftSlope = 0.0, rightSlope = 0.0;
    int    wingPoints = 0;
    int    wingReach = 0;         // how far out the wings were measured, steps

    bool   haveSlack = false;
    double slackSteps = 0.0;
    double slackAtLeast = 0.0;    // when !haveSlack and it was probed

    bool   haveRepeat = false;
    double repeatScatterPct = 0.0;   // spread of the edge score over the repeats, % of its median
    int    repeats = 0;

    double returnRatio = 0.0;     // edge score back at the start over the score at the start
};

// What the optics make of it. Zero where an input is missing.
struct FocusScale {
    double umPerStep = 0.0;
    double zoneUm = 0.0;
    double zoneSteps = 0.0;
    double blurPxPerStep = 0.0;
};
inline FocusScale ScaleFrom(const FocusCalibration& c, double pixelUm, double focalRatio,
                            double obstruction) {
    FocusScale s;
    if (!c.haveScale) return s;
    s.umPerStep = UmPerStep(c.hfdPxPerStep, pixelUm, focalRatio, obstruction);
    s.zoneUm = CriticalZoneUm(focalRatio);
    s.zoneSteps = s.umPerStep > 0 ? s.zoneUm / s.umPerStep : 0.0;
    s.blurPxPerStep = c.hfdPxPerStep / AnnulusHfdFactor(obstruction);
    return s;
}

// One block of focus_calibration.ini. Appended, never rewritten: the
// figures change with altitude and temperature, and the history is the
// point. A value that was not measured is left out, not written as 0.
inline std::string FormatCalibration(const FocusCalibration& c, const FocusScale& s,
                                     const std::string& utcIso, float tempC, bool haveTemp,
                                     double altitudeDeg, const std::string& target,
                                     double pixelUm, double focalRatio, double obstruction) {
    std::string out;
    char b[200];
    auto add = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof(b), fmt, a...); out += b; };
    add("[calibration %s]\n", utcIso.c_str());
    add("ok=%d\n", c.ok ? 1 : 0);
    if (!target.empty()) add("target=%s\n", target.c_str());
    add("position=%d\n", c.position);
    if (haveTemp) add("focuser_temp_c=%.1f\n", (double)tempC);
    if (altitudeDeg > -90.0) add("altitude_deg=%.1f\n", altitudeDeg);
    add("pixel_um=%.2f\nfocal_ratio=%.2f\nobstruction=%.2f\n", pixelUm, focalRatio, obstruction);
    if (c.hfdAtFocusPx > 0) add("hfd_at_focus_px=%.2f\n", (double)c.hfdAtFocusPx);
    if (c.haveScale) {
        add("hfd_px_per_step=%.5f\n", c.hfdPxPerStep);
        add("wing_slopes_px_per_step=%.5f,%.5f\n", c.leftSlope, c.rightSlope);
        add("wing_points=%d\nwing_reach_steps=%d\n", c.wingPoints, c.wingReach);
        if (s.umPerStep > 0) {
            add("blur_px_per_step=%.5f\n", s.blurPxPerStep);
            add("um_per_step=%.3f\n", s.umPerStep);
            add("critical_zone_um=%.0f\n", s.zoneUm);
            add("critical_zone_steps=%.0f\n", s.zoneSteps);
        }
    }
    if (c.haveSlack) add("slack_steps=%.0f\n", c.slackSteps);
    else if (c.slackAtLeast > 0) add("slack_at_least_steps=%.0f\n", c.slackAtLeast);
    if (c.haveRepeat) add("repeat_scatter_pct=%.1f\nrepeats=%d\n", c.repeatScatterPct, c.repeats);
    if (c.returnRatio > 0) add("return_ratio=%.2f\n", c.returnRatio);
    if (!c.message.empty()) add("note=%s\n", c.message.c_str());
    out += "\n";
    return out;
}

// One line for the panel and the log.
inline std::string CalibrationSummary(const FocusCalibration& c, const FocusScale& s) {
    char b[300];
    std::string out = c.ok ? "Focuser calibrated:" : "Focuser calibration incomplete:";
    if (c.haveScale && s.umPerStep > 0) {
        std::snprintf(b, sizeof(b), " %.2f um a step, the focus zone is %.0f steps;", s.umPerStep, s.zoneSteps);
        out += b;
    } else if (c.haveScale) {
        std::snprintf(b, sizeof(b), " HFD grows %.4f px a step;", c.hfdPxPerStep);
        out += b;
    }
    if (c.haveSlack) { std::snprintf(b, sizeof(b), " slack %.0f steps;", c.slackSteps); out += b; }
    else if (c.slackAtLeast > 0) { std::snprintf(b, sizeof(b), " slack over %.0f steps;", c.slackAtLeast); out += b; }
    if (c.haveRepeat) { std::snprintf(b, sizeof(b), " repeats to %.0f%%;", c.repeatScatterPct); out += b; }
    if (!c.message.empty()) out += " " + c.message;
    return out;
}

}  // namespace raduk::core
