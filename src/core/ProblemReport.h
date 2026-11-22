#pragma once
// ============================================================
//  ProblemReport.h -- phase 171: what the troubleshooting report holds
//
//  The owner, at the scope on 2026-11-22: "I want a troubleshooting
//  button built into RadukCapture so that when I click it I can describe
//  the issue, close the window, and then it builds the file and sends it
//  to Claude." The app gathers the evidence; tools/sky_handoff.py sends
//  it to Claude Code on this PC (phase 156.4), which investigates and
//  opens a draft pull request.
//
//  What goes in is decided here, pure and tested (tests/problem_report.cpp),
//  because a report leaves the PC:
//    - the owner's words, the time, the version, and a snapshot of the
//      app's state (target, profile, devices), as report.json;
//    - the log's last lines, with every line naming the site taken out;
//    - rig.ini, with the site, the Dash token and anything that looks
//      like a key, token, secret or password taken out.
//  The site never leaves the PC (the project's rule); nor does a secret.
// ============================================================

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace raduk::core::report {

inline std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A rig.ini key that must not leave the PC.
inline bool SecretKey(const std::string& key) {
    const std::string k = Lower(key);
    if (k.rfind("site", 0) == 0) return true;                 // SiteLatDeg, SiteLonDeg, SiteElevM, SiteSource
    for (const char* w : {"token", "key", "secret", "password", "passwd", "leobenurl", "latitude", "longitude"})
        if (k.find(w) != std::string::npos) return true;
    return false;
}

// rig.ini with every secret line's value replaced. Comments, sections and
// everything else kept, so the shape of the settings is still readable.
inline std::string RedactIni(const std::string& text) {
    std::string out;
    size_t i = 0;
    while (i <= text.size()) {
        size_t e = text.find('\n', i);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(i, e - i);
        const size_t eq = line.find('=');
        const size_t first = line.find_first_not_of(" \t");
        if (eq != std::string::npos && first != std::string::npos && line[first] != ';' && line[first] != '#') {
            std::string key = line.substr(first, eq - first);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            if (SecretKey(key)) line = line.substr(0, eq + 1) + "(removed from the report)";
        }
        out += line;
        if (e < text.size()) out += '\n';
        i = e + 1;
    }
    return out;
}

// A log line that names where the telescope is. The app logs the site at
// startup, when it is pushed to the mount and read back from it.
inline bool SiteLine(const std::string& line) {
    const std::string l = Lower(line);
    for (const char* w : {"site:", "site ->", "site read", "sitelat", "sitelon", "latitude", "longitude",
                          "lat=", "lon=", "windows location", "gps fix"})
        if (l.find(w) != std::string::npos) return true;
    return false;
}

// The log's last maxLines lines, with the site lines replaced.
inline std::string LogTail(const std::string& log, size_t maxLines, int* removed = nullptr) {
    std::vector<std::pair<size_t, size_t>> lines;
    size_t i = 0;
    while (i < log.size()) {
        size_t e = log.find('\n', i);
        if (e == std::string::npos) e = log.size();
        lines.push_back({i, e});
        i = e + 1;
    }
    const size_t from = lines.size() > maxLines ? lines.size() - maxLines : 0;
    std::string out;
    int n = 0;
    for (size_t k = from; k < lines.size(); ++k) {
        std::string line = log.substr(lines[k].first, lines[k].second - lines[k].first);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (SiteLine(line)) { line = "(a line naming the site, removed from the report)"; ++n; }
        out += line + "\n";
    }
    if (removed) *removed = n;
    return out;
}

inline std::string JsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o + "\"";
}

// The app's state when the owner pressed REPORT: plain fields, each the
// app's own words, so this header needs nothing from the app.
struct ReportState {
    std::string stamp;        // YYYYMMDD_HHMMSS, local
    std::string night;        // the evening's date (ValidationLog's NightOf)
    std::string version;
    bool        emulator = false;
    std::vector<std::pair<std::string, std::string>> fields;   // "target" -> "Saturn", ...
    std::vector<std::string> files;   // what else is in the folder
};

inline std::string ReportJson(const std::string& description, const ReportState& s) {
    std::string j = "{\n\"format\":\"raduk-problem-report\",\"version\":1,\n";
    j += "\"stamp\":" + JsonStr(s.stamp) + ",\"night\":" + JsonStr(s.night) + ",\"app\":" + JsonStr(s.version) +
         ",\"emulator\":" + (s.emulator ? "true" : "false") + ",\n";
    j += "\"description\":" + JsonStr(description) + ",\n\"state\":{";
    for (size_t i = 0; i < s.fields.size(); ++i)
        j += (i ? "," : "") + std::string("\n ") + JsonStr(s.fields[i].first) + ":" + JsonStr(s.fields[i].second);
    j += "\n},\n\"files\":[";
    for (size_t i = 0; i < s.files.size(); ++i) j += (i ? "," : "") + JsonStr(s.files[i]);
    j += "]\n}\n";
    return j;
}

} // namespace raduk::core::report
