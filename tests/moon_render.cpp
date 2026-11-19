// ============================================================
//  Tonight's Moon under the map's grid (phase 162e)
//
//  src/sky/MoonRender.h paints the zero-libration disc as albedo times
//  tonight's sunlight. Checked on a flat grey Moon, where the light alone
//  shows, and on the real LRO map in data/.
//
//  THE ONES THAT MATTER:
//
//   4. At first quarter the terminator is on the central meridian: east
//      lit, west dark. Drawn the wrong way round, the picture would argue
//      with the map's own colours.
//   5. The picture agrees with the map: every section the map calls "in
//      sunlight" is drawn lit, and every section with no light on it is
//      drawn dark. They come from the same sunDir and must not disagree.
// ============================================================

#include "sky/MoonRender.h"
#include "gui/WicImage.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace raduk::sky;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) ++g_fail;
}

// A real evening's geometry, with the Sun put where a test wants it.
static LunarView ViewWithSun(int light) {
    LunarView v = LunarViewAt(2461000.5, LunarObserver{});
    if (light == 0) v.sunDir = (v.centre * -1.0).Unit();   // full
    if (light == 1) v.sunDir = v.axisY;                     // first quarter: Sun over 90 E
    if (light == 2) v.sunDir = v.centre.Unit();             // new
    return v;
}

