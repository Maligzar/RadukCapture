#pragma once
// ============================================================
// FocusVerdict.h -- is the target in focus? (2026-11-15)
//
// The sharpness figure under the histogram is a Laplacian variance
// scaled by SHARP_CEIL (core/Sharpness.h). Its size depends on the
// target, its brightness, the gain and the seeing as much as on the
// focus: 2% can be the best a dim star will ever read, and 40% a soft
// Jupiter. So the number alone cannot say "in focus", and the old
// labels (SHARP, PK, AV) left the reader to guess -- on the night it
// was asked about, PK 2% AV 2% sat beside a histogram that looked fine.
//
// What can say it is a reference taken at known-good focus: the live
// readings just after autofocus finished successfully on this target,
// when the focuser has been left at the fitted best. Live sharpness is
// then judged against that:
//
//     >= 85% of the reference  FOCUSED
//     >= 60%                   SOFT     (refocus worth trying)
//     below                    OUT      (refocus)
//
// The reference is the live figure, not autofocus's own reported score,
// because the Moon's path scores with the edge metric (phase 145.3) and
// that is a different scale. It is dropped when the target changes, since
// a reference from Jupiter says nothing about Vega. Gain and exposure
// changes move the figure too; the 60%/85% bands leave room for an AE
// that is hunting a little, and a large change of either is the
// operator's cue to run autofocus again anyway.
//
// Without a reference the verdict is NO REF and the readout says RUN AF:
// that is the honest answer, and the button that answers it is beside it.
//
// No Windows, no D2D: tests/focus_verdict.cpp drives it directly.
// ============================================================

#include <cstddef>

namespace raduk::core {

enum class FocusState { NoReference, Measuring, Focused, Soft, Out };

inline const char* FocusStateLabel(FocusState s) {
    switch (s) {
        case FocusState::Focused:   return "FOCUSED";
        case FocusState::Soft:      return "SOFT";
        case FocusState::Out:       return "OUT";
        case FocusState::Measuring: return "WAIT";
        default:                    return "NO REF";
    }
}

class FocusReference {
public:
    static constexpr int   kRefSamples   = 10;    // readings averaged into the reference
    static constexpr int   kNowSamples   = 8;     // readings averaged into "now"
    static constexpr float kFocusedRatio = 0.85f;
    static constexpr float kSoftRatio    = 0.60f;

    // The target changed: what was measured no longer applies.
    void Reset() { m_ref = 0.0f; m_armed = 0; m_sum = 0.0; m_nowN = 0; m_nowHead = 0; }

    // Autofocus has just finished successfully, focuser at the best fit.
    // The next kRefSamples live readings become the reference.
    void ArmAfterAutofocus() { m_ref = 0.0f; m_armed = kRefSamples; m_sum = 0.0; }

    // Every live sharpness reading (percent, as FrameAnalyzer reports it).
    void Submit(float pct) {
        if (!(pct >= 0.0f)) return;   // NaN or negative: not a reading
        m_now[m_nowHead] = pct;
        m_nowHead = (m_nowHead + 1) % kNowSamples;
        if (m_nowN < kNowSamples) ++m_nowN;
        if (m_armed > 0) {
            m_sum += pct;
            if (--m_armed == 0) m_ref = (float)(m_sum / kRefSamples);
        }
    }

    bool  HasReference() const { return m_ref > 0.0f; }
    bool  Measuring()    const { return m_armed > 0; }
    float Reference()    const { return m_ref; }
    // Mean of the last kNowSamples readings; 0 before any.
    float Now() const {
        if (m_nowN == 0) return 0.0f;
        double s = 0.0;
        for (int i = 0; i < m_nowN; ++i) s += m_now[i];
        return (float)(s / m_nowN);
    }
    // Now as a fraction of the reference; 0 without one.
    float Ratio() const { return m_ref > 0.0f ? Now() / m_ref : 0.0f; }

    FocusState State() const {
        if (m_armed > 0)    return FocusState::Measuring;
        if (m_ref <= 0.0f || m_nowN == 0) return FocusState::NoReference;
        const float r = Ratio();
        if (r >= kFocusedRatio) return FocusState::Focused;
        if (r >= kSoftRatio)    return FocusState::Soft;
        return FocusState::Out;
    }

private:
    float  m_ref = 0.0f;
    int    m_armed = 0;
    double m_sum = 0.0;
    float  m_now[kNowSamples] = {};
    int    m_nowN = 0;
    int    m_nowHead = 0;
};

} // namespace raduk::core
