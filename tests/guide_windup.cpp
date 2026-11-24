// guide_windup: the rate guider's control law, replayed against a simple
// model of the mount. 2026-11-22 22:53: centring from 480 px off, the
// integral wound up while the corrections were pinned at the never-reverse
// limit, the planet overshot to 146 px and the lock was abandoned.
#include "core/GuideLaw.h"
#include <cstdio>
#include <cmath>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct Run { float overshoot; float finalErr; float settleS; };

// The plant: each axis's error in axis arcseconds shrinks by what the
// correction moves it, and grows by `drift` (the tracking rate's own
// error). The control loop runs every 0.5 s, as the correction thread does.
static Run Simulate(bool antiWindup, float e0Az, float e0Alt, float baseAz, float baseAlt,
                    float driftAz, float driftAlt, float seconds = 90.0f,
                    float reverseGuardMinBase = GuideLawParams{}.reverseGuardMinBase) {
    GuideLawParams p; p.antiWindup = antiWindup; p.reverseGuardMinBase = reverseGuardMinBase;
    GuideAxisState sAz, sAlt;
    float eAz = e0Az, eAlt = e0Alt, dt = 0.5f;
    const float sAz0 = eAz > 0 ? 1.0f : -1.0f, sAlt0 = eAlt > 0 ? 1.0f : -1.0f;
    float overshoot = 0.0f, settle = -1.0f;
    for (float t = 0; t < seconds; t += dt) {
        const float cAz  = GuideAxisStep(eAz,  baseAz,  dt, p, sAz);
        const float cAlt = GuideAxisStep(eAlt, baseAlt, dt, p, sAlt);
        eAz  -= (cAz  - driftAz)  * dt;
        eAlt -= (cAlt - driftAlt) * dt;
        // Overshoot: how far past the centre, on the far side, either axis went.
        if (eAz * sAz0 < 0)   overshoot = std::fmax(overshoot, std::fabs(eAz));
        if (eAlt * sAlt0 < 0) overshoot = std::fmax(overshoot, std::fabs(eAlt));
        const float err = std::sqrt(eAz * eAz + eAlt * eAlt);
        if (err < 2.0f) { if (settle < 0) settle = t; } else settle = -1.0f;
    }
    return { overshoot, std::sqrt(eAz * eAz + eAlt * eAlt), settle };
}

int main() {
    // The night's start, from the log: axis err az -136.7 alt 63.2,
    // rates az 17.47 alt -7.32, a small leftover drift.
    const Run before = Simulate(false, -136.7f, 63.2f, 17.47f, -7.32f, 0.3f, -0.2f);
    const Run after  = Simulate(true,  -136.7f, 63.2f, 17.47f, -7.32f, 0.3f, -0.2f);
    std::printf("  night, old law: overshoot %.1f\", final %.1f\"\n", before.overshoot, before.finalErr);
    std::printf("  night, new law: overshoot %.1f\", final %.1f\", settled at %.1f s\n",
                after.overshoot, after.finalErr, after.settleS);
    // The old law overshoots by tens of arcseconds (the model: about 24",
    // ~120 px at 0.2"/px; the night's 146 px also had the camera's delay,
    // which this model leaves out).
    CHECK(before.overshoot > 15.0f);
    CHECK(after.overshoot < 5.0f);
    CHECK(after.finalErr < 2.0f);
    CHECK(after.settleS >= 0.0f && after.settleS < 40.0f);

    // A short approach (the usual case) works either way.
    const Run shortOld = Simulate(false, -10.0f, 5.0f, 17.47f, -7.32f, 0.3f, -0.2f);
    const Run shortNew = Simulate(true,  -10.0f, 5.0f, 17.47f, -7.32f, 0.3f, -0.2f);
    CHECK(shortOld.finalErr < 2.0f);
    CHECK(shortNew.finalErr < 2.0f);
    CHECK(shortNew.overshoot < 5.0f);

    // The integral's real job survives: a tracking rate 9 "/s off (19:47,
    // eighty minutes into a lock) is absorbed and the error settles.
    const Run stale = Simulate(true, 2.0f, -1.0f, 15.64f, 10.64f, 9.0f, -3.5f, 180.0f);
    std::printf("  stale rates, new law: final %.1f\", settled at %.1f s\n", stale.finalErr, stale.settleS);
    CHECK(stale.finalErr < 2.0f);

    // A single step: pinned by the error's own sign does not integrate;
    // unpinned does.
    {
        GuideLawParams p; GuideAxisState s;
        GuideAxisStep(-136.7f, 17.47f, 0.5f, p, s);   // pinned at 1 - base
        CHECK(s.integ == 0.0f);
        GuideAxisStep(-2.0f, 17.47f, 0.5f, p, s);      // near centre, free
        CHECK(s.integ < 0.0f);
    }

    // 2026-11-24, 20:32: Saturn about 15 minutes from the meridian. Locked
    // at 7" off with the alt base at -3.19 "/s; the planet then needed the
    // alt axis to go the other way (a drift the correction must cancel at
    // about +10 "/s), and the never-reverse rule held the correction at
    // +2.19. The error grew 8 "/s and the lock was abandoned at 154 px.
    {
        const Run old = Simulate(true, 0.4f, 12.3f, 26.62f, -3.19f, 0.0f, 10.2f, 6.0f, 0.0f);
        std::printf("  meridian, guard always: final %.1f\"\n", old.finalErr);
        CHECK(old.finalErr > 40.0f);                 // the night: 46" at 6 s and growing
        const Run now = Simulate(true, 0.4f, 12.3f, 26.62f, -3.19f, 0.0f, 10.2f, 90.0f);
        std::printf("  meridian, slow axis may reverse: final %.1f\", settled at %.1f s\n",
                    now.finalErr, now.settleS);
        CHECK(now.finalErr < 2.0f && now.settleS >= 0.0f);
        // A fast axis is still never reversed.
        GuideLawParams p; GuideAxisState st;
        CHECK(-7.32f + GuideAxisStep(500.0f, -7.32f, 0.5f, p, st) <= -1.0f + 1e-4f);
        // A slow one may cross zero.
        GuideAxisState st2;
        CHECK(-3.19f + GuideAxisStep(500.0f, -3.19f, 0.5f, p, st2) > 0.0f);
    }

    std::printf("guide_windup: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
