#pragma once
// ============================================================
//  MoonMapProject -- which sections of the Moon a project holds (phase 162)
//
//  One project is one full-Moon mosaic in the making. It remembers every
//  recording taken of every section (sky/MoonMap.h has the grid), over as
//  many nights as it takes, and answers the one question the map window
//  asks: is every section captured? When it is, the project is ready to
//  be stitched, and Manifest() is the list of files to hand the stitcher.
//  Stitching itself is another program's job.
//
//  Several projects over the months are how the Moon is compared with
//  itself, so a project is a small text file of its own and never a
//  setting.
//
//  A capture keeps the lighting and libration it was taken under. Two
//  sections taken a week apart do not match in shadow, and whoever
//  stitches them needs to be able to see that in the list.
//
//  Pure but for the file calls: tests/moon_map_project.cpp.
// ============================================================

#include "../sky/MoonMap.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace raduk::core {

struct MoonMapCapture {
    std::string section;            // "NE-A1"
    double   jdUt = 0.0;
    double   sunAltDeg = 0.0;       // over the section's aim point
    double   colongitudeDeg = 0.0;
    double   librLonDeg = 0.0, librLatDeg = 0.0;
    uint32_t frames = 0;
    int      frameW = 0, frameH = 0;
    std::string file;               // empty: marked by hand
};

class MoonMapProject {
public:
    static constexpr const char* kMagic = "RadukCapture moon map 1";
    static constexpr const char* kExtension = ".moonmap";

    void Init(const std::string& name, int perSide, double createdJd) {
        m_name = name;
        m_perSide = perSide;
        m_createdJd = createdJd;
        m_sections = raduk::sky::BuildMoonSections(perSide);
        m_captures.clear();
    }

    bool Valid() const { return !m_sections.empty(); }
    const std::string& Name() const { return m_name; }
    int    PerSide() const { return m_perSide; }
    double CreatedJd() const { return m_createdJd; }
    const std::vector<raduk::sky::MoonSection>& Sections() const { return m_sections; }
    const std::vector<MoonMapCapture>& Captures() const { return m_captures; }

    const raduk::sky::MoonSection* Find(const std::string& id) const {
        for (const auto& s : m_sections) if (s.id == id) return &s;
        return nullptr;
    }

    int CaptureCount(const std::string& id) const {
        int n = 0;
        for (const auto& c : m_captures) if (c.section == id) ++n;
        return n;
    }
    bool IsCaptured(const std::string& id) const { return CaptureCount(id) > 0; }

    // The newest capture of a section, which is the one the manifest lists.
    const MoonMapCapture* Latest(const std::string& id) const {
        const MoonMapCapture* best = nullptr;
        for (const auto& c : m_captures)
            if (c.section == id && (!best || c.jdUt >= best->jdUt)) best = &c;
        return best;
    }

    int Total() const { return (int)m_sections.size(); }
    int Captured() const {
        int n = 0;
        for (const auto& s : m_sections) if (IsCaptured(s.id)) ++n;
        return n;
    }
    // Every section has at least one capture: ready to stitch.
    bool Complete() const { return Valid() && Captured() == Total(); }

    int QuadrantTotal(raduk::sky::MoonQuadrant q) const {
        int n = 0;
        for (const auto& s : m_sections) if (s.quad == q) ++n;
        return n;
    }
    int QuadrantCaptured(raduk::sky::MoonQuadrant q) const {
        int n = 0;
        for (const auto& s : m_sections) if (s.quad == q && IsCaptured(s.id)) ++n;
        return n;
    }
    bool QuadrantComplete(raduk::sky::MoonQuadrant q) const {
        return QuadrantTotal(q) > 0 && QuadrantCaptured(q) == QuadrantTotal(q);
    }

    // False for a section this grid does not have: a capture filed under
    // a name nobody can find again is worse than one refused.
    bool AddCapture(const MoonMapCapture& c) {
        if (!Find(c.section)) return false;
        m_captures.push_back(c);
        return true;
    }

    // Forget a section's captures (the files stay on disk). For one that
    // turned out soft, clouded or off target.
    bool Clear(const std::string& id) {
        if (!Find(id)) return false;
        std::vector<MoonMapCapture> keep;
        for (const auto& c : m_captures) if (c.section != id) keep.push_back(c);
        const bool changed = keep.size() != m_captures.size();
        m_captures.swap(keep);
        return changed;
    }

    // "17 of 84 sections (NE 5/21, NW 4/21, SW 4/21, SE 4/21)"
    std::string Summary() const {
        using Q = raduk::sky::MoonQuadrant;
        char b[200];
        std::snprintf(b, sizeof(b),
            "%d of %d sections (NE %d/%d, NW %d/%d, SW %d/%d, SE %d/%d)%s",
            Captured(), Total(),
            QuadrantCaptured(Q::NE), QuadrantTotal(Q::NE),
            QuadrantCaptured(Q::NW), QuadrantTotal(Q::NW),
            QuadrantCaptured(Q::SW), QuadrantTotal(Q::SW),
            QuadrantCaptured(Q::SE), QuadrantTotal(Q::SE),
            Complete() ? ": READY FOR MOSAIC" : "");
        return b;
    }

