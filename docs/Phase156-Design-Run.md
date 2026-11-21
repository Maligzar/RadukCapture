# Phase 156: how the unattended run carries out its rules (design, for review)

Written 2026-11-21 at the owner's request, before any of it is built. The rules are done: `core/SkyRun.h` (PR #186) decides pre-flight, which groups apply, the order, and what to do on a result, a lost target, clouds, a stall or the end of the dark window. This document is about the part that **acts**: who carries out each step on the night, and what each group actually does, minute by minute.

The owner answered the open questions on 2026-11-21 (below), and A is being built.

## The owner's answers (2026-11-21)

1. **The dark window:** the Sun below **-10°**, as proposed. It is a setting, `SkyDarkSunAltDeg` (default -10).
2. **Unattended focuser moves:** **yes**, the run may defocus by ±700 steps on its own for item 47, inside the travel guard.
3. **The exposure ladder's gain:** **yes**, the run sets gain itself to bring the highlight near 70%.
4. **The star-field group's star:** **from a list the owner gives**, `SkyStars=Vega,Arcturus,...` in his order of preference. With no list, the group is not available tonight, and says so.
5. **How much of a night:** **the whole dark window** may be used.
6. **The handoff:** **yes**. Claude Code analyses the bundle and opens a draft PR, and never merges.
7. **The `collected` status:** **yes**. It is in `SkyRun.h` and the checklist updater, where it leads a pass on the same item, since it asks something of the owner.

The owner did not choose between A and B outright. His answers fit A, the recommendation, so A is what is built: the app runs the night, and Dash starts it and shows it.


## 1. The question: where the run lives

| | **A. The app runs it; Dash starts and shows it** | **B. Playwright runs it through Dash** |
|---|---|---|
| The rules | Once, in C++ (`SkyRun.h`), where they are tested today | Twice: C++ and a JavaScript copy, kept in step by a shared case file |
| If the browser or Node dies at 2 a.m. | The run carries on; Dash reconnects and shows where it is | The run stops where it was, possibly mid-slew or mid-sweep |
| What it exercises | The app's own code for each step (GoTo, lock, autofocus, record) | Every step through Dash's buttons, so Dash itself is tested all night |
| On your phone | A SKY RUN card: the plan, what is running, why it is holding, STOP | Dash as usual; the run's state lives in the Node process, not the app |
| Where the code is | Mostly Windows-only (`main.cpp` and the controllers); the decisions stay pure | Mostly JavaScript; some new Dash controls (box change, defocus offset) |
| How it is tested | Pure tests on CI; each group as an emulator scenario on StarMachine | Playwright against the emulator on StarMachine; the rules' JS copy on CI |
| Your rule: every control at the scope is in Dash, tested through it | Met by giving each step its Dash control and spec, apart from the run | Met by the run itself |

**Recommendation: A, with Playwright as the starter and the witness.** (Confidence: medium-high.)
- **The deciding reason** is the 2 a.m. failure. An unattended run that stops when a browser tab crashes is not unattended. The run's state belongs in the process that holds the hardware.
- **Rules kept once.** A second copy of the rules in JavaScript is a second place for them to drift apart, and nothing else would test that copy.
- **Your Dash rule is still met, as a separate check.** Each step a group uses (GoTo, CENTROID, AUTO FOCUS, record, the box change, a jog) gets its Dash control and a Playwright spec. So Dash stays tested as a remote control, and the night does not depend on it.
- **Playwright's part** (`specs/skyrun.spec.js`, `@live`): it presses SKY RUN through Dash, watches the card, takes a screenshot every 15 minutes, and checks the bundle once the app parks. It is the witness of the night, not the hands.
- **What would change my mind:** if you want the night itself to be the test of Dash, B is the honest choice, and the cost is the JavaScript copy of the rules.

## 2. A finding: most of the checklist cannot be "passed" by a machine

Reading the items the groups settle, about half need a person's judgment, not a number:

| Item | What decides it | What the run can honestly say |
|---|---|---|
| 13 throughput | frames per second against expected; 0 unexplained drops | **passed** or **failed** |
| 3, 15 plate scale and row order | a solve, and its scale against the configured scale | **passed** (solved, scale recorded) or **failed** (no solve); whether the scale changes is the validation report's call over several nights |
| 48, 49, 50 lock, jog, small box and edge | lock held; re-centred within 25 s; calibration stayed in the box; the edge refused with its message | **passed** or **failed** |
| 53 pointing | the GoTo's error before the sync, in arcminutes | **collected**: the number; what counts as good is yours to set |
| 47 autofocus | each sweep completes; repeatability across two sweeps | **passed** or **failed** on completion; **collected** for repeatability |
| 1 sharpness distribution | many nights of real frames (the validation report) | **collected** |
| 55 exposure ladder | four stacks compared side by side in AutoStakkert | **collected**: the four recordings; the comparison is yours |
| 57 Autorun round a Jupiter event | the note beside each run against the stacked image | **collected** |
| 58 the fine pass | which of three positions looks sharpest by eye | **collected** |
| 18 the lunar mosaic | whether it stitches | **collected** |

- **The proposal:** add a status, `collected`, meaning the evidence is gathered and your eye is owed. The checklist line then reads `**Sky run 20261125: COLLECTED, your eye owed** (planet on Saturn: 4 ladder recordings in captures\...)`.
- **The rule this follows:** a status says no more than the run knows. `passed` is used only where a machine-checkable criterion exists. Without this, an item judged by eye would be marked passed because the recording happened.

## 3. The groups, step by step

Each step uses code the app already has. Each group starts with a GoTo, checked against `WithinLimits` at the moment of the slew, never only when planned.

