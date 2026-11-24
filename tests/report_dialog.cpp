// report_dialog: the REPORT window stays open (2026-11-24). Its text box had
// control id 2 (IDCANCEL); giving it the focus on Show sent WM_COMMAND(2,
// EN_SETFOCUS), which the dialog read as CANCEL and hid the window at once:
// "[REPORT] could not be shown (error 0)", visible-style 0, on every press.
#include "gui/ReportDialog.h"
#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif
#include <cstdio>
#include <string>

using raduk::gui::ReportDialog;
static int g_fail = 0;
static void Check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("\n=== The REPORT window ===\n");
    HWND parent = CreateWindowExW(0, L"STATIC", L"report_dialog test", WS_OVERLAPPEDWINDOW,
                                  100, 100, 400, 300, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    ShowWindow(parent, SW_SHOWNOACTIVATE);
    ReportDialog d;
    Check(d.Create(parent), "created, owned by the main window");

    std::printf("\n1. It shows, and stays shown\n");
    Check(d.Show(), "Show() succeeds (it hid itself here before the fix)");
    Check(d.IsVisible(), "visible");
    Check(!d.UsedFallback() && d.FailureDetail().empty(), "the first window, no fallback needed");

    std::printf("\n2. The text box's own notifications do not close it\n");
    const HWND h = d.Handle(), edit = d.EditHandle();
    Check(GetDlgCtrlID(edit) != IDOK && GetDlgCtrlID(edit) != IDCANCEL, "the text box's id is neither IDOK nor IDCANCEL");
    SetFocus(edit);
    SetWindowTextW(edit, L"typing a description");          // a real EN_CHANGE
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(ReportDialog::kIdEdit, EN_SETFOCUS), (LPARAM)edit);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(ReportDialog::kIdEdit, EN_CHANGE), (LPARAM)edit);
    Check(d.IsVisible(), "still open after focus and typing");
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(IDCANCEL, EN_SETFOCUS), 0);   // id 2, but not a click
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(IDOK, EN_CHANGE), 0);         // id 1, but not a click
    Check(d.IsVisible(), "a non-click notification carrying id 1 or 2 is not a command");

    std::printf("\n3. CANCEL and SEND still work\n");
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
    Check(!d.IsVisible(), "CANCEL closes it");
    std::wstring sent; bool shots = false; int calls = 0;
    d.SetSendCallback([&](const std::wstring& t, bool s) { sent = t; shots = s; ++calls; });
    Check(d.Show() && d.IsVisible(), "it opens again");
    SetWindowTextW(d.EditHandle(), L"autofocus left a donut");
    SendMessageW(d.Handle(), WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
    Check(calls == 1 && sent == L"autofocus left a donut" && shots, "SEND hands over the words and the screenshot choice");
    Check(!d.IsVisible(), "and closes the window");

    DestroyWindow(parent);
    std::printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
