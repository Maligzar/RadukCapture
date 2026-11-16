#pragma once
// ============================================================
//  AcquireFrame.h -- phase 157.0c: find and calibrate wide, image small
//
//  The owner, watching the first soak (2026-11-15): on a 640x480 ROI a
//  recalibration left the star almost off screen, and tracking struggled
//  to bring it to the centre. The soak's numbers said the same: 0 locks in
//  7, every one refused because a GoTo's pointing error left the target
//  near the frame's edge, and calibration moves it a few hundred pixels.
//
//  So an auto-acquire (PLANET LOCK, or a search promoted to one) runs on
//  a large frame, and the profile's frame comes back when it ends:
//
//    - widen when an auto-acquire starts, the frame is smaller than the
//      wide frame, and nothing is recording. The wide frame is centred
//      where the small one was, so what was in view stays in view;
//    - restore when the auto-acquire is no longer running, for whatever
//      reason (locked, lost, calibration refused, stopped), or when a
//      recording is about to start. Locked with a centroid: centred on
//      the target. Otherwise: where the small frame was;
//    - forget, without moving anything, if the frame is changed by hand
//      while wide. The operator has taken over.
//
//  Pure: no Windows, no camera. main.cpp applies what it says through
//  applyFormat and reports back the size the camera granted (NoteApplied),
//  because the camera clamps and the real frame is what counts.
//  tests/acquire_frame.cpp checks it.
// ============================================================

namespace raduk::core {

class WideAcquire {
public:
    enum class Act { None, Widen, Restore, Forget };

    struct Out {
        Act         act = Act::None;
        int         w = 0, h = 0;      // frame size to apply
        int         cx = 0, cy = 0;    // its centre, sensor pixels
        const char* why = "";
    };

    struct In {
        bool enabled       = true;     // rig.ini AcquireWide
        bool acquiring     = false;    // the tracker's auto-acquire is running
        bool recording     = false;
        bool locked        = false;    // the tracker reached LOCKED
        bool centroidValid = false;
        int  centroidX = 0, centroidY = 0;   // sensor pixels
        int  roiW = 0, roiH = 0;             // the frame now
        int  roiCx = 0, roiCy = 0;           // its centre, sensor pixels
        int  sensorW = 0, sensorH = 0;
        int  wideMaxW = 0, wideMaxH = 0;     // 0: the whole sensor
    };

    bool Wide() const { return m_wide; }
    int  SavedW() const { return m_savedW; }
    int  SavedH() const { return m_savedH; }

    // The size the wide frame should be, before the camera has its say.
    static void WideSize(const In& in, int& w, int& h) {
        w = in.sensorW;
        h = in.sensorH;
        if (in.wideMaxW > 0 && in.wideMaxW < w) w = in.wideMaxW;
        if (in.wideMaxH > 0 && in.wideMaxH < h) h = in.wideMaxH;
    }

    // Called every main-loop pass, and right where an auto-acquire is
    // started, so the widen lands before the tracker's next Tick can
    // begin calibrating on the small frame.
    Out Step(const In& in) {
        Out o;
        if (!m_wide) {
            if (!in.enabled || !in.acquiring || in.recording) return o;
            if (in.sensorW <= 0 || in.sensorH <= 0 || in.roiW <= 0 || in.roiH <= 0) return o;
            int w = 0, h = 0;
            WideSize(in, w, h);
            if (in.roiW >= w && in.roiH >= h) return o;     // already wide enough
            m_wide   = true;
            m_savedW = in.roiW;  m_savedH = in.roiH;
            m_savedCx = in.roiCx; m_savedCy = in.roiCy;
            m_wideW = w; m_wideH = h;
            o.act = Act::Widen;
            o.w = w; o.h = h; o.cx = in.roiCx; o.cy = in.roiCy;
            o.why = "acquisition: wide frame to find and calibrate";
            return o;
        }
        if (in.roiW != m_wideW || in.roiH != m_wideH) {
            m_wide = false;
            o.act = Act::Forget;
            o.why = "the frame was changed by hand while wide: left as it is";
            return o;
        }
        if (in.recording)
            return RestoreOut(in, "a recording started on the wide frame");
        if (!in.acquiring)
            return RestoreOut(in, in.locked ? "locked: the profile's frame, on the target"
                                            : "acquisition ended: the profile's frame");
        return o;
    }

    // Right before a recording starts: a wide frame must not be recorded.
    Out BeforeRecording(const In& in) {
        if (!m_wide) return Out{};
        return RestoreOut(in, "a recording is starting: the profile's frame");
    }

    // What the camera actually granted for the wide frame.
    void NoteApplied(int w, int h) {
        if (m_wide) { m_wideW = w; m_wideH = h; }
    }

private:
    Out RestoreOut(const In& in, const char* why) {
        m_wide = false;
        Out o;
        o.act = Act::Restore;
        o.w = m_savedW; o.h = m_savedH;
        const bool onTarget = in.locked && in.centroidValid;
        o.cx = onTarget ? in.centroidX : m_savedCx;
        o.cy = onTarget ? in.centroidY : m_savedCy;
        o.why = why;
        return o;
    }

    bool m_wide = false;
    int  m_savedW = 0, m_savedH = 0, m_savedCx = 0, m_savedCy = 0;
    int  m_wideW = 0, m_wideH = 0;
};

} // namespace raduk::core
