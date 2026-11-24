// focus_panel_text: the FOCUSER panel's words (2026-11-24 redesign).
#include "core/FocusPanelText.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // The row is in order of size, symmetric round STOP.
    for (int i = 0; i < 8; ++i) CHECK(kFocusPanelSteps[i] < kFocusPanelSteps[i + 1]);
    for (int i = 0; i < 4; ++i) CHECK(kFocusPanelSteps[i] == -kFocusPanelSteps[8 - i]);
    CHECK(FocusStepLabel(-5000) == L"−5000" && FocusStepLabel(10) == L"+10" &&
          FocusStepLabel(0) == L"STOP");

    CHECK(FocusStateLine(false, true, false) == L"NO FOCUSER");
    CHECK(FocusStateLine(true, false, false).find(L"NOT TRUSTED") != std::wstring::npos);
    CHECK(FocusStateLine(true, true, true) == L"MOVING");
    CHECK(FocusStateLine(true, true, false) == L"AT REST");

    std::wstring l = FocusLastLine(-10, 170933, true, "");
    CHECK(l == L"Last: −10 from 170933 to 170923");
    l = FocusLastLine(1000, 170000, true, "", 175000);
    CHECK(l.find(L"stopped the move to 175000 first") != std::wstring::npos);
    l = FocusLastLine(5000, 179000, false, "beyond the travel guard");
    CHECK(l == L"Last: +5000 did not move: beyond the travel guard");
    CHECK(FocusLastLine(10, -1, true, "") == L"Last: +10 sent");

    l = FocusGotoLine(170923, 183154, true, "");
    std::printf("  %ls\n", l.c_str());
    CHECK(l == L"Last: GO TO 170923 from 183154 (-12231)");
    CHECK(FocusGotoLine(5, 100, false, "untrusted").find(L"did not move: untrusted") != std::wstring::npos);

    std::printf("focus_panel_text: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
