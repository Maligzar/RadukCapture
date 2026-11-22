// af_sweep_judge: Phase 167.5. A sweep is judged before anything is fitted
// to it. Tonight's sweep (2026-11-22 23:10, Saturn through haze) is replayed
// from its own log lines.
#include "core/FocusDirection.h"
#include <cstdio>
#include <cmath>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // 1. The night's sweep, as logged: positions 165665..171665 every 600.
    //    Out: flat 14.6 for seven points, then the fade. Back: 4.5-4.8 everywhere.
    {
        std::vector<float> out  = {14.6f, 14.8f, 14.7f, 14.6f, 14.6f, 14.3f, 14.0f, 8.3f, 6.3f, 5.1f, 4.8f};
        // back is logged from 171665 down to 165665; stored per position:
        std::vector<float> back = {4.6f, 4.6f, 4.5f, 4.7f, 4.6f, 4.6f, 4.6f, 4.6f, 4.6f, 4.8f, 4.5f};
        auto j = JudgeSweep(out, back);
        std::printf("  night: median ratio %.2f, spread %.2f -> %s\n", j.medianRatio, j.spread,
                    j.usable ? "usable" : j.why.c_str());
        CHECK(!j.usable);
        CHECK(j.medianRatio > 3.0f);
        CHECK(j.why.find("sky changed") != std::string::npos);
    }
    // 2. A good sweep: a V-curve, read twice with seeing noise and the
    //    return pass shifted by backlash. Usable.
    {
        std::vector<float> out, back;
        for (int i = 0; i < 11; ++i) {
            const float x  = (i - 5) / 2.5f;
            const float xb = (i - 4.6f) / 2.5f;   // backlash: about half a step
            out.push_back(4.0f + 10.0f * std::exp(-x * x)  * (1.0f + 0.05f * ((i % 3) - 1)));
            back.push_back(4.0f + 10.0f * std::exp(-xb * xb) * (1.0f - 0.04f * ((i % 2) ? 1 : -1)));
        }
        auto j = JudgeSweep(out, back);
        std::printf("  V-curve: median ratio %.2f, spread %.2f -> %s\n", j.medianRatio, j.spread,
                    j.usable ? "usable" : j.why.c_str());
        CHECK(j.usable);
    }
    // 3. A steady fade of 30% across the whole run (the 2026-11-08 case,
    //    dew or haze): the curve keeps its shape, the passes disagree a
    //    little. Still usable.
    {
        std::vector<float> out, back;
        const int n = 11;
        for (int i = 0; i < n; ++i) {
            const float x = (i - 5) / 2.5f;
            const float v = 4.0f + 10.0f * std::exp(-x * x);
            const float tOut  = (float)i / (2 * n);              // measured early
            const float tBack = (float)(2 * n - 1 - i) / (2 * n); // measured late
            out.push_back(v * (1.0f - 0.30f * tOut));
            back.push_back(v * (1.0f - 0.30f * tBack));
        }
        auto j = JudgeSweep(out, back);
        std::printf("  steady fade: median ratio %.2f -> %s\n", j.medianRatio, j.usable ? "usable" : j.why.c_str());
        CHECK(j.usable);
    }
    // 4. No peak: haze so thick the score does not move with focus.
    {
        std::vector<float> out  = {9.6f, 9.7f, 9.6f, 9.6f, 9.5f, 9.4f, 9.3f};
        std::vector<float> back = {9.5f, 9.6f, 9.7f, 9.5f, 9.6f, 9.4f, 9.4f};
        auto j = JudgeSweep(out, back);
        CHECK(!j.usable);
        CHECK(j.why.find("no focus peak") != std::string::npos);
    }
    // 5. One-pass sweeps (no return scores) are judged on spread only.
    {
        std::vector<float> out  = {5, 8, 12, 14, 12, 8, 5};
        std::vector<float> back(7, 0.0f);
        CHECK(JudgeSweep(out, back).usable);
    }
    // 6. The A/B's fade guard: tonight 4.6 now against 14.6 before.
    CHECK(FadedTooFar(4.6f, 14.6f));
    CHECK(!FadedTooFar(12.0f, 14.6f));   // a dimming the A/B can still judge
    CHECK(!FadedTooFar(0.0f, 14.6f));    // nothing measured: not this rule's call
    CHECK(!FadedTooFar(4.6f, 0.0f));
    // 7. The verdict: the start stands only when the target faded AND
    //    neither pass of the sweep found a peak.
    {
        // Tonight, 2026-11-22 23:10, per position 165665..171665.
        const std::vector<float> nightOut  = {14.6f, 14.8f, 14.7f, 14.6f, 14.6f, 14.3f, 14.0f, 8.3f, 6.3f, 5.1f, 4.8f};
        const std::vector<float> nightBack = {4.6f, 4.6f, 4.5f, 4.7f, 4.6f, 4.6f, 4.6f, 4.6f, 4.6f, 4.8f, 4.5f};
        CHECK(!PassHasPeak(nightOut));
        CHECK(!PassHasPeak(nightBack));
        CHECK(SweepSpoiledByFade(4.6f, 14.6f, nightOut, nightBack));
        // autofocus_saturn case 3 (CI, 2026-11-22): the target fades through
        // the whole sweep, the passes disagree 4.45x, the start re-measures
        // at 0.5 against 141.1 - and each pass still peaks near focus.
        const std::vector<float> c3Out  = {19.3f, 20.1f, 22.6f, 26.5f, 30.4f, 34.5f, 41.3f, 43.3f, 38.2f, 27.0f, 17.3f};
        const std::vector<float> c3Back = {1.0f, 1.4f, 2.1f, 3.2f, 5.0f, 7.8f, 12.6f, 17.2f, 20.0f, 19.0f, 17.1f};
        CHECK(PassHasPeak(c3Out));
        CHECK(PassHasPeak(c3Back));
        CHECK(!SweepSpoiledByFade(0.5f, 141.1f, c3Out, c3Back));
        // A start that did not fade never stands on this rule.
        CHECK(!SweepSpoiledByFade(12.0f, 14.6f, nightOut, nightBack));
        // One pass peaked: trust it.
        CHECK(!SweepSpoiledByFade(4.6f, 14.6f, c3Out, nightBack));
        // autofocus_saturn case 9 (CI, 2026-11-22, 4 runs in 7): the target
        // fades to 47%, focus sits 500 steps inside the sweep's end so
        // neither pass peaks by 1.15x - but both swing 12-16x and rise
        // together by position. That is focus: the fit stands.
        const std::vector<float> c9Out  = {15.3f, 20.2f, 28.1f, 47.0f, 72.5f, 102.0f, 150.9f, 175.0f, 181.4f, 182.4f, 167.4f};
        const std::vector<float> c9Back = {11.0f, 14.2f, 22.4f, 33.4f, 52.7f, 85.8f, 124.3f, 164.8f, 166.1f, 180.9f, 173.0f};
        CHECK(!PassHasPeak(c9Out) && !PassHasPeak(c9Back));
        CHECK(PassesAgree(c9Out, c9Back));
        CHECK(!SweepSpoiledByFade(59.8f, 128.0f, c9Out, c9Back));
        // The night's passes do not agree (the return pass is flat).
        CHECK(!PassesAgree(nightOut, nightBack));
        // Opposite shapes do not agree either.
        CHECK(!PassesAgree({1.0f, 2.0f, 4.0f, 8.0f}, {8.0f, 4.0f, 2.0f, 1.0f}));
        // A peak at an end of the sweep is not a peak (focus not bracketed).
        CHECK(!PassHasPeak({5.0f, 8.0f, 12.0f, 20.0f}));
        CHECK(!PassHasPeak({4.0f, 4.1f}));
    }

    std::printf("af_sweep_judge: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