    // ── The file ──────────────────────────────────────────────
    std::string Serialize() const {
        std::ostringstream o;
        o << kMagic << "\n";
        o << "name=" << m_name << "\n";
        char b[64];
        std::snprintf(b, sizeof(b), "%.6f", m_createdJd);
        o << "created_jd=" << b << "\n";
        o << "sections_per_side=" << m_perSide << "\n";
        for (const auto& c : m_captures) {
            char line[256];
            std::snprintf(line, sizeof(line), "capture=%s|%.6f|%.2f|%.2f|%.2f|%.2f|%u|%d|%d|",
                          c.section.c_str(), c.jdUt, c.sunAltDeg, c.colongitudeDeg,
                          c.librLonDeg, c.librLatDeg, c.frames, c.frameW, c.frameH);
            o << line << c.file << "\n";
        }
        return o.str();
    }

    bool Parse(const std::string& text, std::string& why) {
        std::istringstream in(text);
        std::string ln;
        if (!std::getline(in, ln)) { why = "empty file"; return false; }
        Chomp(ln);
        if (ln != kMagic) { why = "not a moon map file"; return false; }

        std::string name;
        double created = 0.0;
        int perSide = 0;
        std::vector<MoonMapCapture> caps;
        int n = 1;
        while (std::getline(in, ln)) {
            ++n;
            Chomp(ln);
            if (ln.empty() || ln[0] == '#') continue;
            const size_t eq = ln.find('=');
            if (eq == std::string::npos) { why = "line " + std::to_string(n) + ": no '='"; return false; }
            const std::string key = ln.substr(0, eq), val = ln.substr(eq + 1);
            if (key == "name") name = val;
            else if (key == "created_jd") created = std::atof(val.c_str());
            else if (key == "sections_per_side") perSide = std::atoi(val.c_str());
            else if (key == "capture") {
                // Nine fields, then the path, which is the rest of the line.
                std::vector<std::string> f;
                size_t a = 0;
                for (int i = 0; i < 9; ++i) {
                    const size_t bar = val.find('|', a);
                    if (bar == std::string::npos) break;
                    f.push_back(val.substr(a, bar - a));
                    a = bar + 1;
                }
                if (f.size() != 9) { why = "line " + std::to_string(n) + ": a capture needs ten fields"; return false; }
                MoonMapCapture c;
                c.section        = f[0];
                c.jdUt           = std::atof(f[1].c_str());
                c.sunAltDeg      = std::atof(f[2].c_str());
                c.colongitudeDeg = std::atof(f[3].c_str());
                c.librLonDeg     = std::atof(f[4].c_str());
                c.librLatDeg     = std::atof(f[5].c_str());
                c.frames         = (uint32_t)std::strtoul(f[6].c_str(), nullptr, 10);
                c.frameW         = std::atoi(f[7].c_str());
                c.frameH         = std::atoi(f[8].c_str());
                c.file           = val.substr(a);
                caps.push_back(c);
            }
            // An unknown key is a newer version's; it is passed over.
        }
        if (perSide < raduk::sky::MOONMAP_MIN_SIDE || perSide > raduk::sky::MOONMAP_MAX_SIDE) {
            why = "sections_per_side is missing or out of range";
            return false;
        }
        Init(name, perSide, created);
        for (const auto& c : caps)
            if (!AddCapture(c)) { why = "a capture names section '" + c.section + "', which this grid has not got"; return false; }
        return true;
    }

    bool Save(const std::string& path) const {
        // Written beside the file and moved over it, so a crash mid-write
        // cannot leave a month of nights as half a file.
        const std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f << Serialize();
            if (!f.good()) return false;
        }
        std::remove(path.c_str());
        return std::rename(tmp.c_str(), path.c_str()) == 0;
    }

    bool Load(const std::string& path, std::string& why) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { why = "could not open " + path; return false; }
        std::stringstream ss;
        ss << f.rdbuf();
        return Parse(ss.str(), why);
    }

    // What to give the stitcher: one line a section, in capture order,
    // with the newest recording of it and the light it was taken in.
    std::string Manifest() const {
        std::ostringstream o;
        o << "# Moon map: " << m_name << "\n";
        o << "# " << Summary() << "\n";
        o << "# section  lat  lon  JD(UT)  sun_alt  colongitude  libration_lon  libration_lat  frames  file\n";
        for (const auto& s : m_sections) {
            const MoonMapCapture* c = Latest(s.id);
            char b[200];
            if (!c) {
                std::snprintf(b, sizeof(b), "%-7s %+6.1f %+7.1f  MISSING\n",
                              s.id.c_str(), s.latDeg, s.lonDeg);
                o << b;
                continue;
            }
            std::snprintf(b, sizeof(b), "%-7s %+6.1f %+7.1f  %.5f  %5.1f  %6.1f  %+5.1f  %+5.1f  %6u  ",
                          s.id.c_str(), s.latDeg, s.lonDeg, c->jdUt, c->sunAltDeg,
                          c->colongitudeDeg, c->librLonDeg, c->librLatDeg, c->frames);
            o << b << (c->file.empty() ? "(marked by hand)" : c->file) << "\n";
        }
        return o.str();
    }

    // A project's name as a file and folder name.
    static std::string SafeName(std::string s) {
        for (char& c : s)
            if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') c = '_';
        if (s.empty()) s = "moon";
        return s;
    }

private:
    static void Chomp(std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    }

    std::string m_name;
    int    m_perSide = 0;
    double m_createdJd = 0.0;
    std::vector<raduk::sky::MoonSection> m_sections;
    std::vector<MoonMapCapture> m_captures;
};

} // namespace raduk::core
