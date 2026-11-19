// ============================================================
//  A captured section's picture (phase 162e)
//
//  src/core/MoonTile.h picks the sharpest of a recording's frames, crops
//  the middle square and scales it; src/gui/MoonTileMaker.h writes it as
//  a PNG with WIC. Checked on planes made here and on a small SER written
//  here byte by byte from the format, so the reader is not marking its
//  own homework.
//
//  THE ONES THAT MATTER:
//
//   2. The sharp frame is picked over blurred ones of the same scene,
//      however bright. A map of soft tiles would say the nights were bad.
//   4. The tile round-trips through the PNG the map reads back.
// ============================================================

#include "core/MoonTile.h"
#include "gui/MoonTileMaker.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace raduk::core;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) ++g_fail;
}

// A scene of craters: dark discs on a bright ground, with fine texture.
static std::vector<uint16_t> Scene(int w, int h, double blur, double gain) {
    std::vector<uint16_t> p((size_t)w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double v = 0.55;
            const double cxs[] = {0.3, 0.7, 0.5}, cys[] = {0.3, 0.6, 0.8}, rs[] = {0.12, 0.08, 0.05};
            for (int k = 0; k < 3; ++k) {
                const double d = std::hypot(x / (double)w - cxs[k], y / (double)h - cys[k]) - rs[k];
                v -= 0.35 / (1.0 + std::exp(d * (blur > 0 ? 60.0 / blur : 600.0)));
            }
            if (blur == 0) v += 0.08 * (((x / 3) + (y / 3)) % 2 ? 1 : -1);   // grain only when sharp
            p[(size_t)y * w + x] = (uint16_t)std::lround(std::clamp(v * gain, 0.0, 1.0) * 65535.0);
        }
    return p;
}

// A SER file, as the specification lays it out: 178 bytes of header,
// then the frames, 8-bit mono here.
static bool WriteSer(const fs::path& path, int w, int h, const std::vector<std::vector<uint8_t>>& frames) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.string().c_str(), "wb") != 0 || !f) return false;
    uint8_t hdr[178] = {};
    std::memcpy(hdr, "LUCAM-RECORDER", 14);
    auto i32 = [&](int off, int32_t v) { std::memcpy(hdr + off, &v, 4); };
    i32(14, 0);                 // LuID
    i32(18, 0);                 // ColorID: mono
    i32(22, 0);                 // LittleEndian (as most writers set it)
    i32(26, w); i32(30, h);
    i32(34, 8);                 // PixelDepthPerPlane
    i32(38, (int32_t)frames.size());
    fwrite(hdr, 1, sizeof(hdr), f);
    for (const auto& fr : frames) fwrite(fr.data(), 1, fr.size(), f);
    fclose(f);
    return true;
}

static std::vector<uint8_t> To8(const std::vector<uint16_t>& p) {
    std::vector<uint8_t> o(p.size());
    for (size_t i = 0; i < p.size(); ++i) o[i] = (uint8_t)(p[i] >> 8);
    return o;
}

