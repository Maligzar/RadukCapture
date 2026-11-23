// seeing_display: Phase 172 finding 3. SEEING read 9.4-9.7/10 in green
// through a blurred disc (2026-11-22). The score is relative, so a steady
// blur reads near 10. While the focus readout says SOFT or OUT, the line
// says so and is never green.
#include "core/SeeingDisplay.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // The night: 9.6, focus OUT. Not green, and it says why.
    SeeingText t = SeeingDisplay(true, 9.6f, FocusState::Out);
    CHECK(t.tone == SeeingTone::Caution);
    CHECK(t.text.find(L"OUT") != std::wstring::npos && t.text.find(L"9.6") != std::wstring::npos);
    t = SeeingDisplay(true, 9.6f, FocusState::Soft);
    CHECK(t.tone == SeeingTone::Caution && t.text.find(L"soft") != std::wstring::npos);
    // In focus, or no reference yet: as before.
    t = SeeingDisplay(true, 9.6f, FocusState::Focused);
    CHECK(t.tone == SeeingTone::Good && t.text == L"SEEING  9.6/10");
    CHECK(SeeingDisplay(true, 9.6f, FocusState::NoReference).tone == SeeingTone::Good);
    CHECK(SeeingDisplay(true, 9.6f, FocusState::Measuring).tone == SeeingTone::Good);
    CHECK(SeeingDisplay(true, 5.0f, FocusState::Focused).tone == SeeingTone::Caution);
    CHECK(SeeingDisplay(true, 2.0f, FocusState::Focused).tone == SeeingTone::Bad);
    // A bad score stays bad-coloured... unless focus is off, which outranks it.
    CHECK(SeeingDisplay(true, 2.0f, FocusState::Out).tone == SeeingTone::Caution);
    // No data.
    t = SeeingDisplay(false, 9.6f, FocusState::Out);
    CHECK(t.tone == SeeingTone::Dim && t.text == L"SEEING  --");
    std::printf("seeing_display: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
