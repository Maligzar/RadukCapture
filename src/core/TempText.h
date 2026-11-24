#pragma once
// ============================================================
// TempText.h -- a temperature, in Celsius and Fahrenheit
// ============================================================
//
// The owner, 2026-11-24: "I need temp displayed in both Celsius and
// Fahrenheit." Every temperature the app shows (the focuser, the camera
// sensor, the air) goes through here, so the two never disagree and no
// display is left in one unit. Logs and files stay in Celsius.
// ============================================================

#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

namespace raduk::core {

inline double CToF(double c) { return c * 9.0 / 5.0 + 32.0; }
inline double FToC(double f) { return (f - 32.0) * 5.0 / 9.0; }

// "17.0°C / 62.6°F"; "--" when not a reading.
inline std::wstring TempCF(double c, int decimals = 1) {
    if (!std::isfinite(c)) return L"--";
    wchar_t b[48];
    std::swprintf(b, 48, L"%.*f°C / %.*f°F", decimals, c, decimals, CToF(c));
    return b;
}

// The compact form for tight places: "17.0°C 62.6°F".
inline std::wstring TempCFShort(double c, int decimals = 1) {
    if (!std::isfinite(c)) return L"--";
    wchar_t b[48];
    std::swprintf(b, 48, L"%.*f°C %.*f°F", decimals, c, decimals, CToF(c));
    return b;
}

} // namespace raduk::core
