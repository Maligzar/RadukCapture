#pragma once
// ============================================================
// MoonMapWindow.h — Phase 162
//
// The Moon by quadrant and section, and what a project holds of it.
//
// The disc is drawn as the naked eye has it from the northern
// hemisphere: north up, Mare Crisium's side to the right. Each section
// is one square of it:
//
//   green          captured; the project has a recording of it
//   amber, bright  wanted, and all of it is in sunlight now
//   amber, faint   wanted, with the terminator across it
//   dark           wanted, and in darkness or past the limb tonight
//
// When every section is green the header says READY FOR MOSAIC, in
// green, and the list of files for the stitcher has been written.
// In night vision hue is gone, so the states are also apart in
// brightness, and a captured section carries a dot.
//
// The window draws and reports clicks. Every button and every click on
// a section leaves through one command callback, the same one Dash and
// a scenario use, so there is one implementation of each.
//
// Phase 162c: the map is one long project. It opens on the ACTIVE
// project, says which night of it tonight is and what is ready, and the
// main row is only what a night needs (GOTO, MARK, CLEAR, START). NEW
// PROJECT and the archive arrows sit in the header strip; an archived
// map is only looked at, and NEW on a map with captures asks first.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "WindowChrome.h"
#include "BundledFonts.h"
#include "Theme.h"
#include "../app/MoonMapController.h"
#include "../core/TextConv.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

namespace raduk::gui {

class MoonMapWindow {
public:
    // "GOTO", "MARK", "CLEAR", "START" (which also stops); "NEW", "PREV",
    // "NEXT", "BACK", "CONFIRM", "CANCEL"; and "SELECT" with a section's
    // name as the argument.
    using Command = std::function<void(const std::string& cmd, const std::string& arg)>;

    bool Create(HWND parent, int x, int y, int w, int h) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::NV(Theme::Rgb::BG_VOID));
        wc.lpszClassName = L"RadukMoonMap";
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            0, L"RadukMoonMap", L"Moon map",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX,
            x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);
        if (!m_hwnd) return false;

        BOOL dark = TRUE;
        DwmSetWindowAttribute(m_hwnd, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(m_hwnd, 19, &dark, sizeof(dark));

        if (!InitD2D()) { DestroyWindow(m_hwnd); m_hwnd = nullptr; return false; }
        {
            chrome::Flags f;
            f.minimize = false;
            m_chrome.Attach(m_hwnd, f);
        }
        ShowWindow(m_hwnd, SW_HIDE);    // a job for some nights, not every night
        return true;
    }

    void Destroy() {
        m_chrome.Release();
        ReleaseD2D();
        if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    }

    void SetModel(const raduk::app::MoonMapController* m) { m_model = m; }
    void SetCommand(Command c) { m_command = std::move(c); }

    HWND Handle() const { return m_hwnd; }
    bool IsVisible() const { return m_hwnd && IsWindowVisible(m_hwnd); }
    void Show()   { if (m_hwnd) { ShowWindow(m_hwnd, SW_SHOW); SetForegroundWindow(m_hwnd); } }
    void Hide()   { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }
    void Toggle() { if (!m_hwnd) return; IsVisible() ? Hide() : Show(); }
    void Refresh() { if (IsVisible()) InvalidateRect(m_hwnd, nullptr, FALSE); }

    // A scenario's `press MOONMAP:<label>`, and `MOONMAP:SELECT NE-A1`.
    bool ScenarioPress(const std::string& what) {
        if (what.rfind("SELECT ", 0) == 0) { Send("SELECT", what.substr(7)); return true; }
        for (const char* b : kButtons)
            if (what == b) { Send(b, ""); return true; }
        for (const char* b : kTopButtons)
            if (what == b) { Send(b, ""); return true; }
        if (what == "CONFIRM" || what == "CANCEL") { Send(what, ""); return true; }
        if (what == "STOP") { Send("START", ""); return true; }
        return false;
    }

