#pragma once
// ============================================================
// ProfileManager.h — Load/save target profiles as INI files
// Milestone 5 — pure Win32 INI via GetPrivateProfileString /
// WritePrivateProfileString. No external dependencies.
// ============================================================

#include "TargetProfile.h"
#include <string>
#include <vector>
#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include "core/Paths.h"

namespace raduk::profiles {

class ProfileManager {
public:
    ProfileManager() = default;

    // Initialize — sets the profiles directory and loads all profiles.
    // Creates default profiles if directory is empty.
    void Init(const std::string& profilesDir = raduk::core::Paths::Profiles()) {
        m_dir = profilesDir;
        CreateDirectoryA(m_dir.c_str(), nullptr);
        LoadAll();
    }

    // All loaded profiles (const)
    const std::vector<TargetProfile>& Profiles() const { return m_profiles; }
    // Mutable access by index — for writing notes/fields back before save
    TargetProfile* ProfileAt(int index) {
        if (index < 0 || index >= (int)m_profiles.size()) return nullptr;
        return &m_profiles[index];
    }

    // Find by name (case-insensitive). Returns nullptr if not found.
    const TargetProfile* Find(const std::string& name) const {
        for (const auto& p : m_profiles) {
            if (_stricmp(p.name.c_str(), name.c_str()) == 0)
                return &p;
        }
        return nullptr;
    }

    // Index of currently active profile (-1 if none)
    int ActiveIndex() const { return m_activeIndex; }

    // Set active profile by index
    void SetActive(int index) {
        if (index >= 0 && index < (int)m_profiles.size())
            m_activeIndex = index;
    }

    const TargetProfile* Active() const {
        if (m_activeIndex < 0 || m_activeIndex >= (int)m_profiles.size())
            return nullptr;
        return &m_profiles[m_activeIndex];
    }

    // Save a profile to its INI file
    bool Save(const TargetProfile& profile) {
        std::string path = m_dir + "\\" + profile.filename;
        const char* p = path.c_str();

        auto WS = [&](const char* sec, const char* key, const std::string& val) {
            WritePrivateProfileStringA(sec, key, val.c_str(), p);
        };
        auto WI = [&](const char* sec, const char* key, int val) {
            char buf[32]; sprintf_s(buf, "%d", val);
            WritePrivateProfileStringA(sec, key, buf, p);
        };
        auto WF = [&](const char* sec, const char* key, double val) {
            char buf[32]; sprintf_s(buf, "%.4f", val);
            WritePrivateProfileStringA(sec, key, buf, p);
        };
        auto WB = [&](const char* sec, const char* key, bool val) {
            WritePrivateProfileStringA(sec, key, val ? "true" : "false", p);
        };

        WS("profile", "name", profile.name);
        WS("profile", "target_type", profile.targetType);
        WS("camera",  "model",       profile.camera);

        WI("camera",  "gain",          profile.gain);
        WF("camera",  "exposure_ms",   profile.exposureMs);
        WI("camera",  "gamma",         profile.gamma);
        WI("camera",  "wb_red",        profile.wbRed);
        WI("camera",  "wb_blue",       profile.wbBlue);
        WI("camera",  "usb_bandwidth", profile.usbBandwidth);
        WB("camera",  "high_speed",    profile.highSpeed);

        WI("roi",     "width",         profile.roiWidth);
        WI("roi",     "height",        profile.roiHeight);
        WI("roi",     "binning",       profile.binning);
        // Phase 149.0b: the format by name, and the file's format version.
        // img_type (ZWO's integer) is no longer written.
        WI("profile", "format_version", raduk::core::kProfileFormatVersion);
        WS("roi",     "pixel_format",  raduk::core::PixelFormatName(profile.imgType));

        WI("focuser", "position",      profile.focusPosition);
        WI("focuser", "step_coarse",   profile.stepCoarse);
        WI("focuser", "step_fine",     profile.stepFine);

        WS("mount",   "tracking_mode", profile.trackingMode);
        WI("mount",   "slew_rate",     profile.slewRate);
        WS("mount",   "goto_target",   profile.gotoTarget);

        WS("capture", "format",        profile.captureFormat);
        WS("capture", "limit_type",    profile.limitType);
        WI("capture", "limit_value",   profile.limitValue);
        WS("capture", "file_prefix",   profile.filePrefix);

        WS("notes",   "text",          profile.notes);
        // Phase 140.2: the exposure band, 0 = none.
        WF("exposure_band", "floor_ms",   profile.bandFloorMs);
        WF("exposure_band", "ceiling_ms", profile.bandCeilMs);

        printf("[ProfileManager] Saved: %s\n", path.c_str());
        return true;
    }

