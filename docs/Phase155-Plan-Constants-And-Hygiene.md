# Phase 155 plan: turning "unproven" into a number, and the hygiene list

From the owner's weakness review and recommendation 3, 2026-11-13. Status: `PLANNED`.

The design's seams, fake clocks and honest status already cover the pure code. What is missing is that **the wiring (phase 153) and the constants (this phase) are not in the same evidence-gated regime.** This phase brings the constants in, in three ways:
- a register of what has never been checked;
- an app that measures its own constants on real nights;
- real recordings replayed in CI.

## Part 1: the register, and an app that measures itself
**What is already planned.** Each unverified figure has a clear-night item, ranked in the architecture document's section 13.1:
- the plate scale, 0.203"/px: calculated, never solved (item 3);
- `SHARP_CEIL` (item 1);
- the light ladder's knees, the seeing bar, the exposure bands (item 55);
- autofocus's noise gate and fade bands (items 47 and 58);
- the satellite and lunar thresholds;
- and more.

The plate scale also needs ASTAP's D80 database installed before that night.

**What is missing** is a way to see them all at once from the code, and to stop new ones arriving silently:
- **155.1 Mark them in the source.** Every constant tuned on synthetic frames, derived without measurement, or taken from a datasheet gets a comment tag `UNVERIFIED(item N): what would verify it`, with N its clear-night item.
- **155.2 The register.**
  - A script (`tools/unverified_report`) greps the tags and writes a table to `docs/UNVERIFIED.md`: constant, value, file and line, item, what verifies it.
  - CI runs it and fails if a tag names an item the checklist does not have, so the two stay in step.
- **155.3 One-solve plate scale.**
  - The app already checks the plate scale on every solve. The step that is missing is offering to write the measured figure to rig.ini `PlateScaleArcsecPx` after three solves agree within 1%, with the change logged.
  - Phase 149.0 derives the nominal figure from the camera and the focal length, so a measured override and a derived nominal live side by side.

**155.4 A validation mode in the app.** On real nights the app collects what each clear-night item needs, as data, without a separate procedure. Each night writes `validation_<date>.json` beside the session log:
- **The plate scale:** every solve's measured scale against the configured one, the delta in percent, and the running agreement (155.3 acts on it).
- **The sharpness distribution:** the histogram of real-frame sharpness, by target, exposure and gain. `SHARP_CEIL` and the gate are then tuned from these files instead of from synthetic frames.
- **The seeing bar:** the seeing score's distribution, and how Autorun's 80% bar would have fallen across the night's frames.
- **Focus:** every sweep's measured backlash, the fine pass's fade ratios and its peak against the coarse one, and the edge metric's noise floor (items 47 and 58).
- **Exposure:** the target meter's highlight and clip share against the exposure band (item 55).

`tools/validation_report` combines several nights into a table with confidence intervals, and recommends a value only when enough data agrees. A clear-night item is then closed by the report's evidence, not by eye.

**155.4a and 155.4b, as built (2026-11-21, 3.3.37, one PR).**
- **`src/core/ValidationLog.h`** (pure) records:
  - every solve's measured scale beside the configured one, with frame size and binning;
  - a 101-bin sharpness histogram per target and per exposure and gain, with bin 100 being the frames at the cap;
  - the normalised seeing, once a second per target;
  - every autofocus result: start, best, gain, the backlash measured and the one in use;
  - the target's highlight, clipped share and frame share against the exposure, beside the profile's band.

  Samples are thinned and capped, so a night stays a few hundred kilobytes. The night is the local date the evening began.
- **`main.cpp`** feeds it from three places:
  - the solve callback;
  - the render loop, taking only valid frames on a target, and none during an autofocus sweep or a slew;
  - the autofocus completion, which runs on the sweep's thread, behind a mutex. The target name comes from the render loop, never from the UI on that thread.

  It writes `validation_<night>_<hhmmss>.json` in the logs folder every 5 minutes and at exit, through a temporary file and a rename. A run that saw nothing writes nothing.
