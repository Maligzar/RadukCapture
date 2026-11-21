# Phase 166: the app first, Dash second

Written 2026-11-21 at the owner's direction. Status: `PARTIAL`: 166.0, 156.2c and 166.2 `WRITTEN` (3.3.41).

## The owner's rule (2026-11-21)

> Dash is a secondary option designed for iPhone or iPad use and for automated testing. I don't ever plan to use it in place of the application.

What this means for every phase from here on:

1. **Every control and every reading lives in the app first.** Dash mirrors the app; it never has something the app lacks. A feature isn't finished until its native control exists.
2. **Dash is a remote and a test harness.** It shows what the app reports and sends the app's own commands. It computes nothing of its own about the sky, the rig or the night.
3. **Testing keeps both surfaces.** The app's controls are tested through the scenario runner (`ScenarioPress`, phases 125 and 127). Dash's mirror of each control keeps its Playwright spec. The earlier rule, "every control at the telescope is reachable from Dash and tested through it", still holds: being reachable from Dash is a requirement on the remote, not the definition of a control.
4. **A plan that puts a feature's only control in Dash is wrong,** and is corrected before it's built.

## The debt, from an audit of every remote command (2026-11-21)

The app accepts 26 remote commands (the `rc.cmd` chain in `main.cpp`). Each was checked for a native control:

| Remote command | Native control | Debt |
|---|---|---|
| `skyrun_start`, `skyrun_stop` | **none** | **Yes.** The sky run can only be started, watched and stopped from Dash (156.2a). Its design doc says "Dash starts it and shows it", which is the error this phase corrects. |
| `weather_refresh` | **none** | **Yes.** The app shows one weather row in the Status window. The detail Dash shows (cloud, wind, gusts, dew risk, seeing, verdict, score) and the refresh are Dash-only. |
| `park` | PARK in the mount window | **Partly.** The state (slewing, parked, failed and why) is shown only in Dash; the app gives a toast. |
| the other 23 (GoTo, jog, STOP, autofocus, focus calibration, focuser moves and trust, exposure, gain, gamma, RECORD, profiles, centroid and lock, the Moon map, the night plan, cables dressed) | each has one | none |

Content that Dash computes for itself, found before this rule and still on the page:
- the NASA and FAA panels are placeholder lists, not data;
- the wind score carries deductions written for one telescope (the 8SE).

Under rule 2 these go, or come from the app.

## Stages

### 166.0: the rule, enforced (size S) — `WRITTEN` 2026-11-21 (3.3.41)
- **The rule in writing:** `docs/TESTING.md`, the top of `ROADMAP-Next.md`, and the 156 design doc corrected (section 1 and section 5).
- **A parity check on CI.** `tools/app_first_check.py` reads every `rc.cmd == "..."` in `main.cpp` and a table in this file (below), and fails if a remote command has no native control listed. A new remote command then can't land without its native control, or at least without saying where it is.
- **No app change.** The table starts with the three debts marked `owed`. The check allows `owed` rows only while their stage is open, by stage number.

### 156.2c: SKY RUN in the app (size M), ahead of 156.4 — `WRITTEN` 2026-11-21 (3.3.41)
The sky run's own surface, native. A window opened from the action registry (with a hotkey, like the Status window) and from the GoTo panel's Tonight tab:
- the pre-flight list, each line ticked or crossed, with every refusal's reason;
- the plan, with each group's status, and each item's status (156.2b);
- the step now running, the hold reason, and why the run ended;
- START (with the groups to run, ticked; all that apply by default) and STOP (parks). ESC keeps its meaning: stop everything, no park;
- `ScenarioPress` names for START, STOP and each group's tick, plus a scenario that starts a run on the emulator with a synthetic dark window and waits for the park.

Dash's card stays, mirroring the window. It's numbered in 156 because it finishes 156.2a's surface. It comes before 156.4 because the first real sky run should be started from the app.

### 166.1: the weather in the app (size S-M)
- **A weather view in the app,** from `core/SiteWeather.h`, showing what Dash shows today: cloud, temperature, humidity, wind and gusts, dew point and risk, pressure, 7Timer seeing, the verdict and the score. It also says where the reading came from, how old it is, and why there is none (offline, not configured, or failed).
- **↻ in the app.** Dash's refresh becomes the mirror.
- The scenario runner presses ↻; Dash's `weather.spec.js` is unchanged.

### 166.2: park state in the app (size S) — `WRITTEN` 2026-11-21 (3.3.41)
- The mount window shows PARKING, PARKED (tracking off) or PARK FAILED with the reason, beside the PARK button, from `ParkController`.
- A disabled PARK button says why, the same as Dash: no `SkyParkAltAz`.
- A scenario for each state.

### 166.3: Dash computes nothing of its own (size S)
- Remove the placeholder NASA and FAA panels, or replace them with what the app knows, if it knows anything.
- The wind score comes from the app's verdict (`SiteWeather`), not from deductions written for one telescope.
- A Dash spec checks that, with no app connected, the page shows no sky or rig figure it made up.

### A second kind of debt, found while building 166.2
The readout's telemetry block runs **only while a Dash client is connected**. Anything the app computes there, it computes for Dash alone. 156.2c's first wiring put the SKY RUN window's refresh there, so the app's own window would have frozen without Dash. It was moved to the readout tick before any of it shipped. 166.0's check reads commands, so it cannot see this.

**166.4 (size S):** read the telemetry block for anything else the app itself needs, and move it out. Add a scenario that drives the app with no Dash connected and checks that the app's own readouts change.

**166.4's audit, done 2026-11-21:** everything else in the block only formats data for Dash (the disk, the weather fields, the mount, the night plan and Moon map rows, the tracker). `pendingEvents`, which the block clears, is never filled, so nothing grows while Dash is away. Still owed: the scenario with no Dash connected.

## Order
166.0 first: it's small, and it stops new debt. Then 156.2c before 156.4, then 166.1 and 166.2, then 166.3. All of it belongs to release 3.4, whose acceptance includes an unattended sky run, which should be started from the app.

## Parity table (read by 166.0's check)

| command | native | stage |
|---|---|---|
| skyrun_start | SKY RUN window START (Shift+R) | done (156.2c) |
| skyrun_stop | SKY RUN window STOP (Shift+R) | done (156.2c) |
| weather_refresh | owed | 166.1 |
| park | mount window PARK, with its state on the button | done (166.2) |
| autofocus | AUTO FOCUS | done |
| autofocus_abort | AUTO FOCUS (second press) | done |
| calibrate_focus | CALIBRATE FOCUS | done |
| cables_dressed | Cables dressed | done |
| centroid_start | CENTROID / PLANET LOCK | done |
| centroid_lock | PLANET LOCK | done |
| centroid_roi | ROI lock | done |
| centroid_reset | RESET | done |
| focuser_trust | the focuser-trust question at connect | done |
| goto_target | GoTo panel | done |
| jog | mount window D-pad | done |
| mount_stop | STOP | done |
| load_profile | profile list | done |
| moonmap | Moon map | done |
| nightplan | Tonight tab | done |
| move_eaf | focuser panel | done |
| move_eaf_rel | focuser panel | done |
| set_exposure | camera panel | done |
| set_gain | camera panel | done |
| set_gamma | camera panel | done |
| start_capture | RECORD | done |
| stop_capture | RECORD | done |
