// ============================================================
//  MoonRender — tonight's Moon, drawn under the map's grid (phase 162e)
//
//  The Moon map's disc is the Moon at zero libration, north up, Mare
//  Crisium's side to the right (MoonMap.h). This paints that disc as it
//  looks tonight:
//
//    brightness = albedo x sunlight
//
//  The albedo is the LRO WAC map (data/moon_wac_4k.png, equirectangular),
//  sampled at each pixel's selenographic latitude and longitude. The
//  sunlight is the body normal dotted with LunarView::sunDir, so the
//  terminator falls where LunarSpotAt says the Sun is on the horizon:
//  the same arithmetic that decides which sections are "in sunlight".
//  The night side keeps a little earthshine so its seas still show, and
//  a place turned away from us tonight by libration is dimmed.
//
//  Two steps, so panning and a new evening cost what they should:
//    Layout()  where each output pixel is on the Moon; trig per pixel,
//              redone only when the disc moves or is resized
//    Shade()   tonight's light; a dot product per pixel
//
//  Pure arithmetic: tests/moon_render.cpp.
// ============================================================

#pragma once

#include "LunarGeometry.h"
#include "MoonMap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace raduk::sky {

class MoonRender {
public:
    // Light on the night side, as a share of the albedo: earthshine, so
    // the dark seas still read. Faint enough that nobody mistakes it for
    // sunlight.
    static constexpr double kEarthshine = 0.07;
    // A place libration turns away from us tonight keeps this much.
    static constexpr double kPastLimb = 0.45;
    // Albedo used when there is no map: a flat grey Moon, still lit.
    static constexpr uint8_t kFlatAlbedo = 170;

    // The albedo map: grey, equirectangular, longitude 0 in the middle,
    // north up. Taken by value; the caller's buffer may go.
    void SetAlbedo(int w, int h, std::vector<uint8_t> grey) {
        if (w <= 0 || h <= 0 || grey.size() < (size_t)w * h) { m_aw = m_ah = 0; m_albedo.clear(); }
        else { m_aw = w; m_ah = h; m_albedo = std::move(grey); }
        m_w = -1;                       // the layout samples it: redo
    }
    bool HasAlbedo() const { return m_aw > 0; }

    // The albedo at a place, bilinear, 0..255.
    double AlbedoAt(double latDeg, double lonDeg) const {
        if (!HasAlbedo()) return kFlatAlbedo;
        double u = (lonDeg + 180.0) / 360.0 * m_aw - 0.5;
        double v = (90.0 - latDeg) / 180.0 * m_ah - 0.5;
        v = (std::min)((std::max)(v, 0.0), (double)m_ah - 1.0);
        const int u0 = (int)std::floor(u), v0 = (int)std::floor(v);
        const double fu = u - u0, fv = v - v0;
        auto at = [&](int x, int y) {
            x = ((x % m_aw) + m_aw) % m_aw;          // longitude wraps
            y = (std::min)((std::max)(y, 0), m_ah - 1);
            return (double)m_albedo[(size_t)y * m_aw + x];
        };
        return (at(u0, v0) * (1 - fu) + at(u0 + 1, v0) * fu) * (1 - fv) +
               (at(u0, v0 + 1) * (1 - fu) + at(u0 + 1, v0 + 1) * fu) * fv;
    }

    // Output pixel (px, py) to the disc: x east, y north, radius 1.
    static void PixelToDisc(double px, double py, double cx, double cy, double r, double& x, double& y) {
        x = (px + 0.5 - cx) / r;
        y = (cy - (py + 0.5)) / r;
    }

