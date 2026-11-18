# Phase 163: a night plan for planets and stars

Status: `GATED (bench)` for the planner and the runner; the panel, main.cpp's
wiring and Dash are `WIRED (emu)`. Not yet tried on the sky.

## What the owner asked for

> "for GoTo, the night plan only allows for NEOs. There should be the
> ability to have a night plan which includes planets and stars by working
> them in when they are in the Good and Prime positions."

The Passes tab already plans and runs a night of satellites and NEOs
(phase 118). The Tonight tab lists objects but cannot plan or run anything.
This phase gives the Tonight tab the same two buttons, for planets and stars.

## The windows

The windows are the owner's rule, from `src/core/SkyConditions.h`:

| Window | Altitude |
|---|---|
| PRIME | 60° and up |
| GOOD | 45° to 60° |
| anything lower | never scheduled |

The altitudes come from computed positions over the night, minute by minute.
The mount's live altitude plays no part in the plan. A planet's position is
worked out afresh for every minute. A star's catalogue position is fixed.

## The span planned

From now until the next astronomical dawn (the Sun back above -18°), or
`NightPlanHours` from now if that is sooner (default 0: to dawn).

Daylight is not ruled out by default. Bright planets and stars can be imaged
by day, and the emulator scenario runs in the afternoon. Two rules guard it:

- **Sun limit.** `NightPlanSunMaxDeg` (default 90, which is off). A minute
  with the Sun above this altitude is not used. Set it to -12, say, for
  dark-sky only.
- **Sun distance.** A minute is never used while the Sun is up and the target
  is within `NightPlanSunAvoidDeg` of it (default 30°). This one is safety:
  the plan runs with nobody watching. The recall guard uses 20°; an
  unattended plan gets more room.

## The schedule rules

Each target wants one **visit**: the overhead of getting there, then a stretch
of imaging.

1. **Overhead.** Slew, settle, lock and focus, as one block before the
   imaging. The slew is worked out from where the tube will be (163b):
   - from the visit before it in time, the target where that visit ends;
   - for the first visit, the mount's position when PLAN or CONTINUE is
     pressed;
   - with no mount position, or with a ticked pass in between (the pass
     leaves the tube anywhere), the longest slew, 180°.

   The slew takes the larger of the azimuth change (the short way round)
   and the altitude change, at `SatSlewDegPerSec` (2.4°/s, the same rate
   the satellite plan uses). Then `NightPlanSettleSec` (20) +
   `NightPlanLockSec` (180) + `NightPlanFocusSec` (120). So a visit beside
   the last one has about 5.5 minutes before its first run; one across the
   sky about 6.5. Rounded up to whole minutes. The overhead may fall while
   the target is still climbing to 45°, so the first run can start as the
   window opens. It may not overlap another visit or a pass. Placing a
   visit before another also checks that the next one's slew from it still
   fits.
2. **Imaging.** `NightPlanVisitMin` (20) minutes, all of it with the target
   at 45° or more, every minute of it allowed by the Sun rules. Never past
   the end of the window.
3. **Prefer PRIME.** Of all the places a visit fits, the one with the most
   minutes at 60° or more wins. A tie goes to the earlier start.
4. **Fall back to GOOD.** A target that never reaches 60° in the span, or
   whose PRIME time is taken, is placed in its GOOD time by the same rule.
5. **Shorten before dropping.** When no free stretch is long enough, the
   visit takes the longest free stretch down to `NightPlanMinVisitMin` (5)
   minutes. The plan says it was shortened, and from what.
6. **No double booking.** Visits never overlap each other.
7. **Passes come first.** Satellite and NEO passes ticked on the Passes tab
   are blocked out, from `NightPlanPassMarginSec` (120) before each pass's
   window to the same after it. A visit never touches that time.
8. **Order of placing.** With six targets or fewer, every order of placing
   them is tried, and the plan kept is the one that schedules the most
   targets, then has the most PRIME minutes, then the most imaging minutes.
   With more than six, the targets with the fewest usable minutes are placed
   first. Either way the result is the same every time for the same inputs.
9. **Never scheduled, and why.** Each target left out is listed with the
   reason: it never reaches 45° in the span; the Sun rules leave it no time;
   it is not a planet or a star; or every stretch of its window is taken.

Each visit in the plan says when (slew, first run, end), why (PRIME or GOOD,
and how many minutes of each), and its peak altitude during the visit.

## Running it

A visit, in order:

1. **Wait** until the visit's slew time. It may start early, when the visit
   before finished early, but only if the target is already as high as its
   plan puts it (60° for a PRIME visit, 45° for a GOOD one), and no ticked
   pass lies between now and its end. A PRIME visit never starts early in
   GOOD. It still ends at its planned end, so nothing later moves.
