// ============================================================
//  The Moon by quadrant and section (phase 162)
//
//  src/sky/MoonMap.h is the grid: where each section is on the disc,
//  what it is called, how big it may be for a frame, and whether it is
//  in sunlight tonight. src/core/MoonMapProject.h is the record of what
//  a project holds. Both pure, both here.
//
//  THE ONES THAT MATTER:
//
//   2. A section must fit the frame at ANY camera angle, so it is sized
//      by its diagonal against the frame's SHORT side. Sized by the long
//      side it looks fine on paper and loses its corners on the night.
//   4. East is Mare Crisium's side. Every sign error in a lunar atlas
//      lives there, and one here would file Crisium under the west limb.
//   7. A project is complete only when EVERY section has a capture. The
//      green light is the whole point, so it is checked at one short.
// ============================================================

#include "sky/MoonMap.h"
#include "core/MoonMapProject.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace raduk::sky;
using raduk::core::MoonMapCapture;
using raduk::core::MoonMapProject;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-66s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) ++g_fail;
}
static std::string N(double v) { char b[32]; snprintf(b, sizeof(b), "%.3f", v); return b; }

static const MoonSection* At(const std::vector<MoonSection>& s, double x, double y) {
    for (const auto& c : s)
        if (x >= c.x0 && x < c.x1 && y >= c.y0 && y < c.y1) return &c;
    return nullptr;
}
static void DiscXY(double latDeg, double lonDeg, double& x, double& y) {
    x = std::cos(DegToRad(latDeg)) * std::sin(DegToRad(lonDeg));
    y = std::sin(DegToRad(latDeg));
}