    // Where each pixel of a w x h output is on the Moon, for a disc of
    // radius r pixels centred at (cx, cy). Does nothing when nothing moved.
    void Layout(int w, int h, double cx, double cy, double r) {
        if (w == m_w && h == m_h && cx == m_cx && cy == m_cy && r == m_r) return;
        m_w = w; m_h = h; m_cx = cx; m_cy = cy; m_r = r;
        m_px.clear();
        if (w <= 0 || h <= 0 || r <= 0.0) return;
        // Only the rows and columns the disc can reach.
        const int x0 = (std::max)(0, (int)std::floor(cx - r - 1)), x1 = (std::min)(w, (int)std::ceil(cx + r + 1));
        const int y0 = (std::max)(0, (int)std::floor(cy - r - 1)), y1 = (std::min)(h, (int)std::ceil(cy + r + 1));
        const double edge = 1.0 / r;                  // one pixel, in disc units
        for (int py = y0; py < y1; ++py)
            for (int px = x0; px < x1; ++px) {
                double x, y;
                PixelToDisc(px, py, cx, cy, r, x, y);
                const double rr = std::sqrt(x * x + y * y);
                if (rr > 1.0 + edge) continue;
                Px p;
                p.index = (uint32_t)((size_t)py * w + px);
                // A pixel the limb crosses is part-covered.
                const double cover = std::clamp((1.0 + edge - rr) / (2.0 * edge), 0.0, 1.0);
                p.cover = (uint8_t)std::lround(cover * 255.0);
                if (rr > 1.0) { x /= rr; y /= rr; }
                // Body axes at zero libration: X at the sub-Earth point
                // (out of the disc), Y east, Z north.
                const double z = std::sqrt((std::max)(0.0, 1.0 - x * x - y * y));
                p.nx = (float)z; p.ny = (float)x; p.nz = (float)y;
                double lat, lon;
                DiscToSelenographic(x, y, lat, lon);
                p.albedo = (uint8_t)std::lround(std::clamp(AlbedoAt(lat, lon), 0.0, 255.0));
                m_px.push_back(p);
            }
    }

    // How much sunlight a place gets, for the cosine of the Sun's angle
    // from its zenith. Full from 6 degrees up, none 1 degree below, and a
    // smooth step between, so the terminator is soft but sits where the
    // map's own "in sunlight" test puts it.
    static double Sunlight(double cosZenith) {
        const double lo = -0.0175, hi = 0.105;       // sin(-1 deg), sin(6 deg)
        const double t = std::clamp((cosZenith - lo) / (hi - lo), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    // Tonight's light. `grey` and `alpha` are sized w x h; outside the
    // disc both are 0. Alpha is the limb's anti-aliasing.
    void Shade(const LunarView& v, std::vector<uint8_t>& grey, std::vector<uint8_t>& alpha) const {
        const size_t n = (size_t)(std::max)(0, m_w) * (std::max)(0, m_h);
        grey.assign(n, 0);
        alpha.assign(n, 0);
        // The Sun and the observer in body axes.
        const double sx = v.sunDir.Dot(v.axisX), sy = v.sunDir.Dot(v.axisY), sz = v.sunDir.Dot(v.axisZ);
        const LVec e = (v.centre * -1.0).Unit();
        const double ex = e.Dot(v.axisX), ey = e.Dot(v.axisY), ez = e.Dot(v.axisZ);
        for (const Px& p : m_px) {
            const double sun = Sunlight(p.nx * sx + p.ny * sy + p.nz * sz);
            double light = kEarthshine + (1.0 - kEarthshine) * sun;
            if (p.nx * ex + p.ny * ey + p.nz * ez <= 0.0) light *= kPastLimb;
            grey[p.index]  = (uint8_t)std::lround(std::clamp(p.albedo * light, 0.0, 255.0));
            alpha[p.index] = p.cover;
        }
    }

    int Width() const  { return m_w; }
    int Height() const { return m_h; }
    size_t DiscPixels() const { return m_px.size(); }

private:
    struct Px {
        uint32_t index = 0;
        float    nx = 0, ny = 0, nz = 0;     // the body normal
        uint8_t  albedo = 0;
        uint8_t  cover = 255;
    };

    int m_aw = 0, m_ah = 0;
    std::vector<uint8_t> m_albedo;
    int m_w = -1, m_h = -1;
    double m_cx = 0, m_cy = 0, m_r = 0;
    std::vector<Px> m_px;
};

} // namespace raduk::sky