int main() {
    const int W = 320, H = 240;

    printf("\n1. Sharpness\n");
    const auto sharp = Scene(W, H, 0, 1.0), soft = Scene(W, H, 4, 1.0), dim = Scene(W, H, 0, 0.4);
    const double s1 = SurfaceSharpness(sharp, W, H), s2 = SurfaceSharpness(soft, W, H), s3 = SurfaceSharpness(dim, W, H);
    char b[96];
    snprintf(b, sizeof(b), "sharp %.4f, soft %.4f, sharp and dim %.4f", s1, s2, s3);
    Check(s1 > 2.0 * s2, "a sharp frame scores well above a blurred one", b);
    Check(std::fabs(s3 - s1) < 0.25 * s1, "and the score does not follow the brightness", b);
    Check(SurfaceSharpness(std::vector<uint16_t>((size_t)W * H, 30000), W, H) == 0.0, "a flat frame scores 0");

    printf("\n2. The tile of a plane\n");
    MoonTile t;
    Check(TileFromPlane(sharp, W, H, 64, t) && t.Valid() && t.size == 64, "64 x 64 from 320 x 240");
    int lo = 255, hi = 0;
    for (uint8_t v : t.grey) { lo = (std::min)(lo, (int)v); hi = (std::max)(hi, (int)v); }
    Check(lo <= 2 && hi >= 250, "stretched black to white", std::to_string(lo) + ".." + std::to_string(hi));
    // The middle square: the crater at x 0.3 of a 4:3 frame is at
    // (0.3 * 320 - 40) / 240 = 0.23 of the square, and dark.
    Check(t.grey[(size_t)(0.3 * 64) * 64 + (size_t)(0.23 * 64)] < 100, "the middle square, cropped from the centre");

    printf("\n3. The sharpest frame of a recording\n");
    const fs::path dir = fs::temp_directory_path() / "raduk_moon_tile";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path ser = dir / "Moon_NE-A1_test.ser";
    Check(WriteSer(ser, W, H, {To8(Scene(W, H, 4, 1.0)), To8(Scene(W, H, 3, 0.9)), To8(Scene(W, H, 0, 0.8)),
                               To8(Scene(W, H, 5, 1.0))}), "a four-frame SER is written");
    std::string why;
    MoonTile r;
    Check(MakeMoonTile(ser.string(), 128, r, why) && r.Valid(), "its tile is made", why);
    Check(r.frame == 2, "from frame 2, the sharp one, though it is the dimmest", std::to_string(r.frame));
    MoonTile none;
    Check(!MakeMoonTile((dir / "absent.ser").string(), 128, none, why) && !none.Valid(), "a missing recording says why", why);
    Check(MeanLevel(Scene(W, H, 0, 0.4)) > 4 * kMinLevelFraction, "the dimmest Moon frame here is well clear of the floor");
    // Black sky with a little noise: a capture of nothing.
    std::vector<uint8_t> sky((size_t)W * H);
    for (size_t i = 0; i < sky.size(); ++i) sky[i] = (uint8_t)(3 + ((i * 2654435761u >> 28) & 3));   // 3..6 DN of 255
    const fs::path empty = dir / "Moon_SW-A1_empty.ser";
    Check(WriteSer(empty, W, H, {sky, sky, sky}) && fs::file_size(empty) == 178 + 3ull * W * H, "a SER of empty sky is written");
    Check(!MakeMoonTile(empty.string(), 128, none, why) && why.find("no surface") != std::string::npos,
          "a recording of empty sky makes no tile: a black square would look like a hole", why);

    printf("\n4. The PNG the map reads back\n");
    const fs::path png = dir / "tiles" / "NE-A1.png";
    Check(raduk::gui::MoonTileMaker::Make({"NE-A1", ser.string(), png.string()}, why), "the maker writes it, folder and all", why);
    raduk::gui::WicPixels px;
    Check(raduk::gui::LoadImageFile(png.wstring(), true, px, why) && px.width == raduk::gui::MoonTileMaker::kTileSize &&
          px.height == raduk::gui::MoonTileMaker::kTileSize, "it reads back at 256 x 256", why);
    MoonTile direct;
    MakeMoonTile(ser.string(), raduk::gui::MoonTileMaker::kTileSize, direct, why);
    Check(px.Valid() && direct.Valid() && px.data == direct.grey, "pixel for pixel");
    Check(!fs::exists(png.string() + ".tmp"), "and no half-written file is left");

    printf("\n5. In the background\n");
    {
        int made = 0, failed = 0;
        std::mutex mx;
        std::condition_variable cv;
        {
            raduk::gui::MoonTileMaker maker([&](bool ok, const raduk::gui::MoonTileMaker::Job&, const std::string&) {
                std::lock_guard<std::mutex> l(mx);
                (ok ? made : failed)++;
                cv.notify_all();
            });
            maker.Add({"NW-A1", ser.string(), (dir / "tiles" / "NW-A1.png").string()});
            maker.Add({"SW-A1", (dir / "absent.ser").string(), (dir / "tiles" / "SW-A1.png").string()});
            std::unique_lock<std::mutex> l(mx);
            cv.wait_for(l, std::chrono::seconds(20), [&] { return made + failed == 2; });
        }   // the destructor joins
        Check(made == 1 && failed == 1, "one made, one refused, both reported",
              std::to_string(made) + " made, " + std::to_string(failed) + " failed");
        Check(fs::exists(dir / "tiles" / "NW-A1.png") && !fs::exists(dir / "tiles" / "SW-A1.png"), "on disk as said");
    }

    fs::remove_all(dir, ec);
    printf("\n%s\n", g_fail == 0 ? "ALL PASSED" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
