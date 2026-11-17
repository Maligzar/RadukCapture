# Phase 157 plan: reliability qualification

From the strategic roadmap review the owner brought on 2026-11-13. This phase fills the gaps the review found in its "stabilization and validation" phase that nothing else here covers. Those gaps are:
- fault injection;
- a long soak;
- independent file checks in CI;
- a qualification report.

Status: `PLANNED`. Part of release 3.4 ([`ROADMAP-Next.md`](ROADMAP-Next.md#next-in-order)).

## What is already covered elsewhere
These are left to the phases that own them, not duplicated here:
- the drop ledger and throughput (146);
- crash-safe headers (135.2);
- the validation mode and the register of unverified constants (155);
- the unattended sky run (156).

## Stages
- **157.0 Fault injection.**
  - New emulator switches (`RADUK_EMU_FAULT=<name>[@seconds]`) inject one fault into a scenario.
  - Each fault has a scenario in `tests/scenarios/fault-*.scn`. The scenario asserts the safe outcome: the motion stops, the mount parks or is left stopped, the file on disk is whole up to the fault, and the screen and the log name the reason.

  | Fault | The safe outcome |
  |---|---|
  | Mount serial replies stop (a timeout) | Guiding releases the axes, the GoTo is abandoned, and "mount not answering" is shown |
  | A reply is lost or garbled | The command is retried once and then refused; never a runaway rate |
  | A slew never finishes | Stopped at the slew timeout, the mount left stopped, the target not assumed reached |
  | The focuser stalls mid-sweep | Autofocus ends with the stall named; the focuser is not driven further |
  | The camera is unplugged mid-recording | The run stops, frames so far are intact (135.2), and the reason is in the record |
  | The disk fills mid-write | 135.1's stop, with every captured frame that fits written |
  | Leoben drops off the LAN | The weather reads as unknown, not clear; nothing waits on it |
- **157.1 A soak.**
  - `tests/run-soak.ps1` runs the emulator for N hours (default 4) through Autorun cycles: focus, record, gap, a GoTo between targets, and a tracking lock.
  - It samples the process's handles, private bytes and thread count every minute, and fails on steady growth.
  - It fails on any frame whose timestamp is not after the one before, and on any unexplained drop in the ledger.
  - It is too long for every push, so it runs nightly or by hand. Its report goes to `reports/soak_<date>.txt`.
- **157.2 Independent file checks in CI.** Today FITS waits on a person opening it in astropy and Siril (checklist 29). Here, a CI step:
  - writes test recordings (SER, FITS);
  - reads the FITS files with astropy, checking that the headers parse, the axes and data match, and the time cards are present;
  - reads the SER files with a small independent Python reader written from the SER specification, not from this code. It checks the header, the frame count against the file size, and the trailer timestamps against the frame count.

  When phases 147 and 148 land, AVI and TIFF are added the same way.
- **157.3 The qualification report.** `docs/QUALIFICATION.md` is a table of configurations that have actually been run: camera, SDK version, firmware, mount, focuser, and the measured figures (sustained frame rate, drops, AF repeatability, pointing error), each with its date and source (146.2, a 156 sky run, a soak). Only measured rows go in; a configuration never run does not appear. It is the compatibility matrix that phases 159 and 160 add to.

## 157.0, as built (2026-11-14), first part
- **`RADUK_EMU_FAULT=name@start[+duration]`** (`src/core/EmuFault.h`, pure, tested by `tests/emu_fault.cpp`) is read by each emulator when it starts:
  - `eaf-stall`: the focuser's motor stops; it is still asked to move, and does not.
  - `camera-drop`: the camera reports itself removed. With a duration it comes back; without one, never. The old `RADUK_EMU_REMOVE_AFTER_S` still works.
  - `slew-stuck`: a GoTo never arrives, and the mount keeps saying it is slewing.
- **`tests/fault_injection.cpp`** runs each against the app's own code, in groups, on CI:
  - a stall mid-sweep: autofocus ends, names it, and drives no further;
  - the camera gone mid-recording: the run stops after the silence limit, with the frames on disk and their record;
  - a short drop: the run rides it out;
  - a stuck slew.
- **A gap it found, fixed.** A camera that died mid-recording left the run open for ever: the frame and time limits are checked only when a frame arrives, and nothing read the camera-fault flag but a log line. `RecordingController` now ends the run when the capture thread's newest frame has not moved for 15 s, or five frame intervals (from the measured rate or the exposure, whichever is longer). The 15 s is tagged UNVERIFIED against item 56.
- **Found: the GoTo panel's slew status had no timeout**, so a GoTo that never arrived showed SLEWING for ever, and the survey loop moved on silently after 90 s. The mosaic (90 s) and satellite (120 s) paths did time out. **Fixed in 157.0b:**
  - `src/core/SlewWatch.h` (pure, tested by `tests/slew_watch.cpp`) gives the panel a 180 s limit, tagged UNVERIFIED against item 53. Past it, the mount is stopped, the panel says so, and acquisition does not start.
  - A survey slew that does not finish ends the survey with the mount stopped.
  - The fault test's `slew-stuck` group drives the watch against the stuck emulated mount.
- **Not injected yet:**
  - a mount that stops answering, which needs the real NexStar transport's timeouts and has no comms layer in the emulator to fake;
  - a disk filling, already covered by `tests/capture_write_fail.cpp` through its seams;
  - Leoben dropping off.

## 157.1 and 157.2, as built (2026-11-15, 3.3.19)
- **157.2, on CI** (`tests/run-file-checks.ps1`, after the bench tests):
  - `tests/write_recordings.cpp` writes, through `SERWriter` and `FITSWriter`: SER mono 8/16 and RGGB 8/16, FITS mono 8/16 and RGGB 16. Odd sizes (67x41, 53x37), pixels from a formula restated in both halves, stamps on a fixed interval, and a manifest.
  - Last, a crash: a SER never closed, the process ended by `TerminateProcess`, frames 1.1 s apart on the frame clock so each one patches the count (135.2).
  - `tools/check_recordings.py --manifest` reads them with nothing from `src`. SER: a reader written from the SER v3 specification checks the FileID, ColorID and depth, the frame count against the file size, the trailer (all 8 bytes per frame or none), every pixel, every interval to the tick, and the stamps against the header's open time. The LittleEndian flag is reported, not trusted (the app writes 0, as FireCapture does). FITS: astropy, with `verify("exception")`, uint16 through BZERO, every pixel, TIMESYS, DATE-OBS < DATE-AVG < DATE-END spanning the frames plus one exposure, EXPTIME, BAYERPAT, and the FRAMES table's MJD_MID against the interval and DATE-OBS.
  - All 8 files pass. Corrupted copies were checked to fail: one pixel, a trailer 4 bytes short, two stamps swapped, and BZERO set to 0.
- **157.1** (`tests/run-soak.ps1 [-Hours 4]`): see the script's header and `docs/TESTING.md`. It generates the cycle scenario (Kochab and Pherkad in turn: GoTo, LOCK, RUNS with autofocus and two 150-frame runs) and runs it in a sandbox until the deadline, then closes the window. It samples the process every minute, scans the recordings with `check_recordings.py --scan`, sums the drop ledger's `unexplained` frames, and counts the locks. Report: `reports/soak_<stamp>.txt`. Needs astropy where the profile records FITS (the emulator's does).
- **Found by the trial soaks** (owed, not fixed here):
  - **A LOCK on Polaris always fails.** Calibration refuses when both axes track at under 0.5"/s ("the mount's axes are not moving"). At Polaris the true sidereal rates here are about 0.05"/s in azimuth and 0.18"/s in altitude, so the check refuses a mount that is tracking correctly. The floor should come from the expected rate for where the scope points. The soak uses Pherkad instead.
  - **A GoTo's pointing error can leave the target near the frame's edge, and nothing recentres it.** On a 640x480 ROI the emulator's 45"/25" error put Kochab at y = 431 of 480. Calibration then refused ("near the edge of the frame"), as designed, but an unattended run (Autorun, Phase 156) has no one to centre it. The soak presses LOCK without requiring it, and reports the locks made and each reason for a refusal.
  - **The fix, from the owner watching the trial soak (2026-11-15): calibrate at a large resolution, then go back to the ROI.** A recalibration on the 640x480 ROI left the star almost off screen, and tracking struggled to bring it to the centre. The frame is too small for calibration's moves of a few hundred pixels, and too small to find the target after a GoTo's error. So acquisition and calibration should switch to a large frame (the full sensor, or a large ROI), lock and centre there, then return to the profile's ROI. **Queued first for the next session as 157.0c**, ahead of the 4-hour soak, because it turns both findings above into one fix. Its check is the soak: locks made on every cycle, with no edge refusals.
  - **The first 30-minute soak passed on everything it measures:** handles and threads flat, private bytes +21 MB/h (limit 64), all 17 recordings read back, 0 unexplained frames. Locks: 0 of 7, every refusal at the frame's edge. That is the case for 157.0c.
  - **The soak could not close the app.** `Process.CloseMainWindow` reached a panel, whose close only hides it. Fixed: the soak finds `RadukCaptureWindow` by enumerating the process's windows (PowerShell passes `$null` to FindWindow as an empty title), posts WM_CLOSE, and answers Yes to "Stop recording and exit?".
  - **Kochab and Pherkad were drawn as Jupiter**, because the emulator falls back to Jupiter for any name it does not know. The owner asked for stars as stars (3.3.19, see the Help row). Out of focus, a star then faded to nothing, because the blur spreads a peak capped at 255 DN. It is now drawn as the SCT's doughnut, at least 40 DN. A 6-minute soak on the star then calibrated, locked, autofocused to 168010 (true focus 168000), recorded, and closed cleanly.
  - **With Kochab now a star, the scenarios that used it found three things** (owed):
    - autofocus from 1500 steps out takes about 3 minutes on a star (autorun.scn waited 120 s);
    - image-only following ("track what is in frame") loses its target and then finds nothing bright enough to follow (motion.scn). Not star-specific: it fails the same way on the Jupiter disc (worst error 0.57 degrees, against 1.05 on the star), so it was failing before this work;
    - a profile made for a target starts at the focuser's live position (3.3.18, deliberate), so a scenario's `profile star.ini focuser position=` no longer reaches it.
    autorun.scn and motion.scn now pin the Jupiter disc they were written against (`RADUK_EMU_TARGET`), since they test their own mechanics; autorun passes that way, and motion still fails as described. guide-edge.scn and guide-jog.scn fail on "no button called 'FINE'"; their target never changed, so that failure is not from this work.
  - **Private bytes jump about 160 MB at the first recording, then hold.** That is a buffer kept, not a leak. The growth rule fits its slope to the later half of the samples so that a step like this does not fail the soak.

## 157.0c, as built (2026-11-16, 3.3.20)
- **What it does:** `core/AcquireFrame.h` (`WideAcquire`) widens the frame to the whole sensor (or `AcquireWideMaxW`/`H`) when an auto-acquire starts on a smaller one and nothing records; it puts the profile's frame back when the sequence ends (centred on the target when locked), right before any recording starts, and forgets if the frame is changed by hand. Wired at each start (the CENTROID button, PLANET LOCK's promote, Dash's `centroid_start`), on every main-loop pass, and before all four recording starts. Telemetry sends `roi_w`, `roi_h`, `acquire_wide`; Dash's ROI row reads `WIDE`.
- **Checks:**
  - `tests/acquire_frame.cpp`: the decisions.
  - `tests/scenarios/acquire-wide.scn`: GoTo Kochab on the Star profile's 640x480 (the soak's failing case), PLANET LOCK. Passed: widened to 3840x2160, calibrated, locked, back to 640x480 centred on the star, no edge refusal.
  - `tests/web/specs/acquire-wide.spec.js` (@live): Dash loads the Star profile, CENTROID shows `WIDE`, RESET restores 640x480. Desktop and phone pass.
  - A 30-minute soak (`soak_20261116_163526`): 4 locks in 4 cycles, 0 refused (the first soak: 0 of 7). Each cycle widened, calibrated and returned to 640x480. Handles and threads flat, 0 unexplained frames.
- **Found while checking it, fixed:**
  - RESET did not end an auto-acquire: `Reset()` left the flag for `Tick()` to clear, and the main loop calls `Tick()` only while the stage is not Off. The wide frame stayed on after Dash's RESET. `Reset()` clears it; `tests/tracking_emulator.cpp` step 6 checks it.
  - Dash's older telemetry strip never showed: `wsApplyState` assigned `wsFCRunning` (left from the FC to RC rename), not `wsRCRunning`. The WIDE label was first put there, where no one would see it; it is now on the remote panel's ROI row as well.
- **Found, owed:**
  - Dash's profile picker (`rcProfile`) relabels Dash only and never sends `load_profile` to the app, against the rule that every control is reachable from Dash. The Playwright spec sends `load_profile` through Dash's own `rcSendCmd`.
  - On the emulator's start-up frame (the whole sensor, before any GoTo) the planet sits at (2655, 2155) of 3840x2160 and calibration refuses it at the edge. A GoTo is what the emulator expects first.
  - That soak failed on two counts, neither from this change: astropy is not installed on StarMachine, so its 7 FITS recordings could not be read back; and private bytes rose +65.4 MB/h over the later half (limit 64) while the bench suite ran beside it, though the last quarter's median was only +5.8 MB on the first's. To be rerun alone, with astropy, ahead of the 4-hour soak.
  - `tests/autofocus_saturn.cpp` takes 18 minutes (on CI as here) and prints through buffered `printf`, so a run watched from outside looks hung.
- **The first 4-hour soak (`soak_20261116_184325`), FAIL, and what it found:**
  - 30 locks made, 0 refused; 62 of 62 recordings pass the independent reader; handles and threads flat.
  - **Autofocus ran across a frame change in 28 sweeps of 30.** The cycle waited a fixed 45 s after PLANET LOCK, then pressed RUNS. Acquisition on the 3840x2160 frame took longer, so the sweep started wide, and the lock put 640x480 back 10 to 70 s into it. The app allowed it from any path (AUTO FOCUS at the scope, RUNS, Dash).
    - **Fixed:** `AutorunRunner` waits while acquisition runs (`Inputs::acquiring`, at most `acquireHoldSec`, 600 s, then fails "acquisition did not finish"), ahead of the target check because on the wide frame the target may not be found yet. `WideAcquire` holds both the widen and the restore while a sweep runs (`In::focusing`). `tests/autorun.cpp` and `tests/acquire_frame.cpp` check both.
    - **The soak** now presses RUNS 3 s after PLANET LOCK, so every cycle exercises the wait, and fails on any `[ACQUIRE] ... WxH -> WxH` between Autorun's "focus before run" and the sweep's result. Run on the failed soak's log, it finds the 28.
  - Private memory stepped up about 190 MB (first-quarter median 617.6 MB, last 814.3 MB; +105.3 MB/h over the later half). The baseline crept 12 MB over two hours, then at about minute 127 handles fell 1511 to 1424 and threads 184 to 170 with +24 MB, and at minute 133 to 135 it stepped a further 100 MB and held. Cause not yet found; to be judged on the rerun, with the overlap gone.
  - 1 unexplained frame: a single timestamp gap in one of 62 runs. To be judged on the rerun.

## Rules
Phase 149's rules hold, and rule 7: nothing here needs the internet. CI's Python step installs astropy from PyPI at build time, which is CI's network, not the app's.

## Versioning
Every PR bumps PATCH. MINOR marks a milestone release (3.4 for this phase), not a phase. Suggested PRs: 157.0, then 157.1 with 157.2, then 157.3.