int main() {
    const int W = 401, H = 401;
    const double cx = 200.5, cy = 200.5, r = 180.0;

    printf("\n1. Where each pixel is\n");
    MoonRender m;
    m.Layout(W, H, cx, cy, r);
    const double area = 3.14159265358979 * r * r;
    Check(std::fabs(m.DiscPixels() - area) / area < 0.02, "the disc covers pi r^2 pixels",
          std::to_string(m.DiscPixels()) + " of " + std::to_string((int)area));
    double x, y, lat, lon;
    MoonRender::PixelToDisc(cx - 0.5 + 0.5 * r, cy - 0.5, cx, cy, r, x, y);
    DiscToSelenographic(x, y, lat, lon);
    Check(std::fabs(lat) < 0.5 && std::fabs(lon - 30.0) < 0.5, "half way to the east limb on the equator is 30 E",
          std::to_string(lat) + ", " + std::to_string(lon));
    MoonRender::PixelToDisc(cx - 0.5, cy - 0.5 - 0.5 * r, cx, cy, r, x, y);
    DiscToSelenographic(x, y, lat, lon);
    Check(std::fabs(lat - 30.0) < 0.5 && std::fabs(lon) < 0.5, "half way up is 30 N, so north is up", std::to_string(lat));

    std::vector<uint8_t> grey, alpha;
    auto at = [&](double dx, double dy) {      // disc units to the pixel's grey
        const int px = (int)std::floor(cx + dx * r), py = (int)std::floor(cy - dy * r);
        return (int)grey[(size_t)py * W + px];
    };

    printf("\n2. Full Moon\n");
    m.Shade(ViewWithSun(0), grey, alpha);
    size_t inside = 0, lit = 0;
    for (size_t i = 0; i < grey.size(); ++i)
        if (alpha[i] == 255) { ++inside; if (grey[i] >= MoonRender::kFlatAlbedo - 2) ++lit; }
    Check(inside > 0 && lit >= inside * 9 / 10, "nearly every pixel lit; the rest turned away by libration",
          std::to_string(lit) + " of " + std::to_string(inside));
    Check(at(0.75, 0.75) == 0 && alpha[0] == 0, "and nothing outside the disc");

    printf("\n3. New Moon\n");
    m.Shade(ViewWithSun(2), grey, alpha);
    // Libration turns a sliver near the limb towards the Sun but away from
    // us: lit, and drawn at the past-limb level. Nothing we face is lit.
    const int shine = (int)std::lround(MoonRender::kFlatAlbedo * MoonRender::kEarthshine);
    const int pastLimbLit = (int)std::lround(MoonRender::kFlatAlbedo * MoonRender::kPastLimb);
    size_t faced = 0, sliver = 0, wrong = 0;
    for (size_t i = 0; i < grey.size(); ++i) {
        if (alpha[i] != 255) continue;
        if (grey[i] <= shine + 1) ++faced;
        else if (grey[i] <= pastLimbLit + 1) ++sliver;
        else ++wrong;
    }
    Check(wrong == 0 && sliver < faced / 20, "nothing we face is brighter than earthshine",
          std::to_string(sliver) + " lit past the limb, " + std::to_string(wrong) + " wrong");
    Check(at(0, 0) > 0, "but the disc is still there, faintly");

    printf("\n4. First quarter\n");
    m.Shade(ViewWithSun(1), grey, alpha);
    Check(at(0.3, 0) >= MoonRender::kFlatAlbedo - 2, "the east half is lit", std::to_string(at(0.3, 0)));
    Check(at(-0.3, 0) <= 13, "the west half is dark", std::to_string(at(-0.3, 0)));
    // Walk the equator from west to east: the first lit pixel is at the meridian.
    double firstLit = 9;
    for (int px = (int)(cx - r); px < (int)(cx + r); ++px)
        if (grey[(size_t)(int)cy * W + px] > MoonRender::kFlatAlbedo / 2) { firstLit = (px + 0.5 - cx) / r; break; }
    Check(std::fabs(firstLit) < 0.05, "and the terminator crosses the equator at the central meridian",
          std::to_string(firstLit));
    Check(at(0.3, 0.5) >= MoonRender::kFlatAlbedo - 2 && at(-0.3, -0.5) <= 13, "north and south alike");

    printf("\n5. The picture agrees with the map\n");
    for (int light : {0, 1}) {
        const LunarView v = ViewWithSun(light);
        m.Shade(v, grey, alpha);
        int bad = 0, sections = 0;
        for (const auto& s : BuildMoonSections(5)) {
            const SectionSky k = SectionSkyAt(v, s);
            if (!k.visible) continue;
            ++sections;
            const int g = at(s.aimX, s.aimY);
            if (k.Capturable() && g < MoonRender::kFlatAlbedo * 0.9) ++bad;
            if (k.litFraction == 0.0 && g > MoonRender::kFlatAlbedo * MoonRender::kEarthshine + 1) ++bad;
        }
        Check(sections > 40 && bad == 0, std::string(light ? "first quarter" : "full Moon") +
              ": in sunlight is drawn lit, unlit is drawn dark", std::to_string(bad) + " of " + std::to_string(sections));
    }

    printf("\n6. Panned and zoomed\n");
    MoonRender z;
    z.Layout(W, H, 50.0, 400.0, 800.0);          // most of the disc off the picture
    z.Shade(ViewWithSun(0), grey, alpha);
    Check(z.DiscPixels() > 0 && z.DiscPixels() <= (size_t)W * H && grey.size() == (size_t)W * H,
          "only the pixels in the picture are laid out", std::to_string(z.DiscPixels()));

    printf("\n7. On the LRO map\n");
    const fs::path data = fs::path(__FILE__).parent_path().parent_path() / "data";
    raduk::gui::WicPixels img;
    std::string why;
    if (!raduk::gui::LoadImageFile((data / "moon_wac_4k.png").wstring(), true, img, why)) {
        Check(false, "data/moon_wac_4k.png loads", why);
    } else {
        MoonRender real;
        real.SetAlbedo(img.width, img.height, std::move(img.data));
        Check(real.HasAlbedo(), "the albedo map is taken");
        Check(std::fabs(real.AlbedoAt(16.2, 59.1) - real.AlbedoAt(-30.0, 10.0)) > 30, "it tells a sea from the highlands");
        real.Layout(W, H, cx, cy, r);
        real.Shade(ViewWithSun(0), grey, alpha);
        double x2, y2;
        // Mare Crisium (16.2 N, 59.1 E) and the southern highlands (30 S, 10 E).
        auto discOf = [](double la, double lo, double& dx, double& dy) {
            const double d2r = 3.14159265358979 / 180.0;
            dx = std::cos(la * d2r) * std::sin(lo * d2r); dy = std::sin(la * d2r);
        };
        discOf(16.2, 59.1, x2, y2);
        const int crisium = at(x2, y2);
        discOf(-30.0, 10.0, x2, y2);
        const int highland = at(x2, y2);
        Check(crisium + 30 < highland, "at full Moon Mare Crisium is drawn dark and the highlands bright",
              std::to_string(crisium) + " vs " + std::to_string(highland));
    }

    printf("\n%s\n", g_fail == 0 ? "ALL PASSED" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
