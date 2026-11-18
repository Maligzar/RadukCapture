// ============================================================
//  MoonMap — the Moon by quadrant and section (phase 162)
//
//  A mosaic of the whole Moon at this focal length is dozens of frames,
//  and they cannot all be taken on one night: the terminator hides part
//  of the disc, libration hides a limb, and the sky closes. So the map
//  is a PROJECT that fills in over nights, and the unit it is counted
//  in has to be a place on the Moon and not a place in the sky.
//
//  MosaicPlan lays panels out on the sky round tonight's disc. That is
//  right for one sitting and useless for a month, because panel 7
//  tonight is a different piece of Moon from panel 7 next week. Here
//  the grid is fixed to the SURFACE:
//
//    - The disc as seen at zero libration, north up, selenographic east
//      (Mare Crisium's side) to the right: the naked-eye Moon from the
//      northern hemisphere. x east, y north, radius 1.
//    - Four quadrants, NE NW SW SE, by the equator and the central
//      meridian.
//    - Each quadrant cut into n x n square sections. A section is named
//      from the disc centre outward: the letter is the row away from
//      the equator, the number the column away from the meridian. NE-A1
//      touches the centre; NE-A5 is on the east limb at the equator.
//
//  A section is aimed at through its selenographic latitude and
//  longitude (LunarGeometry.h), so libration, parallax and the pole's
//  tilt are all taken out by the same code that points at a crater.
//
//  ── How big a section is ───────────────────────────────────
//
//  The camera's rotation against lunar north is not known and, on an
//  alt-az mount, turns through the night. So a section must fit in the
//  frame AT ANY ANGLE: its diagonal has to be inside the frame's SHORT
//  side, less the overlap. That is a smaller section than the frame's
//  long side suggests, and it is the honest one.
//
//  Pure arithmetic, gated by tests/moon_map.cpp.
// ============================================================

#pragma once

#include "LunarGeometry.h"

#include <cmath>
#include <string>
#include <vector>

