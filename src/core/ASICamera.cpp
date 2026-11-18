#include "ASICamera.h"
#include <cstdio>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include "CoreLog.h"

namespace raduk::core {

ASICamera::~ASICamera() { Close(); }

ASICamera::ASICamera(ASICamera&& other) noexcept
    : m_cameraId(other.m_cameraId.load()), m_info(other.m_info), m_isOpen(other.m_isOpen),
      m_isCapturing(other.m_isCapturing), m_controls(std::move(other.m_controls)),
      m_serial(std::move(other.m_serial)) {
    m_settings.Restore(other.m_settings.Get());
    other.m_settings.Clear();
    other.m_cameraId = -1; other.m_isOpen = false; other.m_isCapturing = false;
}

ASICamera& ASICamera::operator=(ASICamera&& other) noexcept {
    if (this != &other) {
        Close();
        m_cameraId = other.m_cameraId.load(); m_info = other.m_info; m_isOpen = other.m_isOpen;
        m_isCapturing = other.m_isCapturing; m_controls = std::move(other.m_controls);
        m_serial = std::move(other.m_serial);
        m_settings.Restore(other.m_settings.Get());
        other.m_settings.Clear();
        other.m_cameraId = -1; other.m_isOpen = false; other.m_isCapturing = false;
    }
    return *this;
}

std::vector<ASI_CAMERA_INFO> ASICamera::Enumerate() {
    int count = ASIGetNumOfConnectedCameras();
    std::vector<ASI_CAMERA_INFO> cameras;
    for (int i = 0; i < count; i++) {
        ASI_CAMERA_INFO info{};
        if (ASIGetCameraProperty(&info, i) == ASI_SUCCESS) cameras.push_back(info);
    }
    return cameras;
}

bool ASICamera::Open(int cameraIndex) {
    if (m_isOpen) return false;
    if (ASIGetCameraProperty(&m_info, cameraIndex) != ASI_SUCCESS) return false;
    m_cameraId = m_info.CameraID;
    if (ASIOpenCamera(m_cameraId) != ASI_SUCCESS) { m_cameraId = -1; return false; }
    if (ASIInitCamera(m_cameraId) != ASI_SUCCESS) { ASICloseCamera(m_cameraId); m_cameraId = -1; return false; }

    // A fresh camera: nothing it was told before applies to it, and its
    // serial is how Reconnect will know it again among others.
    m_settings.Clear();
    m_serial.clear();
    {
        ASI_SN sn{};
        if (ASIGetSerialNumber(m_cameraId, &sn) == ASI_SUCCESS)
            m_serial = SerialHex(sn.id, (int)sizeof(sn.id));
    }

    int numControls = 0;
    ASIGetNumOfControls(m_cameraId, &numControls);
    m_controls.clear();
    for (int i = 0; i < numControls; i++) {
        ASI_CONTROL_CAPS caps{};
        if (ASIGetControlCaps(m_cameraId, i, &caps) == ASI_SUCCESS) m_controls.push_back(caps);
    }
    m_isOpen = true;
    return true;
}

void ASICamera::Close() {
    if (!m_isOpen) return;
    if (m_isCapturing) StopVideoCapture();
    ASICloseCamera(m_cameraId);
    m_isOpen = false; m_cameraId = -1;
    m_settings.Clear();
}

bool ASICamera::SetROI(int width, int height, int binning, ASI_IMG_TYPE imgType) {
    if (!m_isOpen) return false;
    width = (width / 8) * 8; height = (height / 2) * 2;
    if (ASISetROIFormat(m_cameraId, width, height, binning, imgType) != ASI_SUCCESS) return false;
    m_settings.Region(width, height, binning, (int)imgType);
    return true;
}

bool ASICamera::GetROI(int& width, int& height, int& binning, ASI_IMG_TYPE& imgType) const {
    return ASIGetROIFormat(m_cameraId, &width, &height, &binning, &imgType) == ASI_SUCCESS;
}

bool ASICamera::SetStartPos(int startX, int startY) {
    startX = (startX / 8) * 8; startY = (startY / 2) * 2;
    if (ASISetStartPos(m_cameraId, startX, startY) != ASI_SUCCESS) return false;
    m_settings.Start(startX, startY);
    return true;
}

bool ASICamera::GetStartPos(int& startX, int& startY) const {
    return ASIGetStartPos(m_cameraId, &startX, &startY) == ASI_SUCCESS;
}

bool ASICamera::SetControl(ASI_CONTROL_TYPE type, long value, bool autoMode) {
    if (ASISetControlValue(m_cameraId, type, value, autoMode ? ASI_TRUE : ASI_FALSE) != ASI_SUCCESS)
        return false;
    m_settings.Control((int)type, value, autoMode);
    return true;
}

long ASICamera::GetControl(ASI_CONTROL_TYPE type) const {
    long val = 0; ASI_BOOL isAuto = ASI_FALSE;
    ASIGetControlValue(m_cameraId, type, &val, &isAuto);
    return val;
}

bool ASICamera::GetControlCaps(ASI_CONTROL_TYPE type, ASI_CONTROL_CAPS& caps) const {
    const ASI_CONTROL_CAPS* found = FindControl(type);
    if (!found) return false;
    caps = *found; return true;
}

void ASICamera::SetGain(int gain) { SetControl(ASI_GAIN, gain); }
void ASICamera::SetExposure(double milliseconds) { SetControl(ASI_EXPOSURE, static_cast<long>(milliseconds * 1000.0)); }
void ASICamera::SetGamma(int gamma) { SetControl(ASI_GAMMA, gamma); }
void ASICamera::SetWhiteBalance(int red, int blue) { SetControl(ASI_WB_R, red); SetControl(ASI_WB_B, blue); }
void ASICamera::SetUSBBandwidth(int percent) { SetControl(ASI_BANDWIDTHOVERLOAD, percent); }
void ASICamera::SetHighSpeedMode(bool enabled) { SetControl(ASI_HIGH_SPEED_MODE, enabled ? 1 : 0); }
void ASICamera::SetFlip(ASI_FLIP_STATUS flip) { SetControl(ASI_FLIP, static_cast<long>(flip)); }

bool ASICamera::StartVideoCapture() {
    if (!m_isOpen || m_isCapturing) return false;
    if (ASIStartVideoCapture(m_cameraId) != ASI_SUCCESS) return false;
    m_isCapturing = true; return true;
}

void ASICamera::StopVideoCapture() {
    if (!m_isOpen || !m_isCapturing) return;
    ASIStopVideoCapture(m_cameraId); m_isCapturing = false;
}

bool ASICamera::RestartCapture() {
    if (!m_isOpen || !m_isCapturing) return false;
    ASIStopVideoCapture(m_cameraId);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Crucial USB settle time
    return ASIStartVideoCapture(m_cameraId) == ASI_SUCCESS;
}

// After a USB drop. The old handle is dead once the cable was pulled:
// ASIStopVideoCapture / ASIStartVideoCapture on it only return
// CAMERA_CLOSED or CAMERA_REMOVED, however long the loop retries. The
// SDK wants the camera closed, enumerated again, opened and initialised,
// and it then sits at its power-on defaults, so every setting is told
// to it again from the cache.
//
// m_isOpen stays true throughout: to the rest of the app the camera
// never closed, and the UI thread keeps reading IsOpen() each frame.
// Its calls with the old ID meanwhile just fail, as they did while the
// cable was out.
bool ASICamera::Reconnect() {
    if (!m_isOpen) return false;
    const int oldId = m_cameraId;
    if (m_isCapturing) ASIStopVideoCapture(oldId);
    ASICloseCamera(oldId);

    // The same model, and if it gave a serial, the same serial: a
    // second camera of another model (a guide camera) must never be
    // picked up in its place.
    int newId = -1;
    const int n = ASIGetNumOfConnectedCameras();
    for (int i = 0; i < n && newId < 0; ++i) {
        ASI_CAMERA_INFO info{};
        if (ASIGetCameraProperty(&info, i) != ASI_SUCCESS) continue;
        if (std::strncmp(info.Name, m_info.Name, sizeof(info.Name)) != 0) continue;
        const int id = info.CameraID;
        if (ASIOpenCamera(id) != ASI_SUCCESS) continue;
        if (ASIInitCamera(id) != ASI_SUCCESS) { ASICloseCamera(id); continue; }
        if (!m_serial.empty()) {
            ASI_SN sn{};
            if (ASIGetSerialNumber(id, &sn) != ASI_SUCCESS ||
                SerialHex(sn.id, (int)sizeof(sn.id)) != m_serial) {
                ASICloseCamera(id);
                continue;
            }
        }
        newId = id;
    }
    if (newId < 0) return false;   // still unplugged; the loop retries

    m_cameraId = newId;
    m_info.CameraID = newId;
    const CameraSettingsSnapshot s = m_settings.Get();
    const bool restored = ReplaySettings(s,
        [this](int w, int h, int b, int t) { return SetROI(w, h, b, (ASI_IMG_TYPE)t); },
        [this](int x, int y)               { return SetStartPos(x, y); },
        [this](int id, long v, bool a)     { return SetControl((ASI_CONTROL_TYPE)id, v, a); });
    raduk::core::CoreLog("[ASICamera] Re-opened %s after a USB drop (ID %d -> %d), "
                         "%zu settings restored%s\n",
                         m_info.Name, oldId, newId, s.controls.size(),
                         restored ? "" : ", some refused");
    if (!m_isCapturing) return true;
    return ASIStartVideoCapture(newId) == ASI_SUCCESS;
}

bool ASICamera::GetVideoFrame(uint8_t* buffer, int bufferSize, int timeoutMs) {
    ASI_ERROR_CODE err;
    return GetVideoFrameEx(buffer, bufferSize, timeoutMs, err);
}

bool ASICamera::GetVideoFrameEx(uint8_t* buffer, int bufferSize, int timeoutMs,
                                 ASI_ERROR_CODE& err) {
    err = ASIGetVideoData(m_cameraId, buffer, bufferSize, timeoutMs);
    return err == ASI_SUCCESS;
}

int ASICamera::GetDroppedFrames() const {
    int dropped = 0; ASIGetDroppedFrames(m_cameraId, &dropped); return dropped;
}

float ASICamera::Temperature() const { return static_cast<float>(GetControl(ASI_TEMPERATURE)) / 10.0f; }

int ASICamera::FrameSize(int width, int height, ASI_IMG_TYPE imgType) {
    if (imgType == ASI_IMG_RAW16) return width * height * 2;
    if (imgType == ASI_IMG_RGB24) return width * height * 3;
    return width * height;
}

const char* ASICamera::ErrorString(ASI_ERROR_CODE err) {
    switch (err) {
        case ASI_SUCCESS: return "SUCCESS"; case ASI_ERROR_TIMEOUT: return "TIMEOUT";
        case ASI_ERROR_CAMERA_REMOVED: return "CAMERA_REMOVED";
        case ASI_ERROR_CAMERA_CLOSED:  return "CAMERA_CLOSED";
        default: return "SDK_ERROR";
    }
}

const ASI_CONTROL_CAPS* ASICamera::FindControl(ASI_CONTROL_TYPE type) const {
    for (const auto& caps : m_controls) if (caps.ControlType == type) return &caps;
    return nullptr;
}

// Phase 157.3. ASICamera2 has no call for the camera's firmware (1.41
// has none), so that stays empty and the log says "not reported".
DeviceIdentity ASICamera::Identity() const {
    DeviceIdentity d;
    d.model = m_info.Name;
    d.sdk   = TidyVersion(ASIGetSDKVersion());
    if (m_isOpen) {
        ASI_SN sn{};
        if (ASIGetSerialNumber(m_cameraId, &sn) == ASI_SUCCESS)
            d.serial = SerialHex(sn.id, (int)sizeof(sn.id));
    }
    return d;
}

} // namespace raduk::core
