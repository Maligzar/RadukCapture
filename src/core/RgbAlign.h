#pragma once
// ============================================================
//  RgbAlign.h -- Phase 168: the preview's RGB ALIGN, corrected
//
//  Atmospheric dispersion spreads a planet into colours along the
//  vertical circle: blue is lifted toward the zenith more than green, and
//  red less. RGB ALIGN undoes that on the display only (recordings are
//  never touched).
//
//  What it did wrong (Phase 23), found by the owner at the scope on
//  2026-11-21 ("I am seeing double"):
//    - Blue moved by the whole computed separation and red by half of it
//      the other way, so blue and red moved apart by 1.5 times the
//      dispersion: at +9 px, 4.5 px of fringe the other way.
//    - It moved along sensor rows only, wherever the zenith actually is
//      in the frame.
//    - It added a correction on top of a physical ADC.
//
//  Now: blue and red together move by exactly the separation, with green
//  as the reference, split by the refractive index at the band's middle;
//  along the zenith's direction in the frame (rig.ini
//  CameraZenithAngleDeg); and nothing automatic when an ADC is fitted.
//
//  Pure: tested by tests/rgb_align.cpp.
// ============================================================

#include <cmath>

namespace raduk::core {

// Air's refractivity (n - 1) at STP, Edlen 1966 (as ADCAssistant). The
// pressure and temperature scale cancels in a ratio, so it is left out.
inline double EdlenRefractivity(double lambdaNm) {
    const double s = 1000.0 / lambdaNm;   // wavenumber, 1/um
    const double s2 = s * s;
    return (8342.13 + 2406030.0 / (130.0 - s2) + 15997.0 / (38.9 - s2)) * 1e-8;
}

// The fraction of the blue-red separation that lies between blue and the
// band's middle (green). About 0.6 for 400-700 nm: blue is spread further
// from green than red is.
inline double BlueShareOfDispersion(double lowNm, double highNm) {
    if (!(highNm > lowNm)) return 0.5;
    const double nb = EdlenRefractivity(lowNm), nr = EdlenRefractivity(highNm);
    const double ng = EdlenRefractivity(0.5 * (lowNm + highNm));
    const double span = nb - nr;
    return span > 0 ? (nb - ng) / span : 0.5;
}

struct RgbAlignShift {
    // Where each channel is sampled FROM, relative to the pixel drawn:
    // displayed(x, y) takes blue from (x + bx, y + by) and red from
    // (x + rx, y + ry). Green stays.
    int bx = 0, by = 0, rx = 0, ry = 0;
    float totalPx = 0.0f;   // the separation corrected, signed (manual included)
};

// separationPx: the computed blue-red dispersion in pixels (0 when not
// known). manualPx: the owner's fine-tune, added to it. zenithAngleDeg:
// the zenith's direction in the frame, clockwise from the frame's top
// (0: zenith toward row 0; 180: toward the bottom, as the default rig
// shows it). adcFitted: a physical ADC does the correcting, so the
// automatic part is 0 and only the manual offset applies.
inline RgbAlignShift ComputeRgbAlign(double separationPx, int manualPx, double zenithAngleDeg,
                                     double blueShare, bool adcFitted) {
    RgbAlignShift s;
    const double total = (adcFitted ? 0.0 : separationPx) + manualPx;
    s.totalPx = (float)total;
    if (total == 0.0) return s;
    const double a = zenithAngleDeg * 3.14159265358979 / 180.0;
    const double ux = std::sin(a), uy = -std::cos(a);   // toward the zenith, x right, y down
    const double b = total * blueShare, r = total * (1.0 - blueShare);
    // Blue sits b toward the zenith of where it belongs: sample it from there.
    // Red sits r away from the zenith: sample it from there.
    s.bx = (int)std::lround(b * ux);
    s.by = (int)std::lround(b * uy);
    s.rx = (int)std::lround(-r * ux);
    s.ry = (int)std::lround(-r * uy);
    return s;
}

} // namespace raduk::core
