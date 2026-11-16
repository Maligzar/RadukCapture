# Testing RadukCapture

How the app is tested, from the fastest check to the sky. Every change runs the first layer on CI; the others run on the imaging PC.

## The workflow: tests run themselves, Claude reads the results
The owner set this on 2026-11-15.

**Tests run on their own:** Playwright, the bench tests and the scenarios. Every one of them is deterministic, scripted and free to repeat, so none needs Claude to watch it or click through it.

**Claude's tokens are kept for the results:**
- reading what failed;
- finding why;
- fixing it;
- deciding what to test next.

In practice:
- **Run the suites without Claude.** On CI, on the imaging PC by hand, or from a scheduled task. A full run produces files, not a conversation.
- **Each suite writes a compact result that Claude reads, not a log to wade through:**
  - Playwright: `tests\web\results\dash-results.json`;
  - the bench tests: the "its last lines" blocks after `run-tests.ps1`'s summary;
  - each scenario: its `report.txt`;
  - the sky: Phase 156's `results.json`.
- **Claude starts from the failures.** A green run needs no review. A red one comes with its failing test's last lines, so the cause can usually be read without re-running anything.
- **Claude does not drive tests interactively** when a script can. A check that keeps being done by hand is a spec waiting to be written. Write it once, then let it run.
- **The same goes for the sky.** Phase 156's unattended run uses these same specs through Dash, and Claude Code only reviews the bundle it leaves.

| Layer | What it drives | Where it runs | How |
|---|---|---|---|
| 1. Bench tests | the code directly: pure headers, the controllers, the headless core on the emulators | CI on every push; any Windows PC | `.\tests\run-tests.ps1` |
| 2. Dash tests (Playwright) | **Dash**, the browser page that controls the app, and through it the running app | the imaging PC, or any PC with the app running | `cd tests\web; npm test` |
| 3. Scenarios | the app's own window, through the scenario runner | the imaging PC (not CI: the runner cannot drive the window) | `.\tests\run-scenarios.ps1 [name]` |
| 4. The sky | the real camera, focuser and mount | outside, on a clear night | `docs/CLEAR-NIGHT-CHECKLIST.md`; Phase 156 automates it |

## 1. Bench tests
- **What they are.** `tests\run-tests.ps1` builds each `tests\*.cpp` with `cl` and runs it. A test passes when it returns 0, and a failure repeats its last lines after the summary.
- **What they cover:**
  - the pure headers, from arithmetic to formats;
  - the controllers in `src/app` (autofocus, recording, Autorun);
  - `app_headless`: the application core on the emulators, by group (`connect`, `focus`, `record`);
  - `fault_injection`: the app's answer to an emulator told to fail (`RADUK_EMU_FAULT`). Its groups are `eaf-stall`, `camera-gone`, `camera-blip` and `slew-stuck`.
- **Adding a test.** Write it as `tests\<name>.cpp` and add a line to the `$Tests` list in `run-tests.ps1`. Pure headers are also built with g++ and the sanitizers, off Windows.

### Independent file checks (157.2)
`tests\run-file-checks.ps1`, on CI after the bench tests. `tests\write_recordings.cpp` writes SER and FITS files through the app's own writers: mono and RGGB, 8 and 16 bits, odd sizes, pixels from a formula, stamps on a fixed interval, and one SER from a process ended mid-run. `tools\check_recordings.py --manifest` reads them with no code from `src`: a SER reader written from the SER v3 specification, and astropy for FITS. It checks every pixel, the header, the frame count against the file size, the trailer or the FRAMES table, and the FITS time cards. Needs `pip install astropy` (CI's network, never the app's). Results: `tests\_build\file_checks\results.json`.

`check_recordings.py --scan <folder>` checks any recordings without a manifest (structure and stamps, no pixels). The soak uses it; it works on a night's captures too.

### The soak (157.1)
`tests\run-soak.ps1 [-Hours 4]`, on the imaging PC, by hand or nightly: too long for CI, and it drives the app's window. It generates a scenario of cycles (Kochab and Pherkad in turn: GoTo, LOCK, RUNS with autofocus and two runs; a lock refused is counted with its reason, not failed) and runs it in its own sandbox until the deadline, then closes the app. It samples handles, private bytes and threads every minute and fails on steady growth, on any recording the independent reader rejects (a stamp not after the one before), on any `unexplained` frames in a run's drop ledger, and on the cycle script failing early. Report: `reports\soak_<stamp>.txt`, samples beside it.

## 2. Dash tests: Playwright
**Dash** (`RadukCaptureDashboard.html`) controls RadukCapture from a browser, over its WebSocket on port 8765. It is what you use outside, from a phone or a tablet at the scope. Playwright drives Dash the way a person does: it clicks Dash's own controls and reads what Dash shows back. So one test checks two things:
- the browser control works;
- the app did what Dash asked.