- **`tools/validation_report.py`** joins any number of files and recommends only on strict rules: 3 solves on 2 nights with the whole 95% CI outside ±0.5% (a binned solve divided by its bin first); 3 nights and 5000 frames before SHARP_CEIL is judged; 5 measured sweeps on 2 nights with a bootstrap CI within 15% for the backlash. The seeing-bar table shows how often 60-90% of the night's best 10 s median would have held. Too little data is said, never filled in. Its `--self-test` runs on CI.
- **Checks:** `tests/validation_log.cpp` covers the night rule across month, year and leap day, the binning and thinning, the caps, and the JSON read back with `MiniJson`. The report's self-test has 13 checks. A round trip from the recorder's JSON through the report was run during the build.
- **155.4b: the fine pass's own figures.** `AutofocusRoutine` now fills an `AFDiagnostics` record through each sweep. It is cleared at `Start()` once the previous thread is joined, and read in the completion callback, which runs on the same thread. It holds:
  - the coarse peak and the fine peak, and whether the fine one was used;
  - each direction's peak, and the backlash measured on *this* sweep;
  - the number of fine passes and of fades corrected, plus the last fade check's ratio and verdict;
  - the last fine curve (positions and edge scores);
  - each fine point's frame-to-frame spread: half the 16th-84th percentile range over the median, which is the edge metric's noise floor measured where it is used.
- **A bug found and fixed in 155.4a's wiring.** `MeasuredBacklash()` keeps the last sweep that measured a backlash, so a sweep that did not measure one would have logged the previous value again, and the report would have counted it twice. The log now records `backlashThisSweep`, which is -1 when none was measured.
- **The report's fine-pass section** covers: the sweeps with a fine pass and how often it was used; the fine-minus-coarse offset with a bootstrap CI; the fade verdicts and ratios; the share of last fine curves that rise and fall once (one interior maximum that is also the highest point); and the noise floor against each curve's own contrast.
- **`autofocus_saturn` checks the record on the simulated sky:**
  - the per-sweep backlash agrees with the measured one;
  - a fine pass records its curve whole, with spreads that are sane fractions;
  - the no-edge-scores fallback records no fine peak;
  - the Moon records no coarse peak;
  - the count of corrected fades equals the number of `scores corrected` lines logged.

**155.5 A regression suite from recorded sessions.**
- **A replay camera.** `SerReplayCamera : ICamera` plays a SER back through the real capture path: the ring buffer, the analyser, the target finder, the focus metrics, the tracking centroid. `SerReader.h` already reads SERs (the player, phase 141.2's photometry), so this is a thin backend. Selected by `RADUK_EMU_SER=<file>`, and usable by the headless harness (153.0).
- **Clips from real nights.**
  - `tools/ser_clip` cuts short clips from real recordings: Saturn and Uranus from 2026-11-07 now, Jupiter and the Moon when they are recorded. A crop round the target, a few dozen frames.
  - Small enough to commit (a budget of about 20 MB for the whole set; no LFS, so a clone works offline), with a README naming each clip's night, target and settings.
- **Expectations.** Each clip has a checked-in file of what the pipeline must produce: target found, centroid stable within N px, HFD and edge score in a band, the fade watch silent, the quality gate's pass rate. The bands are set from the first run and reviewed by the owner.
- **In CI.** The replays run in CI, so a change to the analyser, the target finder or the focus metric is tested against **real** frames on every push, with no camera present.
- A defocus series from a real autofocus sweep, once one is recorded, becomes the fine pass's real-frame test.

## Part 2: hygiene
Found 2026-11-13. Where the review was imprecise, this records what is actually there.

| Item | What is there | Fix |
|---|---|---|
| `printf` logging in core code | **Done in 3.3.7.** Measured when built: 170 calls in **27 files** of `src/core` and `src/sky` (the 52 first counted was too high), not just `RingBuffer.h:44`. In this GUI app printf goes to a console window that is not kept, which is how the 2026-11-07 mount failures left no trace (`CoreLog.h`'s own header). `CoreLog` exists and is used by the mount and the tracker | Route every core `printf` through `CoreLog`, file by file. A CI grep then forbids new `printf` in `src/core` and `src/sky`, outside `CoreLog.h` itself and the bench tests |
| Dead entry points in `CentroidTracker` (**done in 3.3.7: all three deleted, the owner's call**) | `ClearCalibration`, `SetDriftCallback` and `StartCentering` have no callers outside the class (grep, 2026-11-13) | Confirm each is not called internally, then delete it, or wire it if it is an intended feature (`StartCentering` may be the manual-centre path; the owner says) |
| Empty `src/analysis`, `src/bridge`, `src/writers` (**the architecture line was already corrected**) | **Not in the repository.** git does not track empty directories, so they exist only in a local working copy | Delete them locally, and correct `RadukCapture-Architecture.md`'s line saying they are in the tree |

## Versioning
Every PR bumps PATCH; the PR that completes the phase bumps MINOR ([`ROADMAP-Next.md`](ROADMAP-Next.md#next-in-order)).
