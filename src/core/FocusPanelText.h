#pragma once
// ============================================================
// FocusPanelText.h -- what the FOCUSER panel says
// ============================================================
//
// The owner, 2026-11-24: "this panel of the focuser is inconsistent. The
// buttons are not all doing what they are supposed to do. Let's improve
// this so it's more obvious what is going on." It had two rows of steps
// mixed by size (5000 and 100 on one row, 1000 and 10 on the other),
// labelled with chevrons rather than numbers, a box in the middle that
// looked like the position but was GO TO's target (and copied the
// position whenever it lost focus, so GO TO could send a number nobody
// typed), and nothing saying what a press had done.
//
// The panel now has: the position, a state line (AT REST, MOVING, NOT
// TRUSTED), one row of steps in order with their numbers, a MOVE TO box
// beside its own GO TO, and a "Last:" line saying what the last press did
// or why it did not move. This file holds the words, so they are tested.
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <string>

namespace raduk::core {

// The nine buttons, in panel order; 0 is STOP.
inline constexpr int kFocusPanelSteps[9] = {-5000, -1000, -100, -10, 0, 10, 100, 1000, 5000};

inline std::wstring FocusStepLabel(int steps) {
    if (steps == 0) return L"STOP";
    wchar_t b[24];
    std::swprintf(b, 24, L"%ls%d", steps < 0 ? L"−" : L"+", std::abs(steps));
    return b;
}

inline std::wstring FocusStateLine(bool open, bool trusted, bool moving) {
    if (!open)    return L"NO FOCUSER";
    if (!trusted) return L"POSITION NOT TRUSTED: confirm it before moving";
    return moving ? L"MOVING" : L"AT REST";
}

// What the last press did. `from` -1 when the position could not be read;
// `replaced` >= 0 when it stopped a move in flight that was going there.
inline std::wstring FocusLastLine(int steps, int from, bool moved, const std::string& refusal,
                                  int replaced = -1) {
    wchar_t b[200];
    const std::wstring step = FocusStepLabel(steps);
    if (!moved) {
        std::wstring why(refusal.begin(), refusal.end());
        std::swprintf(b, 200, L"Last: %ls did not move%ls%ls", step.c_str(),
                      why.empty() ? L"" : L": ", why.c_str());
        return b;
    }
    if (from < 0)
        std::swprintf(b, 200, L"Last: %ls sent", step.c_str());
    else
        std::swprintf(b, 200, L"Last: %ls from %d to %d", step.c_str(), from, from + steps);
    std::wstring s = b;
    if (replaced >= 0) {
        std::swprintf(b, 200, L" (stopped the move to %d first)", replaced);
        s += b;
    }
    return s;
}

inline std::wstring FocusGotoLine(int target, int from, bool moved, const std::string& refusal) {
    wchar_t b[200];
    if (!moved) {
        std::wstring why(refusal.begin(), refusal.end());
        std::swprintf(b, 200, L"Last: GO TO %d did not move%ls%ls", target,
                      why.empty() ? L"" : L": ", why.c_str());
    } else if (from < 0) {
        std::swprintf(b, 200, L"Last: GO TO %d sent", target);
    } else {
        std::swprintf(b, 200, L"Last: GO TO %d from %d (%+d)", target, from, target - from);
    }
    return b;
}

} // namespace raduk::core
