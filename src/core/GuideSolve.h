#pragma once
// ============================================================
// GuideSolve.h ── pixels into pulses, in one place
//
// Phase 51 measures how the image responds to a guide pulse: pulse
// East, see where the centroid went, pulse North, same again. That
// gives a 2x2 matrix from pulse milliseconds to pixels, and it is
// MEASURED, so it already contains the camera's rotation in the
// focuser, the mirror flip of the optical train and the sign of each
// axis. Nothing else in the app knows which way is north in the frame,
// and nothing else needs to.
//
// Inverting that matrix is how a pixel displacement becomes a pair of
// pulses. The guide loop has always done it inline; phase 47 (click to
// recentre) needs exactly the same arithmetic, so it lives here and
// both call it. Two copies would be two conventions, and a sign error
// in one of them moves the telescope the wrong way in the dark.
// ============================================================

#include <cmath>

namespace raduk::core {

// The measured response, px per ms of pulse. Mirrors the four
// coefficients of GuideCalibration; taken as plain numbers so this
// header stays free of the tracker, the camera and Windows, and can be
// tested on its own.
struct GuideResponse {
    float exPerMs = 0.0f, eyPerMs = 0.0f;   // East pulse
    float nxPerMs = 0.0f, nyPerMs = 0.0f;   // North pulse
};

// Pulse durations, signed: positive is East / North, negative is
// West / South. Milliseconds, before any clamp the caller applies.
struct GuidePulses {
    bool  valid   = false;
    float eastMs  = 0.0f;
    float northMs = 0.0f;
};

// Solve M*(tE,tN) = (ddx,ddy), where (ddx,ddy) is the displacement the
// IMAGE should make, in pixels.
//
// A near-zero determinant means the two measured responses are
// parallel: the calibration cannot tell the axes apart, and solving it
// would hand back enormous pulses in confident directions. The guide
// loop has always refused that case and so does this.
inline GuidePulses SolveGuidePulses(const GuideResponse& r,
                                    float ddx, float ddy) {
    GuidePulses out;
    const float det = r.exPerMs * r.nyPerMs - r.nxPerMs * r.eyPerMs;
    const float eMag2 = r.exPerMs * r.exPerMs + r.eyPerMs * r.eyPerMs;
    const float nMag2 = r.nxPerMs * r.nxPerMs + r.nyPerMs * r.nyPerMs;
    if (std::abs(det) < 1e-8f || eMag2 <= 0.0f || nMag2 <= 0.0f) return out;
    out.valid   = true;
    out.eastMs  = ( r.nyPerMs * ddx - r.nxPerMs * ddy) / det;
    out.northMs = (-r.eyPerMs * ddx + r.exPerMs * ddy) / det;
    return out;
}

// On an alt-az mount the azimuth response shrinks with cos(altitude),
// so a calibration taken at 30 degrees over-corrects near the zenith.
// Rescale the East column to the altitude now. Negative altitudes mean
// "not known", and then nothing is rescaled rather than guessed.
inline GuideResponse RescaleForAltitude(GuideResponse r,
                                        float altCalDeg, float altNowDeg) {
    if (altCalDeg < 0.0f || altNowDeg < 0.0f) return r;
    const float cCal = std::cos(altCalDeg * 0.01745329f);
    const float cNow = std::cos(altNowDeg * 0.01745329f);
    if (cCal > 0.05f && cNow > 0.05f) {
        const float k = cNow / cCal;
        r.exPerMs *= k;
        r.eyPerMs *= k;
    }
    return r;
}

// ── Phase 172 finding 11: a lock after ALIGN in seconds, not 36 ──
//
// 2026-11-22 22:52: after ★ ALIGN the lock recalibrated from scratch, 36 s
// of drift windows and offset moves, though nothing about the response had
// changed. On an alt-az mount the camera rides the tube, so the image moves
// the same way for an axis wherever it points; only the azimuth response
// shrinks with cos(altitude), which RescaleForAltitude already applies. A
// sync or a GoTo changes the tracking RATES, never that geometry.
//
// So a lock may keep the last calibration's response and re-measure only
// the rates and the axis directions, when:
//   - there is a calibration and its altitude is known;
//   - the frame is the same size (a different size can be a different
//     binning, so a different pixel scale);
//   - each axis turned the same way for "direction 0" as it did then
//     (a different sign means the mount, or its wiring, is not the same);
//   - it is under maxAgeSec old (someone may have rotated the camera).
// Anything else calibrates in full, and the guider's runaway check still
// stands behind a kept one: a wrong response aborts and clears it.
struct GeometryReuse { bool ok = false; const char* why = ""; };

inline GeometryReuse CanReuseGeometry(bool calValid, float calAltDeg,
                                      unsigned calW, unsigned calH,
                                      float calSignAz, float calSignAlt,
                                      unsigned frameW, unsigned frameH,
                                      float signAz, float signAlt,
                                      double ageSec, double maxAgeSec = 4.0 * 3600.0) {
    GeometryReuse r;
    if (!calValid)                       { r.why = "no calibration to keep"; return r; }
    if (calAltDeg < 0.0f)                { r.why = "the calibration has no altitude"; return r; }
    if (calW == 0 || calH == 0)          { r.why = "the calibration's frame size is not known"; return r; }
    if (calW != frameW || calH != frameH){ r.why = "the frame size changed since the calibration"; return r; }
    if (calSignAz == 0.0f || calSignAlt == 0.0f) { r.why = "the calibration's axis directions are not known"; return r; }
    if ((calSignAz > 0.0f) != (signAz > 0.0f) || (calSignAlt > 0.0f) != (signAlt > 0.0f))
                                         { r.why = "an axis turned the other way this time"; return r; }
    if (!(ageSec >= 0.0) || ageSec > maxAgeSec) { r.why = "the calibration is too old"; return r; }
    r.ok = true;
    r.why = "same frame, same axis directions";
    return r;
}

// ── Phase 47: click a pixel, put it in the middle ───────────
//
// Centring a target by jogging is a game of overshoot in the dark: the
// jog moves the mount, not the image, and which screen direction a jog
// produces depends on where the camera happens to sit in the focuser.
// The calibration already knows that, so a click can be exact.
//
// Everything that could make the move wrong is refused rather than
// approximated. This drives the telescope, and a plausible-looking
// pulse in an unknown direction is worse than no pulse.
struct RecentrePlan {
    bool  valid    = false;
    const char* why = "";          // why not, when !valid
    int   eastMs   = 0;            // signed: >0 East, <0 West
    int   northMs  = 0;            // signed: >0 North, <0 South
    float offsetPx = 0.0f;         // how far the click was from centre
    float offsetArcsec = 0.0f;     // the same, if the scale is known
    float wantedMs = 0.0f;         // longest pulse asked for, before the clamp
};

inline RecentrePlan PlanRecentre(float clickX, float clickY,
                                 int frameW, int frameH,
                                 const GuideResponse& resp,
                                 float arcsecPerPx,
                                 int maxPulseMs) {
    RecentrePlan p;
    if (frameW <= 0 || frameH <= 0) { p.why = "no frame"; return p; }
    if (clickX < 0.0f || clickY < 0.0f ||
        clickX > (float)frameW || clickY > (float)frameH) {
        p.why = "click outside the frame";
        return p;
    }

    // The image has to move the other way from the click: a target to
    // the right of centre travels left to get there.
    const float ddx = (float)frameW * 0.5f - clickX;
    const float ddy = (float)frameH * 0.5f - clickY;
    p.offsetPx     = std::sqrt(ddx * ddx + ddy * ddy);
    p.offsetArcsec = (arcsecPerPx > 0.0f) ? p.offsetPx * arcsecPerPx : 0.0f;

    // A click on the centre is not a request to move. The threshold is
    // the tracker's own idea of centred, so the two agree about when
    // there is nothing left to do.
    if (p.offsetPx < 3.0f) { p.why = "already centred"; return p; }

    const GuidePulses g = SolveGuidePulses(resp, ddx, ddy);
    if (!g.valid) { p.why = "guide calibration is degenerate"; return p; }

    p.wantedMs = (std::abs(g.eastMs) > std::abs(g.northMs))
                     ? std::abs(g.eastMs) : std::abs(g.northMs);
    if (maxPulseMs > 0 && p.wantedMs > (float)maxPulseMs) {
        // Deliberately not clamped-and-sent. A clamped pulse lands
        // somewhere nobody asked for, which on a planet at 0.2"/px
        // means the target leaves the frame and the next click is
        // guesswork. A GoTo is the right tool for that distance.
        p.why = "too far to pulse -- slew there instead";
        return p;
    }

    p.valid   = true;
    p.eastMs  = (int)(g.eastMs  > 0.0f ? g.eastMs  + 0.5f : g.eastMs  - 0.5f);
    p.northMs = (int)(g.northMs > 0.0f ? g.northMs + 0.5f : g.northMs - 0.5f);
    // Pulses under 10 ms are below what a mount will honour; the guide
    // loop drops them for the same reason.
    if (std::abs(p.eastMs)  < 10) p.eastMs  = 0;
    if (std::abs(p.northMs) < 10) p.northMs = 0;
    if (p.eastMs == 0 && p.northMs == 0) {
        p.valid = false;
        p.why = "move is below the mount's shortest pulse";
    }
    return p;
}

} // namespace raduk::core
