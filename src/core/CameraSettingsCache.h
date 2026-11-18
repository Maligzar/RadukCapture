#pragma once
// ============================================================
//  CameraSettingsCache -- what the camera was last told, so a
//  replugged camera can be told it again
//
//  A pulled USB cable kills the SDK handle. Stopping and starting
//  video on it (RestartCapture) cannot bring it back: the camera has
//  to be found and opened again, possibly under a new ID, and it
//  comes back at its power-on defaults -- full sensor, default gain
//  and exposure. Nothing can be read from the dead handle, so the
//  settings have to be remembered as they are set.
//
//  The camera records every setting that succeeded; Reconnect()
//  replays them onto the new handle in an order the SDK accepts:
//  region first (ZWO's SetROIFormat re-centres the window), then the
//  start position, then the controls, each once, with its last value.
//
//  Written from the UI thread (setters), read from the capture thread
//  (Reconnect), so it is locked. Pure, no SDK headers, the IDs are the
//  SDK's own integers: tests/camera_replug.cpp.
// ============================================================

#include <mutex>
#include <vector>

namespace raduk::core {

struct CameraSettingsSnapshot {
    bool haveRegion = false;
    int  width = 0, height = 0, binning = 1, imgType = 0;
    bool haveStart = false;
    int  startX = 0, startY = 0;
    struct Control { int id; long value; bool autoMode; };
    std::vector<Control> controls;   // in the order first set
};

class CameraSettingsCache {
public:
    void Region(int w, int h, int bin, int type) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_s.haveRegion = true;
        m_s.width = w; m_s.height = h; m_s.binning = bin; m_s.imgType = type;
        // A new format is centred by the SDK; a start position set
        // before it no longer holds, and replaying it would move the
        // window somewhere the app never put it.
        m_s.haveStart = false;
    }
    void Start(int x, int y) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_s.haveStart = true; m_s.startX = x; m_s.startY = y;
    }
    void Control(int id, long value, bool autoMode) {
        std::lock_guard<std::mutex> lk(m_mx);
        for (auto& c : m_s.controls)
            if (c.id == id) { c.value = value; c.autoMode = autoMode; return; }
        m_s.controls.push_back({id, value, autoMode});
    }
    void Clear() {
        std::lock_guard<std::mutex> lk(m_mx);
        m_s = CameraSettingsSnapshot{};
    }
    // For a camera object being moved: the mutex cannot move, the state can.
    void Restore(const CameraSettingsSnapshot& s) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_s = s;
    }
    CameraSettingsSnapshot Get() const {
        std::lock_guard<std::mutex> lk(m_mx);
        return m_s;
    }

private:
    mutable std::mutex     m_mx;
    CameraSettingsSnapshot m_s;
};

// Replays a snapshot through the camera's own setters. Returns false if
// any setting was refused; the rest are still applied, since a camera
// streaming with one wrong control is better than one not streaming.
template <class RegionFn, class StartFn, class ControlFn>
bool ReplaySettings(const CameraSettingsSnapshot& s, RegionFn region,
                    StartFn start, ControlFn control) {
    bool ok = true;
    if (s.haveRegion) ok &= region(s.width, s.height, s.binning, s.imgType);
    if (s.haveStart)  ok &= start(s.startX, s.startY);
    for (const auto& c : s.controls) ok &= control(c.id, c.value, c.autoMode);
    return ok;
}

} // namespace raduk::core