2. **GoTo** by name. That loads the target's profile (3.3.18) and arms the
   search, as any GoTo does. A slew refused (the cord) skips this target,
   not the night.
3. **Settle**, then **lock**: PLANET LOCK's full acquisition, which since
   3.3.20 widens to the full sensor and returns to the profile's ROI.
   `NightPlanLockSec` doubles as the lock's limit, at least 300 s, because
   centring can take up to about 5 minutes in the afternoon. No lock in time
   skips the target. Nothing is recorded on a target that would drift out of
   a planetary ROI.
4. **RUNS**: the same sequence as the RUNS button, with that target's
   profile loaded: focus, then runs, waiting for acquisition to end. It
   starts 3 s after the lock, once frames flow at the profile's ROI: the
   frame has just shrunk from the whole sensor, and until then there is no
   frame rate and so no run length. (Found by the scenario: without the
   wait, RUNS was refused as "no room on the capture disk".)
5. **Move on** when the runs are done or the visit's planned end comes,
   whichever is first. At the planned end the sequence is stopped and the
   file in progress is closed.

The plan stops (not skips) on:

- **STOP**, from the Tonight tab, Dash, or the emergency stop;
- **RECORD or RUNS** pressed by a person during a visit;
- **a slew that never finishes**, which is a fault in the telescope that the
  next target would meet again.

A visit whose slot passes while the mount is busy (a pass, a mosaic, the
Moon map, a person recording) is skipped with that reason.

Every step and skip goes to the session log with its reason, and to
`raduk_log.txt` as `[NIGHTPLAN]` lines.

## Pause and continue (163b)

Asked for by the owner the same evening: "the ability to pause the plan and
then continue if I want to stay longer on a spot or change the plan during
the night, when this happens the downstream must calculate based on sky and
slew availability."

- **PAUSE** stops the plan's clock and nothing else. The runs, the lock and
  the mount are left as they are, and are the operator's. A visit can run
  past its slot for as long as wanted. The ticks can be changed. PLAN is
  refused while paused, because CONTINUE does it.
- **CONTINUE** keeps what is done. The visit it was paused on counts as
  visited if its runs had started; its runs are stopped if still going. A
  visit paused before its runs started is planned again.
- Then every ticked target not yet visited tonight is **planned again from
  now** by the same rules as PLAN: windows from the sky as it is then, the
  slew, lock and focus before each visit, the ticked passes kept clear. A
  target skipped earlier (no lock, say) gets another chance.
- Nothing left, or nothing that fits a window: the plan ends and says why.
- The list keeps what was done above the new plan. STOP works while paused.

Slews are worked out from where the tube is, as in rule 1: CONTINUE takes
the mount's position at that moment for the first slew. 2.4°/s is the
satellite plan's figure, not measured for GoTo on this mount, and the cord
may send the azimuth axis the long way round, which this does not know.

## Where it lives

| Piece | File |
|---|---|
| Planner and runner, pure | `src/core/NightPlan.h` |
| Bench test | `tests/night_plan.cpp` |
| Wiring (hooks: GoTo, lock, RUNS, session log) | `src/app/NightPlanController.h` |
| Tonight tab: ticks, PLAN TONIGHT, RUN TONIGHT, the timed plan | `src/gui/GoToPanel.h` |
| One command path for buttons, Dash and scenarios | `nightPlanCommand` in `src/main.cpp` |
| Dash: NIGHT PLAN section | `RadukCaptureDashboard.html` |
| Playwright spec | `tests/web/specs/night-plan.spec.js` |
| Emulator scenario: plan and run Kochab and Pherkad | `tests/scenarios/night-plan-stars.scn` |

Commands, the same from every route: `TICK <name>`, `UNTICK <name>`,
`CLEAR`, `PLAN`, `RUN` (start, or stop when running), `STOP`.

## The scenario's site

From the real site (about 33° N) Kochab peaks at about 49° and Pherkad at
about 51°, so they are in GOOD for only a couple of hours a day. The scenario
puts the sandbox site at 65° N, where both stay above 45° all day and are in
PRIME about half of it. The emulator's sky follows the rig's site.

PRIME comes and goes over the day there, and the planner will wait hours for
it. So the scenario plans one hour only (`NightPlanHours=1`): both visits fit
in the hour whatever the time, and the steps do not name which star is first.

## The owner's to change

These are defaults, each a `rig.ini` key:

- Daylight is allowed (`NightPlanSunMaxDeg=90`).
- 20 minutes of imaging a visit (`NightPlanVisitMin`).
- A target that will not lock is skipped, not recorded on the mount's
  tracking.

## Not yet tried on the sky

Everything in this phase. The planner's arithmetic is gated on the bench. The
run is checked in the emulator on two stars.
