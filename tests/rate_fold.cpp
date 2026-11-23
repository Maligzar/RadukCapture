// rate_fold: Phase 172 finding 4. During a long lock the alt-az tracking
// rate changes as the target moves; the integral carried the whole change
// (+8 to +9 "/s on azimuth at 19:47, 2026-11-22). A calm lock now moves its
// integral into the base rate, so the integral keeps its headroom.
#include "core/GuideLaw.h"
#include <cmath>
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct Run { float worstErr = 0, finalErr = 0, maxInteg = 0, finalBase = 0; int folds = 0; };

// One axis, locked, 0.5 s steps. The true rate drifts linearly; the
// error grows by (true - commanded) * dt, plus a little seeing.
static Run Simulate(bool fold, float startRate, float drift_per_min, float minutes) {
    GuideLawParams gp; gp.kp = 0.3f; gp.ki = 0.05f; gp.maxCorr = 30.0f; gp.maxInteg = 15.0f;
    GuideAxisState st;
    RateFoldState fs;
    float base = startRate, err = 0.0f;
    Run r;
    unsigned seed = 7u;
    const float dt = 0.5f;
    const int steps = (int)(minutes * 60.0f / dt);
    for (int i = 0; i < steps; ++i) {
        const float trueRate = startRate + drift_per_min * (i * dt / 60.0f);
        seed = seed * 1103515245u + 12345u;
        const float seeing = (((int)((seed >> 16) % 201)) - 100) / 200.0f;   // +-0.5"
        const float seen = err + seeing;
        const float corr = GuideAxisStep(seen, base, dt, gp, st);
        const float cmd = base + corr;
        if (fold) {
            const float before = base + st.integ;
            if (FoldIntegralIntoBase(std::fabs(seen), true, dt, base, st.integ, fs)) {
                ++r.folds;
                // The command does not jump: base + integ is the same.
                if (std::fabs((base + st.integ) - before) > 1e-4f) { std::printf("  fold changed the command\n"); ++fails; }
            }
        }
        // The axis error the guider wants to remove: the image falls
        // behind by what the commanded rate lacks.
        err += (trueRate - cmd) * dt;
        r.worstErr = std::fmax(r.worstErr, std::fabs(err));
        r.maxInteg = std::fmax(r.maxInteg, std::fabs(st.integ));
    }
    r.finalErr = std::fabs(err);
    r.finalBase = base;
    return r;
}

int main() {
    // 1. The night: 15.64 "/s at lock, +9 "/s over 80 minutes.
    const Run off = Simulate(false, 15.64f, 9.0f / 80.0f, 80.0f);
    const Run on  = Simulate(true,  15.64f, 9.0f / 80.0f, 80.0f);
    std::printf("  80 min, +9\"/s: without fold integral %.1f, worst %.1f\"; with fold integral %.1f, worst %.1f\", "
                "%d folds, base %.2f\n", off.maxInteg, off.worstErr, on.maxInteg, on.worstErr, on.folds, on.finalBase);
    CHECK(off.maxInteg > 8.0f);                 // what the night showed
    CHECK(on.maxInteg < 3.0f);                  // the integral stays small
    CHECK(on.folds > 0);
    CHECK(std::fabs(on.finalBase - (15.64f + 9.0f)) < 1.5f);   // the base follows the true rate
    CHECK(on.worstErr <= off.worstErr + 0.5f);  // and the lock is no worse

    // 2. A longer lock with a larger change. Without the fold the integral
    //    clamps and the proportional term carries the rest, so the planet
    //    sits off centre by shortfall / kp (17" here, ~80 px) and the
    //    offset keeps growing with the drift. With it, centred.
    const Run off2 = Simulate(false, 15.0f, 20.0f / 120.0f, 120.0f);
    const Run on2  = Simulate(true,  15.0f, 20.0f / 120.0f, 120.0f);
    std::printf("  120 min, +20\"/s: without fold integral %.1f, final %.1f\"; with fold integral %.1f, final %.1f\"\n",
                off2.maxInteg, off2.finalErr, on2.maxInteg, on2.finalErr);
    CHECK(off2.maxInteg >= 14.9f);              // clamped
    CHECK(off2.finalErr > 10.0f);               // held well off centre
    CHECK(on2.finalErr < 3.0f && on2.maxInteg < 4.0f);

    // 3. When it must not fold.
    {
        float base = 15.0f, integ = 4.0f;
        RateFoldState fs;
        CHECK(!FoldIntegralIntoBase(0.5f, false, 120.0f, base, integ, fs));      // not locked
        CHECK(!FoldIntegralIntoBase(5.0f, true, 120.0f, base, integ, fs));       // error large
        CHECK(!FoldIntegralIntoBase(0.5f, true, 30.0f, base, integ, fs));        // calm, not long enough
        CHECK(FoldIntegralIntoBase(0.5f, true, 31.0f, base, integ, fs));         // 61 s calm: folds
        CHECK(base == 19.0f && integ == 0.0f);
        float b2 = 15.0f, i2 = 0.2f; RateFoldState f2;
        CHECK(!FoldIntegralIntoBase(0.5f, true, 120.0f, b2, i2, f2));            // integral is noise
        float b3 = 2.0f, i3 = -1.5f; RateFoldState f3;
        CHECK(!FoldIntegralIntoBase(0.5f, true, 120.0f, b3, i3, f3));            // into the never-reverse band
        float b4 = 3.0f, i4 = -5.0f; RateFoldState f4;
        CHECK(!FoldIntegralIntoBase(0.5f, true, 120.0f, b4, i4, f4));            // across zero
        float b5 = -7.3f, i5 = -1.2f; RateFoldState f5;                          // a negative base works too
        CHECK(FoldIntegralIntoBase(0.5f, true, 120.0f, b5, i5, f5) && std::fabs(b5 + 8.5f) < 1e-5f);
    }

    std::printf("rate_fold: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