namespace raduk::sky {

// The largest the Moon gets from the ground: extreme perigee, overhead.
// 2 x asin(1737.4 / (356500 - 6378)) = 34.1'. Sections are sized for it,
// because a section is a fixed piece of Moon and is biggest then.
inline constexpr double MOON_MAX_DIAMETER_ARCMIN = 34.1;

// The share of the frame's short side kept as margin round a section.
inline constexpr double MOONMAP_OVERLAP = 0.20;

inline constexpr int MOONMAP_MIN_SIDE = 1;
inline constexpr int MOONMAP_MAX_SIDE = 12;      // 'A'..'L', 144 a quadrant

// A corner square holding less of the disc than this is not a section:
// its sliver is inside its neighbours' frames.
inline constexpr double MOONMAP_MIN_DISC_FRACTION = 0.05;

// Aim points and lighting samples stay inside this radius. At the very
// limb a degree of longitude is nothing on the sky and libration decides
// whether the point exists at all.
inline constexpr double MOONMAP_MAX_AIM_RADIUS = 0.97;

// The Sun must be this high over every sampled point of a section for
// it to count as lit. Lower is the terminator, where part of the frame
// is black and a later night will do better.
inline constexpr double MOONMAP_MIN_SUN_ALT_DEG = 1.0;

enum class MoonQuadrant { NE, NW, SW, SE };

inline const char* MoonQuadrantName(MoonQuadrant q) {
    switch (q) {
        case MoonQuadrant::NE: return "NE";
        case MoonQuadrant::NW: return "NW";
        case MoonQuadrant::SW: return "SW";
        case MoonQuadrant::SE: return "SE";
    }
    return "NE";
}

struct MoonSection {
    MoonQuadrant quad = MoonQuadrant::NE;
    int    row = 0, col = 0;         // from the centre outward, from 0
    std::string id;                  // "NE-A1"
    double x0 = 0, x1 = 0;           // disc units, east positive
    double y0 = 0, y1 = 0;           // north positive
    double discFraction = 0.0;       // how much of the square is Moon
    double aimX = 0, aimY = 0;       // where the frame centre goes
    double latDeg = 0, lonDeg = 0;   // the same, selenographic
};

// A section's side on the sky, when the Moon is `moonArcmin` across.
inline double SectionSideArcmin(int perSide, double moonArcmin) {
    return perSide > 0 ? moonArcmin * 0.5 / perSide : 0.0;
}

// Does a frame aimed at the middle of a section hold all of it, however
// the camera is turned? The diagonal against the short side.
inline bool FrameHoldsSection(const FieldOfView& fov, int perSide,
                              double moonArcmin, double overlap) {
    const double shortSide = fov.widthArcmin < fov.heightArcmin
                           ? fov.widthArcmin : fov.heightArcmin;
    if (shortSide <= 0.0 || perSide <= 0) return false;
    return SectionSideArcmin(perSide, moonArcmin) * std::sqrt(2.0)
           <= shortSide * (1.0 - overlap) + 1e-9;
}

// Sections along one side of a quadrant for this frame. 0 when there is
// no frame to size them for.
inline int SectionsPerSide(const FieldOfView& fov,
                           double overlap = MOONMAP_OVERLAP,
                           double moonArcmin = MOON_MAX_DIAMETER_ARCMIN) {
    const double shortSide = fov.widthArcmin < fov.heightArcmin
                           ? fov.widthArcmin : fov.heightArcmin;
    if (shortSide <= 0.0 || moonArcmin <= 0.0) return 0;
    if (overlap < 0.0)  overlap = 0.0;
    if (overlap > 0.45) overlap = 0.45;
    const double usable = shortSide * (1.0 - overlap);
    int n = (int)std::ceil(moonArcmin * 0.5 * std::sqrt(2.0) / usable - 1e-9);
    if (n < MOONMAP_MIN_SIDE) n = MOONMAP_MIN_SIDE;
    if (n > MOONMAP_MAX_SIDE) n = MOONMAP_MAX_SIDE;
    return n;
}

inline std::string MoonSectionId(MoonQuadrant q, int row, int col) {
    std::string s = MoonQuadrantName(q);
    s += '-';
    s += (char)('A' + row);
    s += std::to_string(col + 1);
    return s;
}

// Disc position to selenographic, at zero libration (orthographic).
inline void DiscToSelenographic(double x, double y, double& latDeg, double& lonDeg) {
    const double r2 = x * x + y * y;
    if (r2 > 1.0) { const double r = std::sqrt(r2); x /= r; y /= r; }
    latDeg = SafeAsinDeg(y);
    const double c = std::sqrt(1.0 - y * y);
    lonDeg = c > 1e-9 ? SafeAsinDeg(x / c) : 0.0;
}

// Every section that holds some Moon, in capture order: rows from the
// north down, each row walked the opposite way to the one above it, so
// the mount steps to a neighbour every time and never flies back across
// the disc (MosaicPlan's serpentine, for the same reason).
inline std::vector<MoonSection> BuildMoonSections(int perSide) {
    std::vector<MoonSection> out;
    if (perSide < MOONMAP_MIN_SIDE || perSide > MOONMAP_MAX_SIDE) return out;
    const int n = perSide, full = 2 * n;
    const double step = 1.0 / n;
    constexpr int SUB = 16;

    for (int gr = 0; gr < full; ++gr) {
        for (int k = 0; k < full; ++k) {
            const int gc = (gr % 2 == 0) ? k : full - 1 - k;
            MoonSection s;
            s.x0 = -1.0 + gc * step;      s.x1 = s.x0 + step;
            s.y1 =  1.0 - gr * step;      s.y0 = s.y1 - step;
            const bool east = gc >= n, north = gr < n;
            s.quad = north ? (east ? MoonQuadrant::NE : MoonQuadrant::NW)
                           : (east ? MoonQuadrant::SE : MoonQuadrant::SW);
            s.col = east  ? gc - n : n - 1 - gc;
            s.row = north ? n - 1 - gr : gr - n;
            s.id  = MoonSectionId(s.quad, s.row, s.col);

            // How much of it is disc, and where the middle of that part is.
            int in = 0;
            double sx = 0.0, sy = 0.0;
            for (int j = 0; j < SUB; ++j)
                for (int i = 0; i < SUB; ++i) {
                    const double x = s.x0 + (i + 0.5) * step / SUB;
                    const double y = s.y0 + (j + 0.5) * step / SUB;
                    if (x * x + y * y <= 1.0) { ++in; sx += x; sy += y; }
                }
            s.discFraction = (double)in / (SUB * SUB);
            if (s.discFraction < MOONMAP_MIN_DISC_FRACTION) continue;
            s.aimX = sx / in;
            s.aimY = sy / in;
            const double r = std::sqrt(s.aimX * s.aimX + s.aimY * s.aimY);
            if (r > MOONMAP_MAX_AIM_RADIUS) {
                s.aimX *= MOONMAP_MAX_AIM_RADIUS / r;
                s.aimY *= MOONMAP_MAX_AIM_RADIUS / r;
            }
            DiscToSelenographic(s.aimX, s.aimY, s.latDeg, s.lonDeg);
            out.push_back(s);
        }
    }
    return out;
}

// ── A section tonight ───────────────────────────────────────
struct SectionSky {
    bool   visible = false;      // every sampled point faces this observer
    double litFraction = 0.0;    // of the sampled points, how many are in sun
    double sunAltDeg = 0.0;      // at the aim point
    double raHours = 0.0, decDeg = 0.0;   // J2000, of the aim point
    // All of it in sunlight and in view: a frame taken now is all Moon.
    bool Capturable() const { return visible && litFraction >= 1.0; }
};

inline SectionSky SectionSkyAt(const LunarView& v, const MoonSection& s) {
    SectionSky k;
    const LunarSpot aim = LunarSpotAt(v, s.latDeg, s.lonDeg);
    k.sunAltDeg = aim.sunAltDeg;
    k.raHours   = aim.raHours;
    k.decDeg    = aim.decDeg;

    int pts = 0, lit = 0;
    bool allSeen = aim.visible;
    auto sample = [&](double x, double y) {
        const double r = std::sqrt(x * x + y * y);
        if (r > MOONMAP_MAX_AIM_RADIUS) return;
        double lat = 0.0, lon = 0.0;
        DiscToSelenographic(x, y, lat, lon);
        const LunarSpot p = LunarSpotAt(v, lat, lon);
        ++pts;
        if (!p.visible) allSeen = false;
        if (p.sunAltDeg >= MOONMAP_MIN_SUN_ALT_DEG) ++lit;
    };
    sample(s.aimX, s.aimY);
    static constexpr double kAt[] = {0.1, 0.5, 0.9};
    for (double fy : kAt)
        for (double fx : kAt)
            sample(s.x0 + fx * (s.x1 - s.x0), s.y0 + fy * (s.y1 - s.y0));
    k.visible     = allSeen;
    k.litFraction = pts > 0 ? (double)lit / pts : 0.0;
    return k;
}

// The sizing, as a sentence for the log when a project is made.
inline std::string DescribeMoonGrid(const FieldOfView& fov, int perSide) {
    char b[240];
    const int total = (int)BuildMoonSections(perSide).size();
    snprintf(b, sizeof(b),
             "%d x %d sections a quadrant, %d on the disc; a section is at "
             "most %.1f' square in a %.1f' x %.1f' frame, which %s.",
             perSide, perSide, total,
             SectionSideArcmin(perSide, MOON_MAX_DIAMETER_ARCMIN),
             fov.widthArcmin, fov.heightArcmin,
             FrameHoldsSection(fov, perSide, MOON_MAX_DIAMETER_ARCMIN, MOONMAP_OVERLAP)
                 ? "holds it at any camera angle"
                 : "does NOT hold it at every camera angle");
    return b;
}

// Nothing here has been pointed at the Moon. The grid, the names and
// the lighting are checked against hand arithmetic and LunarGeometry;
// whether a GoTo lands a section inside its frame is the mount's
// alignment, and a clear night's question.
inline constexpr bool MOON_MAP_PROVEN_ON_SKY = false;

} // namespace raduk::sky
