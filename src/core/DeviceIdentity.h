#pragma once
// ============================================================
//  DeviceIdentity -- Phase 157.3: what a device says it is
//
//  The qualification report (docs/QUALIFICATION.md) needs, for every
//  configuration run, the camera's, focuser's and mount's model,
//  firmware, SDK and serial. Until this, nothing wrote the firmware
//  anywhere, and the mount's hand-control version reached only the
//  core log, never the session log a night is read from.
//
//  Each driver fills what its SDK can tell; what it cannot is left
//  empty and written as "not reported", never guessed. ZWO's camera
//  SDK (ASICamera2 1.41) has no firmware call at all, so a ZWO camera
//  always says so.
//
//  Pure, no SDK headers: tests/device_identity.cpp.
// ============================================================

#include <cstdio>
#include <string>

namespace raduk::core {

struct DeviceIdentity {
    std::string model;
    std::string firmware;
    std::string sdk;
    std::string serial;
};

// "ZWO EAF; firmware 3.0.4; SDK 1.7.7; serial 1a2b3c4d5e6f7081".
// Empty fields read "not reported", so the line always has all four
// and a reader can tell "unknown" from "left out".
inline std::string DescribeIdentity(const DeviceIdentity& d) {
    auto or_ = [](const std::string& s) { return s.empty() ? std::string("not reported") : s; };
    return or_(d.model) + "; firmware " + or_(d.firmware) + "; SDK " + or_(d.sdk) +
           "; serial " + or_(d.serial);
}

// An SDK's 8-byte serial as lower-case hex. All zero means the device
// gave none (ZWO's SDKs leave it zeroed when the firmware lacks one).
inline std::string SerialHex(const unsigned char* id, int n) {
    if (!id || n <= 0) return {};
    bool any = false;
    for (int i = 0; i < n; ++i) if (id[i]) { any = true; break; }
    if (!any) return {};
    std::string s;
    char b[3];
    for (int i = 0; i < n; ++i) { std::snprintf(b, sizeof(b), "%02x", id[i]); s += b; }
    return s;
}

// "3.0.4" from three version bytes.
inline std::string VersionString(int major, int minor, int build) {
    char b[48];
    std::snprintf(b, sizeof(b), "%d.%d.%d", major, minor, build);
    return b;
}

// "1, 41, 0" (how ZWO's SDKs spell their own version) to "1.41.0".
inline std::string TidyVersion(const char* v) {
    std::string out;
    if (!v) return out;
    for (const char* p = v; *p; ++p) {
        if (*p == ' ') continue;
        out += (*p == ',') ? '.' : *p;
    }
    return out;
}

} // namespace raduk::core
