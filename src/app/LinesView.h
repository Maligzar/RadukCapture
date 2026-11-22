#pragma once
// ============================================================
//  LinesView -- phases 156.2c and 166.1: a window of lines and buttons
//
//  The app's SKY RUN and weather windows say different things the same
//  way: a headline, lines in a tone each, and a row of buttons that are on
//  or off. What each says is decided by a pure builder (SkyRunView.h,
//  WeatherView.h), tested on Linux; gui/LinesWindow.h only paints it.
// ============================================================

#include <string>
#include <vector>

namespace raduk::app {

struct ViewLine {
    enum class Tone { Normal, Dim, Good, Accent, Warn, Bad };
    std::string text;
    Tone tone = Tone::Normal;
    bool heading = false;
};

struct ViewButton {
    std::string id;       // what a click or a scenario press names
    std::string label;    // UTF-8
    bool enabled = false;
    ViewLine::Tone tone = ViewLine::Tone::Accent;
};

struct LinesView {
    std::string headline;
    std::vector<ViewLine> lines;
    std::vector<ViewButton> buttons;

    const ViewButton* Button(const std::string& id) const {
        for (const auto& b : buttons) if (b.id == id) return &b;
        return nullptr;
    }
    bool Has(const std::string& needle) const {
        for (const auto& l : lines) if (l.text.find(needle) != std::string::npos) return true;
        return false;
    }
};

inline bool SameView(const LinesView& a, const LinesView& b) {
    if (a.headline != b.headline || a.lines.size() != b.lines.size() || a.buttons.size() != b.buttons.size()) return false;
    for (size_t i = 0; i < a.lines.size(); i++)
        if (a.lines[i].text != b.lines[i].text || a.lines[i].tone != b.lines[i].tone || a.lines[i].heading != b.lines[i].heading)
            return false;
    for (size_t i = 0; i < a.buttons.size(); i++)
        if (a.buttons[i].id != b.buttons[i].id || a.buttons[i].label != b.buttons[i].label ||
            a.buttons[i].enabled != b.buttons[i].enabled)
            return false;
    return true;
}

} // namespace raduk::app
