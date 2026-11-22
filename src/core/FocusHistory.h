#pragma once
// ============================================================
// FocusHistory.h -- every autofocus sweep, and what came after, as data
// ============================================================
//
// Phase 174.0. The owner, 2026-11-22, imaging through cloud: "a big
// learning opportunity for my autofocus ... capture the meta data in a way
// that I can build a history of focusing so that my autofocuser starts to
// learn."
//
// One JSON object per line in <logs>\focus_history.jsonl, two kinds:
//   "sweep"    : the whole sweep (the coarse curve out and back with the
//                planet's peak at each step, the fine pass, the sweep check,
//                the same-sky A/B), the verdict, and the conditions
//                (target, filter, exposure, gain, frame, temperatures,
//                altitude, seeing).
//   "followup" : where the owner took the focuser by hand in the minutes
//                after a sweep, and the sharpness each position read. The
//                last rest is the ground truth a sweep is judged against
//                (2026-11-22: autofocus left 165,984; the owner settled on
//                168,893).
//
// Append-only and self-describing, so tools (Phase 174.1-174.3: a
// temperature model, replay, an MCP server for an LLM to read) can join
// nights without the app. Pure: the caller writes the line.
// ============================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace raduk::core::focushist {

inline constexpr int kFormat = 1;

// What was true when the sweep ran. NaN / empty = not known.
struct Conditions {
    std::string target, filter, profile;
    double exposureMs = NAN, gain = NAN;
    int width = 0, height = 0, bitDepth = 0;
    double focuserTempC = NAN, sensorTempC = NAN;
    double altDeg = NAN;
    double seeing = NAN;          // the app's 0-10 figure
    double sharpnessPct = NAN;    // the live readout just before the sweep
};

struct Sweep {
    int64_t unixSec = 0;
    std::string id;               // "<unix>-<start>", the followup's key
    std::string appVersion;
    Conditions at;
    bool success = false, fineOnly = false;
    int start = 0, result = 0;
    double startSharp = NAN, resultSharp = NAN, improvement = NAN;
    std::string message;
    int backlashMeasured = -1, backlashInUse = -1;
    // The coarse pass (the last one, if it swept again).
    std::vector<int> coarsePos;
    std::vector<double> coarseOut, coarseBack;
    std::vector<int> coarsePeakOut, coarsePeakBack;
    int coarsePeak = -1;
    double sweepRatio = NAN;      // out/back median disagreement
    // The fine pass.
    bool fineUsed = false;
    int finePeak = -1, finePasses = 0;
    std::vector<int> finePos;
    std::vector<double> fineScore, fineSpread;
    double fadeRatio = NAN;
    std::string fadeAction;
    // The same-sky A/B, when it ran.
    bool abRan = false;
    double abStartBefore = NAN, abStartNow = NAN, abFit = NAN, abEdgeStart = NAN, abEdgeFit = NAN;
    std::string abVerdict;
    bool skyChanged = false;
};

struct Rest { int64_t unixSec = 0; int pos = 0; double sharpnessPct = NAN; };

struct Followup {
    std::string sweepId;
    int64_t sweepUnix = 0;
    int sweepResult = 0;
    std::vector<Rest> rests;      // each position the focuser came to rest at, by hand
    std::string endedBy;          // "window" | "autofocus" | "app closing"
};

// ── JSON, by hand (no dependency; the format is small and fixed) ──────

inline std::string Esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
                else o += c;
        }
    }
    return o;
}
inline std::string Num(double v, int prec = 4) {
    if (!std::isfinite(v)) return "null";
    char b[40]; snprintf(b, sizeof(b), "%.*g", prec + 3, v);
    return b;
}
inline std::string Str(const std::string& s) { return "\"" + Esc(s) + "\""; }
template <class T> inline std::string Arr(const std::vector<T>& v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ","; o += Num((double)v[i]); }
    return o + "]";
}

inline std::string ConditionsJson(const Conditions& c) {
    return "{\"target\":" + Str(c.target) + ",\"filter\":" + Str(c.filter) + ",\"profile\":" + Str(c.profile) +
           ",\"exposure_ms\":" + Num(c.exposureMs) + ",\"gain\":" + Num(c.gain) +
           ",\"width\":" + std::to_string(c.width) + ",\"height\":" + std::to_string(c.height) +
           ",\"bit_depth\":" + std::to_string(c.bitDepth) +
           ",\"focuser_temp_c\":" + Num(c.focuserTempC) + ",\"sensor_temp_c\":" + Num(c.sensorTempC) +
           ",\"alt_deg\":" + Num(c.altDeg) + ",\"seeing\":" + Num(c.seeing) +
           ",\"sharpness_pct\":" + Num(c.sharpnessPct) + "}";
}

