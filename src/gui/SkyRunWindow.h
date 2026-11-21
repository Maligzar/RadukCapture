#pragma once
// ============================================================
// SkyRunWindow.h — Phase 156.2c: SKY RUN, in the app (Shift+R)
//
// The owner's rule (2026-11-21): the app is the instrument; Dash is a
// remote for an iPhone or iPad and a test harness, never used in its
// place. The sky run had its controls only in Dash (156.2a). This is its
// own window: the state, the step now running, every reason it will not
// start, the plan with each group's and each item's status, and START
// and STOP. Dash's SKY RUN card mirrors it.
//
// What it says is decided by app/SkyRunView.h (pure, tested on Linux);
// this paints it and turns clicks into the two callbacks. Painted in GDI
// on the window chrome, like the Status window, so night vision applies
// through Theme::NV.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <functional>
#include <string>
#include <vector>

#include "WindowChrome.h"
#include "Theme.h"
#include "../app/SkyRunView.h"
#include "../core/TextConv.h"

#pragma comment(lib, "dwmapi.lib")

namespace raduk::gui {

class SkyRunWindow {
public:
    void SetStartCallback(std::function<void()> f) { m_onStart = std::move(f); }
    void SetStopCallback(std::function<void()> f)  { m_onStop = std::move(f); }

    bool Create(HWND parent, int x, int y, int w, int h) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;              // painted whole in WM_PAINT
        wc.lpszClassName = L"RadukSkyRun";
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            0, L"RadukSkyRun", L"Sky run",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX,
            x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);
        if (!m_hwnd) return false;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(m_hwnd, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(m_hwnd, 19, &dark, sizeof(dark));
        chrome::Flags f;
        f.minimize = false;
        m_chrome.Attach(m_hwnd, f);
        return true;   // hidden until asked for
    }

    void Destroy() {
        m_chrome.Release();
        if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    }

    HWND Handle() const { return m_hwnd; }
    bool IsVisible() const { return m_hwnd && IsWindowVisible(m_hwnd); }
    void Show()   { if (m_hwnd) { ShowWindow(m_hwnd, SW_SHOW); SetForegroundWindow(m_hwnd); } }
    void Hide()   { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }
    void Toggle() { if (!m_hwnd) return; IsVisible() ? Hide() : Show(); }

