#pragma once
// ============================================================
//  AutofocusFeed -- Phase 153.0: what the live stream gives autofocus
//
//  Moved out of main.cpp's render loop unchanged in behaviour (it was
//  main.cpp:7114-7138 and :7259-7263). Two feeds:
//    - Sharpness: every fresh analysis with a measured half-flux
//      diameter, as the coarse sweep's score (1000/HFD).
//    - FastScore: during the fine pass (phase 145.2), the newest raw
//      frame scored on the edge metric, at most every 33 ms, over a
//      region whose size is fixed for the run because the score is
//      normalised by it.
//
//  A template over the routine, so a test can drive it with a fake and
//  the app with AutofocusRoutine. The frame and the analysis come in as
//  plain views, so nothing here needs Windows, the ring or the analyzer.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "../core/FocusMetric.h"
#include "Clock.h"
#include "UiSink.h"

namespace raduk::app {

struct FrameView {
    const uint8_t* data = nullptr;
    uint32_t width = 0, height = 0;
    uint8_t  bitDepth = 8;
    bool     bayerRaw = false;
    uint64_t timeUs = 0;        // phase 161.1: when the frame was taken
};

struct AnalysisView {
    bool  valid = false;
    bool  centroidValid = false;
    float centroidX = 0.0f, centroidY = 0.0f;
    float centroidBgDN = 0.0f;
    uint8_t centroidPeak = 0;
    bool  surfaceTracked = false;
    bool  focusValid = false;
    float focusScore = 0.0f;
    float focusHfdPx = 0.0f;
    uint64_t frameTimeUs = 0;   // phase 161.1: the frame it was measured on
};

class AutofocusFeed {
public:
    static constexpr double kFastIntervalSec = 0.033;

    // The fine pass's edge region, half its side in pixels: the whole
    // surface when the target fills the frame, else sized from the disc
    // (a uniform disc's half-flux diameter is sqrt(2) x its radius).
    static int EdgeRegionHalf(bool filled, float hfdPx) {
        if (filled) return 128;
        const float r = hfdPx / 1.41421356f;
        return (std::max)(48, (std::min)(200, (int)(r * 1.3f) + 24));
    }

    template <class Routine>
    void FastScore(Routine& af, const FrameView& f, const AnalysisView& a,
                   const IClock& clock, IUiSink* ui) {
        if (!af.IsRunning()) { m_edgeHalf = -1; m_saidWaiting = false; }
        if (!(f.data && af.WantsFastScores() && f.bayerRaw)) return;
        const double now = clock.Now();
        if (now - m_lastFast < kFastIntervalSec) return;
        m_lastFast = now;
        const bool filled = af.FineOnly() || (a.valid && a.surfaceTracked);
        if (m_edgeHalf <= 0) {
            // Phase 161.5b: sized from the target IN FOCUS. The routine
            // knows that size from the coarse sweep's best point; the live
            // analysis at this moment is of the fine pass's first point,
            // which is out of focus, or of nothing at all. With no size
            // the region used to fall to its 48 px floor, which on Jupiter
            // (220 px across) sits inside the disc with no limb in it, and
            // the pass ran on that without a word. Now it waits: no score
            // is sent until a size is known, and a pass that never gets
            // one ends as "too few frames" and keeps the coarse result.
            float hfd = af.FineRegionHfdPx();
            if (!(hfd > 0.0f)) hfd = a.valid ? a.focusHfdPx : 0.0f;
            if (!filled && !(hfd > 0.0f)) {
                if (ui && !m_saidWaiting) {
                    m_saidWaiting = true;
                    ui->Line("[AF] fine pass region: waiting for the target's size (no half-flux diameter yet)\n");
                }
                return;
            }
            m_saidWaiting = false;
            m_edgeHalf = EdgeRegionHalf(filled, hfd);
            if (ui) {
                char b[128];
                std::snprintf(b, sizeof(b), "[AF] fine pass region: %d px each side of the target%s%s\n",
                              m_edgeHalf, filled ? " (the surface)" : "",
                              m_channel == raduk::core::EdgeChannel::Luminance ? "" : ", green sites only");
                ui->Line(b);
            }
        }
        float ecx = f.width * 0.5f, ecy = f.height * 0.5f;
        if (a.valid && a.centroidValid) { ecx = a.centroidX; ecy = a.centroidY; }
        const float bg = filled ? 0.0f : a.centroidBgDN;
        const raduk::core::EdgeFocus ef = raduk::core::MeasureEdgeFocus(
            f.data, f.width, f.height, f.bitDepth, ecx, ecy, m_edgeHalf, bg, m_scratch,
            raduk::core::kEdgeGateK, m_channel);
        if (ef.valid) af.SubmitFastScore(ef.score, f.timeUs);
    }

    template <class Routine>
    static void Sharpness(Routine& af, const AnalysisView& a, bool fresh) {
        if (fresh && a.valid && af.IsRunning() && a.focusValid)
            af.SubmitSharpness(a.focusScore, a.centroidValid, (int)a.centroidPeak, a.frameTimeUs);
    }

    int EdgeHalf() const { return m_edgeHalf; }

    // Phase 161.5b: which sites the edge metric scores (FocusMetric.h).
    void SetChannel(raduk::core::EdgeChannel c) { m_channel = c; }
    raduk::core::EdgeChannel Channel() const { return m_channel; }

private:
    int    m_edgeHalf = -1;
    bool   m_saidWaiting = false;
    raduk::core::EdgeChannel m_channel = raduk::core::EdgeChannel::Luminance;
    double m_lastFast = -1e18;
    raduk::core::EdgeScratch m_scratch;
};

} // namespace raduk::app
