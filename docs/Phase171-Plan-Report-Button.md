# Phase 171: report a problem to Claude, from the app

The owner, at the scope on 2026-11-22: "I want a troubleshooting button built into RadukCapture so that when I click it I can describe the issue, close the window, and then it builds the file and sends it to Claude." The owner chose to send it to Claude Code on the capture PC, built on #189's hand-off. Status: `WRITTEN` (3.3.49).

## What it does
1. **Opening it:** REPORT on the rail (above HELP), `Ctrl+Shift+R`, or "Report a problem to Claude" in the command palette (`gui/ReportDialog.h`).
2. **The window:** a box for what happened, a tick for screenshots of the app's windows (on by default), SEND and CANCEL.
3. **SEND closes the window at once.** On the UI thread the app takes:
   - the words;
   - its state: target, profile, camera, exposure, frame, capture rate and drops, recording, AE, focuser, autofocus, mount, tracker, sky run, park;
   - the screenshots (`WindowShot.h`).

   These are written to `<data root>\reports\<stamp>\`.
4. **Behind, on its own thread:**
   - the log's last 4000 lines and rig.ini, each with the site and secrets removed (`core/ProblemReport.h`, gated by `problem_report`), and `report.json`;
   - then `tools\sky_handoff.py <folder>`, found a few folders above the exe in the clone the app was built from, with Python.
5. **The hand-off** (`sky_bundle.bundle_report`, `sky_handoff`'s report kind):
   - a summary;
   - the night's validation log;
   - a `report/<stamp>` branch from master in its own worktree, with the screenshots kept out of the commit (a `.gitignore` in the folder) but there for Claude to look at;
   - pushed, then Claude Code started headless with `tools/sky_handoff_prompt.md`'s report section: find the cause, fix it with a test if the evidence is clear, open a draft pull request, never merge.
6. **A notice says how it ended:** sent, saved but sending failed (and why), or saved with no tools or Python found. `Offline=1` keeps it on the PC.

## What never leaves the PC
- **The site.** rig.ini's `Site*` keys and every log line that names it are removed.
- **Secrets.** The Dash token and any key, secret or password are removed, as is Leoben's address.
- **The screenshots** are not committed. They show what was on screen, which can include the RA and Dec readout but not the site.

## Tests
- `problem_report`: the redaction and `report.json`;
- `sky_bundle --self-test` and `sky_handoff --self-test`: the report kind, its own branch, screenshots not committed;
- `command_palette`: `Ctrl+Shift+R`;
- `toolbar_layout`: twelve rail buttons at 600 px (the rail buttons went from 50 to 44 px);
- `tests/scenarios/report-problem.scn`.

## Owes
One real report from the capture PC, which needs Python, git with push rights and Claude Code logged in there.

## At the scope, 2026-11-22 (the first try)
The owner pressed REPORT in a test build and "nothing happened. If it popped a window it could have been hidden behind another."

**What the code showed.** The wiring was right: rail button, `Ctrl+Shift+R` and the palette all open the dialog. But the dialog was the one window not on top, while the mount controller, GoTo and Help are all `WS_EX_TOPMOST`. Nothing was logged either way, so the cause could not be read from the night's log.

**Fixed:**
- the window is topmost;
- a window that failed to create is created again on the next press;
- each step logs under `[REPORT]`: opened from where, shown at what position, or could not be shown with the Windows error, plus a notice on screen.

**Owes:** one report sent from StarMachine.

**`try_branch.bat`** (the owner: "I want this all on the master so future nights it's easier to develop outside"): one command to build a pushed branch at the scope and one to return to master.

## At the scope, 2026-11-22 (the second try, 3.3.49)
The log, in both sessions:
```
[REPORT] opened from the REPORT button
[REPORT] the report window could not be shown (error 0)
```
- **What it shows.** The button worked, and the window had been created at startup with no error logged. But after it was shown and set topmost, Windows did not report it as visible.
- **The cause is not yet known.** The code is right on paper, and error 0 means Windows recorded none.

**3.3.57:**
- **When it fails,** the window's state is logged in full: handle, whether it still exists, style and extended style, position, owner, the owner's visibility and whether the owner is minimised.
- **Then a fallback.** The owned tool window is destroyed and a plain top-level window is opened in its place: no owner, a normal title bar, in the taskbar, topmost, centred on the main screen's work area.
- **The log says which window opened.**

**Owes:** one press at the scope. The log line names the cause for the code to fix properly.

## The cause, found at the scope (2026-11-24, 3.3.74)
The 3.3.57 diagnostics told the story. Three presses gave the same result: the window existed (`IsWindow 1`), centred at 680,350, with no error, but `visible-style 0`. The plain fallback did exactly the same.

**The window hid itself in the instant it was shown.**
- `Show()` gives the text box the focus.
- The text box tells its parent with `WM_COMMAND`, carrying its control id and `EN_SETFOCUS`.
- The text box's id was 2, which is `IDCANCEL`, and `WM_COMMAND` checked only the id.
- So focusing the box was read as CANCEL, and the window hid.

The label had id 1, `IDOK`. Typing would have closed the window too, through `EN_CHANGE`.

**The fix.**
- The controls now have their own ids: 101, 102 and 103.
- SEND and CANCEL act only on `BN_CLICKED`.
- `tests/report_dialog.cpp` shows the dialog, sends the text box's focus and change notifications plus id-1 and id-2 non-clicks, and checks it stays open. CANCEL and SEND still work.

Every version since 3.3.48 had this bug. The topmost change (3.3.49) and the plain fallback (3.3.57) were both answers to the wrong question, though the fallback's diagnostics are what found the real one.