    // From the readout (about 2 Hz). Repaints only when something changed.
    void Update(const raduk::app::SkyRunViewModel& v) {
        if (Same(v, m_view)) return;
        m_view = v;
        if (m_hwnd && IsVisible()) InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    // Phase 127's scenario runner: "START" and "STOP", as a click would.
    bool ScenarioPress(const std::string& name) {
        if (name == "START") { if (!m_view.canStart) return false; if (m_onStart) m_onStart(); return true; }
        if (name == "STOP")  { if (!m_view.canStop)  return false; if (m_onStop)  m_onStop();  return true; }
        return false;
    }

private:
    static bool Same(const raduk::app::SkyRunViewModel& a, const raduk::app::SkyRunViewModel& b) {
        if (a.state != b.state || a.canStart != b.canStart || a.canStop != b.canStop || a.lines.size() != b.lines.size())
            return false;
        for (size_t i = 0; i < a.lines.size(); i++)
            if (a.lines[i].text != b.lines[i].text || a.lines[i].tone != b.lines[i].tone) return false;
        return true;
    }

    static COLORREF ToneColour(raduk::app::SkyRunLine::Tone t) {
        using T = raduk::app::SkyRunLine::Tone;
        switch (t) {
            case T::Good:   return Theme::NV(Theme::Rgb::ACCENT3);
            case T::Accent: return Theme::NV(Theme::Rgb::ACCENT);
            case T::Warn:   return Theme::NV(Theme::Rgb::ACCENT2);
            case T::Bad:    return Theme::NV(Theme::Rgb::ACCENT4);
            case T::Dim:    return Theme::NV(Theme::Rgb::TEXT_DIM);
            default:        return Theme::NV(Theme::Rgb::TEXT);
        }
    }

    static HFONT MakeFont(int px, bool bold, const char* face) {
        return CreateFontA(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    }

    // accent is a palette token; Theme::Brush and NV apply night vision.
    void DrawButton(HDC hdc, const RECT& r, const wchar_t* text, bool enabled, COLORREF accent, HFONT f) {
        FillRect(hdc, &r, Theme::Brush(enabled ? Theme::Rgb::PANEL : Theme::Rgb::DEEP));
        FrameRect(hdc, &r, Theme::Brush(enabled ? accent : Theme::Rgb::BORDER));
        HGDIOBJ old = SelectObject(hdc, f);
        SetTextColor(hdc, Theme::NV(enabled ? accent : Theme::Rgb::TEXT_DIM));
        RECT t = r;
        DrawTextW(hdc, text, -1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
    }

    void Draw(HDC hdc, const RECT& body) {
        const int W = body.right, H = body.bottom;
        FillRect(hdc, &body, Theme::Brush(Theme::Rgb::DEEP));
        const int pad = 10, fontPx = 14, lineH = 19, btnH = 30;
        HFONT hf = MakeFont(fontPx + 4, true, "Barlow Condensed");
        HFONT bf = MakeFont(fontPx, false, "Space Mono");
        HFONT kf = MakeFont(fontPx + 1, true, "Barlow Condensed");
        SetBkMode(hdc, TRANSPARENT);

        // The headline.
        HGDIOBJ old = SelectObject(hdc, hf);
        SetTextColor(hdc, Theme::NV(Theme::Rgb::TEXT));
        RECT hr = { pad, pad, W - pad, pad + lineH + 6 };
        const std::wstring head = L"SKY RUN: " + raduk::core::ToWide(m_view.state);
        DrawTextW(hdc, head.c_str(), -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // The lines, wrapped, down to the buttons.
        int y = hr.bottom + 6;
        const int bottom = H - pad - btnH - 8;
        for (const auto& l : m_view.lines) {
            SelectObject(hdc, l.heading ? kf : bf);
            SetTextColor(hdc, ToneColour(l.tone));
            const std::wstring w = raduk::core::ToWide(l.text);
            RECT measure = { pad, y, W - pad, y };
            DrawTextW(hdc, w.c_str(), -1, &measure, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
            if (measure.bottom > bottom) {
                RECT more = { pad, y, W - pad, bottom };
                SetTextColor(hdc, Theme::NV(Theme::Rgb::TEXT_DIM));
                DrawTextW(hdc, L"... (enlarge the window for the rest)", -1, &more, DT_LEFT | DT_SINGLELINE);
                break;
            }
            RECT r = { pad, y, W - pad, measure.bottom };
            DrawTextW(hdc, w.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK);
            y = measure.bottom + 3;
        }
        SelectObject(hdc, old);

        // START and STOP.
        const int bw = (W - pad * 3) / 2;
        m_startRect = { pad, H - pad - btnH, pad + bw, H - pad };
        m_stopRect  = { pad * 2 + bw, H - pad - btnH, W - pad, H - pad };
        DrawButton(hdc, m_startRect, L"\u25B6  START", m_view.canStart, Theme::Rgb::ACCENT3, kf);
        DrawButton(hdc, m_stopRect, L"\u25A0  STOP (parks)", m_view.canStop, Theme::Rgb::ACCENT4, kf);

        DeleteObject(hf); DeleteObject(bf); DeleteObject(kf);
    }

    void Click(int x, int y) {
        const POINT p = { x, y };
        if (PtInRect(&m_startRect, p) && m_view.canStart && m_onStart) m_onStart();
        else if (PtInRect(&m_stopRect, p) && m_view.canStop && m_onStop) m_onStop();
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        SkyRunWindow* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<SkyRunWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        } else {
            self = reinterpret_cast<SkyRunWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

        LRESULT chromeResult = 0;
        if (self->m_chrome.Filter(hwnd, msg, wp, lp, chromeResult)) return chromeResult;

        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT cr; GetClientRect(hwnd, &cr);
                const int top = self->m_chrome.CaptionHeight();
                self->m_chrome.DrawGdi(hdc, cr.right);
                POINT was;
                SetViewportOrgEx(hdc, 0, top, &was);
                RECT body = { 0, 0, cr.right, cr.bottom - top };
                self->m_top = top;
                self->Draw(hdc, body);
                SetViewportOrgEx(hdc, was.x, was.y, nullptr);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_LBUTTONUP:
                self->Click((short)LOWORD(lp), (short)HIWORD(lp) - self->m_top);
                return 0;
            case WM_SIZE:
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_KEYDOWN:
                // Esc here is still the emergency stop's key app-wide; in
                // this window it only closes it, like the Status window.
                if (wp == VK_ESCAPE) { self->Hide(); return 0; }
                break;
            case WM_CLOSE:
                self->Hide();
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND         m_hwnd = nullptr;
    WindowChrome m_chrome;
    raduk::app::SkyRunViewModel m_view;
    RECT         m_startRect = {}, m_stopRect = {};
    int          m_top = 0;
    std::function<void()> m_onStart, m_onStop;
};

} // namespace raduk::gui
