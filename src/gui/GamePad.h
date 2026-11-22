#pragma once
// ============================================================
// GamePad.h -- reads an Xbox controller through XInput (Phase 172.0)
// ============================================================
//
// XInput ships with Windows. It is loaded at run time, so a PC without
// it (or without a controller) simply has no pad: no link change, no
// install, nothing online. The mapping is core/PadMap.h; this file only
// reads the state.
// ============================================================

#include "core/PadMap.h"
#include <windows.h>
#include <xinput.h>

namespace raduk::gui {

static_assert(raduk::core::Btn::DpadUp    == XINPUT_GAMEPAD_DPAD_UP,    "PadMap bit");
static_assert(raduk::core::Btn::DpadDown  == XINPUT_GAMEPAD_DPAD_DOWN,  "PadMap bit");
static_assert(raduk::core::Btn::DpadLeft  == XINPUT_GAMEPAD_DPAD_LEFT,  "PadMap bit");
static_assert(raduk::core::Btn::DpadRight == XINPUT_GAMEPAD_DPAD_RIGHT, "PadMap bit");
static_assert(raduk::core::Btn::LB        == XINPUT_GAMEPAD_LEFT_SHOULDER,  "PadMap bit");
static_assert(raduk::core::Btn::RB        == XINPUT_GAMEPAD_RIGHT_SHOULDER, "PadMap bit");
static_assert(raduk::core::Btn::A         == XINPUT_GAMEPAD_A, "PadMap bit");
static_assert(raduk::core::Btn::B         == XINPUT_GAMEPAD_B, "PadMap bit");
static_assert(raduk::core::Btn::X         == XINPUT_GAMEPAD_X, "PadMap bit");
static_assert(raduk::core::Btn::Y         == XINPUT_GAMEPAD_Y, "PadMap bit");

class GamePad {
public:
    GamePad() {
        // 1_4 on Windows 8 and later; 9_1_0 ships with every Windows since Vista.
        const wchar_t* dlls[] = { L"xinput1_4.dll", L"xinput9_1_0.dll" };
        for (auto* d : dlls) {
            m_dll = LoadLibraryW(d);
            if (m_dll) {
                m_get = reinterpret_cast<GetFn>(GetProcAddress(m_dll, "XInputGetState"));
                if (m_get) break;
                FreeLibrary(m_dll); m_dll = nullptr;
            }
        }
    }
    ~GamePad() { if (m_dll) FreeLibrary(m_dll); }
    GamePad(const GamePad&) = delete;
    GamePad& operator=(const GamePad&) = delete;

    bool Available() const { return m_get != nullptr; }

    // The first connected controller. With none connected the slots are
    // scanned at most once a second: polling an empty slot every frame is
    // a known stall (Microsoft's XInput guidance, recalled).
    raduk::core::PadState Read() {
        raduk::core::PadState s;
        if (!m_get) return s;
        const DWORD now = GetTickCount();
        if (m_slot < 0 && now - m_lastScan < 1000) return s;
        if (m_slot < 0) {
            m_lastScan = now;
            for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
                XINPUT_STATE st{};
                if (m_get(i, &st) == ERROR_SUCCESS) { m_slot = (int)i; break; }
            }
            if (m_slot < 0) return s;
        }
        XINPUT_STATE st{};
        if (m_get((DWORD)m_slot, &st) != ERROR_SUCCESS) { m_slot = -1; m_lastScan = now; return s; }
        s.connected = true;
        s.buttons   = st.Gamepad.wButtons;
        s.lx        = st.Gamepad.sThumbLX;
        s.ly        = st.Gamepad.sThumbLY;
        return s;
    }

private:
    using GetFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
    HMODULE m_dll = nullptr;
    GetFn   m_get = nullptr;
    int     m_slot = -1;
    DWORD   m_lastScan = 0;
};

} // namespace raduk::gui
