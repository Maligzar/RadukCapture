#pragma once
// ============================================================
// FlushStatus.h -- what the capture panel says while RAM drains to disk
// ============================================================
//
// The owner, 2026-11-24: "application froze while trying to image. The
// memory was almost full when I stopped the recording. If the memory is
// actively flushing when the recording is stopped we need some sort of
// indicator from that panel."
//
// STOP now returns at once and the writer drains on its own thread
// (CaptureManager::RequestStop). While it does, the panel's status line
// reads, for example:
//     FLUSHING  12,345 frames  37%  ~0:45 left
// The time left is from the rate achieved since STOP, withheld for the
// first second and until a frame has gone, as the REC line's estimate is.
//
// Pure: the caller gives the counts and the time since STOP.
// ============================================================

#include <cstdint>
#include <cwchar>
#include <string>

namespace raduk::core {

inline std::wstring Thousands(uint64_t n) {
    std::wstring d = std::to_wstring(n), out;
    int k = 0;
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        if (k && k % 3 == 0) out.insert(out.begin(), L',');
        out.insert(out.begin(), *it);
        ++k;
    }
    return out;
}

inline std::wstring FlushLine(uint64_t pending, uint64_t startPending, double sinceStopSec) {
    std::wstring s = L"FLUSHING  " + Thousands(pending) + L" frames";
    if (startPending > 0 && pending <= startPending) {
        const uint64_t done = startPending - pending;
        wchar_t b[64];
        std::swprintf(b, 64, L"  %d%%", (int)(100.0 * (double)done / (double)startPending));
        s += b;
        if (sinceStopSec > 1.0 && done > 0) {
            const double left = (double)pending * sinceStopSec / (double)done;
            std::swprintf(b, 64, L"  ~%d:%02d left", (int)(left / 60), (int)left % 60);
            s += b;
        }
    }
    return s;
}

} // namespace raduk::core
