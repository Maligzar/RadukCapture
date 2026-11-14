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
- **Found and owed: the GoTo panel's slew status has no timeout.** A GoTo that never arrives shows SLEWING for ever, and the survey loop moves on silently after 90 s. The mosaic (90 s) and satellite (120 s) paths do time out. 157.0b adds the panel's timeout.
- **Not injected yet:**
  - a mount that stops answering, which needs the real NexStar transport's timeouts and has no comms layer in the emulator to fake;
  - a disk filling, already covered by `tests/capture_write_fail.cpp` through its seams;
  - Leoben dropping off.

## Rules
Phase 149's rules hold, and rule 7: nothing here needs the internet. CI's Python step installs astropy from PyPI at build time, which is CI's network, not the app's.

## Versioning
Every PR bumps PATCH. MINOR marks a milestone release (3.4 for this phase), not a phase. Suggested PRs: 157.0, then 157.1 with 157.2, then 157.3.
