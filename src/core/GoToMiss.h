#pragma once
// ============================================================
// GoToMiss.h -- a GoTo that "completed" but put nothing in the frame
// ============================================================
//
// 2026-11-24 at the scope: the hand control reported itself aligned at
// startup, from an earlier setup, and the app took its word. Four GoTos
// (Saturn, Deneb, the Moon, Saturn) all logged "complete" while the tube
// pointed at the horizon. The mount cannot know its model is stale; the
// camera can. A bright target (a planet or the Moon) that is still not in
// the frame some seconds after a GoTo is the mark of a wrong alignment,
// and two in a row all but certain. This says so, once per GoTo.
//
// Pure: the caller feeds the GoTo's completion and, each tick, whether the
// analyser sees a target.
// ============================================================

#include <algorithm>
#include <cctype>
#include <string>

namespace raduk::core {

class GoToMissWatch {
public:
    static constexpr double kWaitSec = 20.0;   // acquisition starts at 3 s; give it time

    // Only targets that cannot be missed in the frame when the pointing is right.
    static bool IsBright(const std::string& name) {
        std::string n;
        for (char c : name) n += (char)std::tolower((unsigned char)c);
        static const char* kBright[] = {"moon", "mercury", "venus", "mars", "jupiter",
                                        "saturn", "uranus", "neptune"};
        for (const char* b : kBright) if (n == b) return true;
        return false;
    }

    void GoToComplete(const std::string& target, double nowSec) {
        m_target = target;
        m_at = nowSec;
        m_armed = IsBright(target);
        m_seen = false;
    }

    // Returns a message to show, once, when the wait passes with nothing in
    // the frame; empty otherwise.
    std::string Tick(double nowSec, bool targetInFrame) {
        if (!m_armed) return {};
        if (targetInFrame) { m_seen = true; m_armed = false; m_missesInRow = 0; return {}; }
        if (nowSec - m_at < kWaitSec) return {};
        m_armed = false;
        ++m_missesInRow;
        if (m_missesInRow >= 2)
            return "GoTo to " + m_target + " finished, but it is not in the frame - the second GoTo in a row "
                   "to miss. The mount's alignment is almost certainly stale: power-cycle the mount, level "
                   "the tube, point it north and run Quick Align on the hand control.";
        return "GoTo to " + m_target + " finished, but it is not in the frame. If the mount or tube moved "
               "since the hand control was last aligned, its alignment is stale: power-cycle the mount and "
               "run Quick Align.";
    }

    int MissesInRow() const { return m_missesInRow; }

private:
    std::string m_target;
    double m_at = 0.0;
    bool m_armed = false, m_seen = false;
    int m_missesInRow = 0;
};

} // namespace raduk::core
