// ============================================================
//  The Moon map window, looked at without the app (phase 162)
//
//  Not in run-tests.ps1: it opens a window, so it cannot gate a merge.
//  It builds the real window over the real controller with a project of
//  this rig's size, and saves what the window draws as PNGs, so its
//  layout can be checked on a PC whose emulator is busy:
//
//      manual-moon-map-window.exe <folder for the pictures>
//
//    1_tonight   a new project under tonight's real light, a few
//                sections captured, one picked
//    2_ready     every section captured: READY FOR MOSAIC
//    3_night     the same, in night vision
//
//  It never takes the keyboard: the window is shown without activating.
// ============================================================

#include "gui/MoonMapWindow.h"
#include "gui/WindowShot.h"
#include "sky/SkyObject.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace raduk;

static void Pump(int ms) {
    const DWORD until = GetTickCount() + (DWORD)ms;
    MSG m;
    while (GetTickCount() < until) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(10);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { printf("usage: manual-moon-map-window <folder>\n"); return 2; }
    const std::wstring out = argv[1];
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const auto dir = std::filesystem::temp_directory_path() / "raduk_moon_map_window";
    std::filesystem::remove_all(dir, ec);

    app::NullUiSink ui;
    app::MoonMapController c(ui);
    gui::MoonMapWindow win;
    app::MoonMapHooks h;
    h.view = [] { return sky::LunarViewAt(sky::CurrentJD(), sky::LunarObserver{}); };
    h.jdNow = [] { return sky::CurrentJD(); };
    h.changed = [&] { win.Refresh(); };
    c.SetHooks(h);
    c.Open(dir.string());

    std::string why;
    const int n = sky::SectionsPerSide(sky::FrameFov(3840, 2160, 0.203));
    if (!c.NewProject("Moon 2026-11-16", n, why)) { printf("no project: %s\n", why.c_str()); return 1; }
    printf("%s\n", sky::DescribeMoonGrid(sky::FrameFov(3840, 2160, 0.203), n).c_str());
    printf("in sunlight now: %d of %d\n", c.CapturableNow(), c.Active().Total());

    if (!win.Create(nullptr, 40, 40, 880, 980)) { printf("no window\n"); return 1; }
    win.SetModel(&c);
    win.SetCommand([&](const std::string& cmd, const std::string& arg) { if (cmd == "SELECT") c.Select(arg); });
    ShowWindow(win.Handle(), SW_SHOWNOACTIVATE);

    // Tonight: whatever is lit, with the first dozen lit sections captured.
    int marked = 0;
    for (size_t i = 0; i < c.Active().Sections().size() && marked < 12; ++i)
        if (c.Sky((int)i).Capturable()) { c.Mark(c.Active().Sections()[i].id); ++marked; }
    c.Select("NE-B3");
    Pump(600);
    int shots = gui::ShotAllWindows(out, L"1_tonight");

    for (const auto& s : c.Active().Sections())
        if (!c.Active().IsCaptured(s.id)) c.Mark(s.id);
    Pump(400);
    shots += gui::ShotAllWindows(out, L"2_ready");

    gui::Theme::SetNightVision(true);
    InvalidateRect(win.Handle(), nullptr, FALSE);
    Pump(400);
    shots += gui::ShotAllWindows(out, L"3_night");

    printf("%d pictures in %ls; %s\n", shots, out.c_str(), c.Active().Summary().c_str());
    win.Destroy();
    std::filesystem::remove_all(dir, ec);
    return shots == 3 ? 0 : 1;
}
