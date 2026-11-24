#pragma once
// ============================================================
// RunsBaseline.h -- RUNS starts from what the owner just set up
// ============================================================
//
// The owner, 2026-11-24: "When I click Runs it means that the target is
// centered and in focus, so this is the baseline for RUNS to start from
// for every single adjustment from that point. This will anchor all of
// the telemetry."
//
// So pressing RUNS records the state of the rig at that moment: where the
// focuser is, how sharp the target reads, the focuser's temperature, the
// altitude, where the target sits in the frame, the exposure and gain.
//   - No sweep before the first run: focus is what the owner just made it.
//   - A refocus sweep is centred on the baseline, not wherever the
//     focuser happens to be, and a result that failed or lands further
//     than maxDrift steps from it goes back to the baseline. The same
//     night a sweep kept a "success" 11,000 steps from focus.
//   - Every run is logged against the baseline.
// The baseline never moves during the sequence: it is the anchor.
//
// Pure: the caller fills the snapshots and acts on the verdicts.
// ============================================================

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace raduk::core {

struct RunsSnapshot {
    int    focusPos     = -1;      // -1 unknown
    double sharpPct     = NAN;
    double focuserTempC = NAN;
    double altDeg       = NAN;
    bool   targetSeen   = false;
    double targetX      = 0, targetY = 0;   // px in the frame
    double exposureMs   = NAN;
    int    gain         = -1;
};

struct RunsBaseline {
    bool         set = false;
    std::string  target;
    RunsSnapshot at;
};

inline constexpr int kRunsFocusMaxDriftDefault = 2000;

inline std::string BaselineLine(const RunsBaseline& b) {
    char t[320];
    const RunsSnapshot& s = b.at;
    std::snprintf(t, sizeof(t),
                  "[RUNS] baseline%s%s: focus %d, sharpness %.1f%%, focuser %.1f C, altitude %.1f deg, "
                  "target %s, %.2f ms gain %d",
                  b.target.empty() ? "" : " for ", b.target.c_str(), s.focusPos, s.sharpPct,
                  s.focuserTempC, s.altDeg,
                  s.targetSeen ? (std::string("at (") + std::to_string((int)std::lround(s.targetX)) + ", " +
                                  std::to_string((int)std::lround(s.targetY)) + ") px").c_str()
                               : "not in the frame",
                  s.exposureMs, s.gain);
    return t;
}

// One run's state against the baseline, as one log line.
inline std::string DeltaLine(const RunsBaseline& b, const RunsSnapshot& n, int run) {
    const RunsSnapshot& s = b.at;
    std::string out = "[RUNS] run " + std::to_string(run) + " against the baseline:";
    char p[96];
    if (n.focusPos >= 0 && s.focusPos >= 0) {
        std::snprintf(p, sizeof(p), " focus %+d steps,", n.focusPos - s.focusPos); out += p;
    }
    if (std::isfinite(n.sharpPct) && std::isfinite(s.sharpPct) && s.sharpPct > 0) {
        std::snprintf(p, sizeof(p), " sharpness %.0f%% of it,", 100.0 * n.sharpPct / s.sharpPct); out += p;
    }
    if (std::isfinite(n.focuserTempC) && std::isfinite(s.focuserTempC)) {
        std::snprintf(p, sizeof(p), " focuser %+.1f C,", n.focuserTempC - s.focuserTempC); out += p;
    }
    if (std::isfinite(n.altDeg) && std::isfinite(s.altDeg)) {
        std::snprintf(p, sizeof(p), " altitude %+.1f deg,", n.altDeg - s.altDeg); out += p;
    }
    if (n.targetSeen && s.targetSeen) {
        std::snprintf(p, sizeof(p), " target %.0f px from its spot,",
                      std::hypot(n.targetX - s.targetX, n.targetY - s.targetY)); out += p;
    } else if (!n.targetSeen) {
        out += " target not in the frame,";
    }
    if (std::isfinite(n.exposureMs)) {
        std::snprintf(p, sizeof(p), " %.2f ms", n.exposureMs); out += p;
        if (std::isfinite(s.exposureMs) && std::fabs(n.exposureMs - s.exposureMs) > 0.005) {
            std::snprintf(p, sizeof(p), " (%+.2f)", n.exposureMs - s.exposureMs); out += p;
        }
    }
    if (n.gain >= 0) {
        std::snprintf(p, sizeof(p), " gain %d", n.gain); out += p;
        if (s.gain >= 0 && n.gain != s.gain) { std::snprintf(p, sizeof(p), " (%+d)", n.gain - s.gain); out += p; }
    }
    if (!out.empty() && out.back() == ',') out.pop_back();
    return out;
}

// A refocus sweep's result, judged against the baseline. The baseline is
// focus the owner confirmed by eye; a sweep may refine it, never replace it
// with something far away or failed.
struct RefocusVerdict {
    bool backToBaseline = false;
    std::string why;
};

inline RefocusVerdict JudgeRefocus(const RunsBaseline& b, bool sweepOk, int result,
                                   int maxDrift = kRunsFocusMaxDriftDefault) {
    RefocusVerdict v;
    if (!b.set || b.at.focusPos < 0) { v.why = "no baseline"; return v; }
    char t[200];
    if (!sweepOk) {
        v.backToBaseline = true;
        std::snprintf(t, sizeof(t), "the sweep failed: back to the baseline %d", b.at.focusPos);
    } else if (result < 0) {
        v.backToBaseline = true;
        std::snprintf(t, sizeof(t), "the sweep gave no position: back to the baseline %d", b.at.focusPos);
    } else if (std::abs(result - b.at.focusPos) > maxDrift) {
        v.backToBaseline = true;
        std::snprintf(t, sizeof(t), "the sweep ended %+d steps from the baseline, beyond %d: back to the baseline %d",
                      result - b.at.focusPos, maxDrift, b.at.focusPos);
    } else {
        std::snprintf(t, sizeof(t), "the sweep ended %+d steps from the baseline: kept", result - b.at.focusPos);
    }
    v.why = t;
    return v;
}

} // namespace raduk::core