### Any planet (about 25 minutes): items 13, 47, 1, 55, 58
1. **GoTo** the planet. The slew is limited to 180 s (SlewWatch, 157.0b); past that is a stall.
2. **PLANET LOCK.** The wide frame, calibration, then back to the ROI (157.0c). If the lock is not made within 5 minutes, the group fails with "lock not made".
3. **Exposure settles** (AE), then **autofocus twice**, starting from +700 and from -700 steps of the current position, inside the focuser's travel guard. Each completes or fails. The two results' difference is item 47's repeatability, and each fine pass's figures go to the validation log (155.4b).
4. **Throughput (item 13):** a 60-second recording at the profile's ROI and full rate. It passes on 0 unexplained frames in the drop ledger and the frame rate reached.
5. **The exposure ladder (item 55):** 90 seconds at each of 2, 5, 10 and 15 ms, with gain set so the target's highlight sits near 70% of full scale. The status is `collected`. **Open question 3** applies here.
6. **Sharpness (item 1)** is gathered from every frame along the way by the validation log, so it costs no extra step.

### A star field (about 10 minutes): items 3, 15, 53
1. **GoTo** the highest bright star inside the limits, from the app's catalog, with at least 20 stars around it in the frame (**open question 4**).
2. **Before syncing,** record the GoTo's landing error from a plate solve (item 53, `collected`).
3. **Solve and sync.** It passes if the solve succeeds, and records the measured scale against the configured one (item 3) and the row order (item 15).

### Tracking (about 15 minutes, on any bright target): items 48, 49, 50
1. **Lock.** Then **box change**: 3840, 1280, 640, 3840. It passes if the lock is held throughout.
2. **Jog** about 200 px, through the same path as the paddle. It passes if the target re-centres within 25 s and the lock is not dropped.
3. **Calibrate in a 640x480 box.** It passes if every move stays inside the box. Then offset the target to within 20% of an edge and press again. It passes on the refusal "centre it roughly, then press again".

### Jupiter (about 40 minutes, only round an event): item 57
- **It runs only** if a Red Spot transit or a shadow transit (the `J` list) begins within the next hour. Otherwise it is not available tonight, and the reason says when the next event is.
- **It runs** `[RUNS]` from an hour before the event. It checks the log for `[AUTORUN] focus put off` inside the spell and for a sweep after it ends. Both found: `collected` (the note against the stack is your call). Either missing: `failed`.

### The Moon (about 30 minutes): items 58 (Moon half), 18
- **GoTo** the Moon, then the **fine-pass-only autofocus** (it fills the frame). Its result goes to the validation log.
- **Item 18:** the Moon map's next sections (Phase 162), as many as fit in 20 minutes. `collected`.

## 4. The night, around the groups

- **The dark window.** The Sun below -10° (**open question 1**), from the app's own ephemeris (`Ephemeris.h` already computes the Sun's altitude). The run waits for it to begin, and parks when it ends.
- **Clouds, from what the camera measures.** The target's flux falls past FadeWatch's threshold, or the target is not seen for 60 s while locked. The forecast (`SiteWeather`, 165) is shown, never acted on alone: a forecast of cloud does not stop a run under a clear sky.
- **Stalls, from the watchdogs the app has:**
  - the camera: a wedge restart that does not recover;
  - the focuser: a failed or refused move, or a position in doubt (157.0e);
  - the mount: a GoTo past SlewWatch's 180 s, or the mount not answering.
- **Park, and only by the app's existing path.** At the end, on a stall, on the dark window closing, or on STOP from Dash or the app.
- **Safety, never widened:**
  - every slew is checked against `SkyHorizon`, `SkySlewAltMax` and `SkySlewAz` at the moment it is sent;
  - focuser offsets stay inside the travel guard (161.0a);
  - nothing in the run can change a limit.

## 5. What Dash gets

- **A SKY RUN card.** It shows the pre-flight list (each line ticked or crossed), the night's plan with each group's status, the step now running, the reason for any hold, and a STOP that parks.
- **The steps' own controls.** Any step without a Dash control yet gets one, with a spec: the box change, a focuser offset, and the run's STOP.

## 6. Build order, if A is chosen

Each is one PR, tested on CI (the pure parts) and in the emulator on StarMachine (the scenarios):
1. **The run's skeleton in the app.** Pre-flight from the live rig, the dark window, the SKY RUN card in Dash, START and STOP, and the park. The tracking group first, because every one of its checks is machine-judged and the emulator can run it.
2. **The planet group,** with the `collected` status.
3. **The star-field group.**
4. **The Jupiter and Moon groups.**
5. **The bundle in `field-results/<night>/`,** and 156.4's handoff to Claude Code.

## 7. Open questions for the owner

1. **The dark window:** the Sun below -10°, -12° (nautical) or -18° (astronomical)? For planets, -10° gives the most time; for the star-field group, -12° is safer.
2. **Unattended focuser moves:** may the run defocus by ±700 steps on its own for item 47? Or only from wherever focus already is? (The 8000-step test in item 47 stays a hand test either way.)
3. **The exposure ladder's gain:** may the run set gain itself to reach about 70%? Or should it record each exposure at the profile's gain and leave matching the brightness to you?
4. **The star-field group's star:** pick from the app's catalog automatically (the highest bright star with a field around it)? Or from a short list you choose?
5. **How much of a night:** may the run use the whole dark window? Or should it stop after a set time, for example 2 hours, so a test night is still an imaging night?
6. **The handoff:** the plan's default is that Claude Code analyses the bundle and opens a draft PR, and never merges. Confirm, or keep it to the bundle and the checklist only.
7. **The `collected` status** (section 2): agree, or keep `passed` and `failed` only?
