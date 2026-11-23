#pragma once
// ============================================================
// LogArchive.h -- every run's log kept by date, not just the last one
// ============================================================
//
// 2026-11-23: the owner ran tools/guide_report.py on the 2026-11-22 night
// and neither raduk_log.txt nor raduk_log.prev.txt held it. Each launch
// moves the last run's log to .prev.txt, replacing the one before, so a
// night survives exactly one restart (a launch for darks is enough to lose
// it). Now the outgoing log is also copied into <logs>\history, named for
// when that run ended, and the newest kHistoryKeep are kept.
//
// The pure parts: the name for a time, and which old names to delete.
// The Windows file calls are in CrashDiag.h's RotateLog.
// ============================================================

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace raduk::core::logarchive {

inline constexpr int kHistoryKeep = 30;
inline constexpr const char* kPrefix = "raduk_log_";

// raduk_log_20261122-031005.txt for 2026-11-22 03:10:05 local.
inline std::string ArchiveName(int year, int month, int day, int hour, int minute, int second) {
    char b[64];
    std::snprintf(b, sizeof(b), "%s%04d%02d%02d-%02d%02d%02d.txt", kPrefix,
                  year, month, day, hour, minute, second);
    return b;
}

// Whether a file name is one of ours (so pruning never touches anything else).
inline bool IsArchiveName(const std::string& n) {
    // raduk_log_ + 8 digits + '-' + 6 digits + .txt
    const std::string p = kPrefix;
    if (n.size() != p.size() + 8 + 1 + 6 + 4 || n.compare(0, p.size(), p) != 0) return false;
    const std::string t = n.substr(p.size());
    for (int i = 0; i < 15; ++i) {
        if (i == 8) { if (t[i] != '-') return false; continue; }
        if (t[i] < '0' || t[i] > '9') return false;
    }
    return t.compare(15, 4, ".txt") == 0;
}

// The archive names to delete so that `keep` remain: the oldest first. The
// names sort by time because the time is written most significant first.
// Two runs ending in the same second share a name; the copy replaces it.
inline std::vector<std::string> ToDelete(std::vector<std::string> names, int keep = kHistoryKeep) {
    names.erase(std::remove_if(names.begin(), names.end(),
                               [](const std::string& n) { return !IsArchiveName(n); }),
                names.end());
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    if (keep < 0) keep = 0;
    if ((int)names.size() <= keep) return {};
    return std::vector<std::string>(names.begin(), names.end() - keep);
}

} // namespace raduk::core::logarchive
