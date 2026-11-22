#pragma once
// ============================================================
// GuideLaw.h -- one step of the rate guider's control law
// ============================================================
//
// PLANET LOCK drives both axes at the measured tracking rate plus a
// correction: proportional for the error now, integral for a tracking rate
// that is a little off, both clamped, and clamped again so an axis never
// reverses (a reversal is backlash, a move that does not happen).
//
// 2026-11-22, 22:53 at the scope: integral windup. Centring from 480 px
// off (the full frame), both corrections sat at the never-reverse limit
// for ten seconds while the integral summed the large error to its own
// clamp (15 "/s). At the centre (2.4") the corrections were still near
// full - az -14.21, alt +6.32, which is 0.5 x -2.6 + 15 clipped - so the
// planet overshot to 146 px and the runaway check abandoned the lock.
//
// The fix is the textbook one, conditional integration: an axis whose
// output is pinned at a limit, by the error's own sign, does not
// integrate. Near centre, unpinned, it integrates as before; that is what
// absorbs a tracking rate gone stale over a long lock (+9 "/s at 19:47
// the same night).
//
// Pure; the caller (CentroidTracker's correction thread) holds the state.
// ============================================================

namespace raduk::core {

struct GuideLawParams {
    float kp      = 0.5f;    // "/s of correction per " of error
    float ki      = 0.05f;   // 1/s^2
    float maxCorr = 30.0f;   // "/s either way
    float maxInteg = 15.0f;  // the integral's own clamp
    bool  antiWindup = true; // false: the law before 2026-11-22, for the test's comparison
};

struct GuideAxisState {
    float integ = 0.0f;
};

// err: axis arcseconds to the centre (the solve's output; positive asks
// for a positive correction). base: the axis's tracking rate. dt: seconds.
// Returns the correction in "/s and updates the integral.
inline float GuideAxisStep(float err, float base, float dt, const GuideLawParams& p,
                           GuideAxisState& st) {
    auto clampf = [](float v, float m) { return v > m ? m : (v < -m ? -m : v); };
    auto reverseClamp = [base](float c) {
        if (base > 1.0f && base + c < 1.0f)   return 1.0f - base;
        if (base < -1.0f && base + c > -1.0f) return -1.0f - base;
        return c;
    };
    // What the output would be with the integral as it stands.
    const float want  = p.kp * err + st.integ;
    const float outNow = reverseClamp(clampf(want, p.maxCorr));
    // Pinned: the limits cut the output, and the error pushes further into them.
    const bool pinned = (outNow != want) && ((want - outNow) * err > 0.0f);
    if (!p.antiWindup || !pinned)
        st.integ = clampf(st.integ + p.ki * err * dt, p.maxInteg);
    return reverseClamp(clampf(p.kp * err + st.integ, p.maxCorr));
}

} // namespace raduk::core