**What Playwright cannot do:** drive the app's own window. That is native Win32 and Direct2D, not a web page. For the window, use the scenarios (layer 3).

### The rule for new controls (the owner, 2026-11-15)
Every control a person needs at the telescope is reachable from Dash. A new feature is not finished until:
- Dash has its control and shows its state;
- the control has a stable `id`;
- a Playwright test drives it.

Today Dash sends `start_capture`, `stop_capture`, `set_gain`, `set_exposure`, `set_gamma`, `autofocus`, `autofocus_abort`, `calibrate_focus`, `move_eaf`, `move_eaf_rel`, `centroid_start`, `centroid_lock`, `centroid_roi`, `centroid_reset`, `cables_dressed`, `mount_stop`, `jog`, `load_profile` and `goto_target` (main.cpp's WebSocket command handler). Controls with no Dash equivalent yet are listed in "Owed" below, as they are found.

### Setting it up on the imaging PC
Node and Playwright are installed on StarMachine. Once, in the repository:
```
cd tests\web
npm install
npx playwright install chromium
```
Nothing here needs the internet once installed (rule 7): the browser and Dash are both on this PC, and the specs refuse any request that tries to leave it.

### Running it
1. **Offline checks, with no app running:**
   ```
   npm run test:offline
   ```
   This loads Dash with every outside request refused, and fails if Dash tries one.
2. **Live checks, against the running app.** Start the emulator build first (`launch_emulator.bat`), or the real rig, then:
   ```
   npm run test:live
   ```
   - The specs open Dash exactly as `[DASH]` does: `file:///…/RadukCaptureDashboard.html#token=<WsToken>`.
   - The token comes from `RADUK_WS_TOKEN`, or else `build_emu\Release\rig.ini`. Point `RADUK_RIG_INI` at the rig's own `rig.ini` for the hardware build.
3. **Everything:**
   ```
   npm test
   ```

Each test runs twice:
- `desktop`: the capture PC's own browser;
- `phone`: a Pixel-sized screen, because that is how Dash is used outside.

Results go to `tests\web\results\dash-results.json`. Phase 156's bundle picks them up from there.

### The specs today
| Spec | Needs | Checks |
|---|---|---|
| `offline.spec.js` | nothing running | Dash loads with no internet, no script errors, and no request leaves the PC |
| `live.spec.js` (`@live`) | the app running | Dash connects with the token, live telemetry arrives, and START/STOP CAPTURE works from the browser |

### Found by it already
**Dash fetches a weather forecast from `api.open-meteo.com`.** It does this even with no site set, asking for latitude 0, longitude 0. That breaks rule 7, airplane mode. The Phase 151 audit missed it, because it checked Dash's files and not its requests. `offline.spec.js` fails on it until it is fixed: owed as 151.4, gate the fetch on `Offline` and a known site, or take the weather from Leoben.

### Writing a new spec
1. **Use Dash's own controls by `id`.** Examples: `#rcRecBtn`, `#rcHeaderStatus`, `#rcFPS`.
2. **Wait on what Dash shows,** not on fixed sleeps. Use `toHaveText`, with a timeout sized to the action: a GoTo takes longer than a gain change.
3. **Call `blockInternet(page)`** from `specs/dash.js` in every spec, so a request leaving the PC is seen.
4. **Tag specs that need the app running** with `@live` in the describe name.
5. **Leave the rig as you found it.** Stop what you start, and never leave a GoTo or a capture running.
6. **Use the emulator for anything that moves the mount** unless the test is meant for the rig and someone is there.

### Why it matters outside
At the scope, Dash on a phone is the control panel. If a control works in Playwright on both the desktop and the phone layout, it works from the phone at the scope.

These tests are also how Phase 156's unattended sky run and Claude Code operate the app: through the same controls a person uses, not through a private back door.

## 3. Scenarios
`tests\run-scenarios.ps1` runs the emulator build through scripts in `tests\scenarios\*.scn`. Each run is sandboxed, and writes a `report.txt` and screenshots. Scenarios are the before-and-after check for any change to `main.cpp`'s wiring (phase 153).

GitHub's runner cannot drive the window: the exe stalled in early start-up there, twice. So scenarios run on the imaging PC.

## 4. The sky
`docs/CLEAR-NIGHT-CHECKLIST.md` lists what only a real night can settle. `docs/UNVERIFIED.md` lists every constant still waiting on one.

Phase 156 runs these as test groups, chosen for what is being observed. Its live-sky groups drive the app through Dash, so the same Playwright specs grow into the sky tests. The results mark the checklist, and the night is handed to Claude Code.

## Owed
- **151.4:** gate Dash's weather fetch (above).
- **Dash controls for what has none yet.** These are listed as each phase adds or finds them. Candidates now:
  - the `[RUNS]` sequence;
  - the mutual-event recording;
  - the CALIBRATE FOCUS result readout;
  - the GoTo panel's stuck-slew notice (157.0b).
