// ============================================================
//  acquire_frame -- phase 157.0c
//
//  The wide-acquisition rule (core/AcquireFrame.h): widen when an
//  auto-acquire starts on a small frame, restore when it ends or a
//  recording starts, centred on the target when locked, and leave a
//  frame changed by hand alone.
// ============================================================

#include "core/AcquireFrame.h"

#include <cstdio>

using raduk::core::WideAcquire;

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    if (ok) printf("  ok    %s\n", what);
    else  { printf("  FAIL  %s\n", what); ++g_fail; }
}

// A 640x480 ROI centred at (1600, 840) on a 3840x2160 sensor, as the soak had.
static WideAcquire::In Small() {
    WideAcquire::In in;
    in.roiW = 640; in.roiH = 480; in.roiCx = 1600; in.roiCy = 840;
    in.sensorW = 3840; in.sensorH = 2160;
    return in;
}

int main() {
    printf("Wide acquisition: find and calibrate wide, image small\n");

    printf("\n1. Widening\n");
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        Check(w.Step(in).act == WideAcquire::Act::None, "nothing while no auto-acquire runs");
        in.acquiring = true;
        const auto o = w.Step(in);
        Check(o.act == WideAcquire::Act::Widen && o.w == 3840 && o.h == 2160,
              "an auto-acquire on 640x480 widens to the whole sensor");
        Check(o.cx == 1600 && o.cy == 840, "centred where the small frame was");
        Check(w.Wide() && w.SavedW() == 640 && w.SavedH() == 480, "and remembers the small frame");

        WideAcquire w2;
        in.wideMaxW = 1920; in.wideMaxH = 1080;
        const auto o2 = w2.Step(in);
        Check(o2.w == 1920 && o2.h == 1080, "AcquireWideMaxW/H caps the wide frame");

        WideAcquire w3;
        WideAcquire::In full = Small();
        full.acquiring = true; full.roiW = 3840; full.roiH = 2160;
        Check(w3.Step(full).act == WideAcquire::Act::None, "already the whole sensor: nothing to do");

        WideAcquire w4;
        WideAcquire::In rec = Small();
        rec.acquiring = true; rec.recording = true;
        Check(w4.Step(rec).act == WideAcquire::Act::None, "never widened while recording");

        WideAcquire w5;
        WideAcquire::In off = Small();
        off.acquiring = true; off.enabled = false;
        Check(w5.Step(off).act == WideAcquire::Act::None, "AcquireWide=0 turns it off");
    }

    printf("\n2. Restoring\n");
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        in.acquiring = true;
        w.Step(in);
        // The camera granted the whole sensor; the frame is now wide.
        in.roiW = 3840; in.roiH = 2160; in.roiCx = 1920; in.roiCy = 1080;
        w.NoteApplied(3840, 2160);
        Check(w.Step(in).act == WideAcquire::Act::None, "held while the auto-acquire runs");

        in.acquiring = false; in.locked = true; in.centroidValid = true;
        in.centroidX = 1710; in.centroidY = 1010;
        const auto o = w.Step(in);
        Check(o.act == WideAcquire::Act::Restore && o.w == 640 && o.h == 480,
              "locked: back to the profile's 640x480");
        Check(o.cx == 1710 && o.cy == 1010, "centred on the target, not where the GoTo left it");
        Check(!w.Wide(), "and no longer wide");
        Check(w.Step(in).act == WideAcquire::Act::None, "once only");
    }
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        in.acquiring = true;
        w.Step(in);
        in.roiW = 3840; in.roiH = 2160;
        w.NoteApplied(3840, 2160);
        in.acquiring = false;      // calibration refused, lost, or stopped
        const auto o = w.Step(in);
        Check(o.act == WideAcquire::Act::Restore && o.cx == 1600 && o.cy == 840,
              "not locked: back where the small frame was");
    }
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        in.acquiring = true;
        w.Step(in);
        in.roiW = 3840; in.roiH = 2160;
        w.NoteApplied(3840, 2160);
        const auto o = w.BeforeRecording(in);
        Check(o.act == WideAcquire::Act::Restore && o.w == 640,
              "a recording about to start gets the profile's frame first");
        Check(w.BeforeRecording(in).act == WideAcquire::Act::None,
              "and a second recording finds nothing to undo");
    }
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        in.acquiring = true;
        w.Step(in);
        in.roiW = 3840; in.roiH = 2160;
        w.NoteApplied(3840, 2160);
        in.recording = true;      // started by a path that did not ask first
        Check(w.Step(in).act == WideAcquire::Act::Restore, "a recording on the wide frame ends it");
    }

    printf("\n3. The camera's say, and the operator's\n");
    {
        WideAcquire w;
        WideAcquire::In in = Small();
        in.acquiring = true;
        w.Step(in);
        // The emulator streams at most 1280x960: that is the wide frame.
        in.roiW = 1280; in.roiH = 960;
        w.NoteApplied(1280, 960);
        Check(w.Step(in).act == WideAcquire::Act::None,
              "a clamped wide frame is still the wide frame");
        in.roiW = 1920; in.roiH = 1080;   // FULL / 1080 pressed by hand
        const auto o = w.Step(in);
        Check(o.act == WideAcquire::Act::Forget && !w.Wide(),
              "a frame changed by hand while wide is left alone");
        in.acquiring = false;
        Check(w.Step(in).act == WideAcquire::Act::None, "and nothing is restored over it");
    }

    printf("\n%s\n", g_fail ? "FAILED" : "all passed");
    return g_fail ? 1 : 0;
}
