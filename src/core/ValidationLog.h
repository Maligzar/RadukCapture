#pragma once
// ============================================================
//  ValidationLog -- phase 155.4: a night's evidence, as data
//
//  Twenty-nine constants in src/ carry an unverified tag, listed in docs/UNVERIFIED.md:
//  each was tuned on synthetic frames, derived without measuring, or taken
//  from a datasheet, and each names the clear-night item that would settle
//  it. Until now an item was closed by watching a night and judging. This
//  records, through every real night, what those items need, so a report
//  over several nights (tools/validation_report.py) can settle a value on
//  evidence, with a confidence interval, and refuse to when there is too
//  little:
//
//    plate scale   every solve's measured scale against the configured
//                  one (items 3 and 15; 155.3 acts on it)
//    sharpness     the distribution of real-frame sharpness, by target,
//                  exposure and gain, including the share at the 100% cap
//                  (item 1: SHARP_CEIL should sit just above the best)
//    seeing        the normalised seeing figure through the night, by
//                  target, so the report can say how Autorun's bar (a
//                  percentage of the night's best, item 37) would have
//                  fallen
//    focus         every autofocus result: where it started and ended, the
//                  gain, the backlash it measured against the one in use
//                  (items 47 and 58); the fine pass's own figures follow
//                  in 155.4b
//    exposure      the target's highlight and clipped share against the
//                  exposure, beside the profile's band (item 55)
//
//  One file per run of the app, validation_<night>_<hhmmss>.json in the
//  logs folder; the report joins them. "Night" is the local date the
//  evening began: before noon counts as the previous date.
//
//  Pure: no windows.h, no file I/O (the caller writes the text), and the
//  times are passed in. tests/validation_log.cpp gates it.
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace raduk::core::validation {

// Sharpness is 0-100 (FrameAnalyzer's sharpnessPct, capped at 100): one
// bin a percent, and bin 100 holds the frames AT the cap.
inline constexpr int kSharpBins = 101;
// Seeing and exposure samples are thinned to one a second per target, and
// capped, so a long night stays a few hundred kilobytes.
inline constexpr int64_t kSampleEveryMs = 1000;
inline constexpr size_t  kMaxSamplesPerTarget = 30000;
inline constexpr size_t  kMaxSolves = 2000;
inline constexpr size_t  kMaxFocusRuns = 500;

// The night a local time belongs to: "YYYYMMDD", the previous date before noon.
inline std::string NightOf(int y, int mo, int d, int hour) {
    if (hour < 12) {
        // Step back a day, by hand: no time library.
        static const int dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        d -= 1;
        if (d < 1) {
            mo -= 1;
            if (mo < 1) { mo = 12; y -= 1; }
            const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
            d = dim[mo - 1] + (mo == 2 && leap ? 1 : 0);
        }
    }
    char b[16];
    std::snprintf(b, sizeof(b), "%04d%02d%02d", y, mo, d);
    return b;
}

struct Solve {
    int64_t unixSec = 0;
    double  measured = 0, configured = 0;   // arcsec per pixel, as the frame was
    int     frameW = 0, frameH = 0, bin = 1;
    std::string target;
};

struct FocusRun {
    int64_t unixSec = 0;
    std::string target;
    bool    success = false, fineOnly = false;
    int     start = 0, best = 0;
    double  bestSharp = 0, improvement = 0;
    int     backlashMeasured = 0, backlashInUse = 0;
    std::string message;
};

struct SeeingSample   { int64_t ms; double v; };
struct ExposureSample { int64_t ms; double expMs, gain, highlightDN, clipPct, framePct; };

struct TargetData {
    // Sharpness histograms, keyed "exposure_ms|gain" (exposure to 0.01 ms).
    std::map<std::string, std::vector<uint32_t>> sharp;
    std::vector<SeeingSample>   seeing;
    std::vector<ExposureSample> exposure;
    double bandFloorMs = 0, bandCeilMs = 0;   // the profile's band, 0 = none
    int64_t lastSeeingMs = -1, lastExposureMs = -1;
    uint64_t framesSeen = 0;
};

// One frame's analysis, as the recorder needs it.
struct Frame {
    int64_t ms = 0;                 // a steady clock, milliseconds
    std::string target;
    double  sharpnessPct = 0;       // 0-100, capped
    double  highlightDN = 0;        // the whole frame's 99.9th percentile
    double  normalisedSeeing = 0;   // SeeingMonitor::Normalised; 0 = no disc
    double  targetHighlightDN = 0, targetClipPct = 0, targetFramePct = 0;
    double  exposureMs = 0, gain = 0;
    double  bandFloorMs = 0, bandCeilMs = 0;
};

class Recorder {
public:
    void Begin(const std::string& night, const std::string& appVersion, int64_t startUnix) {
        *this = Recorder{};
        m_night = night;
        m_version = appVersion;
        m_startUnix = startUnix;
    }

    // A frame measured on a target. The caller passes only frames worth
    // having: a valid analysis, not during an autofocus sweep or a slew.
    void AddFrame(const Frame& f) {
        if (f.target.empty() || f.highlightDN <= 0) return;
        TargetData& t = m_targets[f.target];
        t.framesSeen++;
        if (f.bandFloorMs > 0 && f.bandCeilMs > f.bandFloorMs) {
            t.bandFloorMs = f.bandFloorMs;
            t.bandCeilMs = f.bandCeilMs;
        }
        // Sharpness, every frame.
        char key[48];
        std::snprintf(key, sizeof(key), "%.2f|%.0f", f.exposureMs, f.gain);
        auto& h = t.sharp[key];
        if (h.empty()) h.assign(kSharpBins, 0);
        const int bin = (int)std::lround(std::clamp(f.sharpnessPct, 0.0, 100.0));
        h[bin]++;
        // Seeing and exposure, once a second.
        if (f.normalisedSeeing > 0 && t.seeing.size() < kMaxSamplesPerTarget &&
            (t.lastSeeingMs < 0 || f.ms - t.lastSeeingMs >= kSampleEveryMs)) {
            t.seeing.push_back({f.ms, f.normalisedSeeing});
            t.lastSeeingMs = f.ms;
        }
        if (f.targetHighlightDN > 0 && t.exposure.size() < kMaxSamplesPerTarget &&
            (t.lastExposureMs < 0 || f.ms - t.lastExposureMs >= kSampleEveryMs)) {
            t.exposure.push_back({f.ms, f.exposureMs, f.gain, f.targetHighlightDN,
                                  f.targetClipPct, f.targetFramePct});
            t.lastExposureMs = f.ms;
        }
        m_dirty = true;
    }

    void AddSolve(const Solve& s) {
        if (!(s.measured > 0.01 && s.measured < 50) || m_solves.size() >= kMaxSolves) return;
        m_solves.push_back(s);
        m_dirty = true;
    }

    void AddFocus(const FocusRun& r) {
        if (m_focus.size() >= kMaxFocusRuns) return;
        m_focus.push_back(r);
        m_dirty = true;
    }

    bool Dirty() const { return m_dirty; }
    void MarkWritten() { m_dirty = false; }
    bool Empty() const { return m_targets.empty() && m_solves.empty() && m_focus.empty(); }
    const std::string& Night() const { return m_night; }
    const std::map<std::string, TargetData>& Targets() const { return m_targets; }
    const std::vector<Solve>& Solves() const { return m_solves; }
    const std::vector<FocusRun>& Focus() const { return m_focus; }

    // The file, as JSON. Seeing and exposure times are seconds from the
    // first sample of the night's run, to three decimals.
    std::string ToJson() const {
        std::string j = "{\n";
        j += "\"format\":\"raduk-validation\",\"version\":1,";
        j += "\"night\":" + Str(m_night) + ",\"app\":" + Str(m_version) +
             ",\"start_unix\":" + std::to_string(m_startUnix) + ",\n";
        j += "\"solves\":[";
        for (size_t i = 0; i < m_solves.size(); i++) {
            const Solve& s = m_solves[i];
            j += (i ? ",\n " : "\n ");
            j += "{\"t\":" + std::to_string(s.unixSec) + ",\"measured\":" + Num(s.measured, 5) +
                 ",\"configured\":" + Num(s.configured, 5) + ",\"w\":" + std::to_string(s.frameW) +
                 ",\"h\":" + std::to_string(s.frameH) + ",\"bin\":" + std::to_string(s.bin) +
                 ",\"target\":" + Str(s.target) + "}";
        }
        j += "],\n\"focus\":[";
        for (size_t i = 0; i < m_focus.size(); i++) {
            const FocusRun& r = m_focus[i];
            j += (i ? ",\n " : "\n ");
            j += "{\"t\":" + std::to_string(r.unixSec) + ",\"target\":" + Str(r.target) +
                 ",\"success\":" + (r.success ? "true" : "false") +
                 ",\"fine_only\":" + (r.fineOnly ? "true" : "false") +
                 ",\"start\":" + std::to_string(r.start) + ",\"best\":" + std::to_string(r.best) +
                 ",\"best_sharp\":" + Num(r.bestSharp, 4) + ",\"improvement\":" + Num(r.improvement, 4) +
                 ",\"backlash_measured\":" + std::to_string(r.backlashMeasured) +
                 ",\"backlash_in_use\":" + std::to_string(r.backlashInUse) +
                 ",\"message\":" + Str(r.message) + "}";
        }
        j += "],\n\"targets\":{";
        bool firstT = true;
        for (const auto& [name, t] : m_targets) {
            j += firstT ? "\n " : ",\n ";
            firstT = false;
            j += Str(name) + ":{\"frames\":" + std::to_string(t.framesSeen) +
                 ",\"band_floor_ms\":" + Num(t.bandFloorMs, 3) + ",\"band_ceil_ms\":" + Num(t.bandCeilMs, 3) +
                 ",\n  \"sharpness\":{";
            bool firstH = true;
            for (const auto& [key, h] : t.sharp) {
                j += firstH ? "" : ",";
                firstH = false;
                j += Str(key) + ":[";
                for (int b = 0; b < kSharpBins; b++) j += (b ? "," : "") + std::to_string(h[b]);
                j += "]";
            }
            j += "},\n  \"seeing\":[";
            const int64_t t0 = t.seeing.empty() ? 0 : t.seeing.front().ms;
            for (size_t i = 0; i < t.seeing.size(); i++)
                j += (i ? "," : "") + std::string("[") + Num((t.seeing[i].ms - t0) / 1000.0, 3) + "," +
                     Num(t.seeing[i].v, 3) + "]";
            j += "],\n  \"exposure\":[";
            const int64_t e0 = t.exposure.empty() ? 0 : t.exposure.front().ms;
            for (size_t i = 0; i < t.exposure.size(); i++) {
                const ExposureSample& e = t.exposure[i];
                j += (i ? "," : "") + std::string("[") + Num((e.ms - e0) / 1000.0, 3) + "," +
                     Num(e.expMs, 3) + "," + Num(e.gain, 0) + "," + Num(e.highlightDN, 1) + "," +
                     Num(e.clipPct, 3) + "," + Num(e.framePct, 2) + "]";
            }
            j += "]}";
        }
        j += "\n}\n}\n";
        return j;
    }

private:
    static std::string Num(double v, int prec) {
        if (!std::isfinite(v)) return "null";
        char b[48];
        std::snprintf(b, sizeof(b), "%.*f", prec, v);
        return b;
    }
    static std::string Str(const std::string& s) {
        std::string o = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
            else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
            else o += (char)c;
        }
        return o + "\"";
    }

    std::string m_night, m_version;
    int64_t     m_startUnix = 0;
    std::map<std::string, TargetData> m_targets;
    std::vector<Solve>    m_solves;
    std::vector<FocusRun> m_focus;
    bool m_dirty = false;
};

} // namespace raduk::core::validation
