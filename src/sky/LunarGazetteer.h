// ============================================================
//  LunarGazetteer — every named place on the Moon (phase 162d)
//
//  LunarFeatures.h is a shortlist of about 70 for the GoTo panel. This is
//  all of them: the IAU/USGS Gazetteer of Planetary Nomenclature, 9087
//  adopted names, kept in data/moon_gazetteer.csv (data/SOURCE-moon.txt
//  says where it came from). The Moon map asks it what is inside a
//  section, which feature is nearest a point, and where a name is.
//
//  Without the file it falls back to the shortlist, so the map still
//  names the big craters on a machine that has lost data/.
//
//  "Inside a section" is judged on the disc at zero libration, the frame
//  the sections are cut in (MoonMap.h): x east, y north, radius 1. A
//  feature on the far side is in no section.
//
//  Pure but for reading the file: tests/lunar_gazetteer.cpp.
// ============================================================

#pragma once

#include "LunarFeatures.h"
#include "MoonMap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace raduk::sky {

struct GazetteerFeature {
    std::string name;
    double latDeg = 0.0;        // +N
    double lonDeg = 0.0;        // +E, -180..180
    double diameterKm = 0.0;    // or length, for a rille or a range
    std::string type;           // "Crater", "Mare", "Satellite Feature", ...
    std::string origin;         // what the name honours; empty in the shortlist
};

class LunarGazetteer {
public:
    // Reads the CSV. False with a reason, and the shortlist is used.
    bool Load(const std::string& path, std::string& why) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { why = "could not open " + path; UseShortlist(); return false; }
        std::stringstream ss;
        ss << f.rdbuf();
        return Parse(ss.str(), why);
    }

    bool Parse(const std::string& text, std::string& why) {
        std::vector<GazetteerFeature> out;
        size_t pos = 0;
        std::vector<std::string> row;
        int line = 0;
        bool header = true;
        while (NextRow(text, pos, row)) {
            ++line;
            if (header) {
                header = false;
                if (row.size() < 4 || row[0] != "name") { why = "not a gazetteer file"; UseShortlist(); return false; }
                continue;
            }
            if (row.size() == 1 && row[0].empty()) continue;
            if (row.size() < 5) { why = "row " + std::to_string(line) + ": too few columns"; UseShortlist(); return false; }
            GazetteerFeature g;
            g.name       = row[0];
            g.latDeg     = std::atof(row[1].c_str());
            g.lonDeg     = std::atof(row[2].c_str());
            g.diameterKm = std::atof(row[3].c_str());
            g.type       = row[4];
            if (row.size() > 5) g.origin = row[5];
            out.push_back(std::move(g));
        }
        if (out.empty()) { why = "no features in the file"; UseShortlist(); return false; }
        m_all = std::move(out);
        m_fromFile = true;
        SortBySize();
        return true;
    }

    // The ~70 of LunarFeatures.h.
    void UseShortlist() {
        m_all.clear();
        for (const auto& f : kLunarFeatures)
            m_all.push_back({f.name, f.latDeg, f.lonDeg, f.sizeKm, f.kind, std::string()});
        m_fromFile = false;
        SortBySize();
    }

    bool FromFile() const { return m_fromFile; }
    size_t Size() const { return m_all.size(); }
    const std::vector<GazetteerFeature>& All() const { return m_all; }

    // Where a point sits on the zero-libration disc. False on the far side.
    static bool DiscXY(double latDeg, double lonDeg, double& x, double& y) {
        constexpr double d2r = 3.14159265358979323846 / 180.0;
        const double la = latDeg * d2r, lo = lonDeg * d2r;
        x = std::cos(la) * std::sin(lo);
        y = std::sin(la);
        return std::cos(la) * std::cos(lo) >= 0.0;
    }

    // The features inside a section, largest first. `max` 0 is all of
    // them; `minKm` leaves out the small ones (a 5 km crater is not worth
    // a line on a card about a 300 km square).
    std::vector<const GazetteerFeature*> FeaturesIn(const MoonSection& s, size_t max = 0,
                                                    double minKm = 0.0) const {
        std::vector<const GazetteerFeature*> out;
        for (const auto& g : m_all) {           // already largest first
            if (g.diameterKm < minKm) break;
            double x = 0.0, y = 0.0;
            if (!DiscXY(g.latDeg, g.lonDeg, x, y)) continue;
            if (x < s.x0 || x >= s.x1 || y < s.y0 || y >= s.y1) continue;
            out.push_back(&g);
            if (max && out.size() >= max) break;
        }
        return out;
    }

    // The feature whose centre is nearest a point, by angle on the sphere.
    // `minKm` skips the small ones, so "near Copernicus" beats "near
    // Copernicus H". Null when there is nothing that big.
    const GazetteerFeature* Nearest(double latDeg, double lonDeg, double minKm = 0.0) const {
        const GazetteerFeature* best = nullptr;
        double bestCos = -2.0;
        for (const auto& g : m_all) {
            if (g.diameterKm < minKm) break;
            const double c = CosAngle(latDeg, lonDeg, g.latDeg, g.lonDeg);
            if (c > bestCos) { bestCos = c; best = &g; }
        }
        return best;
    }

    // By name, ignoring case: exact first, then names that begin with it,
    // then names that contain it, each largest first.
    std::vector<const GazetteerFeature*> Search(const std::string& q, size_t max = 10) const {
        const std::string k = Lower(q);
        std::vector<const GazetteerFeature*> exact, prefix, inside;
        if (k.empty()) return exact;
        for (const auto& g : m_all) {
            const std::string n = Lower(g.name);
            if (n == k) exact.push_back(&g);
            else if (n.compare(0, k.size(), k) == 0) prefix.push_back(&g);
            else if (n.find(k) != std::string::npos) inside.push_back(&g);
        }
        exact.insert(exact.end(), prefix.begin(), prefix.end());
        exact.insert(exact.end(), inside.begin(), inside.end());
        if (max && exact.size() > max) exact.resize(max);
        return exact;
    }

private:
    void SortBySize() {
        std::stable_sort(m_all.begin(), m_all.end(), [](const GazetteerFeature& a, const GazetteerFeature& b) {
            return a.diameterKm > b.diameterKm;
        });
    }

    static double CosAngle(double la1, double lo1, double la2, double lo2) {
        constexpr double d2r = 3.14159265358979323846 / 180.0;
        return std::sin(la1 * d2r) * std::sin(la2 * d2r) +
               std::cos(la1 * d2r) * std::cos(la2 * d2r) * std::cos((lo1 - lo2) * d2r);
    }

    static std::string Lower(std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    // One RFC 4180 row: commas, quotes, "" for a quote inside quotes, and
    // a newline inside quotes kept. False at the end of the text.
    static bool NextRow(const std::string& t, size_t& pos, std::vector<std::string>& row) {
        row.clear();
        if (pos >= t.size()) return false;
        std::string cell;
        bool quoted = false;
        while (pos < t.size()) {
            const char c = t[pos++];
            if (quoted) {
                if (c == '"') {
                    if (pos < t.size() && t[pos] == '"') { cell += '"'; ++pos; }
                    else quoted = false;
                } else {
                    cell += c;
                }
            } else if (c == '"') {
                quoted = true;
            } else if (c == ',') {
                row.push_back(cell); cell.clear();
            } else if (c == '\n' || c == '\r') {
                if (c == '\r' && pos < t.size() && t[pos] == '\n') ++pos;
                break;
            } else {
                cell += c;
            }
        }
        row.push_back(cell);
        return true;
    }

    std::vector<GazetteerFeature> m_all;
    bool m_fromFile = false;
};

} // namespace raduk::sky
