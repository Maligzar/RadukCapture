#pragma once
// ============================================================
// SeeingDisplay.h -- what the SEEING line says, and in what colour
// ============================================================
//
// Phase 172 finding 3 (2026-11-22): SEEING read 9.4 to 9.7/10 on a hazy
// night, in green, through a blurred disc. The score is RELATIVE by design
// (FrameAnalyzer: each frame's sharpness against the best of the last
// ~10 s), which is what the capture gate needs. But a blurred disc barely
// changes from frame to frame, so every frame is close to the best and the
// score sits near 10. It measures steadiness against itself, not focus or
// the night.
//
// Phase 167's rule: the score is not to be trusted while the focus readout
// is well below its reference. So when that readout says SOFT or OUT, the
// line says so and is never green. The number stays (the gate still uses
// it); it just stops looking like good news.
//
// Pure, no Windows.
// ============================================================

#include <cstdio>
#include <string>

#include "FocusVerdict.h"

namespace raduk::core {

enum class SeeingTone { Dim, Good, Caution, Bad };

struct SeeingText {
    std::wstring text;
    SeeingTone   tone = SeeingTone::Dim;
};

inline SeeingText SeeingDisplay(bool valid, float score, FocusState focus) {
    SeeingText r;
    wchar_t b[64];
    if (!valid) { r.text = L"SEEING  --"; return r; }
    if (focus == FocusState::Out || focus == FocusState::Soft) {
        std::swprintf(b, 64, L"SEEING  %.1f  focus %ls", score,
                      focus == FocusState::Out ? L"OUT" : L"soft");
        r.text = b;
        r.tone = SeeingTone::Caution;
        return r;
    }
    std::swprintf(b, 64, L"SEEING  %.1f/10", score);
    r.text = b;
    r.tone = score >= 7.0f ? SeeingTone::Good : score >= 4.0f ? SeeingTone::Caution : SeeingTone::Bad;
    return r;
}

} // namespace raduk::core
