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