    // Load a single profile from an INI file path
    bool Load(const std::string& path, TargetProfile& out) {
        const char* p = path.c_str();

        auto RI = [&](const char* sec, const char* key, int def) -> int {
            return (int)GetPrivateProfileIntA(sec, key, def, p);
        };
        // Grown rather than fixed. GetPrivateProfileStringA fills
        // whatever it is given and says nothing about the rest, and
        // the old char[512] was enough only by luck: planet_focus
        // ships a 323-byte note. A note typed past the buffer came
        // back short on the LoadAll the EDIT dialog runs straight
        // after saving -- the dialog would hand over the whole note
        // and the profile in memory would already be missing its
        // tail. A full value returns less than size-1, so grow until
        // it does.
        auto RS = [&](const char* sec, const char* key, const char* def) -> std::string {
            std::vector<char> buf(512);
            for (;;) {
                DWORD got = GetPrivateProfileStringA(sec, key, def, buf.data(),
                                                     (DWORD)buf.size(), p);
                if (got < buf.size() - 1) return std::string(buf.data(), got);
                if (buf.size() >= (1u << 20)) return std::string(buf.data(), got);
                buf.resize(buf.size() * 2);
            }
        };
        auto RF = [&](const char* sec, const char* key, double def) -> double {
            char buf[32]; sprintf_s(buf, "%.4f", def);
            char result[32];
            GetPrivateProfileStringA(sec, key, buf, result, sizeof(result), p);
            return atof(result);
        };
        auto RB = [&](const char* sec, const char* key, bool def) -> bool {
            std::string v = RS(sec, key, def ? "true" : "false");
            return (v == "true" || v == "1" || v == "yes");
        };

        out.name         = RS("profile", "name", "Unknown");
        // A file without these keeps what the caller seeded (the
        // factory default for the standard profiles).
        out.targetType   = RS("profile", "target_type", out.targetType.c_str());
        out.camera       = RS("camera",  "model",       out.camera.c_str());

        out.gain         = RI("camera",  "gain",          100);
        out.exposureMs   = RF("camera",  "exposure_ms",   8.0);
        out.gamma        = RI("camera",  "gamma",         50);
        out.wbRed        = RI("camera",  "wb_red",        52);
        out.wbBlue       = RI("camera",  "wb_blue",       47);
        out.usbBandwidth = RI("camera",  "usb_bandwidth", 80);
        out.highSpeed    = RB("camera",  "high_speed",    true);

        out.roiWidth     = RI("roi",     "width",         1280);
        out.roiHeight    = RI("roi",     "height",        960);
        out.binning      = RI("roi",     "binning",       1);
        // Phase 149.0b: version 2 names the format; version 1 (no
        // format_version key) carried ZWO's integer in img_type.
        {
            const int ver = RI("profile", "format_version", 1);
            raduk::core::ImageType t = raduk::core::ImageType::Raw8;
            if (ver >= 2 && raduk::core::PixelFormatFromName(RS("roi", "pixel_format", "raw8"), t))
                out.imgType = t;
            else
                out.imgType = raduk::core::PixelFormatFromLegacyZwo(RI("roi", "img_type", 0));
            out.formatVersionRead = ver;
        }

        out.focusPosition = RI("focuser", "position",    50000);
        out.stepCoarse    = RI("focuser", "step_coarse", 100);
        out.stepFine      = RI("focuser", "step_fine",   10);

        out.trackingMode = RS("mount",   "tracking_mode", "alt_az");
        out.slewRate     = RI("mount",   "slew_rate",     9);
        out.gotoTarget   = RS("mount",   "goto_target",   "");

        out.captureFormat = RS("capture", "format",      "SER");
        out.limitType     = RS("capture", "limit_type",  "frames");
        out.limitValue    = RI("capture", "limit_value", 3000);
        out.filePrefix    = RS("capture", "file_prefix", "");

        out.notes = RS("notes", "text", "");
        // Phase 140.2: a file without the section keeps the default band.
        out.bandFloorMs = RF("exposure_band", "floor_ms",   out.bandFloorMs);
        out.bandCeilMs  = RF("exposure_band", "ceiling_ms", out.bandCeilMs);

        // Extract filename from path
        size_t slash = path.find_last_of("\\/");
        out.filename = (slash != std::string::npos) ? path.substr(slash + 1) : path;

        return true;
    }

