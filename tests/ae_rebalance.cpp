// ae_rebalance: Phase 170.0. In band but off the light path, AE slides
// along the path at the same light. The owner, 2026-11-21, at Saturn:
// 25 ms at gain 230 - "Why is AE not doing this automatically?"
#include "core/AutoExposureAssistant.h"
#include <cmath>
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    LightLadder l;   // 1-50 ms, knee 20; gain 182, 350, 570

    // 1. The night's setting and its point on the path, same light.
    const LightPoint night{25.0, 230};
    const double light = LightStops(l, night);
    const LightPoint t = OnPath(l, light);
    std::printf("  25 ms g230: %.2f stops; on the path %.2f ms g%d; %.2f stops off\n",
                light, t.expMs, t.gain, OffPathStops(l, night));
    CHECK(std::fabs(t.expMs - 20.0) < 0.01 && t.gain >= 247 && t.gain <= 251);
    CHECK(std::fabs(LightStops(l, t) - light) < 0.02);
    CHECK(OffPathStops(l, night) < -0.3);                     // exposure should fall

    // 2. One step lands there (0.32 stops is under a stop), light kept.
    Rebalance r = RebalanceStep(l, night);
    CHECK(r.move && r.to.expMs == t.expMs && r.to.gain == t.gain);
    // A point on the path does not move.
    CHECK(!RebalanceStep(l, t).move);
    CHECK(!RebalanceStep(l, {12.0, 182}).move);               // on the exposure rung
    CHECK(!RebalanceStep(l, {20.0, 300}).move);               // on the gain rung

    // 3. Far off: a stop a step, the light the same each step, then home.
    LightPoint p{4.0, 450};                                   // short exposure, gain high
    const double L0 = LightStops(l, p);
    int steps = 0;
    for (; steps < 10; ++steps) {
        r = RebalanceStep(l, p);
        if (!r.move) break;
        CHECK(std::fabs(std::log2(r.to.expMs / p.expMs)) <= 1.0 + 1e-6);
        CHECK(std::fabs(LightStops(l, r.to) - L0) < 0.03);
        p = r.to;
    }
    std::printf("  4 ms g450 -> %.2f ms g%d in %d steps\n", p.expMs, p.gain, steps);
    CHECK(steps >= 2 && steps <= 4);
    CHECK(std::fabs(OffPathStops(l, p)) <= 0.15);

    // 4. Light past the path's ends: nothing on the path matches, so nothing moves.
    CHECK(!RebalanceStep(l, {80.0, 570}).move);
    CHECK(!RebalanceStep(l, {0.0, 200}).move);

    // 5. The assistant: in band and off the path, it suggests the step,
    //    says so, and is not settled (autofocus waits for it to finish).
    {
        AutoExposureAssistant ae;
        ae.SetEnabled(true); ae.SetAuto(true);
        ae.Feed(200.0f, 10.0f, 0.0f, 230, 25.0);
        const AESuggestion s = ae.Tick();
        std::printf("  %s\n", s.text.c_str());
        CHECK(s.exposureMs < 25.0 && s.gain > 230);
        CHECK(s.text.find("rebalance") != std::string::npos);
        CHECK(!ae.Settled());
        // On the path afterwards: nothing more to do.
        AutoExposureAssistant on;
        on.SetEnabled(true); on.SetAuto(true);
        on.Feed(200.0f, 10.0f, 0.0f, s.gain, s.exposureMs);
        const AESuggestion s2 = on.Tick();
        CHECK(s2.exposureMs == s.exposureMs && s2.gain == s.gain && on.Settled());
        // Held (a recording or a sweep): no rebalance.
        AutoExposureAssistant held;
        held.SetEnabled(true); held.SetAuto(true); held.SetHold("recording");
        held.Feed(200.0f, 10.0f, 0.0f, 230, 25.0);
        const AESuggestion s3 = held.Tick();
        CHECK(s3.exposureMs == 25.0 && s3.gain == 230);
        // Switched off: no rebalance.
        AutoExposureAssistant off;
        auto cfg = off.Config_(); cfg.rebalanceMaxStops = 0.0; off.SetConfig(cfg);
        off.SetEnabled(true); off.SetAuto(true);
        off.Feed(200.0f, 10.0f, 0.0f, 230, 25.0);
        const AESuggestion s4 = off.Tick();
        CHECK(s4.exposureMs == 25.0 && s4.gain == 230);
    }

    std::printf("ae_rebalance: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
