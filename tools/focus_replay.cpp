// focus_replay (Phase 174.2): recorded autofocus sweeps through today's
// rules, scored against where the owner settled by hand.
//
//   focus_replay.exe <logs>\focus_history.jsonl
//   focus_replay.exe <logs>\focus_history.jsonl --temp   (Phase 174.1)
//
// Prints one line per sweep: the recorded result and verdict, today's, the
// position the owner settled on (when a followup recorded one) and the
// error of each. Exits 1 if any sweep would now end further from where the
// owner settled than it did (a regression), so a rule change can be gated
// on the whole history. Built beside the app: build\Release\focus_replay.exe.
#include "core/FocusReplay.h"
#include "core/FocusTempModel.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: focus_replay <focus_history.jsonl> [--temp]\n");
        return 2;
    }
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::string text; char buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);

    if (argc >= 3 && std::string(argv[2]) == "--temp") {
        // Phase 174.1: settled focus against focuser temperature, per target and filter.
        using namespace raduk::core::focustemp;
        const auto pts = PointsFromHistory(text);
        std::printf("%zu point(s) with a settled position and a temperature\n\n", pts.size());
        std::printf("%-34s %6s %6s %13s %11s %9s  %s\n", "target|filter", "points", "nights", "temp C",
                    "steps/C", "scatter", "trust");
        for (const auto& f : FitAll(pts)) {
            char range[32] = "-", slope[16] = "-", sc[16] = "-";
            if (f.points) std::snprintf(range, sizeof(range), "%.1f..%.1f", f.tMin, f.tMax);
            if (f.ok) { std::snprintf(slope, sizeof(slope), "%.1f", f.slope); std::snprintf(sc, sizeof(sc), "%.0f", f.scatter); }
            std::printf("%-34s %6d %6d %13s %11s %9s  %s\n", f.key.c_str(), f.points, f.nights, range, slope, sc,
                        f.confident ? "confident" : ("not yet: " + f.why).c_str());
        }
        return 0;
    }
    using namespace raduk::core::replay;
    const ReplayReport rep = ReplayHistory(text);
    std::printf("%d line(s) read, %d unreadable, %zu sweep(s)\n\n", rep.linesRead, rep.linesBad, rep.sweeps.size());
    std::printf("%-24s %-9s %9s %-22s %9s %-22s %9s %8s %8s\n", "sweep", "target", "recorded", "verdict",
                "today", "verdict", "settled", "err was", "err now");
    int better = 0, worse = 0, scored = 0;
    for (const auto& r : rep.sweeps) {
        char settled[16] = "-", ew[16] = "-", en[16] = "-";
        if (r.settledKnown) {
            std::snprintf(settled, sizeof(settled), "%d", r.settled);
            std::snprintf(ew, sizeof(ew), "%d", r.errRecorded);
            std::snprintf(en, sizeof(en), "%d", r.errReplayed);
            ++scored;
            if (r.errReplayed < r.errRecorded) ++better;
            if (r.errReplayed > r.errRecorded) ++worse;
        }
        std::printf("%-24s %-9s %9d %-22s %9d %-22s %9s %8s %8s\n", r.id.c_str(), r.target.c_str(),
                    r.recorded, r.recordedVerdict.c_str(), r.replayed, r.replayedVerdict.c_str(), settled, ew, en);
    }
    const auto reg = Regressions(rep);
    std::printf("\n%d sweep(s) with a settled position: today's rules nearer on %d, further on %d.\n",
                scored, better, worse);
    if (!reg.empty()) {
        std::printf("REGRESSION: %zu sweep(s) end more than 50 steps further from where the owner settled:\n", reg.size());
        for (const auto& r : reg) std::printf("  %s: %d -> %d steps\n", r.id.c_str(), r.errRecorded, r.errReplayed);
        return 1;
    }
    std::printf("No regression.\n");
    return 0;
}
