#pragma once
// ============================================================
//  MoonTile — a small picture of a captured section (phase 162e)
//
//  When a section of the Moon map is captured, its cell on the map shows
//  the owner's own picture of it, so over the nights the map turns into
//  his photograph of the Moon. The picture is one frame of the recording,
//  the sharpest of a sample, cropped to a square from the middle and
//  scaled down: a progress picture, not the mosaic, which is stacked and
//  stitched by other programs.
//
//  Sharpness here is the surface's: mean squared gradient over the mean
//  squared, on the frame binned 2 x 2 (which also averages a colour
//  camera's Bayer pattern away). SerScan's score looks for a planet on
//  black sky, and a Moon section fills the frame.
//
//  Pure but for reading the SER: tests/moon_tile.cpp.
// ============================================================

#include "SerReader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace raduk::core {

struct MoonTile {
    int size = 0;                    // square, size x size
    std::vector<uint8_t> grey;
    int64_t frame = -1;              // which frame of the recording
    bool Valid() const { return size > 0 && grey.size() == (size_t)size * size; }
};

// 2 x 2 binning of a 16-bit plane. w and h are halved (rounded down).
inline void Bin2(const std::vector<uint16_t>& p, int w, int h, std::vector<float>& out, int& bw, int& bh) {
    bw = w / 2; bh = h / 2;
    out.assign((size_t)bw * bh, 0.0f);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) {
            const size_t i = (size_t)(2 * y) * w + 2 * x;
            out[(size_t)y * bw + x] = 0.25f * ((float)p[i] + p[i + 1] + p[i + w] + p[i + w + 1]);
        }
}

// Larger is sharper, whatever the brightness. 0 for a flat or empty frame.
inline double SurfaceSharpness(const std::vector<uint16_t>& p, int w, int h) {
    if (w < 8 || h < 8 || p.size() < (size_t)w * h) return 0.0;
    std::vector<float> b; int bw, bh;
    Bin2(p, w, h, b, bw, bh);
    double sum = 0.0, g2 = 0.0;
    size_t n = 0;
    // Every other row: plenty for a ranking, half the time.
    for (int y = 1; y < bh - 1; y += 2)
        for (int x = 1; x < bw - 1; ++x) {
            const float c = b[(size_t)y * bw + x];
            const float gx = b[(size_t)y * bw + x + 1] - b[(size_t)y * bw + x - 1];
            const float gy = b[(size_t)(y + 1) * bw + x] - b[(size_t)(y - 1) * bw + x];
            g2 += (double)gx * gx + (double)gy * gy;
            sum += c;
            ++n;
        }
    if (n == 0) return 0.0;
    const double mean = sum / n;
    return mean > 1.0 ? (g2 / n) / (mean * mean) : 0.0;
}

// The middle square of a plane, scaled to size x size by averaging,
// stretched so 0.5% of it is black and 0.2% white.
inline bool TileFromPlane(const std::vector<uint16_t>& p, int w, int h, int size, MoonTile& out) {
    out = MoonTile{};
    if (size <= 0 || w < 2 || h < 2 || p.size() < (size_t)w * h) return false;
    std::vector<float> b; int bw, bh;
    Bin2(p, w, h, b, bw, bh);
    const int side = (std::min)(bw, bh);
    const int x0 = (bw - side) / 2, y0 = (bh - side) / 2;
    std::vector<float> t((size_t)size * size, 0.0f);
    for (int ty = 0; ty < size; ++ty) {
        const int sy0 = y0 + (int)((int64_t)ty * side / size), sy1 = (std::max)(sy0 + 1, y0 + (int)((int64_t)(ty + 1) * side / size));
        for (int tx = 0; tx < size; ++tx) {
            const int sx0 = x0 + (int)((int64_t)tx * side / size), sx1 = (std::max)(sx0 + 1, x0 + (int)((int64_t)(tx + 1) * side / size));
            double s = 0.0; int n = 0;
            for (int y = sy0; y < sy1 && y < bh; ++y)
                for (int x = sx0; x < sx1 && x < bw; ++x) { s += b[(size_t)y * bw + x]; ++n; }
            t[(size_t)ty * size + tx] = n ? (float)(s / n) : 0.0f;
        }
    }
    std::vector<float> sorted = t;
    std::sort(sorted.begin(), sorted.end());
    const float lo = sorted[(size_t)(0.005 * (sorted.size() - 1))];
    const float hi = sorted[(size_t)(0.998 * (sorted.size() - 1))];
    const float span = (std::max)(1.0f, hi - lo);
    out.size = size;
    out.grey.resize(t.size());
    for (size_t i = 0; i < t.size(); ++i)
        out.grey[i] = (uint8_t)std::lround(std::clamp((t[i] - lo) / span, 0.0f, 1.0f) * 255.0f);
    return true;
}

// A frame darker than this, on average, has no lit surface in it: black
// sky, cloud, a lens cap. The Moon's lit surface is tens of percent of
// full scale at any exposure the Moon profile uses. Not a sharpness
// floor: sharpness is divided by the mean squared, so noise on a black
// frame scores high.
inline constexpr double kMinLevelFraction = 0.04;

inline double MeanLevel(const std::vector<uint16_t>& p) {
    if (p.empty()) return 0.0;
    double s = 0.0;
    for (size_t i = 0; i < p.size(); i += 7) s += p[i];      // a sample is plenty
    return s / ((p.size() + 6) / 7) / 65535.0;
}

// The tile of a recording: the sharpest of up to `samples` frames spread
// through it. False with the reason.
inline bool MakeMoonTile(const std::string& serPath, int size, MoonTile& out, std::string& why, int samples = 24) {
    out = MoonTile{};
    ser::Reader r;
    if (!r.Open(serPath)) { why = r.GetInfo().error.empty() ? "could not open " + serPath : r.GetInfo().error; return false; }
    const ser::Info& in = r.GetInfo();
    if (in.frames <= 0) { why = "no frames in " + serPath; return false; }
    const int64_t n = in.frames;
    const int k = (int)(std::min<int64_t>)(n, (std::max)(1, samples));
    std::vector<uint8_t> raw;
    std::vector<uint16_t> plane, best;
    double bestScore = -1.0;
    int64_t bestFrame = -1;
    for (int i = 0; i < k; ++i) {
        const int64_t f = (int64_t)((double)(i + 0.5) * n / k);
        if (!r.ReadFrame(f, raw)) continue;
        ser::ToPlane16(raw.data(), in, plane);
        if (MeanLevel(plane) < kMinLevelFraction) continue;   // no lit surface in it
        const double s = SurfaceSharpness(plane, in.width, in.height);
        if (s > bestScore) { bestScore = s; bestFrame = f; best.swap(plane); }
    }
    // Cloud, a capture of the sky beside the Moon, a lens cap: nothing to
    // show. A black square on the map would look like a hole in the Moon;
    // the cell stays plain green instead.
    if (bestFrame < 0) { why = "the recording shows no surface: no frame sampled is bright enough"; return false; }
    if (!TileFromPlane(best, in.width, in.height, size, out)) { why = "the frame is too small"; return false; }
    out.frame = bestFrame;
    return true;
}

} // namespace raduk::core
