#pragma once
// ============================================================
//  EmuFault -- Phase 157.0: faults the emulator can be told to commit
//
//  RADUK_EMU_FAULT holds a comma-separated list of faults, each
//  name@start[+duration], in seconds from when the device starts (the
//  focuser opens, the camera starts streaming, the mount connects):
//
//    RADUK_EMU_FAULT=camera-drop@20+5     the camera vanishes at 20 s, back at 25 s
//    RADUK_EMU_FAULT=camera-drop@20       ...and never comes back
//    RADUK_EMU_FAULT=eaf-stall@3          the focuser's motor stops at 3 s
//    RADUK_EMU_FAULT=slew-stuck@0         no GoTo ever arrives
//
//  Each emulator asks for its own faults by name. A fault without a
//  duration lasts for ever. Each one has a test that checks the app's
//  safe answer to it (tests/fault_injection.cpp).
//
//  Pure: the parser takes the string; Active reads the environment.
// ============================================================

#include <cstdlib>
#include <string>
#include <vector>

namespace raduk::core {

struct EmuFault {
    std::string name;
    double startSec = 0.0;
    double durationSec = -1.0;   // < 0: for ever

    // Is it in effect t seconds after the device started?
    bool InEffect(double t) const {
        if (t < startSec) return false;
        return durationSec < 0.0 || t < startSec + durationSec;
    }
};

// Parses "a@1,b@2+3". Malformed entries are skipped.
inline std::vector<EmuFault> ParseEmuFaults(const std::string& s) {
    std::vector<EmuFault> out;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(',', i);
        if (j == std::string::npos) j = s.size();
        std::string item = s.substr(i, j - i);
        while (!item.empty() && item.front() == ' ') item.erase(item.begin());
        while (!item.empty() && item.back() == ' ') item.pop_back();
        const size_t at = item.find('@');
        if (!item.empty() && at != std::string::npos && at > 0) {
            EmuFault f;
            f.name = item.substr(0, at);
            const std::string rest = item.substr(at + 1);
            const size_t plus = rest.find('+');
            char* end = nullptr;
            const std::string startStr = rest.substr(0, plus);
            f.startSec = std::strtod(startStr.c_str(), &end);
            bool ok = !startStr.empty() && end && *end == '\0' && f.startSec >= 0.0;
            if (ok && plus != std::string::npos) {
                const std::string durStr = rest.substr(plus + 1);
                f.durationSec = std::strtod(durStr.c_str(), &end);
                ok = !durStr.empty() && end && *end == '\0' && f.durationSec > 0.0;
            }
            if (ok) out.push_back(f);
        }
        i = j + 1;
    }
    return out;
}

// The fault of this name, if one is set.
inline bool FindEmuFault(const std::vector<EmuFault>& faults, const std::string& name, EmuFault& out) {
    for (const auto& f : faults) if (f.name == name) { out = f; return true; }
    return false;
}

// From the environment (RADUK_EMU_FAULT), read when a device starts.
inline bool ActiveEmuFault(const std::string& name, EmuFault& out) {
    const char* v = std::getenv("RADUK_EMU_FAULT");
    if (!v || !*v) return false;
    return FindEmuFault(ParseEmuFaults(v), name, out);
}

} // namespace raduk::core