    // Reload all profiles from disk
    void LoadAll() {
        m_profiles.clear();

        // Standard profiles — loaded in order shown in ControlPanel dropdown
        // Every planet (2026-11-08: the list stopped at Venus, so Mercury,
        // Uranus and Neptune had no profile of their own).
        static const char* defaults[] = {
            "jupiter.ini", "saturn.ini",  "mars.ini",    "venus.ini",
            "mercury.ini", "uranus.ini",  "neptune.ini",
            "moon.ini",    "dso.ini",     "star.ini",    "planet_focus.ini"
        };

        static TargetProfile (*factories[])() = {
            DefaultJupiter, DefaultSaturn,  DefaultMars,    DefaultVenus,
            DefaultMercury, DefaultUranus,  DefaultNeptune,
            DefaultMoon,    DefaultDSO,     DefaultStar,    DefaultPlanetFocus
        };
        static_assert(sizeof(defaults) / sizeof(defaults[0]) ==
                      sizeof(factories) / sizeof(factories[0]),
                      "every profile file needs its factory");

        for (int i = 0; i < (int)(sizeof(defaults) / sizeof(defaults[0])); i++) {
            std::string path = m_dir + "\\" + defaults[i];
            TargetProfile profile = factories[i](); // start with defaults

            // If file exists, load it (overrides defaults)
            FILE* f = nullptr;
            fopen_s(&f, path.c_str(), "r");
            if (f) {
                fclose(f);
                Load(path, profile);
            } else {
                // File doesn't exist — write the defaults
                profile.filename = defaults[i];
                Save(profile);
            }

            m_profiles.push_back(profile);
        }

        // Profiles made for a target (CreateFor), after the standard
        // ones, in name order. Only names ending exactly in ".ini":
        // FindFirstFile's pattern also matches backups such as
        // saturn.ini.working-2026-10-08 on some volumes.
        std::vector<std::string> extra;
        WIN32_FIND_DATAA fd{};
        HANDLE hf = FindFirstFileA((m_dir + "\\*.ini").c_str(), &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const std::string fn = fd.cFileName;
                if (fn.size() < 5 || _stricmp(fn.c_str() + fn.size() - 4, ".ini") != 0) continue;
                bool standard = false;
                for (const char* d : defaults) if (_stricmp(d, fn.c_str()) == 0) standard = true;
                if (!standard) extra.push_back(fn);
            } while (FindNextFileA(hf, &fd));
            FindClose(hf);
        }
        std::sort(extra.begin(), extra.end(), [](const std::string& a, const std::string& b) {
            return _stricmp(a.c_str(), b.c_str()) < 0;
        });
        for (const auto& fn : extra) {
            TargetProfile profile = DefaultStar();   // fields a file lacks
            profile.notes.clear();
            profile.histogram_target.clear();
            if (Load(m_dir + "\\" + fn, profile)) m_profiles.push_back(profile);
        }

        printf("[ProfileManager] Loaded %d profiles from %s\n",
               (int)m_profiles.size(), m_dir.c_str());
    }

    // ── A profile per target ─────────────────────────────────────
    //
    // The profile for a target is the one named for it. Among several
    // of that name, one made for the connected camera wins, then one
    // made for any camera (camera empty), then any. -1 when there is
    // none.
    int FindForTarget(const std::string& target, const std::string& camera) const {
        int any = -1, blank = -1;
        for (int i = 0; i < (int)m_profiles.size(); ++i) {
            const auto& p = m_profiles[i];
            if (_stricmp(p.name.c_str(), target.c_str()) != 0) continue;
            if (!camera.empty() && _stricmp(p.camera.c_str(), camera.c_str()) == 0) return i;
            if (p.camera.empty() && blank < 0) blank = i;
            if (any < 0) any = i;
        }
        return blank >= 0 ? blank : any;
    }

    // A file name for a target: lower case, letters and digits, the
    // rest folded to '_'. "M 31" -> "m_31.ini", "Alpha Lyr" -> "alpha_lyr.ini".
    static std::string FileNameFor(const std::string& target) {
        std::string s;
        for (unsigned char c : target) {
            if (std::isalnum(c)) s += (char)std::tolower(c);
            else if (!s.empty() && s.back() != '_') s += '_';
        }
        while (!s.empty() && s.back() == '_') s.pop_back();
        if (s.empty()) s = "target";
        return s + ".ini";
    }

    // Makes, saves and adds a profile for a target that has none, with
    // the settings of `seed` (the class of target: Star for a star, DSO
    // for a deep-sky object, otherwise the profile in use). Its own name,
    // file, file prefix and an empty note: the note is for this object.
    // Returns its index.
    int CreateFor(const std::string& target, TargetProfile seed, const std::string& camera) {
        seed.name       = target;
        seed.camera     = camera;
        seed.gotoTarget = target;
        seed.notes.clear();
        seed.histogram_target.clear();
        std::string base = FileNameFor(target);
        base.resize(base.size() - 4);
        seed.filePrefix = base + "_";
        // Never over another profile's file.
        std::string fn = base + ".ini";
        for (int n = 2; FileTaken(fn); ++n) fn = base + "_" + std::to_string(n) + ".ini";
        seed.filename = fn;
        Save(seed);
        m_profiles.push_back(seed);
        return (int)m_profiles.size() - 1;
    }

    const std::string& Dir() const { return m_dir; }

private:
    bool FileTaken(const std::string& fn) const {
        for (const auto& p : m_profiles)
            if (_stricmp(p.filename.c_str(), fn.c_str()) == 0) return true;
        return GetFileAttributesA((m_dir + "\\" + fn).c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    std::string              m_dir;
    std::vector<TargetProfile> m_profiles;
    int                      m_activeIndex = 0; // Default to Jupiter
};

} // namespace raduk::profiles
