#pragma once
// ============================================================
// LinesWindow.h — phases 156.2c and 166.1: a window of lines and buttons
//
// The owner's rule (2026-11-21): the app is the instrument; Dash is a
// remote for an iPhone or iPad and a test harness, never used in its
// place. The sky run (Shift+R) and the weather (Shift+W) had their full
// view only in Dash; each now has a window here, and Dash mirrors it.
//
// What a window says is an app::LinesView built by a pure function
// (app/SkyRunView.h, app/WeatherView.h), tested on Linux; this paints it
// (a headline, lines in their tones, a row of buttons) and turns a click
// on an enabled button into the callback, with the button's id. Painted
// in GDI on the window chrome, like the Status window, so night vision
// applies through Theme.
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
#include "../app/LinesView.h"
#include "../core/TextConv.h"

#pragma comment(lib, "dwmapi.lib")

namespace raduk::gui {

class LinesWindow {
public:
    // A click on an enabled button, or a scenario's press, with its id.
    void SetButtonCallback(std::function<void(const std::string&)> f) { m_onButton = std::move(f); }

    // className must differ per window; title is the caption's text.
    bool Create(HWND parent, const wchar_t* className, const wchar_t* title, int x, int y, int w, int h) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;              // painted whole in WM_PAINT
        wc.lpszClassName = className;
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            0, className, title,
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
    void Update(const raduk::app::LinesView& v) {
        if (raduk::app::SameView(v, m_view)) return;
        m_view = v;
        if (m_hwnd && IsVisible()) InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    // Phase 127's scenario runner: press a button by its id, as a click
    // would. False when there is no such button or it is off.
    bool ScenarioPress(const std::string& id) {
        const auto* b = m_view.Button(id);
        if (!b || !b->enabled) return false;
        if (m_onButton) m_onButton(id);
        return true;
    }

private:
    static COLORREF ToneColour(raduk::app::ViewLine::Tone t) { return Theme::NV(ToneToken(t)); }

    static HFONT MakeFont(int px, bool bold, const char* face) {
        return CreateFontA(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    }

    // The tone's palette token (Theme::Brush and NV apply night vision).
    static COLORREF ToneToken(raduk::app::ViewLine::Tone t) {
        using T = raduk::app::ViewLine::Tone;
        switch (t) {
            case T::Good: return Theme::Rgb::ACCENT3;
            case T::Warn: return Theme::Rgb::ACCENT2;
            case T::Bad:  return Theme::Rgb::ACCENT4;
            case T::Dim:  return Theme::Rgb::TEXT_DIM;
            case T::Normal: return Theme::Rgb::TEXT;
            default:      return Theme::Rgb::ACCENT;
        }
    }

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
        const std::wstring head = raduk::core::ToWide(m_view.headline);
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

        // The buttons, side by side across the bottom.
        m_rects.clear();
        const int n = (int)m_view.buttons.size();
        if (n > 0) {
            const int bw = (W - pad * (n + 1)) / n;
            for (int i = 0; i < n; i++) {
                const auto& b = m_view.buttons[(size_t)i];
                RECT r = { pad + i * (bw + pad), H - pad - btnH, pad + i * (bw + pad) + bw, H - pad };
                m_rects.push_back(r);
                const std::wstring label = raduk::core::ToWide(b.label);
                DrawButton(hdc, r, label.c_str(), b.enabled, ToneToken(b.tone), kf);
            }
        }
        DeleteObject(hf); DeleteObject(bf); DeleteObject(kf);
    }

    void Click(int x, int y) {
        const POINT p = { x, y };
        for (size_t i = 0; i < m_rects.size() && i < m_view.buttons.size(); i++)
            if (PtInRect(&m_rects[i], p)) {
                const auto& b = m_view.buttons[i];
                if (b.enabled && m_onButton) m_onButton(b.id);
                return;
            }
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        LinesWindow* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<LinesWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        } else {
            self = reinterpret_cast<LinesWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
    raduk::app::LinesView m_view;
    std::vector<RECT> m_rects;
    int          m_top = 0;
    std::function<void(const std::string&)> m_onButton;
};

} // namespace raduk::gui
