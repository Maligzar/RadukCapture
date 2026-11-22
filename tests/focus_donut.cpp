// ============================================================
//  Phase 167: a defocused Saturn through an SCT, measured
//
//  The owner at the scope, 2026-11-21: autofocus took a nearly focused
//  Saturn "entirely out of focus", to a donut (the secondary mirror's
//  shadow in a defocused image). The emulator blurs a planet with a box
//  blur, so no test had ever seen an SCT's defocus. This draws one:
//  a Saturn-like globe and thin ring, convolved with an annulus whose hole
//  is the central obstruction (0.34), with noise, the globe saturating.
//
//  What it shows, and what the fix rests on:
//    1. Both autofocus scores fall steadily with defocus (HFD and the
//       edge metric are not fooled by the donut as such).
//    2. But HFD falls SLOWLY on a disc this size: with the middle already
//       darkening it still scores over half of focus, so the old check
//       ("revert only if the score halved") stayed silent until the donut
//       was deep.
//    3. A drop of a tenth (the new check) is reached well before then.
//    4. The edge metric separates near-focus positions far more strongly
//       than HFD does (why it judges the fine pass, and HFD the far ones).
// ============================================================

#include "core/FocusMetric.h"
#include "core/TargetFinder.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

struct Shot { float hfdScore, edge, centre, peak; };

// A 640x480 8-bit frame: globe radius G px, ring ellipse 1.25-2.3 G with
// b/a 0.12 at 35 degrees, defocused by an annulus of outer radius R px
// and hole obs*R, sky 12, noise sigma 6, saturating at 255.
static Shot Frame(float G, int R, float obs = 0.34f, unsigned seed = 1) {
    const int W = 640, H = 480;
    const float cx = W / 2.f, cy = H / 2.f, ang = 35.f * 3.14159265f / 180.f;
    const float ca = std::cos(ang), sa = std::sin(ang);
    std::vector<float> scene((size_t)W * H, 0.f);
    std::vector<int> lit;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const float dx = x - cx, dy = y - cy;
            const float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
            const float r = std::sqrt(dx * dx + dy * dy);
            float val = 0;
            if (r < G) val = 300.f * (0.6f + 0.4f * std::sqrt(1 - r * r / (G * G)));
            const float e = std::sqrt(u * u + (v / 0.12f) * (v / 0.12f));
            if (val == 0 && e > 1.25f * G && e < 2.3f * G) val = 220.f;
            if (val > 0) { scene[(size_t)y * W + x] = val; lit.push_back(y * W + x); }
        }
    std::vector<std::pair<int, int>> k;
    if (R == 0) k.push_back({0, 0});
    else
        for (int dy = -R; dy <= R; dy++)
            for (int dx = -R; dx <= R; dx++) {
                const float d = std::sqrt((float)(dx * dx + dy * dy));
                if (d <= R && d >= obs * R) k.push_back({dx, dy});
            }
    std::vector<float> img((size_t)W * H, 12.f);
    const float wgt = 1.f / (float)k.size();
    for (int i : lit) {
        const int x = i % W, y = i / W;
        const float v = scene[(size_t)i] * wgt;
        for (auto& o : k) {
            const int X = x + o.first, Y = y + o.second;
            if (X >= 0 && X < W && Y >= 0 && Y < H) img[(size_t)Y * W + X] += v;
        }
    }
    std::mt19937 rng(seed);
    std::normal_distribution<float> n(0, 6);
    std::vector<uint8_t> b((size_t)W * H);
    float peak = 0;
    for (size_t i = 0; i < b.size(); i++) {
        peak = std::max(peak, img[i]);
        b[i] = (uint8_t)std::max(0.f, std::min(255.f, img[i] + n(rng)));
    }
    const TargetFind tf = FindBrightTarget(b.data(), W, H, 8);
    const FocusMeasure fm = MeasurePlanetFocus(b.data(), W, H, 8, tf);
    EdgeScratch es;
    const EdgeFocus ef = MeasureEdgeFocus(b.data(), W, H, 8, tf.x, tf.y, 160, tf.bgDN, es);
    return { fm.valid ? fm.score : 0.f, ef.valid ? ef.score : 0.f, img[(size_t)(H / 2) * W + W / 2], peak };
}

int main() {
    printf("focus_donut (phase 167)\n");
    const float G = 40;
    std::vector<int> Rs;
    std::vector<Shot> s;
    for (int R = 0; R <= 96; R += 8) { Rs.push_back(R); s.push_back(Frame(G, R)); }
    for (size_t i = 0; i < s.size(); i++)
        printf("    R=%3d  HFD score %6.2f  edge %8.4f  centre %6.1f of peak %6.1f\n",
               Rs[i], s[i].hfdScore, s[i].edge, s[i].centre, s[i].peak);

    printf("\n1. Both scores fall with defocus\n");
    bool hfdDown = true, edgeDown = true;
    for (size_t i = 1; i < s.size(); i++) {
        if (s[i].hfdScore >= s[i - 1].hfdScore) hfdDown = false;
        if (s[i].edge >= s[i - 1].edge) edgeDown = false;
    }
    Check(hfdDown, "the HFD score falls at every step out from focus");
    Check(edgeDown, "the edge score falls at every step out from focus");

    printf("\n2. The old check saw a donut only once it was deep\n");
    size_t donut = 0, deep = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (!donut && s[i].centre < 0.96f * s[i].peak) donut = i;   // the middle starts to darken
        if (!deep && s[i].centre < 0.85f * s[i].peak) deep = i;     // a plain donut
    }
    Check(donut > 0 && deep > donut, "the middle darkens, then the image is a plain donut",
          "R=" + std::to_string(donut ? Rs[donut] : -1) + ", R=" + std::to_string(deep ? Rs[deep] : -1));
    const float frac = donut ? s[donut].hfdScore / s[0].hfdScore : 0.f;
    Check(donut > 0 && frac > 0.50f,
          "with the middle darkening it still scores over half: the old 0.50 check stays silent",
          std::to_string(frac));

    printf("\n3. The new check (a tenth worse) fires well before\n");
    size_t first = 0;
    for (size_t i = 1; i < s.size(); i++)
        if (s[i].hfdScore < 0.90f * s[0].hfdScore) { first = i; break; }
    Check(first > 0 && first < donut, "a tenth worse is reached long before the middle darkens",
          "R=" + std::to_string(first ? Rs[first] : -1));

    printf("\n4. Near focus, the edge metric separates far more than HFD\n");
    const float hfdRatio = s[1].hfdScore / s[0].hfdScore, edgeRatio = s[1].edge / s[0].edge;
    Check(edgeRatio < 0.6f && hfdRatio > 0.9f, "8 px of defocus: edge falls by over 40%, HFD by under 10%",
          "edge " + std::to_string(edgeRatio) + ", HFD " + std::to_string(hfdRatio));

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
