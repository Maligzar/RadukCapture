// ============================================================
//  LightLadder -- how auto-exposure shares light between exposure
//  and gain
//
//  Phase 112, the owner's call: "AE should be changing all settings",
//  and option A of docs/Phase112-Plan-Camera-Review.md.
//
//  Exposure and gain are not interchangeable. Once the light is
//  matched, a shorter exposure freezes more seeing and a lower gain is
//  less noise, and on this sensor crossing 182 is worth having by
//  itself. So there is one path through the two, walked forwards for
//  more light and backwards for less:
//
//    (expMin, 0) -> (expMin, floor)      gain up to the HCG floor
//                -> (expKnee, floor)     exposure, the cheap light
//                -> (expKnee, gainKnee)  gain, keeping the seeing frozen
//                -> (expMax, gainKnee)   exposure, now costing sharpness
//                -> (expMax, gainMax)    gain, the last resort
//
//  A correction is measured in stops, because that is the one unit the
//  two levers share. One that crosses a rung spills into the next
//  lever, so a large error moves both sliders in one step.
//
//  Gain is 0.1 dB a unit, so a stop is 60.2 units whatever the gain is.
//  The assistant used to multiply gain by its correction factor as if
//  it were linear, which at gain 200 turned a 1.2 into 1.58.
//
//  A point off the path, set by hand or by a profile, is walked from
//  where it is: each rule looks only at the lever it would move.
//
//  The knees are CameraRails::RailsFor's. UNVERIFIED ON SKY.
// ============================================================

#pragma once

#include <algorithm>
#include <cmath>

namespace raduk::core {

inline constexpr double GAIN_UNITS_PER_STOP = 60.206;  // 200 log10(2)

struct LightLadder {
    double expMinMs  = 1.0;
    double expKneeMs = 20.0;
    double expMaxMs  = 50.0;
    int    gainFloor = 182;
    int    gainKnee  = 350;
    int    gainMax   = 570;
};

struct LightPoint {
    double expMs = 0.0;
    int    gain  = 0;
};

inline LightPoint WalkLadder(const LightLadder& l, double expMs, int gainIn,
                             double stops) {
    double exp  = expMs;
    double gain = (double)gainIn;
    if (exp <= 0.0) return { expMs, gainIn };

    // Each moves its lever toward `to` by as much of what is left as
    // fits, and does nothing if the lever is already past it.
    auto expUp = [&](double to) {
        if (stops <= 0.0 || exp >= to) return;
        const double take = (std::min)(stops, std::log2(to / exp));
        exp *= std::pow(2.0, take); stops -= take;
        if (stops <= 1e-9) stops = 0.0;
    };
    auto expDown = [&](double to) {
        if (stops >= 0.0 || exp <= to) return;
        const double take = (std::max)(stops, std::log2(to / exp));
        exp *= std::pow(2.0, take); stops -= take;
        if (stops >= -1e-9) stops = 0.0;
    };
    auto gainUp = [&](double to) {
        if (stops <= 0.0 || gain >= to) return;
        const double take = (std::min)(stops, (to - gain) / GAIN_UNITS_PER_STOP);
        gain += take * GAIN_UNITS_PER_STOP; stops -= take;
        if (stops <= 1e-9) stops = 0.0;
    };
    auto gainDown = [&](double to) {
        if (stops >= 0.0 || gain <= to) return;
        const double take = (std::max)(stops, (to - gain) / GAIN_UNITS_PER_STOP);
        gain += take * GAIN_UNITS_PER_STOP; stops -= take;
        if (stops >= -1e-9) stops = 0.0;
    };

    const double knee = (std::min)(l.expKneeMs, l.expMaxMs);
    gainUp(l.gainFloor);
    expUp(knee);
    gainUp(l.gainKnee);
    expUp(l.expMaxMs);
    gainUp(l.gainMax);

    gainDown(l.gainKnee);
    expDown(knee);
    gainDown(l.gainFloor);
    expDown(l.expMinMs);
    gainDown(0.0);

    // To the microsecond, which is the camera's resolution.
    return { std::round(exp * 1000.0) / 1000.0, (int)std::lround(gain) };
}

// ── Phase 170.0: back onto the path at the same light ────────────
//
// The owner at the scope, 2026-11-21, recording Saturn at 25 ms and gain
// 230: "Why is AE not doing this automatically?" AE walks the path above
// only when the brightness is wrong, so a setting off it (from a profile
// or a hand change) stays wherever the highlight lands in band. 25 ms at
// 230 is the same light as about 20 ms at 249 on the path: fewer frames
// lost to seeing for the same signal.
//
// Light is counted in stops above (expMin, gain 0); the point on the path
// with that light is the walk from there. A rebalance trades one lever
// against the other at constant light, at most maxStepStops a step, so the
// highlight stays in band throughout.

// The light at a setting, in stops above (expMin, gain 0).
inline double LightStops(const LightLadder& l, const LightPoint& p) {
    if (p.expMs <= 0.0 || l.expMinMs <= 0.0) return 0.0;
    return std::log2(p.expMs / l.expMinMs) + p.gain / GAIN_UNITS_PER_STOP;
}

// The point on the path with that light (the path ends at (expMax, gainMax)).
inline LightPoint OnPath(const LightLadder& l, double stops) {
    return WalkLadder(l, l.expMinMs, 0, (std::max)(0.0, stops));
}

// How far a setting is from the path, in stops of exposure (gain moves the
// other way by the same amount when the light is kept).
inline double OffPathStops(const LightLadder& l, const LightPoint& p) {
    if (p.expMs <= 0.0) return 0.0;
    const LightPoint t = OnPath(l, LightStops(l, p));
    return std::log2(t.expMs / p.expMs);
}

struct Rebalance {
    bool       move = false;
    LightPoint to;
    double     offStops = 0.0;   // before this step: + means exposure should rise
};

// One step toward the path at constant light. Nothing when the setting is
// within `tolStops` of the path, or when its light is beyond the path's
// ends (then no point on the path has the same light).
inline Rebalance RebalanceStep(const LightLadder& l, const LightPoint& p,
                               double maxStepStops = 1.0, double tolStops = 0.15) {
    Rebalance r;
    r.to = p;
    if (p.expMs <= 0.0) return r;
    const double light = LightStops(l, p);
    const LightPoint t = OnPath(l, light);
    if (std::fabs(LightStops(l, t) - light) > 0.05) return r;      // off the path's ends
    r.offStops = std::log2(t.expMs / p.expMs);
    if (std::fabs(r.offStops) <= tolStops) return r;
    const double step = std::clamp(r.offStops, -maxStepStops, maxStepStops);
    if (step == r.offStops) {
        r.to = t;                                                  // the last step lands on the path
    } else {
        r.to.expMs = std::round(p.expMs * std::pow(2.0, step) * 1000.0) / 1000.0;
        r.to.gain  = (int)std::lround(p.gain - step * GAIN_UNITS_PER_STOP);
    }
    r.move = (r.to.expMs != p.expMs || r.to.gain != p.gain);
    return r;
}

} // namespace raduk::core