int main() {
    // This rig: 3840 x 2160 at 0.203"/px, 12.99' x 7.31'.
    const FieldOfView rig = FrameFov(3840, 2160, 0.203);

    printf("\n1. How big a section is\n");
    {
        Check(std::fabs(SectionSideArcmin(5, 34.1) - 3.41) < 1e-9, "a fifth of the radius of a 34.1' Moon is 3.41'");
        const int n = SectionsPerSide(rig);
        Check(n == 5, "this rig's frame wants 5 sections a side", std::to_string(n));
        Check(SectionsPerSide(FieldOfView{}) == 0, "no frame, no size");
        Check(SectionsPerSide(FrameFov(6000, 6000, 1.0)) == 1, "a frame of 100' holds a quadrant in one");
        Check(SectionsPerSide(FrameFov(640, 480, 0.203)) == MOONMAP_MAX_SIDE,
              "a 640x480 crop runs into the ceiling rather than away");
    }

    printf("\n2. The frame holds a section at any camera angle\n");
    {
        Check(FrameHoldsSection(rig, 5, MOON_MAX_DIAMETER_ARCMIN, MOONMAP_OVERLAP), "five a side fits, with the overlap");
        Check(!FrameHoldsSection(rig, 4, MOON_MAX_DIAMETER_ARCMIN, MOONMAP_OVERLAP),
              "four a side does not: its diagonal is past the short side's margin");
        // The long side would say three is plenty. It is not.
        Check(SectionSideArcmin(3, 34.1) < rig.widthArcmin && !FrameHoldsSection(rig, 3, 34.1, 0.0),
              "three a side fits the LONG side and still does not hold");
        Check(!FrameHoldsSection(FrameFov(640, 480, 0.203), MOONMAP_MAX_SIDE, 34.1, MOONMAP_OVERLAP),
              "the 640x480 crop at the ceiling is reported as not holding");
        // Rotate the frame through a quarter turn round a section's
        // middle: every corner of the section stays inside.
        const double half = SectionSideArcmin(5, 34.1) * 0.5;
        bool inside = true;
        for (int deg = 0; deg <= 90; deg += 5) {
            const double c = std::cos(DegToRad(deg)), s = std::sin(DegToRad(deg));
            for (int k = 0; k < 4; ++k) {
                const double px = (k & 1) ? half : -half, py = (k & 2) ? half : -half;
                const double fx = px * c + py * s, fy = -px * s + py * c;
                if (std::fabs(fx) > rig.widthArcmin * 0.5 || std::fabs(fy) > rig.heightArcmin * 0.5) inside = false;
            }
        }
        Check(inside, "checked by turning the frame through 90 degrees");
    }

    printf("\n3. The grid\n");
    const auto g1 = BuildMoonSections(1);
    const auto g5 = BuildMoonSections(5);
    {
        Check(g1.size() == 4, "one a side is the four quadrants");
        bool pi4 = true;
        for (const auto& s : g1) if (std::fabs(s.discFraction - 0.7854) > 0.02) pi4 = false;
        Check(pi4, "each of them is pi/4 Moon");
        Check(BuildMoonSections(0).empty() && BuildMoonSections(13).empty(), "no grid outside 1..12 a side");

        // Counted again a different way: a fine raster over each square.
        int expect = 0;
        for (int gr = 0; gr < 10; ++gr)
            for (int gc = 0; gc < 10; ++gc) {
                int in = 0;
                for (int j = 0; j < 100; ++j)
                    for (int i = 0; i < 100; ++i) {
                        const double x = -1.0 + (gc + (i + 0.5) / 100.0) * 0.2;
                        const double y =  1.0 - (gr + (j + 0.5) / 100.0) * 0.2;
                        if (x * x + y * y <= 1.0) ++in;
                    }
                if (in >= 500) ++expect;
            }
        Check((int)g5.size() == expect, "five a side: the sections holding 5% or more of Moon",
              std::to_string(g5.size()) + " against " + std::to_string(expect));

        int q[4] = {0, 0, 0, 0};
        std::set<std::string> ids;
        double area = 0.0;
        for (const auto& s : g5) {
            ++q[(int)s.quad];
            ids.insert(s.id);
            area += s.discFraction * 0.04;
        }
        Check(q[0] == q[1] && q[1] == q[2] && q[2] == q[3], "the same number in every quadrant", std::to_string(q[0]));
        Check(ids.size() == g5.size(), "no two share a name");
        Check(std::fabs(area - 3.14159) < 0.03, "together they are the disc", N(area));
    }

    printf("\n4. Names, and which way is east\n");
    {
        auto id = [&](double x, double y) { const auto* s = At(g5, x, y); return s ? s->id : std::string("none"); };
        Check(id(0.1, 0.1) == "NE-A1",   "the square north-east of the centre is NE-A1", id(0.1, 0.1));
        Check(id(0.9, 0.1) == "NE-A5",   "the east limb at the equator is NE-A5", id(0.9, 0.1));
        Check(id(-0.1, 0.5) == "NW-C1",  "letters run away from the equator", id(-0.1, 0.5));
        Check(id(0.3, -0.9) == "SE-E2",  "numbers run away from the meridian", id(0.3, -0.9));
        Check(id(0.9, 0.9) == "none",    "the corner of the square is not Moon");

        double x = 0, y = 0;
        DiscXY(16.18, 59.10, x, y);      // Mare Crisium, from LunarFeatures.h
        Check(id(x, y) == "NE-B5", "Mare Crisium is on the EAST side, north of the equator", id(x, y));
        DiscXY(-43.30, -11.22, x, y);    // Tycho
        Check(id(x, y) == "SW-D1", "Tycho is south and a little west", id(x, y));
        DiscXY(9.62, -20.08, x, y);      // Copernicus
        Check(id(x, y) == "NW-A2", "Copernicus is west, just north", id(x, y));
    }

    printf("\n5. Where a section is aimed\n");
    {
        bool inCell = true, inRadius = true, back = true;
        for (const auto& s : g5) {
            if (s.aimX < s.x0 || s.aimX > s.x1 || s.aimY < s.y0 || s.aimY > s.y1) inCell = false;
            if (std::sqrt(s.aimX * s.aimX + s.aimY * s.aimY) > MOONMAP_MAX_AIM_RADIUS + 1e-9) inRadius = false;
            double x = 0, y = 0;
            DiscXY(s.latDeg, s.lonDeg, x, y);
            if (std::fabs(x - s.aimX) > 1e-9 || std::fabs(y - s.aimY) > 1e-9) back = false;
        }
        Check(inCell, "every aim point is inside its own section");
        Check(inRadius, "and inside 0.97 of the radius, off the very limb");
        Check(back, "latitude and longitude give the aim point back");

        // Serpentine: the mount steps to a neighbour, never across the disc.
        double worst = 0.0;
        for (size_t i = 1; i < g5.size(); ++i)
            worst = (std::max)(worst, std::hypot(g5[i].aimX - g5[i - 1].aimX, g5[i].aimY - g5[i - 1].aimY));
        Check(worst < 0.2 * 2.6, "no step in capture order is longer than 2.6 sections", N(worst / 0.2));
        Check(g5.front().aimY > 0.8 && g5.back().aimY < -0.8, "from the north limb to the south");
    }

    printf("\n6. Sunlight tonight\n");
    {
        LunarView v = LunarViewAt(2461000.5, LunarObserver{});
        const double sep = 0.6;   // every section is within this of the disc centre, degrees
        bool near = true;
        for (const auto& s : g5) {
            const SectionSky k = SectionSkyAt(v, s);
            if (std::fabs(k.decDeg - v.decDeg) > sep) near = false;
        }
        Check(near, "every section's coordinates are on the Moon");

        // The Sun behind the observer: full Moon.
        v.sunDir = (v.centre * -1.0).Unit();
        int lit = 0;
        for (const auto& s : g5) if (SectionSkyAt(v, s).Capturable()) ++lit;
        Check(lit == (int)g5.size(), "at full Moon every section is in sunlight", std::to_string(lit));

        // The Sun behind the Moon: new.
        v.sunDir = v.centre.Unit();
        lit = 0;
        double frac = 0.0;
        for (const auto& s : g5) { const auto k = SectionSkyAt(v, s); if (k.Capturable()) ++lit; frac += k.litFraction; }
        Check(lit == 0 && frac == 0.0, "at new Moon none is");

        // The Sun over longitude 100 east, a day before first quarter: the
        // terminator runs down longitude 10 east, through the first
        // column east of the meridian.
        v.sunDir = (v.axisX * std::cos(DegToRad(100.0)) + v.axisY * std::sin(DegToRad(100.0))).Unit();
        bool eastLit = true, westDark = true;
        int straddle = 0;
        lit = 0;
        for (const auto& s : g5) {
            const auto k = SectionSkyAt(v, s);
            if (k.Capturable()) ++lit;
            if (s.x0 >= 0.2 - 1e-9 && k.visible && !k.Capturable()) eastLit = false;
            if (s.x1 <= -0.2 + 1e-9 && k.litFraction > 0.0) westDark = false;
            if (k.litFraction > 0.0 && k.litFraction < 1.0) ++straddle;
        }
        Check(eastLit, "near first quarter the east, clear of the terminator, is lit");
        Check(westDark, "and the west is dark");
        Check(straddle > 0, "sections on the terminator are neither", std::to_string(straddle));
        Check(lit > (int)g5.size() / 4 && lit < (int)g5.size() / 2, "under half the map can be taken", std::to_string(lit));
    }

    printf("\n7. A project: complete only when every section is captured\n");
    {
        MoonMapProject p;
        p.Init("Moon 2026-11", 5, 2461000.5);
        Check(p.Valid() && p.Total() == (int)g5.size(), "a new project holds the whole grid");
        Check(p.Captured() == 0 && !p.Complete(), "and has captured nothing");

        MoonMapCapture c;
        c.section = "NE-Z9";
        Check(!p.AddCapture(c), "a capture of a section the grid has not got is refused");

        for (size_t i = 0; i + 1 < g5.size(); ++i) {
            c = MoonMapCapture{};
            c.section = g5[i].id; c.jdUt = 2461001.0 + i * 0.001; c.frames = 500;
            c.file = "D:\\RadukCapture\\captures\\MoonMap_Moon_2026-11\\Moon_" + g5[i].id + "_x.ser";
            p.AddCapture(c);
        }
        Check(p.Captured() == p.Total() - 1 && !p.Complete(), "one short of all is not complete");
        Check(p.Summary().find("READY") == std::string::npos, "and does not say ready", p.Summary());
        Check(!p.QuadrantComplete(g5.back().quad) && p.QuadrantComplete(MoonQuadrant::NE),
              "the quadrant with the gap is the one not done");

        c = MoonMapCapture{};
        c.section = g5.back().id; c.jdUt = 2461008.0; c.frames = 480; c.file = "C:\\a folder\\last one.ser";
        c.sunAltDeg = 12.5; c.colongitudeDeg = 33.25; c.librLonDeg = -4.5; c.librLatDeg = 6.25; c.frameW = 3840; c.frameH = 2160;
        p.AddCapture(c);
        Check(p.Complete(), "the last section completes it");
        Check(p.Summary().find("READY FOR MOSAIC") != std::string::npos, "and it says so", p.Summary());

        // A second, newer capture of one section.
        c = MoonMapCapture{};
        c.section = "NE-A1"; c.jdUt = 2461020.0; c.frames = 900; c.file = "D:\\newer.ser";
        p.AddCapture(c);
        Check(p.CaptureCount("NE-A1") == 2 && p.Captured() == p.Total(), "a second capture does not count twice");
        Check(p.Latest("NE-A1") && p.Latest("NE-A1")->file == "D:\\newer.ser", "the newest is the one listed");
        Check(p.Manifest().find("D:\\newer.ser") != std::string::npos &&
              p.Manifest().find("MISSING") == std::string::npos, "the stitcher's list has every section");

        // The file.
        MoonMapProject q;
        std::string why;
        Check(q.Parse(p.Serialize(), why), "a project reads its own file back", why);
        Check(q.Name() == p.Name() && q.PerSide() == 5 && q.Captures().size() == p.Captures().size() &&
              q.Complete() && std::fabs(q.CreatedJd() - 2461000.5) < 1e-6, "with the same name, grid and captures");
        const MoonMapCapture* l = q.Latest(g5.back().id);
        Check(l && l->file == "C:\\a folder\\last one.ser" && l->frames == 480 && l->frameW == 3840 &&
              std::fabs(l->sunAltDeg - 12.5) < 0.01 && std::fabs(l->librLonDeg + 4.5) < 0.01 &&
              std::fabs(l->colongitudeDeg - 33.25) < 0.01, "a path with spaces, and the light it was taken in");

        const auto dir = std::filesystem::temp_directory_path() / "raduk_moon_map_test";
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
        const std::string path = (dir / (MoonMapProject::SafeName("Moon 2026-11") + MoonMapProject::kExtension)).string();
        Check(p.Save(path) && p.Save(path), "saved, and saved again over itself");
        MoonMapProject r;
        Check(r.Load(path, why) && r.Complete(), "and loaded from disk", why);
        Check(!std::filesystem::exists(path + ".tmp"), "nothing half-written is left beside it");

        Check(q.Clear("NE-A1") && !q.IsCaptured("NE-A1") && !q.Complete(), "clearing a section takes the green away");
        Check(q.Manifest().find("NE-A1") != std::string::npos && q.Manifest().find("MISSING") != std::string::npos,
              "and the list then says which is missing");
        Check(!q.Clear("NE-A1"), "clearing it twice changes nothing");

        MoonMapProject bad;
        Check(!bad.Parse("something else\nname=x\n", why), "a file that is not a moon map is refused", why);
        Check(!bad.Parse(std::string(MoonMapProject::kMagic) + "\nname=x\n", why), "one with no grid size too", why);
        Check(!bad.Parse(std::string(MoonMapProject::kMagic) + "\nsections_per_side=2\ncapture=NE-C3|1|0|0|0|0|1|1|1|f.ser\n", why),
              "and one whose capture is not on its grid", why);
        Check(bad.Parse(std::string(MoonMapProject::kMagic) + "\nsections_per_side=2\nfuture_key=1\n", why) && bad.Total() == 16,
              "a key from a later version is passed over");
        Check(MoonMapProject::SafeName("Moon 2026-11: a/b") == "Moon_2026-11__a_b", "a name is made safe for a file");
        std::filesystem::remove_all(dir, ec);
    }

    Check(!MOON_MAP_PROVEN_ON_SKY, "the map does not claim to have seen the Moon");

    printf("\n%s\n", g_fail == 0 ? "ALL PASSED" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
