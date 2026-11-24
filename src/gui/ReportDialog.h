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
#include <cstdio>
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

    bool Create(HWND parent) { return CreateStyled(parent, false); }

    // 2026-11-22: on the owner's PC the owned tool window was created but
    // never became visible ("could not be shown (error 0)", twice). The
    // fallback is the plainest window Windows has: no owner, a normal title
    // bar, in the taskbar, centred on the main screen's work area.
    bool CreateStyled(HWND parent, bool plain) {
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
        m_plain = plain;
        m_hwnd = plain
            ? CreateWindowExW(WS_EX_APPWINDOW | WS_EX_TOPMOST, L"RadukReport", L"Report a problem to Claude",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              0, 0, 560, 380, nullptr, nullptr, GetModuleHandle(nullptr), this)
            : CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"RadukReport", L"Report a problem to Claude",
                              WS_POPUP | WS_CAPTION | WS_SYSMENU,
                              0, 0, 560, 380, parent, nullptr, GetModuleHandle(nullptr), this);
        if (!m_hwnd) m_lastError = GetLastError();
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
        // 2026-11-24: the cause of "could not be shown". The text box had
        // control id 2, which is IDCANCEL (and the label id 1, IDOK). Show()
        // gives the box the focus; the box tells its parent with
        // WM_COMMAND(id 2, EN_SETFOCUS), and WM_COMMAND read that as
        // CANCEL and hid the window in the same instant. Typing would have
        // done the same (EN_CHANGE). Own ids, clear of IDOK and IDCANCEL.
        m_label = mk(L"STATIC", L"What happened? Say what you did, what you saw, and what you expected. "
                                L"The log, the settings and the app's state are added for you; your site "
                                L"and passwords are left out.", SS_LEFT, kIdLabel);
        m_edit  = mk(L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP, kIdEdit);
        m_shots = mk(L"BUTTON", L"Include screenshots of the app's windows", BS_AUTOCHECKBOX | WS_TABSTOP, kIdShots);
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
        m_diag.clear();
        if (ShowOnce()) return true;
        // Not visible: say everything about the window, then the plain one.
        m_diag = Describe();
        if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
        if (!CreateStyled(m_parent, true)) return false;
        return ShowOnce();
    }
    bool UsedFallback() const { return m_plain; }
    // What the first attempt looked like when it failed (empty if it did not).
    const std::string& FailureDetail() const { return m_diag; }

    std::string Describe() const {
        char b[320];
        if (!m_hwnd) { snprintf(b, sizeof(b), "no window (create error %lu)", m_lastError); return b; }
        RECT r{}; GetWindowRect(m_hwnd, &r);
        const LONG_PTR st = GetWindowLongPtrW(m_hwnd, GWL_STYLE), ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        HWND owner = GetWindow(m_hwnd, GW_OWNER);
        snprintf(b, sizeof(b), "hwnd %p IsWindow %d visible-style %d style 0x%llx ex 0x%llx rect %ld,%ld %ldx%ld "
                 "owner %p owner-visible %d iconic-owner %d plain %d",
                 (void*)m_hwnd, IsWindow(m_hwnd) ? 1 : 0, (st & WS_VISIBLE) ? 1 : 0,
                 (unsigned long long)st, (unsigned long long)ex, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 (void*)owner, owner ? (IsWindowVisible(owner) ? 1 : 0) : -1, owner ? (IsIconic(owner) ? 1 : 0) : -1,
                 m_plain ? 1 : 0);
        return b;
    }

private:
    bool ShowOnce() {
        if (!m_hwnd && m_parent) CreateStyled(m_parent, m_plain);   // a failed Create gets a second try
        if (!m_hwnd) { if (!m_lastError) m_lastError = GetLastError(); return false; }
        RECT pr{}, r{};
        if (m_plain || !m_parent || !GetWindowRect(m_parent, &pr) || pr.right <= pr.left)
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &pr, 0);   // the main screen's work area
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

public:
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
                // Only a click on SEND or CANCEL. A notification from any
                // other control (a focus change, a text change) is not a
                // command to close (2026-11-24, above).
                if (HIWORD(wp) == BN_CLICKED && LOWORD(wp) == IDOK)     { self->Send(); return 0; }
                if (HIWORD(wp) == BN_CLICKED && LOWORD(wp) == IDCANCEL) { self->Hide(); return 0; }
                break;
            case WM_CLOSE:
                self->Hide();
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

public:
    // Control ids: never 1 (IDOK) or 2 (IDCANCEL), which belong to the buttons.
    static constexpr int kIdLabel = 101, kIdEdit = 102, kIdShots = 103;
    HWND EditHandle() const { return m_edit; }   // for tests/report_dialog.cpp
private:
    HWND m_hwnd = nullptr, m_parent = nullptr;
    unsigned long m_lastError = 0;
    bool m_plain = false;
    std::string m_diag;
    HWND m_label = nullptr, m_edit = nullptr, m_shots = nullptr, m_send = nullptr, m_cancel = nullptr;
    SendFn m_onSend;
};

} // namespace raduk::gui
