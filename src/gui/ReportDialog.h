#pragma once
// ============================================================
// ReportDialog.h — Phase 171: "Report a problem to Claude"
//
// The owner, 2026-11-22: "I want a troubleshooting button built into
// RadukCapture so that when I click it I can describe the issue, close
// the window, and then it builds the file and sends it to Claude."
//
// REPORT on the rail (or Ctrl+Shift+R, or the command palette) opens
// this: a box for what happened, a tick for including screenshots of the
// app's windows, and SEND / CANCEL. SEND closes the window at once and
// hands the words to the app, which gathers the evidence and sends it on
// in the background (main.cpp; core/ProblemReport.h decides what goes in,
// tools/sky_handoff.py sends it to Claude Code on this PC).
//
// Standard Win32 controls on a dark popup, so typing, selection and the
// clipboard all behave as everywhere else in Windows.
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

#include "Theme.h"

#pragma comment(lib, "dwmapi.lib")

namespace raduk::gui {

class ReportDialog {
public:
    using SendFn = std::function<void(const std::wstring& text, bool screenshots)>;
    void SetSendCallback(SendFn f) { m_onSend = std::move(f); }

    bool Create(HWND parent) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"RadukReport";
        RegisterClassExW(&wc);
        m_parent = parent;
        // 2026-11-22 at the scope: REPORT pressed and "nothing happened".
        // The mount controller, GoTo and Help are all topmost; this window
        // was not, so it could open behind them. It is topmost now.
        m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"RadukReport", L"Report a problem to Claude",
                                 WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 560, 380, parent, nullptr,
                                 GetModuleHandle(nullptr), this);
        if (!m_hwnd) return false;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(m_hwnd, 20, &dark, sizeof(dark));
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
            HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10,
                                     m_hwnd, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
            SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
            return h;
        };
        m_label = mk(L"STATIC", L"What happened? Say what you did, what you saw, and what you expected. "
                                L"The log, the settings and the app's state are added for you; your site "
                                L"and passwords are left out.", SS_LEFT, 1);
        m_edit  = mk(L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP, 2);
        m_shots = mk(L"BUTTON", L"Include screenshots of the app's windows", BS_AUTOCHECKBOX | WS_TABSTOP, 3);
        SendMessageW(m_shots, BM_SETCHECK, BST_CHECKED, 0);
        m_send   = mk(L"BUTTON", L"SEND", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK);
        m_cancel = mk(L"BUTTON", L"CANCEL", BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL);
        Layout();
        return true;
    }

    HWND Handle() const { return m_hwnd; }
    bool IsVisible() const { return m_hwnd && IsWindowVisible(m_hwnd); }

    // Opens centred on the main window, the box empty and focused.
    // Returns false, with the reason in LastError(), when it cannot show.
    bool Show() {
        if (!m_hwnd && m_parent) Create(m_parent);   // a failed first Create gets a second try
        if (!m_hwnd) { m_lastError = GetLastError(); return false; }
        RECT pr{}, r{};
        GetWindowRect(m_parent, &pr);
        GetWindowRect(m_hwnd, &r);
        const int w = r.right - r.left, h = r.bottom - r.top;
        SetWindowPos(m_hwnd, HWND_TOP, pr.left + ((pr.right - pr.left) - w) / 2,
                     pr.top + ((pr.bottom - pr.top) - h) / 2, 0, 0, SWP_NOSIZE);
        SetWindowTextW(m_edit, L"");
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ShowWindow(m_hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(m_hwnd);
        SetFocus(m_edit);
        return IsWindowVisible(m_hwnd) != FALSE;
    }
    unsigned long LastError() const { return m_lastError; }
    RECT Placement() const { RECT r{}; if (m_hwnd) GetWindowRect(m_hwnd, &r); return r; }
    void Hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

    // Phase 127's scenarios: type the words, then SEND.
    bool ScenarioSend(const std::wstring& text) {
        if (!m_hwnd) return false;
        SetWindowTextW(m_edit, text.c_str());
        Send();
        return true;
    }

private:
    void Layout() {
        RECT cr; GetClientRect(m_hwnd, &cr);
        const int W = cr.right, H = cr.bottom, pad = 10, bh = 28;
        MoveWindow(m_label, pad, pad, W - 2 * pad, 34, TRUE);
        MoveWindow(m_edit, pad, pad + 40, W - 2 * pad, H - 40 - pad - bh - 34 - pad, TRUE);
        MoveWindow(m_shots, pad, H - pad - bh - 28, W - 2 * pad, 22, TRUE);
        MoveWindow(m_cancel, W - pad - 100, H - pad - bh, 100, bh, TRUE);
        MoveWindow(m_send, W - 2 * pad - 200, H - pad - bh, 100, bh, TRUE);
    }

    void Send() {
        const int n = GetWindowTextLengthW(m_edit);
        std::wstring text((size_t)n, L'\0');
        if (n > 0) GetWindowTextW(m_edit, text.data(), n + 1);
        const bool shots = SendMessageW(m_shots, BM_GETCHECK, 0, 0) == BST_CHECKED;
        Hide();   // closed at once, as asked; the work goes on behind
        if (m_onSend) m_onSend(text, shots);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ReportDialog* self = nullptr;
        if (msg == WM_NCCREATE) {
            self = static_cast<ReportDialog*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        } else {
            self = reinterpret_cast<ReportDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        switch (msg) {
            case WM_ERASEBKGND: {
                RECT r; GetClientRect(hwnd, &r);
                FillRect((HDC)wp, &r, Theme::Brush(Theme::Rgb::DEEP));
                return 1;
            }
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLORBTN: {
                HDC dc = (HDC)wp;
                SetTextColor(dc, Theme::NV(Theme::Rgb::TEXT));
                SetBkColor(dc, Theme::NV(Theme::Rgb::DEEP));
                return (LRESULT)Theme::Brush(Theme::Rgb::DEEP);
            }
            case WM_COMMAND:
                if (LOWORD(wp) == IDOK)     { self->Send(); return 0; }
                if (LOWORD(wp) == IDCANCEL) { self->Hide(); return 0; }
                break;
            case WM_CLOSE:
                self->Hide();
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND m_hwnd = nullptr, m_parent = nullptr;
    unsigned long m_lastError = 0;
    HWND m_label = nullptr, m_edit = nullptr, m_shots = nullptr, m_send = nullptr, m_cancel = nullptr;
    SendFn m_onSend;
};

} // namespace raduk::gui
