// screen_rotation: full-screen windows move one screen along every 15
// minutes, so no panel holds one picture all night (screen burn).
#include "core/ScreenRotation.h"
#include <cstdio>

using namespace raduk::core::rotation;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static Screen Scr(long l, long t, long w, long h, bool primary = false) {
    Screen s; s.full = {l, t, l + w, t + h}; s.work = {l, t, l + w, t + h - 40}; s.primary = primary; return s;
}
static const Move* For(const std::vector<Move>& v, intptr_t id) {
    for (const auto& m : v) if (m.id == id) return &m;
    return nullptr;
}

int main() {
    // The rig: a primary with the main window, and four display screens,
    // given out of order (enumeration order is not left to right).
    const std::vector<Screen> s = {
        Scr(0, 0, 2560, 1440, true),      // 0 primary
        Scr(4480, 0, 1920, 1080),         // 1 second
        Scr(2560, 0, 1920, 1080),         // 2 first
        Scr(8320, 0, 1920, 1080),         // 3 last
        Scr(6400, 0, 1920, 1080),         // 4 third
    };
    // Display order by x: 2 (2560), 1 (4480), 4 (6400), 3 (8320).

    // 1. Four full-screen windows, one each, filled three different ways;
    //    the main window maximised on the primary is excluded.
    std::vector<Win> w(6);
    w[0].id = 100; w[0].rect = {-8, -8, 2568, 1408}; w[0].zoomed = true; w[0].excluded = true;   // main
    w[1].id = 1; w[1].rect = s[2].full;                                    // sky map, whole screen
    w[2].id = 2; w[2].rect = s[1].work;                                    // status, work area
    w[3].id = 3; w[3].rect = {6392, -8, 8328, 1048}; w[3].zoomed = true;   // object panel, maximised
    w[4].id = 4; w[4].rect = {8322, 1, 10238, 1079};                       // weather, within the slack
    w[5].id = 5; w[5].rect = {4600, 100, 5000, 500};                       // a small panel: not full-screen
    auto m = PlanRotation(s, w);
    CHECK(m.size() == 4);
    CHECK(!For(m, 100) && !For(m, 5));
    CHECK(For(m, 1) && For(m, 1)->to == 1 && For(m, 1)->fill == Fill::Full);
    CHECK(For(m, 1)->target.l == 4480 && For(m, 1)->target.b == 1080);     // whole screen at its new home
    CHECK(For(m, 2) && For(m, 2)->to == 4 && For(m, 2)->fill == Fill::Work);
    CHECK(For(m, 2)->target.b == 1040);                                     // work area kept
    CHECK(For(m, 3) && For(m, 3)->to == 3 && For(m, 3)->fill == Fill::Maximized);
    CHECK(For(m, 4) && For(m, 4)->to == 2);                                 // the last goes to the first

    // 2. Four turns bring every window home.
    {
        std::vector<Win> cur = {w[1], w[2], w[3], w[4]};
        const std::vector<Win> start = cur;
        for (int turn = 0; turn < 4; ++turn) {
            auto mv = PlanRotation(s, cur);
            CHECK(mv.size() == 4);
            for (auto& x : cur) {
                const Move* p = For(mv, x.id);
                if (!p) continue;
                if (p->fill == Fill::Maximized) {   // as the app: restore, move, maximise
                    x.rect = {p->target.l - 8, p->target.t - 8, p->target.r + 8, p->target.b + 8};
                } else x.rect = p->target;
            }
        }
        const auto now = PlanRotation(s, cur), then = PlanRotation(s, start);
        for (size_t i = 0; i < cur.size(); ++i) {
            const Move* back = For(now, cur[i].id);
            const Move* orig = For(then, start[i].id);
            CHECK(back && orig && back->from == orig->from);   // each is on its first screen again
        }
    }

    // 3. A screen with no full-screen window is never rotated onto.
    {
        std::vector<Win> two = {w[1], w[2]};       // on screens 2 and 1 only
        auto mv = PlanRotation(s, two);
        CHECK(mv.size() == 2);
        CHECK(For(mv, 1)->to == 1 && For(mv, 2)->to == 2);   // they swap
        for (const auto& x : mv) CHECK(x.to != 3 && x.to != 4 && x.to != 0);
    }

    // 4. One screen in use, or none: nothing moves.
    CHECK(PlanRotation(s, {w[1]}).empty());
    CHECK(PlanRotation(s, {}).empty());
    // Hidden and excluded windows do not count.
    {
        Win hidden = w[2]; hidden.visible = false;
        CHECK(PlanRotation(s, {w[1], hidden}).empty());
    }

    // 5. Two full-screen windows stacked on one screen travel together.
    {
        Win under = w[1]; under.id = 9;
        auto mv = PlanRotation(s, {w[1], under, w[2]});
        CHECK(mv.size() == 3 && For(mv, 9)->to == For(mv, 1)->to);
    }

    // 6. The timer: off at 0; the first period starts at the first tick;
    //    a turn waits while the owner is dragging, then comes.
    {
        RotationTimer t;
        t.Configure(0);
        CHECK(!t.Due(0, false) && !t.Due(100000, false));
        t.Configure(15);
        CHECK(!t.Due(1000, false));
        CHECK(!t.Due(1000 + 899, false));
        CHECK(!t.Due(1000 + 900, true));       // busy: put off
        CHECK(t.Due(1000 + 905, false));       // then it comes
        CHECK(!t.Due(1000 + 906, false));      // and the next period starts from it
        CHECK(t.Due(1000 + 905 + 900, false));
        t.Restart(5000);
        CHECK(!t.Due(5000 + 899, false) && t.Due(5000 + 900, false));
    }

    std::printf("screen_rotation: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
