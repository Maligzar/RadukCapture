#pragma once
// ============================================================
// ScreenRotation.h -- full-screen windows move round their screens
// ============================================================
//
// The owner, 2026-11-22: "To prevent screen burn the 4 display monitors
// should rotate their full screen windows from monitor to monitor every
// 15 minutes." A sky map or status board held still for a night leaves
// its outline on a panel; moving each one to the next screen in turn
// spreads the wear across all of them.
//
// What takes part:
//   - every window that fills a screen (its whole area or its work
//     area, or maximised there), except those the caller excludes: the
//     main window with the live preview, which presents through the
//     adapter of its own screen and is not moved;
//   - the screens holding such windows, and only those. A screen with
//     ordinary panels and no full-screen window is never covered by a
//     window rotated onto it.
// Screens are taken left to right (then top to bottom), and every
// full-screen window on screen i moves to screen i+1, the last to the
// first. A window keeps how it filled its screen: whole area, work area
// or maximised.
//
// Pure: the caller enumerates monitors and windows and applies the moves.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace raduk::core::rotation {

struct Rect { long l = 0, t = 0, r = 0, b = 0; };

struct Screen {
    Rect full, work;
    bool primary = false;
};

struct Win {
    intptr_t id = 0;          // the caller's handle
    Rect rect;
    bool zoomed = false;      // maximised
    bool visible = true;
    bool excluded = false;    // never moved (the main window)
};

enum class Fill { None, Full, Work, Maximized };

// How a window fills a screen. `slack` absorbs the borders Windows adds
// round a window placed edge to edge.
inline bool Covers(const Rect& w, const Rect& s, long slack) {
    return std::labs(w.l - s.l) <= slack && std::labs(w.t - s.t) <= slack &&
           std::labs(w.r - s.r) <= slack && std::labs(w.b - s.b) <= slack;
}

inline Fill FillOf(const Win& w, const Screen& s, long slack = 10) {
    if (w.zoomed) {
        // A maximised window's rect overhangs the work area by its border.
        const long cx = (w.rect.l + w.rect.r) / 2, cy = (w.rect.t + w.rect.b) / 2;
        if (cx >= s.full.l && cx < s.full.r && cy >= s.full.t && cy < s.full.b) return Fill::Maximized;
        return Fill::None;
    }
    if (Covers(w.rect, s.full, slack)) return Fill::Full;
    if (Covers(w.rect, s.work, slack)) return Fill::Work;
    return Fill::None;
}

struct Move {
    intptr_t id = 0;
    int from = -1, to = -1;   // indexes into the screens given
    Fill fill = Fill::None;
    Rect target;              // where it goes (for Maximized: the new screen's work area)
};

// The moves for one turn. Fewer than two screens with a full-screen
// window: nothing to do.
inline std::vector<Move> PlanRotation(const std::vector<Screen>& screens, const std::vector<Win>& wins) {
    struct Found { size_t win; int screen; Fill fill; };
    std::vector<Found> found;
    for (size_t i = 0; i < wins.size(); ++i) {
        const Win& w = wins[i];
        if (!w.visible || w.excluded) continue;
        for (int s = 0; s < (int)screens.size(); ++s) {
            const Fill f = FillOf(w, screens[s]);
            if (f != Fill::None) { found.push_back({i, s, f}); break; }
        }
    }
    // The screens that take part, left to right, then top to bottom.
    std::vector<int> ring;
    for (const auto& f : found)
        if (std::find(ring.begin(), ring.end(), f.screen) == ring.end()) ring.push_back(f.screen);
    if (ring.size() < 2) return {};
    std::sort(ring.begin(), ring.end(), [&](int a, int b) {
        const Rect& ra = screens[a].full; const Rect& rb = screens[b].full;
        return ra.l != rb.l ? ra.l < rb.l : ra.t < rb.t;
    });
    std::vector<Move> out;
    for (const auto& f : found) {
        const size_t at = (size_t)(std::find(ring.begin(), ring.end(), f.screen) - ring.begin());
        const int to = ring[(at + 1) % ring.size()];
        Move m;
        m.id = wins[f.win].id; m.from = f.screen; m.to = to; m.fill = f.fill;
        m.target = f.fill == Fill::Full ? screens[to].full : screens[to].work;
        out.push_back(m);
    }
    return out;
}

// When the next turn is due. Off when minutes <= 0. A turn is put off,
// not skipped, while the owner is dragging something (`busy`).
class RotationTimer {
public:
    void Configure(int minutes) { m_periodSec = minutes > 0 ? (int64_t)minutes * 60 : 0; }
    bool Enabled() const { return m_periodSec > 0; }
    // Called on each tick with seconds on any steady clock.
    bool Due(int64_t nowSec, bool busy) {
        if (!Enabled()) return false;
        if (m_lastSec < 0) { m_lastSec = nowSec; return false; }   // the first period starts now
        if (nowSec - m_lastSec < m_periodSec || busy) return false;
        m_lastSec = nowSec;
        return true;
    }
    void Restart(int64_t nowSec) { m_lastSec = nowSec; }   // after a layout change or a manual turn
private:
    int64_t m_periodSec = 0;
    int64_t m_lastSec = -1;
};

} // namespace raduk::core::rotation
