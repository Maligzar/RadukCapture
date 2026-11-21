// ============================================================
// APEXTelemetry.cpp — APEX dashboard telemetry + command parser
// Out-of-line definitions for APEXTelemetry.h.
// ============================================================

#include "APEXTelemetry.h"

namespace raduk::net {

std::string APEXTelemetry::ToJSON() const {
    std::ostringstream j;
    j << std::fixed << std::setprecision(3);
    j << "{";

    // Identity
    j << "\"rc_running\":"  << (rc_running ? "true" : "false") << ",";
    j << "\"camera\":"      << jsonStr(camera) << ",";

    // Camera
    j << "\"fps\":"         << fps           << ",";
    j << "\"exposure_ms\":" << exposure_ms   << ",";
    j << "\"gain\":"        << gain          << ",";
    j << "\"gamma\":"       << gamma         << ",";
    j << "\"wb_red\":"      << wb_red        << ",";
    j << "\"wb_blue\":"     << wb_blue       << ",";
    j << "\"usb_bw\":"      << usb_bw        << ",";
    j << "\"sensor_temp\":" << sensor_temp   << ",";
    j << "\"width\":"       << width         << ",";
    j << "\"height\":"      << height        << ",";
    j << "\"bit_depth\":"   << bit_depth     << ",";

    // EAF
    j << "\"eaf_position\":" << eaf_position << ",";
    j << "\"eaf_temp\":"     << eaf_temp     << ",";
    j << "\"eaf_moving\":"   << (eaf_moving ? "true" : "false") << ",";
    j << "\"eaf_trusted\":"        << (eaf_trusted ? "true" : "false") << ",";   // 157.0e
    j << "\"eaf_saved_position\":" << eaf_saved_position << ",";
    j << "\"eaf_trust_problem\":"  << jsonStr(eaf_trust_problem) << ",";

    // Mount
    j << "\"mount_ra\":"       << jsonStr(mount_ra)       << ",";
    j << "\"mount_dec\":"      << jsonStr(mount_dec)      << ",";
    j << "\"mount_tracking\":" << jsonStr(mount_tracking) << ",";

    // Capture
    j << "\"recording\":"       << (recording ? "true" : "false") << ",";
    j << "\"frames_recorded\":" << frames_recorded << ",";
    j << "\"frames_dropped\":"  << frames_dropped  << ",";
    j << "\"limit_frames\":"    << limit_frames    << ",";
    j << "\"elapsed_sec\":"     << elapsed_sec     << ",";
    j << "\"bytes_written\":"   << bytes_written   << ",";
    j << "\"write_rate_mbs\":"  << write_rate_mbs  << ",";

    // RAM
    j << "\"ram_used_slots\":"  << ram_used_slots  << ",";
    j << "\"ram_total_slots\":" << ram_total_slots << ",";

    // Storage
    j << "\"disk_free_gb\":"    << disk_free_gb    << ",";
    j << "\"output_dir\":"      << jsonStr(output_dir)  << ",";
    j << "\"file_prefix\":"     << jsonStr(file_prefix) << ",";
    j << "\"capture_file\":"    << jsonStr(capture_file) << ",";

    // Analysis
    j << "\"seeing_score\":" << seeing_score << ",";
    j << "\"seeing_rms\":"   << seeing_rms   << ",";

    // Planet lock / centroid tracking
    j << "\"planet_lock\":"   << (planet_lock ? "true" : "false") << ",";
    j << "\"roi_w\":"         << roi_w << ",";
    j << "\"roi_h\":"         << roi_h << ",";
    j << "\"acquire_wide\":"  << (acquire_wide ? "true" : "false") << ",";
    j << "\"night_vision\":"  << (night_vision ? "true" : "false") << ",";
    j << "\"drift_ra_as\":"   << drift_ra_as  << ",";
    j << "\"drift_dec_as\":"  << drift_dec_as << ",";

    // Phase 162: the Moon map
    j << "\"moonmap_name\":"     << jsonStr(moonmap_name) << ",";
    j << "\"moonmap_side\":"     << moonmap_side << ",";
    j << "\"moonmap_captured\":" << moonmap_captured << ",";
    j << "\"moonmap_total\":"    << moonmap_total << ",";
    j << "\"moonmap_lit\":"      << moonmap_lit << ",";
    j << "\"moonmap_ready\":"    << (moonmap_ready ? "true" : "false") << ",";
    j << "\"moonmap_running\":"  << (moonmap_running ? "true" : "false") << ",";
    j << "\"moonmap_section\":"  << jsonStr(moonmap_section) << ",";
    j << "\"moonmap_phase\":"    << jsonStr(moonmap_phase) << ",";
    j << "\"moonmap_selected\":" << jsonStr(moonmap_selected) << ",";
    j << "\"moonmap_cells\":"    << jsonStr(moonmap_cells) << ",";
    j << "\"nightplan_running\":"    << (nightplan_running ? "true" : "false") << ",";
    j << "\"nightplan_paused\":"     << (nightplan_paused ? "true" : "false") << ",";
    j << "\"nightplan_state\":"      << jsonStr(nightplan_state) << ",";
    j << "\"nightplan_target\":"     << jsonStr(nightplan_target) << ",";
    j << "\"nightplan_ticked\":"     << jsonStr(nightplan_ticked) << ",";
    j << "\"nightplan_candidates\":" << jsonStr(nightplan_candidates) << ",";
    j << "\"nightplan_rows\":"       << jsonStr(nightplan_rows) << ",";
    j << "\"park_state\":"   << jsonStr(park_state) << ",";
    j << "\"park_reason\":"  << jsonStr(park_reason) << ",";
    j << "\"park_set\":"     << (park_set ? "true" : "false") << ",";
    j << "\"skyrun_phase\":"    << jsonStr(skyrun_phase) << ",";
    j << "\"skyrun_step\":"     << jsonStr(skyrun_step) << ",";
    j << "\"skyrun_refusals\":" << jsonStr(skyrun_refusals) << ",";
    j << "\"skyrun_rows\":"     << jsonStr(skyrun_rows) << ",";
    j << "\"skyrun_reason\":"   << jsonStr(skyrun_reason) << ",";
    // Phase 165.4: the weather. A NaN reading goes out as null.
    {
        auto num = [&](const char* k, double v, int prec) {
            j << "\"" << k << "\":";
            if (std::isfinite(v)) j << std::setprecision(prec) << v << std::setprecision(3);
            else j << "null";
            j << ",";
        };
        j << "\"wx_configured\":"   << (wx_configured ? "true" : "false") << ",";
        j << "\"wx_fresh\":"        << (wx_fresh ? "true" : "false") << ",";
        j << "\"wx_source\":"       << jsonStr(wx_source) << ",";
        j << "\"wx_state\":"        << jsonStr(wx_state) << ",";
        j << "\"wx_age_min\":"      << wx_age_min << ",";
        j << "\"wx_fetched_unix\":" << wx_fetched_unix << ",";
        num("wx_cloud_pct", wx_cloud_pct, 0);
        num("wx_temp_c", wx_temp_c, 1);
        num("wx_humidity_pct", wx_humidity_pct, 0);
        num("wx_wind_ms", wx_wind_ms, 2);
        num("wx_gust_ms", wx_gust_ms, 2);
        num("wx_dew_c", wx_dew_c, 1);
        num("wx_msl_hpa", wx_msl_hpa, 1);
        num("wx_seeing_7timer", wx_seeing_7timer, 0);
        j << "\"wx_dew_risk\":"     << jsonStr(wx_dew_risk) << ",";
        j << "\"wx_verdict\":"      << jsonStr(wx_verdict) << ",";
        j << "\"wx_score\":"        << wx_score << ",";
        j << "\"net_offline\":"     << (net_offline ? "true" : "false") << ",";
    }

    // Histogram — 256-element array, downsampled to 64 buckets for bandwidth
    // Each bucket sums 4 adjacent bins. Client expects 256 values — pad back out.
    j << "\"histogram\":[";
    for (int i = 0; i < 256; i++) {
        j << histogram[i];
        if (i < 255) j << ",";
    }
    j << "],";

    // Session events
    j << "\"session_events\":[";
    for (size_t i = 0; i < session_events.size(); i++) {
        const auto& ev = session_events[i];
        j << "{\"msg\":" << jsonStr(ev.msg)
          << ",\"type\":" << jsonStr(ev.type) << "}";
        if (i + 1 < session_events.size()) j << ",";
    }
    j << "]";

    j << "}";
    return j.str();
}

std::string APEXTelemetry::jsonStr(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch(c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                // Escape all control chars AND all non-ASCII bytes.
                // This guarantees the JSON is valid UTF-8 regardless of
                // the Windows code page — critical for WebSocket text frames.
                if (c < 0x20 || c > 0x7E) {
                    char buf[8]; sprintf_s(buf, "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += "\"";
    return out;
}

int APEXCommand::ArgInt(int defaultVal) const {
    if (arg.empty()) return defaultVal;
    try { return std::stoi(arg); }
    catch (...) { return defaultVal; }
}

bool APEXCommand::Parse(const std::string& json, APEXCommand& out) {
    // Minimal parser for flat {"cmd":"...","arg":"..."} objects.
    // Also handles numeric arg: {"cmd":"move_eaf","arg":14500}
    out.cmd.clear(); out.arg.clear();

    // Extract a quoted string value for a given key
    auto extractStr = [&](const std::string& key) -> std::string {
        std::string k = "\"" + key + "\":\"";
        size_t pos = json.find(k);
        if (pos == std::string::npos) return "";
        pos += k.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    // Extract a bare numeric value for a given key (no surrounding quotes)
    auto extractNum = [&](const std::string& key) -> std::string {
        std::string k = "\"" + key + "\":";
        size_t pos = json.find(k);
        if (pos == std::string::npos) return "";
        pos += k.size();
        // Skip optional whitespace
        while (pos < json.size() && json[pos] == ' ') ++pos;
        if (pos >= json.size() || json[pos] == '"') return ""; // quoted — handled above
        size_t end = pos;
        // isdigit() on a plain char is undefined for bytes >= 0x80, which
        // are negative on MSVC -- and this string came off the network.
        while (end < json.size() &&
               (isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) ++end;
        return json.substr(pos, end - pos);
    };

    out.cmd = extractStr("cmd");
    if (out.cmd.empty()) return false;

    // Try quoted arg first, then numeric
    out.arg = extractStr("arg");
    if (out.arg.empty()) out.arg = extractNum("arg");
    return true;
}

} // namespace raduk::net
