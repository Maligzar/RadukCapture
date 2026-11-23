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

// ── Phase 172 finding 4: the base rate follows a long lock ──────────
//
// 2026-11-22 19:47: the guider was adding +8 to +9 "/s on azimuth to a
// 15.64 "/s base measured 80 minutes earlier. Alt-az rates change as the
// target moves, and the integral was carrying the whole change. It held
// 1.5", but a jump then started from a large integral, and a longer lock
// would have hit the integral's 15 "/s clamp, after which the proportional
// term carries the rest and the planet sits off centre by the shortfall
// over kp (tests/rate_fold.cpp: 17", about 80 px, two hours in).
//
// At steady state the integral IS the rate error, so when the lock has
// been calm for a while the integral moves into the base rate: base +=
// integ, integ = 0. The motor command does not change by a step (the
// correction loses exactly what the base gains); the integral starts
// again with its whole headroom, and the base stays the true rate.
//
// Not while centring, not while the error is large (the integral then
// holds a transient, not a rate), and never across zero: a base that
// would change sign, or fall inside the never-reverse band, is left.
struct RateFoldState { float calmSec = 0.0f; };

struct RateFoldParams {
    float calmAs  = 2.0f;    // error under this counts as calm
    float holdSec = 60.0f;   // calm this long before a fold
    float minInteg = 0.5f;   // a smaller integral is noise, not a rate
};

inline bool FoldIntegralIntoBase(float errAs, bool locked, float dt, float& base, float& integ,
                                 RateFoldState& st, const RateFoldParams& p = {}) {
    if (!locked || !(errAs <= p.calmAs)) { st.calmSec = 0.0f; return false; }
    st.calmSec += dt;
    if (st.calmSec < p.holdSec) return false;
    if (integ < p.minInteg && integ > -p.minInteg) return false;
    const float nb = base + integ;
    if ((nb > 0.0f) != (base > 0.0f) || (nb < 1.0f && nb > -1.0f)) return false;
    base = nb;
    integ = 0.0f;
    st.calmSec = 0.0f;
    return true;
}

} // namespace raduk::core
