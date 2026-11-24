#pragma once
// ============================================================
// StatusFit.h -- which status-bar fields fit, by priority
// ============================================================
//
// 2026-11-24, the owner: adding the clock and Fahrenheit made the status
// bar's fields run into each other. Every field had a fixed width guessed
// for its text; the EAF field ("EAF: 186211  16.0°C 60.8°F") grew past its
// slot and drew over the mount's RA. Now each field is measured, and when
// the bar is too narrow for all of them the least important go first,
// whole, rather than any field drawing over another.
//
// Pure: widths in, which to keep out.
// ============================================================

#include <cstddef>
#include <vector>

namespace raduk::core {

// widths: each field's measured width. priority: 1 is kept longest;
// larger numbers are dropped first (ties: the later field first). gap:
// the space between two kept fields. Returns which to keep.
inline std::vector<bool> FitByPriority(const std::vector<float>& widths, const std::vector<int>& priority,
                                       float gap, float avail) {
    const size_t n = widths.size();
    std::vector<bool> keep(n, true);
    auto total = [&]() {
        float t = 0.0f; int k = 0;
        for (size_t i = 0; i < n; ++i) if (keep[i]) { t += widths[i]; ++k; }
        return t + (k > 1 ? gap * (float)(k - 1) : 0.0f);
    };
    while (total() > avail) {
        int drop = -1;
        for (size_t i = 0; i < n; ++i) {
            if (!keep[i]) continue;
            if (drop < 0 || priority[i] >= priority[(size_t)drop]) drop = (int)i;
        }
        if (drop < 0) break;
        keep[(size_t)drop] = false;
    }
    return keep;
}

} // namespace raduk::core