inline std::string SweepJson(const Sweep& s) {
    return "{\"kind\":\"sweep\",\"format\":" + std::to_string(kFormat) +
           ",\"id\":" + Str(s.id) + ",\"unix\":" + std::to_string(s.unixSec) +
           ",\"app\":" + Str(s.appVersion) + ",\"at\":" + ConditionsJson(s.at) +
           ",\"success\":" + (s.success ? "true" : "false") + ",\"fine_only\":" + (s.fineOnly ? "true" : "false") +
           ",\"start\":" + std::to_string(s.start) + ",\"result\":" + std::to_string(s.result) +
           ",\"start_sharp\":" + Num(s.startSharp) + ",\"result_sharp\":" + Num(s.resultSharp) +
           ",\"improvement\":" + Num(s.improvement) + ",\"message\":" + Str(s.message) +
           ",\"backlash_measured\":" + std::to_string(s.backlashMeasured) +
           ",\"backlash_in_use\":" + std::to_string(s.backlashInUse) +
           ",\"coarse\":{\"pos\":" + Arr(s.coarsePos) + ",\"out\":" + Arr(s.coarseOut) + ",\"back\":" + Arr(s.coarseBack) +
           ",\"peak_out\":" + Arr(s.coarsePeakOut) + ",\"peak_back\":" + Arr(s.coarsePeakBack) +
           ",\"peak\":" + std::to_string(s.coarsePeak) + ",\"out_back_ratio\":" + Num(s.sweepRatio) + "}" +
           ",\"fine\":{\"used\":" + (s.fineUsed ? "true" : "false") + ",\"peak\":" + std::to_string(s.finePeak) +
           ",\"passes\":" + std::to_string(s.finePasses) + ",\"pos\":" + Arr(s.finePos) +
           ",\"score\":" + Arr(s.fineScore) + ",\"spread\":" + Arr(s.fineSpread) +
           ",\"fade_ratio\":" + Num(s.fadeRatio) + ",\"fade_action\":" + Str(s.fadeAction) + "}" +
           ",\"ab\":{\"ran\":" + (s.abRan ? "true" : "false") + ",\"start_before\":" + Num(s.abStartBefore) +
           ",\"start_now\":" + Num(s.abStartNow) + ",\"fit\":" + Num(s.abFit) +
           ",\"edge_start\":" + Num(s.abEdgeStart) + ",\"edge_fit\":" + Num(s.abEdgeFit) +
           ",\"verdict\":" + Str(s.abVerdict) + "}" +
           ",\"sky_changed\":" + (s.skyChanged ? "true" : "false") + "}";
}

inline std::string FollowupJson(const Followup& f) {
    std::string r = "[";
    for (size_t i = 0; i < f.rests.size(); ++i) {
        if (i) r += ",";
        r += "{\"unix\":" + std::to_string(f.rests[i].unixSec) + ",\"pos\":" + std::to_string(f.rests[i].pos) +
             ",\"sharpness_pct\":" + Num(f.rests[i].sharpnessPct) + "}";
    }
    r += "]";
    const int settled = f.rests.empty() ? f.sweepResult : f.rests.back().pos;
    return "{\"kind\":\"followup\",\"format\":" + std::to_string(kFormat) + ",\"sweep\":" + Str(f.sweepId) +
           ",\"sweep_unix\":" + std::to_string(f.sweepUnix) + ",\"sweep_result\":" + std::to_string(f.sweepResult) +
           ",\"rests\":" + r + ",\"settled\":" + std::to_string(settled) +
           ",\"moved_from_result\":" + std::to_string(settled - f.sweepResult) +
           ",\"ended_by\":" + Str(f.endedBy) + "}";
}

// ── The followup: what the owner did with the focuser after a sweep ──
//
// Fed every readout tick. After a sweep ends it watches for kWindowSec:
// each time the focuser comes to rest (unmoved for kRestSec) somewhere new,
// that position and the sharpness then are kept. A new sweep, or the end
// of the window, closes it; a followup with no hand moves is still written
// (it says the owner left the result alone, which is a label too).
class FollowupTracker {
public:
    static constexpr int64_t kWindowSec = 15 * 60;
    static constexpr int64_t kRestSec = 5;

    void SweepEnded(const std::string& id, int64_t unixSec, int result) {
        m_open = true;
        m_f = Followup{};
        m_f.sweepId = id; m_f.sweepUnix = unixSec; m_f.sweepResult = result;
        m_lastPos = result; m_lastMoveUnix = unixSec; m_restRecorded = true;
    }

    // Returns true and fills `out` when a followup is complete.
    bool Tick(int64_t unixSec, bool afRunning, int pos, double sharpnessPct, Followup& out) {
        if (!m_open) return false;
        if (afRunning) return Close("autofocus", out);
        if (pos != m_lastPos) { m_lastPos = pos; m_lastMoveUnix = unixSec; m_restRecorded = false; }
        else if (!m_restRecorded && unixSec - m_lastMoveUnix >= kRestSec) {
            m_f.rests.push_back({unixSec, pos, sharpnessPct});
            m_restRecorded = true;
        } else if (m_restRecorded && !m_f.rests.empty() && m_f.rests.back().pos == pos &&
                   std::isfinite(sharpnessPct)) {
            // While it rests, keep the best reading there: the first one can
            // be a frame from mid-move.
            if (!std::isfinite(m_f.rests.back().sharpnessPct) || sharpnessPct > m_f.rests.back().sharpnessPct)
                m_f.rests.back().sharpnessPct = sharpnessPct;
        }
        if (unixSec - m_f.sweepUnix >= kWindowSec) return Close("window", out);
        return false;
    }
    bool Close(const char* why, Followup& out) {
        if (!m_open) return false;
        m_open = false;
        m_f.endedBy = why;
        out = m_f;
        return true;
    }
    bool Open() const { return m_open; }

private:
    bool m_open = false;
    Followup m_f;
    int m_lastPos = 0;
    int64_t m_lastMoveUnix = 0;
    bool m_restRecorded = true;
};

} // namespace raduk::core::focushist