private:
    static constexpr const char* kButtons[] = {"GOTO", "MARK", "CLEAR", "START"};
    static constexpr int kButtonCount = 4;
    // The header strip: the archive arrows, BACK, and NEW PROJECT.
    static constexpr const char* kTopButtons[] = {"PREV", "NEXT", "BACK", "NEW"};
    static constexpr int kTopCount = 4;

    void Send(const std::string& cmd, const std::string& arg) { if (m_command) m_command(cmd, arg); }

    bool InitD2D() {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_factory)))
            return false;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(&m_dwrite))))
            return false;
        RECT rc; GetClientRect(m_hwnd, &rc);
        m_w = rc.right; m_h = rc.bottom;
        if (FAILED(m_factory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(m_w, m_h)), &m_rt)))
            return false;
        m_rt->CreateSolidColorBrush(Theme::Color(Theme::TEXT), &m_brush);
        MakeFonts();
        return true;
    }

    void MakeFonts() {
        if (!m_dwrite) return;
        auto make = [&](IDWriteTextFormat*& f, float size, DWRITE_TEXT_ALIGNMENT a) {
            if (f) { f->Release(); f = nullptr; }
            m_dwrite->CreateTextFormat(Fonts::FamilyFor(L"Space Mono"), Fonts::CollectionFor(L"Space Mono"),
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                size, L"en-us", &f);
            if (f) {
                f->SetTextAlignment(a);
                f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
        };
        const float base = (std::max)(12.0f, (std::min)(22.0f, (float)(std::min)(m_w, m_h) / 42.0f));
        m_base = base;
        make(m_font,   base,          DWRITE_TEXT_ALIGNMENT_LEADING);
        make(m_fontC,  base,          DWRITE_TEXT_ALIGNMENT_CENTER);
        make(m_fontBig, base * 1.5f,  DWRITE_TEXT_ALIGNMENT_LEADING);
        make(m_fontSmall, base * 0.72f, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    void ReleaseD2D() {
        auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        rel(m_brush); rel(m_font); rel(m_fontC); rel(m_fontBig); rel(m_fontSmall);
        rel(m_rt); rel(m_dwrite); rel(m_factory);
    }

    void OnResize(int w, int h) {
        if (w <= 0 || h <= 0) return;
        m_w = w; m_h = h;
        if (m_rt) m_rt->Resize(D2D1::SizeU(w, h));
        MakeFonts();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    void Text(const std::wstring& s, IDWriteTextFormat* f, const D2D1_RECT_F& r, uint32_t argb, float alpha = 1.0f) {
        if (!f || s.empty()) return;
        m_brush->SetColor(Theme::Color(argb, alpha));
        m_rt->DrawText(s.c_str(), (UINT32)s.size(), f, r, m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void Fill(const D2D1_RECT_F& r, uint32_t argb, float alpha) {
        m_brush->SetColor(Theme::Color(argb, alpha));
        m_rt->FillRectangle(r, m_brush);
    }
    void Frame(const D2D1_RECT_F& r, uint32_t argb, float alpha, float width) {
        m_brush->SetColor(Theme::Color(argb, alpha));
        m_rt->DrawRectangle(r, m_brush, width);
    }
    static std::wstring W(const std::string& s) { return raduk::core::ToWide(s); }

    // Disc units to pixels: east to the right, north up.
    D2D1_RECT_F CellRect(const raduk::sky::MoonSection& s) const {
        return D2D1::RectF(m_cx + (float)s.x0 * m_r, m_cy - (float)s.y1 * m_r,
                           m_cx + (float)s.x1 * m_r, m_cy - (float)s.y0 * m_r);
    }

    void Draw() {
        if (!m_rt || !m_font) return;
        m_rt->BeginDraw();
        m_rt->Clear(Theme::Color(Theme::BG_VOID));

        const float top = (float)m_chrome.CaptionHeight();
        const float pad = m_base * 0.8f;
        const float lineH = m_base * 1.7f;
        const float headH = lineH * 2.6f;
        const float footH = lineH * 2.9f;

        const bool have = m_model && m_model->HasProject();
        const auto* m = m_model;
        const bool archive = have && m->ViewingArchive();
        const raduk::core::MoonMapProject* shownPtr = have ? &m->Shown() : nullptr;

        // ── Header strip: the project, the archive arrows, NEW PROJECT ──
        {
            const float rowTop = top + pad * 0.5f, rowH = lineH * 1.3f;
            const float sbw = m_base * 2.6f, nbw = m_base * 9.0f, bbw = m_base * 5.0f, gap = pad * 0.4f;
            float x = (float)m_w - pad;
            for (auto& r : m_top) r = D2D1::RectF(0, 0, 0, 0);
            const bool many = m && m->ProjectCount() > 1;
            const bool running = have && m->runner.Running();
            auto button = [&](int i, float w, const wchar_t* label, bool live) {
                x -= w;
                m_top[i] = D2D1::RectF(x, rowTop, x + w, rowTop + rowH);
                Fill(m_top[i], i == 3 && m && m->NewArmed() ? Theme::ACCENT4 : Theme::PANEL, 1.0f);
                Frame(m_top[i], Theme::BORDER, 1.0f, 1.0f);
                Text(label, m_fontC, m_top[i], live ? Theme::TEXT : Theme::TEXT_DIM, live ? 1.0f : 0.6f);
                x -= gap;
            };
            button(3, nbw, L"NEW PROJECT", !running && !archive);
            if (many) {
                button(1, sbw, L"\x25B8", !running);
                // Which map of how many, between the arrows.
                const float cw = m_base * 3.4f;
                wchar_t n[24];
                swprintf(n, 24, L"%d/%d", m->ShownIndex() + 1, m->ProjectCount());
                Text(n, m_fontC, D2D1::RectF(x - cw, rowTop, x, rowTop + rowH), Theme::TEXT_DIM);
                x -= cw + gap;
                button(0, sbw, L"\x25C2", !running);
            }
            if (archive) button(2, bbw, L"BACK", true);

            const std::wstring title = have ? W(shownPtr->Name()) : L"No Moon map project";
            Text(title, m_fontBig, D2D1::RectF(pad, rowTop, x - gap, rowTop + rowH),
                 archive ? Theme::ACCENT2 : Theme::TEXT);

            auto nights = [](int n) { return n == 1 ? L"1 night" : (std::to_wstring(n) + L" nights"); };
            std::wstring line;
            uint32_t col = Theme::TEXT_DIM;
            wchar_t b[260];
            if (!have) {
                line = L"Press NEW PROJECT to start your first map.";
            } else if (archive) {
                swprintf(b, 260, L"ARCHIVE   %d of %d sections, %ls.   BACK to return",
                         shownPtr->Captured(), shownPtr->Total(), nights(shownPtr->Nights()).c_str());
                line = b;
                col = Theme::ACCENT2;
            } else if (m->Active().Complete()) {
                swprintf(b, 260, L"READY FOR MOSAIC in %ls.   NEW PROJECT starts another map",
                         nights(m->Active().Nights()).c_str());
                line = b;
                col = Theme::ACCENT3;
            } else {
                const int ready = m->CapturableNow();
                swprintf(b, 260, L"Night %d   %d of %d captured   ", m->NightNumber(),
                         m->Active().Captured(), m->Active().Total());
                line = b;
                if (ready > 0) {
                    swprintf(b, 260, L"%d ready now \x2192 START", ready);
                    line += b;
                    col = Theme::TEXT;
                } else {
                    line += L"nothing in sunlight now";
                }
            }
            if (have && m->runner.Running()) {
                swprintf(b, 260, L"   RUN: %hs, %hs (%d of %d)", m->CurrentSection().c_str(),
                         raduk::app::MoonMapController::Runner::PhaseName(m->runner.State()),
                         m->runner.Index() + 1, m->runner.Total());
                line += b;
                col = Theme::ACCENT;
            }
            Text(line, m_font, D2D1::RectF(pad, rowTop + rowH, (float)m_w - pad, top + headH), col);
        }

        // ── The disc ─────────────────────────────────────────
        const float availW = (float)m_w - pad * 2.0f;
        const float availH = (float)m_h - top - headH - footH - pad * 2.0f;
        m_r  = (std::max)(20.0f, (std::min)(availW, availH) * 0.5f);
        m_cx = (float)m_w * 0.5f;
        m_cy = top + headH + pad + availH * 0.5f;

        if (have) {
            const auto& P = *shownPtr;
            const auto& secs = P.Sections();
            const bool sky = m->HaveSky() && !archive;
            const std::string running = m->CurrentSection();

            ID2D1EllipseGeometry* disc = nullptr;
            m_factory->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(m_cx, m_cy), m_r, m_r), &disc);
            ID2D1Layer* layer = nullptr;
            m_rt->CreateLayer(&layer);
            if (disc && layer) {
                m_rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), disc), layer);
                Fill(D2D1::RectF(m_cx - m_r, m_cy - m_r, m_cx + m_r, m_cy + m_r), Theme::PANEL, 1.0f);
                for (size_t i = 0; i < secs.size(); ++i) {
                    const D2D1_RECT_F r = CellRect(secs[i]);
                    const bool done = P.IsCaptured(secs[i].id);
                    if (done) {
                        Fill(r, Theme::ACCENT3, 0.78f);
                    } else if (sky && m->Sky((int)i).Capturable()) {
                        Fill(r, Theme::ACCENT2, 0.55f);
                    } else if (sky && m->Sky((int)i).visible && m->Sky((int)i).litFraction > 0.0) {
                        Fill(r, Theme::ACCENT2, 0.16f);
                    }
                    Frame(r, Theme::BORDER, 1.0f, 1.0f);
                }
                m_rt->PopLayer();
            }

            // Names and the captured dot, over the fills.
            const float cell = m_r / (float)P.PerSide();
            for (size_t i = 0; i < secs.size(); ++i) {
                // A limb sliver's aim point is at the limb; its label sits
                // a little inside, so it does not hang off the disc.
                const double ar = std::sqrt(secs[i].aimX * secs[i].aimX + secs[i].aimY * secs[i].aimY);
                const float pull = ar > 0.93 ? (float)(0.93 / ar) : 1.0f;
                const float ax = m_cx + (float)secs[i].aimX * pull * m_r;
                const float ay = m_cy - (float)secs[i].aimY * pull * m_r;
                const bool done = P.IsCaptured(secs[i].id);
                if (done) {
                    m_brush->SetColor(Theme::Color(Theme::BG_VOID, 0.85f));
                    m_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ax, ay - cell * 0.22f), cell * 0.07f, cell * 0.07f), m_brush);
                }
                if (cell >= m_base * 2.6f)
                    Text(W(secs[i].id.substr(3)), m_fontSmall,
                         D2D1::RectF(ax - cell * 0.5f, ay - cell * 0.1f, ax + cell * 0.5f, ay + cell * 0.4f),
                         done ? Theme::BG_VOID : Theme::TEXT_DIM, done ? 0.9f : 1.0f);
            }

            // The quadrants: the equator and the central meridian.
            m_brush->SetColor(Theme::Color(Theme::TEXT, 0.8f));
            m_rt->DrawLine(D2D1::Point2F(m_cx - m_r, m_cy), D2D1::Point2F(m_cx + m_r, m_cy), m_brush, 2.0f);
            m_rt->DrawLine(D2D1::Point2F(m_cx, m_cy - m_r), D2D1::Point2F(m_cx, m_cy + m_r), m_brush, 2.0f);
            m_brush->SetColor(Theme::Color(Theme::TEXT_DIM));
            m_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(m_cx, m_cy), m_r, m_r), m_brush, 1.5f);

            // The section in hand, and the one picked.
            for (size_t i = 0; i < secs.size(); ++i) {
                if (secs[i].id == running)        Frame(CellRect(secs[i]), Theme::ACCENT, 1.0f, 3.0f);
                else if (secs[i].id == m->Selected()) Frame(CellRect(secs[i]), Theme::TEXT, 1.0f, 2.5f);
            }

            // A count in each corner, green when that quadrant is done.
            using Q = raduk::sky::MoonQuadrant;
            const struct { Q q; float sx, sy; } corners[] = {
                {Q::NE, 1, -1}, {Q::NW, -1, -1}, {Q::SW, -1, 1}, {Q::SE, 1, 1}};
            for (const auto& c : corners) {
                wchar_t b[48];
                swprintf(b, 48, L"%hs %d/%d", raduk::sky::MoonQuadrantName(c.q),
                         P.QuadrantCaptured(c.q), P.QuadrantTotal(c.q));
                const float x = m_cx + c.sx * m_r * 0.86f, y = m_cy + c.sy * m_r * 0.93f;
                Text(b, m_fontC, D2D1::RectF(x - m_base * 5, y - lineH * 0.5f, x + m_base * 5, y + lineH * 0.5f),
                     P.QuadrantComplete(c.q) ? Theme::ACCENT3 : Theme::TEXT);
            }
            if (disc)  disc->Release();
            if (layer) layer->Release();
        }

        // ── The picked section, and the buttons ──────────────
        const float footTop = (float)m_h - footH;
        for (auto& r : m_btn)  r = D2D1::RectF(0, 0, 0, 0);
        for (auto& r : m_conf) r = D2D1::RectF(0, 0, 0, 0);

        if (have && m->NewArmed()) {
            // NEW on a map with captures: say what happens, and ask.
            wchar_t b[260];
            swprintf(b, 260, L"New map? '%hs' (%d of %d) goes to the archive.",
                     m->Active().Name().c_str(), m->Active().Captured(), m->Active().Total());
            Text(b, m_font, D2D1::RectF(pad, footTop, (float)m_w - pad, footTop + lineH), Theme::ACCENT4);
            const float cw = ((float)m_w - pad * 2.5f) / 2.0f;
            const wchar_t* labels[2] = {L"START NEW MAP", L"KEEP GOING"};
            for (int i = 0; i < 2; ++i) {
                const float x = pad + i * (cw + pad * 0.5f);
                m_conf[i] = D2D1::RectF(x, footTop + lineH * 1.25f, x + cw, footTop + lineH * 1.25f + lineH * 1.3f);
                Fill(m_conf[i], i == 0 ? Theme::ACCENT4 : Theme::PANEL, 1.0f);
                Frame(m_conf[i], Theme::BORDER, 1.0f, 1.0f);
                Text(labels[i], m_fontC, m_conf[i], Theme::TEXT);
            }
        } else {
            const auto* P = shownPtr;
            if (have && !m->Selected().empty()) {
                const auto* s = P->Find(m->Selected());
                int idx = -1;
                for (size_t i = 0; i < P->Sections().size(); ++i)
                    if (P->Sections()[i].id == m->Selected()) idx = (int)i;
                if (s && idx >= 0) {
                    wchar_t b[260];
                    const int n = P->CaptureCount(s->id);
                    const wchar_t* light = L"";
                    wchar_t sun[64] = L"";
                    if (m->HaveSky() && !archive) {
                        const auto& k = m->Sky(idx);
                        light = !k.visible ? L"past the limb tonight"
                              : k.Capturable() ? L"in sunlight"
                              : k.litFraction > 0.0 ? L"on the terminator" : L"in darkness";
                        swprintf(sun, 64, L", Sun %+.0f deg", k.sunAltDeg);
                    }
                    swprintf(b, 260, L"%hs   lat %+.1f  lon %+.1f   %ls%ls   %ls",
                             s->id.c_str(), s->latDeg, s->lonDeg, light, sun,
                             n == 0 ? L"not captured" : n == 1 ? L"captured" : L"captured more than once");
                    Text(b, m_font, D2D1::RectF(pad, footTop, (float)m_w - pad, footTop + lineH), Theme::TEXT);
                }
            } else if (have) {
                Text(archive ? L"An archived map: click a section to see it." : L"Click a section to pick it.",
                     m_font, D2D1::RectF(pad, footTop, (float)m_w - pad, footTop + lineH), Theme::TEXT_DIM);
            }

            const float bw = ((float)m_w - pad * 2.0f - pad * 0.5f * (kButtonCount - 1)) / kButtonCount;
            const bool running = have && m->runner.Running();
            for (int i = 0; i < kButtonCount; ++i) {
                const float x = pad + i * (bw + pad * 0.5f);
                m_btn[i] = D2D1::RectF(x, footTop + lineH * 1.25f, x + bw, footTop + lineH * 1.25f + lineH * 1.3f);
                const bool start = i == kButtonCount - 1;
                const bool live = have && !archive && (start || !m->Selected().empty());
                Fill(m_btn[i], start && running ? Theme::ACCENT4 : Theme::PANEL, 1.0f);
                Frame(m_btn[i], Theme::BORDER, 1.0f, 1.0f);
                const std::wstring label = start ? (running ? L"STOP" : L"START") : W(kButtons[i]);
                Text(label, m_fontC, m_btn[i], live ? Theme::TEXT : Theme::TEXT_DIM, live ? 1.0f : 0.6f);
            }
        }

        m_chrome.Draw(m_rt, m_dwrite, m_w);
        if (m_rt->EndDraw() == D2DERR_RECREATE_TARGET) {
            ReleaseD2D();
            InitD2D();
        }
    }

    static bool Inside(const D2D1_RECT_F& r, int x, int y) {
        return r.right > r.left && x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

    void OnClick(int x, int y) {
        for (int i = 0; i < 2; ++i)
            if (Inside(m_conf[i], x, y)) { Send(i == 0 ? "CONFIRM" : "CANCEL", ""); return; }
        for (int i = 0; i < kTopCount; ++i)
            if (Inside(m_top[i], x, y)) { Send(kTopButtons[i], ""); return; }
        for (int i = 0; i < kButtonCount; ++i)
            if (Inside(m_btn[i], x, y)) { Send(kButtons[i], ""); return; }
        if (!m_model || !m_model->HasProject() || m_r <= 0.0f) return;
        const double dx = (x - m_cx) / m_r, dy = (m_cy - y) / m_r;
        if (dx * dx + dy * dy > 1.0) return;
        for (const auto& s : m_model->Shown().Sections())
            if (dx >= s.x0 && dx < s.x1 && dy >= s.y0 && dy < s.y1) {
                Send("SELECT", s.id);
                return;
            }
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MoonMapWindow* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<MoonMapWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        } else {
            self = reinterpret_cast<MoonMapWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

        LRESULT chromeResult = 0;
        if (self->m_chrome.Filter(hwnd, msg, wp, lp, chromeResult))
            return chromeResult;

        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                self->Draw();
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_SIZE:
                self->OnResize(LOWORD(lp), HIWORD(lp));
                return 0;
            case WM_LBUTTONDOWN:
                self->OnClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                return 0;
            case WM_KEYDOWN:
                if (wp == VK_ESCAPE) {
                    // Esc first takes back an armed NEW, then closes.
                    if (self->m_model && self->m_model->NewArmed()) self->Send("CANCEL", "");
                    else self->Hide();
                    return 0;
                }
                break;
            case WM_ERASEBKGND:
                return 1;
            case WM_CLOSE:
                self->Hide();
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND                    m_hwnd    = nullptr;
    ID2D1Factory*           m_factory = nullptr;
    IDWriteFactory*         m_dwrite  = nullptr;
    WindowChrome            m_chrome;
    ID2D1HwndRenderTarget*  m_rt      = nullptr;
    ID2D1SolidColorBrush*   m_brush   = nullptr;
    IDWriteTextFormat*      m_font = nullptr, *m_fontC = nullptr, *m_fontBig = nullptr, *m_fontSmall = nullptr;
    int                     m_w = 900, m_h = 900;
    float                   m_base = 14.0f;
    float                   m_cx = 0, m_cy = 0, m_r = 0;
    D2D1_RECT_F             m_btn[kButtonCount] = {};
    D2D1_RECT_F             m_top[kTopCount] = {};
    D2D1_RECT_F             m_conf[2] = {};
    const raduk::app::MoonMapController* m_model = nullptr;
    Command                 m_command;
};

} // namespace raduk::gui
