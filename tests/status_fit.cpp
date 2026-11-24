// status_fit: status-bar fields never overlap; the least important go
// first when the bar is narrow (2026-11-24).
#include "core/StatusFit.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    const std::vector<float> w = {40, 70, 80, 110, 230, 90, 300, 150, 90};
    const std::vector<int>   p = { 1,  2,  6,   7,   5,  4,   3,   2,  6};
    float all = 0; for (float x : w) all += x;
    all += 8 * 12.0f;
    auto k = FitByPriority(w, p, 12.0f, all);
    for (bool b : k) CHECK(b);                         // everything fits
    k = FitByPriority(w, p, 12.0f, all - 1.0f);         // one short: the last priority-7
    CHECK(!k[3]);
    for (size_t i = 0; i < k.size(); ++i) if (i != 3) CHECK(k[i]);
    k = FitByPriority(w, p, 12.0f, all - 150.0f);       // then the 6s, later first
    CHECK(!k[3] && !k[8] && k[2]);
    k = FitByPriority(w, p, 12.0f, 500.0f);             // narrow: only the most important
    float t = 0; int n = 0;
    for (size_t i = 0; i < k.size(); ++i) if (k[i]) { t += w[i]; ++n; }
    CHECK(t + 12.0f * (n - 1) <= 500.0f);
    CHECK(k[0] && k[1] && k[7]);                        // LIVE, FPS and the clock stay
    CHECK(!k[4] && !k[5]);
    k = FitByPriority(w, p, 12.0f, 10.0f);              // nothing fits: nothing drawn
    for (bool b : k) CHECK(!b);
    std::printf("status_fit: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
