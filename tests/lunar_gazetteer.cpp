// ============================================================
//  The Moon map's local data (phase 162d)
//
//  src/sky/LunarGazetteer.h reads data/moon_gazetteer.csv, the IAU/USGS
//  names, and says what lies inside a section; src/gui/WicImage.h reads
//  data/moon_wac_4k.png, the LRO albedo map. Both are checked here on
//  the real files in the repository, and the gazetteer on text made by
//  hand as well.
//
//  THE ONES THAT MATTER:
//
//   2. Copernicus, Tycho and Mare Crisium land in the sections their
//      positions put them in, worked out by hand below. A feature in the
//      wrong square sends the section card's names to the wrong place.
//   5. Without the file the map still has names: the shortlist.
//   7. The albedo map is the right way round: Mare Crisium dark, the
//      southern highlands bright, longitude 0 in the middle.
// ============================================================

#include "sky/LunarGazetteer.h"
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

static const MoonSection* SectionNamed(const std::vector<MoonSection>& v, const std::string& id) {
    for (const auto& s : v) if (s.id == id) return &s;
    return nullptr;
}
static bool Holds(const std::vector<const GazetteerFeature*>& v, const std::string& name) {
    for (const auto* g : v) if (g->name == name) return true;
    return false;
}

int main() {
    const fs::path data = fs::path(__FILE__).parent_path().parent_path() / "data";

    printf("\n1. The file in the repository\n");
    LunarGazetteer gz;
    std::string why;
    Check(gz.Load((data / "moon_gazetteer.csv").string(), why) && gz.FromFile(), "data/moon_gazetteer.csv is read", why);
    Check(gz.Size() > 9000, "with every adopted name, over 9000", std::to_string(gz.Size()));
    bool sorted = true;
    for (size_t i = 1; i < gz.All().size(); ++i) if (gz.All()[i].diameterKm > gz.All()[i - 1].diameterKm) sorted = false;
    Check(sorted && gz.All().front().name == "Oceanus Procellarum", "largest first, Oceanus Procellarum at the top", gz.All().front().name);
    const auto cop = gz.Search("Copernicus");
    Check(!cop.empty() && cop.front()->name == "Copernicus", "Copernicus is found by name");
    // The same place as the GoTo shortlist, which was read from the same gazetteer.
    double sl = 0, so = 0;
    for (const auto& f : kLunarFeatures) if (std::string(f.name) == "Copernicus") { sl = f.latDeg; so = f.lonDeg; }
    Check(!cop.empty() && std::fabs(cop.front()->latDeg - sl) < 0.01 && std::fabs(cop.front()->lonDeg - so) < 0.01,
          "where the shortlist has it (9.62 N, 20.08 W)");
    Check(!cop.empty() && cop.front()->type == "Crater" && !cop.front()->origin.empty(),
          "a crater, with what its name honours", cop.empty() ? "" : cop.front()->origin);

    printf("\n2. Features in the right sections (4 a side)\n");
    const auto secs = BuildMoonSections(4);
    // By hand: Copernicus x = cos 9.62 sin -20.08 = -0.338, y = sin 9.62 = 0.167:
    // west, north, column 1, row 0. Tycho x = -0.141, y = -0.686: SW, column 0,
    // row 2. Mare Crisium x = 0.824, y = 0.279: NE, column 3, row 1.
    const struct { const char* name; const char* section; } cases[] = {
        {"Copernicus", "NW-A2"}, {"Tycho", "SW-C1"}, {"Mare Crisium", "NE-B4"}};
    for (const auto& c : cases) {
        const MoonSection* s = SectionNamed(secs, c.section);
        Check(s && Holds(gz.FeaturesIn(*s), c.name), std::string(c.name) + " is in " + c.section);
        int in = 0;
        for (const auto& t : secs) if (Holds(gz.FeaturesIn(t), c.name)) ++in;
        Check(in == 1, std::string("and in no other section"), std::to_string(in));
    }
    const MoonSection* nw = SectionNamed(secs, "NW-A2");
    const auto top = nw ? gz.FeaturesIn(*nw, 3, 20.0) : std::vector<const GazetteerFeature*>{};
    bool big = top.size() == 3;
    for (size_t i = 1; i < top.size(); ++i) if (top[i]->diameterKm > top[i - 1]->diameterKm) big = false;
    for (const auto* g : top) if (g->diameterKm < 20.0) big = false;
    Check(big, "a card's three biggest, 20 km and up, largest first",
          top.empty() ? "" : top[0]->name + ", " + (top.size() > 1 ? top[1]->name : ""));

    printf("\n3. Near side and far side\n");
    size_t nearCount = 0, placed = 0;
    for (const auto& g : gz.All()) {
        double x, y;
        if (!LunarGazetteer::DiscXY(g.latDeg, g.lonDeg, x, y)) continue;
        ++nearCount;
        for (const auto& s : secs)
            if (x >= s.x0 && x < s.x1 && y >= s.y0 && y < s.y1) { ++placed; break; }
    }
    Check(nearCount > 4000 && placed >= nearCount * 99 / 100, "99% of near-side names fall in a section; the rest in limb slivers",
          std::to_string(placed) + " of " + std::to_string(nearCount));
    const auto farSide = gz.Search("Tsiolkovskiy");
    double fx, fy;
    Check(!farSide.empty() && !LunarGazetteer::DiscXY(farSide.front()->latDeg, farSide.front()->lonDeg, fx, fy),
          "Tsiolkovskiy is on the far side");
    int farIn = 0;
    for (const auto& s : secs) if (!farSide.empty() && Holds(gz.FeaturesIn(s), farSide.front()->name)) ++farIn;
    Check(farIn == 0, "and in no section");

    printf("\n4. Nearest and search\n");
    const auto* n1 = gz.Nearest(9.7, -20.1, 50.0);
    Check(n1 && n1->name == "Copernicus", "nearest 50 km feature to 9.7 N 20.1 W is Copernicus", n1 ? n1->name : "");
    const auto* n2 = gz.Nearest(9.7, -20.1);
    Check(n2 && std::fabs(n2->latDeg - 9.7) < 0.6 && std::fabs(n2->lonDeg + 20.1) < 0.6,
          "with no size floor, the nearest name of any size, within half a degree", n2 ? n2->name : "");
    const auto t = gz.Search("tycho");
    Check(!t.empty() && t.front()->name == "Tycho", "search ignores case and puts the exact name first");
    const auto mare = gz.Search("Mare ", 3);
    Check(mare.size() == 3 && mare[0]->name == "Mare Frigoris", "names that begin with it, largest first", mare.empty() ? "" : mare[0]->name);

    printf("\n5. Without the file\n");
    LunarGazetteer none;
    Check(!none.Load((data / "no_such_file.csv").string(), why) && !none.FromFile(), "a missing file says why", why);
    Check(none.Size() == sizeof(kLunarFeatures) / sizeof(kLunarFeatures[0]), "and the shortlist is used",
          std::to_string(none.Size()));
    Check(nw && Holds(none.FeaturesIn(*nw), "Copernicus"), "which still puts Copernicus in NW-A2");

    printf("\n6. The CSV as written by hand\n");
    LunarGazetteer h;
    const std::string text =
        "name,lat,lon,diameter_km,type,origin\r\n"
        "Small,1,2,3,Crater,plain\r\n"
        "\"Comma, Inside\",10,20,50,Crater,\"Said \"\"hello\"\".\"\r\n"
        "Two Lines,0,0,10,Mons,\"first\nsecond\"\n";
    Check(h.Parse(text, why) && h.Size() == 3, "three rows, with CRLF and LF", why);
    Check(h.All()[0].name == "Comma, Inside" && h.All()[0].origin == "Said \"hello\".", "a comma and doubled quotes inside quotes");
    Check(h.All()[1].origin == "first\nsecond", "a line break inside quotes is kept");
    LunarGazetteer bad;
    Check(!bad.Parse("hello,world\n1,2\n", why) && !bad.FromFile() && bad.Size() > 0, "a file that is not the gazetteer is refused", why);
    Check(!bad.Parse("name,lat,lon,diameter_km,type\nOnly,1,2\n", why), "a short row is refused", why);

    printf("\n7. The albedo map\n");
    raduk::gui::WicPixels img;
    const bool ok = raduk::gui::LoadImageFile((data / "moon_wac_4k.png").wstring(), true, img, why);
    Check(ok && img.width == 4096 && img.height == 2048 && img.bytesPerPixel == 1, "data/moon_wac_4k.png decodes to 4096 x 2048 grey", why);
    if (ok) {
        auto mean = [&](double lat, double lon) {
            const int cx = (int)((lon + 180.0) / 360.0 * img.width), cy = (int)((90.0 - lat) / 180.0 * img.height);
            double s = 0; int n = 0;
            for (int dy = -6; dy <= 6; ++dy) for (int dx = -6; dx <= 6; ++dx) { s += img.Grey(cx + dx, cy + dy); ++n; }
            return s / n;
        };
        const double crisium = mean(16.2, 59.1), highland = mean(-30.0, 10.0), imbrium = mean(34.7, -14.9);
        char b[96];
        snprintf(b, sizeof(b), "Crisium %.0f, Imbrium %.0f, highlands %.0f", crisium, imbrium, highland);
        Check(crisium + 30 < highland && imbrium + 30 < highland, "maria dark, highlands bright, where lat and lon say", b);
        // Mirror the map east to west and Crisium would land at 59 W, in
        // Oceanus Procellarum's dark; north to south, in the highlands.
        Check(mean(-16.2, 59.1) > crisium + 30, "and not upside down");
    }
    Check(!raduk::gui::LoadImageFile((data / "no_such.png").wstring(), true, img, why) && !img.Valid(), "a missing image says why", why);

    printf("\n%s\n", g_fail == 0 ? "ALL PASSED" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
