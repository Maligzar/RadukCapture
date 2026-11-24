#pragma once
// ============================================================
// RecordingCap.h -- the limit a recording will really stop at
// ============================================================
//
// The owner, 2026-11-24: "It should adjust to the limits based on rotation
// time, so even if I pick 100000, the application knows that it will cut
// at 48900 because of the derot stop feature ... as soon as I click record
// it changes the frames to the actual amount it will be capturing and
// thus the countdown timer is accurate."
//
// With [DEROT] stop on (or in a sequence) a recording ends at the
// derotation window whatever the frame or time limit says. Pressing
// RECORD works out which comes first. When the window does, the limit the
// recording runs to, and the one shown in OUTPUT, becomes the window:
//   - a frame limit: the window times the frame rate now, rounded down;
//   - a time limit: the window, in whole seconds rounded down.
// The window still stops the recording as before; this only makes the
// limit and the countdown say so. The owner's own figure comes back when
// the recording ends, so the next target (one with no window, or a longer
// one) is not held to this one's.
//
// Pure: the caller supplies the limits, the window and the rate.
// ============================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace raduk::core {

struct RecordingCap {
    bool        capped = false;   // the window comes first
    uint32_t    frames = 0;       // the frame limit to run to (frame limits)
    double      sec    = 0.0;     // the time limit to run to (time limits)
    std::string why;              // one log line, empty when not capped
};

// byFrames: the OUTPUT choice. limitFrames / limitSec: the owner's figures.
// derotSec: the window, 0 when there is none or the stop is off. fps: the
// rate the camera is delivering now; 0 unknown.
inline RecordingCap CapForWindow(bool byFrames, uint32_t limitFrames, double limitSec,
                                 double derotSec, double fps, const std::string& body = "") {
    RecordingCap c;
    c.frames = limitFrames;
    c.sec    = limitSec;
    if (!(derotSec > 0.0)) return c;
    const int wm = (int)(derotSec / 60.0), ws = (int)derotSec % 60;
    const std::string on = body.empty() ? "" : " on " + body;
    char t[200];
    if (byFrames) {
        // Without a rate there is no frame figure to give; the window
        // still stops the recording.
        if (!(fps > 0.0) || limitFrames == 0) return c;
        const double atWindow = std::floor(derotSec * fps);
        if (atWindow < 1.0 || atWindow >= (double)limitFrames) return c;
        c.capped = true;
        c.frames = (uint32_t)atWindow;
        std::snprintf(t, sizeof(t),
                      "%u frames, not %u: the derotation window%s is %d:%02d at %.1f fps",
                      c.frames, limitFrames, on.c_str(), wm, ws, fps);
    } else {
        if (limitSec > 0.0 && limitSec <= derotSec) return c;
        const double whole = std::floor(derotSec);
        if (whole < 1.0) return c;
        c.capped = true;
        c.sec = whole;
        if (limitSec > 0.0)
            std::snprintf(t, sizeof(t), "%.0f s, not %.0f s: the derotation window%s is %d:%02d",
                          c.sec, limitSec, on.c_str(), wm, ws);
        else
            std::snprintf(t, sizeof(t), "%.0f s: the derotation window%s is %d:%02d",
                          c.sec, on.c_str(), wm, ws);
    }
    c.why = t;
    return c;
}

} // namespace raduk::core
