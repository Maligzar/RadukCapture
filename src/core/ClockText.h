#pragma once
// ============================================================
// ClockText.h -- the time, local and UTC, for the screens
// ============================================================
//
// The owner, 2026-11-24: "Time would be helpful on the main app and on
// the Saturn panel page." The status bar and the target panel (the one
// the audience reads) show the local time with seconds and the UTC time
// beside it: local is what people go by, UTC is what the log, the event
// times and every observation record use.
//
// Pure: the caller passes the two broken-down times.
// ============================================================

#include <ctime>
#include <cwchar>
#include <string>

namespace raduk::core {

// "21:43:07  ·  04:43 UTC"
inline std::wstring ClockLineFrom(const std::tm& local, const std::tm& utc) {
    wchar_t b[48];
    std::swprintf(b, 48, L"%02d:%02d:%02d  ·  %02d:%02d UTC",
                  local.tm_hour, local.tm_min, local.tm_sec, utc.tm_hour, utc.tm_min);
    return b;
}

inline std::wstring ClockLine(std::time_t t = std::time(nullptr)) {
    std::tm lt{}, ut{};
#ifdef _WIN32
    localtime_s(&lt, &t);
    gmtime_s(&ut, &t);
#else
    localtime_r(&t, &lt);
    gmtime_r(&t, &ut);
#endif
    return ClockLineFrom(lt, ut);
}

} // namespace raduk::core
