// focus_temp_model: Phase 174.1. Settled focus against focuser temperature,
// fitted robustly per target and filter, trusted only on enough data.
#include "core/FocusTempModel.h"
#include <cstdio>
#include <string>

using namespace raduk::core::focustemp;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

// One sweep and its followup, as the app writes them.
static std::string Pair(const char* id, long long unix, const char* target, const char* filter,
                        const char* temp, int result, int settled) {
    char b[1024];
    std::snprintf(b, sizeof(b),
        "{\"kind\":\"sweep\",\"format\":1,\"id\":\"%s\",\"unix\":%lld,\"at\":{\"target\":\"%s\",\"filter\":\"%s\",\"focuser_temp_c\":%s},\"result\":%d}\n"
        "{\"kind\":\"followup\",\"format\":1,\"sweep\":\"%s\",\"settled\":%d}\n",
        id, unix, target, filter, temp, result, id, settled);
    return b;
}

int main() {
    const long long night1 = 1795300223;           // 2026-11-22 evening
    const long long night2 = night1 + 86400;
    const long long night3 = night1 + 2 * 86400;

    // 1. A clean -120 steps per C, over three nights and 6 C, plus one bad
    //    point (a sweep through cloud the owner fixed far off). Theil-Sen holds.
    std::string h;
    int k = 0;
    auto add = [&](long long unix, double t, int settled, const char* tgt = "Saturn") {
        char id[32], ts[16];
        std::snprintf(id, sizeof(id), "s%d", ++k);
        std::snprintf(ts, sizeof(ts), "%.2f", t);
        h += Pair(id, unix, tgt, "UV/IR cut", ts, settled, settled);
    };
    const double temps[] = {16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0};
    const long long when[] = {night1, night1 + 3600, night1 + 7200, night2, night2 + 3600, night3, night3 + 3600};
    for (int i = 0; i < 7; ++i) add(when[i], temps[i], (int)(170000 - 120 * (temps[i] - 10.0)) + (i % 2 ? 15 : -15));
    add(night2 + 7200, 12.5, 160000);   // the outlier
    const auto pts = PointsFromHistory(h);
    CHECK(pts.size() == 8);
    const Fit f = FitPoints(KeyOf("Saturn", "UV/IR cut"), pts);
    std::printf("  saturn: %d points, %d nights, %.1f-%.1f C, slope %.1f steps/C, scatter %.0f, %s\n",
                f.points, f.nights, f.tMin, f.tMax, f.slope, f.scatter, f.confident ? "confident" : f.why.c_str());
    CHECK(f.ok && f.confident);
    CHECK(f.nights == 3);
    CHECK(std::fabs(f.slope + 120.0) < 15.0);
    CHECK(std::fabs(f.Predict(13.0) - (170000 - 360)) < 60.0);
    CHECK(f.scatter < 60.0);          // the outlier does not inflate it

    // 2. A refocus is suggested only past the critical focus zone.
    CHECK(!RefocusSuggested(f, 14.0, 13.5, 100.0));   // 60 steps: inside the zone
    CHECK(RefocusSuggested(f, 14.0, 12.5, 100.0));    // 180 steps: outside
    CHECK(!RefocusSuggested(f, 14.0, 12.5, 0.0));     // no zone known: never
    CHECK(!RefocusSuggested(f, NAN, 12.5, 100.0));

    // 3. One night only: it fits, and says it is not to be trusted.
    std::string one;
    for (int i = 0; i < 6; ++i) {
        char id[16], ts[16]; std::snprintf(id, sizeof(id), "n%d", i); std::snprintf(ts, sizeof(ts), "%.1f", 15.0 - i * 0.6);
        one += Pair(id, night1 + i * 900, "Jupiter", "UV/IR cut", ts, 168000 + i * 70, 168000 + i * 70);
    }
    const Fit g = FitPoints(KeyOf("Jupiter", "UV/IR cut"), PointsFromHistory(one));
    CHECK(g.ok && !g.confident);
    CHECK(g.why.find("nights") != std::string::npos);
    CHECK(!RefocusSuggested(g, 15.0, 10.0, 10.0));

    // 4. Without a followup or a temperature there is no point: no guessing.
    std::string partial =
        "{\"kind\":\"sweep\",\"id\":\"a\",\"unix\":1,\"at\":{\"target\":\"Mars\",\"filter\":\"\",\"focuser_temp_c\":12},\"result\":5}\n"
        "{\"kind\":\"sweep\",\"id\":\"b\",\"unix\":1,\"at\":{\"target\":\"Mars\",\"filter\":\"\",\"focuser_temp_c\":null},\"result\":5}\n"
        "{\"kind\":\"followup\",\"sweep\":\"b\",\"settled\":7}\n"
        "not json\n";
    CHECK(PointsFromHistory(partial).empty());
    const Fit e = FitPoints("Mars|", PointsFromHistory(partial));
    CHECK(!e.ok && !e.confident && e.points == 0);

    // 5. Same temperature every time: no slope can be drawn.
    std::string flat;
    for (int i = 0; i < 6; ++i) {
        char id[16]; std::snprintf(id, sizeof(id), "f%d", i);
        flat += Pair(id, night1 + i * 86400, "Moon", "", "9.0", 150000 + i, 150000 + i);
    }
    const Fit fl = FitPoints("Moon|", PointsFromHistory(flat));
    CHECK(!fl.ok && fl.points == 6);

    // 6. Keys stay apart: Saturn's data does not move Jupiter's line.
    const auto all = FitAll(PointsFromHistory(h + one));
    CHECK(all.size() == 2);

    // 7. The night boundary: 02:00 belongs to the evening before.
    CHECK(NightOf(night1) == NightOf(night1 + 4 * 3600));

    std::printf("focus_temp_model: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
