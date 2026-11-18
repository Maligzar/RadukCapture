# Qualification: the configurations that have actually been run

Phase 157.3. This is the compatibility matrix that phases 159 and 160 (QHY, Player One) add to.

## The rules for this file
- **Only measured rows go in.** A camera, mount or focuser that has never been run with the app does not appear, however likely it is to work.
- **Every figure has a date and a source:** a file, a log, a report or a test that someone can open again.
- **A figure that was not measured says so.** It is not filled in from a spec sheet, an estimate or a guess. Where the owner saw something but the app did not measure it, the notes say that.
- **Rows are added, not rewritten.** A later run on the same configuration gets its own row. A row is changed only to correct it, with the correction said.
- **Hardware and the emulator are kept apart.** The emulator's rows show our side keeps up. They say nothing about a real camera over real USB.

## 1. Hardware configurations run

| # | Camera | Camera SDK | Firmware | Mount | Focuser | Focuser SDK | PC | App | First run |
|---|---|---|---|---|---|---|---|---|---|
| H1 | ZWO ASI678MC (colour, RGGB, 2.0 µm) | ASICamera2 1.41 | not recorded | Celestron 8SE (NexStar SE, alt-az), app-driven rate guiding | ZWO EAF | EAF_focuser 1.7.7 | StarMachine | 3.0.0 | 2026-11-07 |

**Sources for H1:**
- The camera's name: the session log `logs/session_20261108_014110.log` ("Opened: ZWO ASI678MC"; header "Telescope: Celestron 8SE Camera: ZWO ASI678MC").
- The pixel size and Bayer pattern: each recording's observation record.
- The SDK versions: the DLLs in `build/Release` (`ASICamera2.dll` 1.41.0.0 and `EAF_focuser.dll` 1.7.7.0, both dated 2026-10-09, before the night).
- **Firmware is not recorded.** Neither the app nor the session log writes the camera's or the EAF's firmware version. Owed: log both at connect, so the next row has them.

## 2. Measured on hardware

### 2.1 Frame rate and drops

| Date | Config | Target | Frame | Exposure | Frames | Duration | Sustained rate | MB/s | Dropped (SDK, buffer) | Timestamp gaps | Limited by | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2026-11-08 02:17 | H1 | Uranus | 640x480 16-bit, USB 90, high-speed on | 19.49 ms, gain 528 | 7105 | 139.0 s | 51.1 fps (interval median 19.56 ms, 19.12 to 20.00 ms, s.d. 0.10 ms) | 31.4 | 0, 0 | 0 | the exposure | `captures/2026-11-08-0918_9-Uranus_…ser` |
| 2026-11-08 02:23 | H1 | Uranus | 640x480 16-bit, USB 90, high-speed on | 26.97 ms | 8571 | 231.7 s | 37.0 fps (interval median 27.04 ms, 26.39 to 27.64 ms, s.d. 0.12 ms) | 22.7 | 0, 0 | 0 | the exposure | `captures/2026-11-08-0925_8-Uranus_…ser` |
| 2026-11-08 02:28 | H1 | Uranus | 640x480 16-bit, USB 90, high-speed on | 26.97 ms | 4698 | 127.0 s | 37.0 fps (interval median 27.04 ms, 26.35 to 27.64 ms, s.d. 0.13 ms) | 22.7 | 0, 0 | 0 | the exposure | `captures/2026-11-08-0929_7-Uranus_…ser` |

**In total:** 20,374 frames in 8.3 minutes on a real ASI678MC over real USB, with no frame lost anywhere. They were measured on 2026-11-17 with `tools/ser_drop_audit` (3.3.21) on each SER's timestamp trailer, and the intervals were read straight from the trailer as well.

**Why the timestamps are evidence:**
- These SERs were converted by `fits2ser` from the FITS files the app wrote that night. `fits2ser` copies each frame's `MJD_MID` from the FITS `FRAMES` table into the SER trailer (`src/core/FitsToSer.h`). It does not make times up.
- The intervals are not on a grid. They jitter by about 0.1 ms, across 1,372 to 1,583 distinct values a file, and none is zero or negative.
- So a missing frame would show as a gap of about twice the median, and there is none.

**What these rows do not answer:**
- **All three runs were limited by the exposure, not by USB.** 31 MB/s is about a tenth of what 146.2 will ask of the camera. The camera's full-sensor rate over USB is still unmeasured. That is 146.2.
- A fourth file from the same minutes, `0920_2` (3840x2160 16-bit), holds 1 frame (a run stopped at once), so there is nothing to time. It is left out.
- The other 37 files of that night (Saturn, 140.0's audit) are no longer on the disk, so their rates cannot be read again. They do not appear.

### 2.2 Locks and guiding on the sky

| Date | Config | Target | Locks made | Ended by the guider ("kept moving away") | Held | Source |
|---|---|---|---|---|---|---|
| 2026-11-08 01:12 to 02:20 | H1, app 3.0.0 (before #105) | Saturn, then Uranus | 5 | 4 | 1-2″ on Saturn at 01:11 (Phase 133, F1) | `logs/session_20261108_004144.log` to `_014110.log`; `docs/Phase133-Plan-Field-Proof.md` |

- These locks ran on the code **before** the fixes in #105:
  - two of the four ends followed a box size change (F5, at 01:21) and a hand jog (F6, at 01:37);
  - both causes are fixed on the bench but not yet on the sky.
- **There is no lock row for the code since then.** That is clear-night items 47 to 53 (133.3).

### 2.3 Not yet measured on hardware
Each of these columns is empty because nothing has measured it. It is not empty because the answer is zero.

| Figure | Why there is no row | Where it will come from |
|---|---|---|
| Full-sensor frame rate and MB/s over USB | Every real run so far was exposure-limited | 146.2: the owner's indoor runs, capped camera at 0.1 ms |
| Autofocus repeatability | No sweep has run on the sky (F9) | 133.3 or a 156 sky run |
| Pointing error | The app has never measured it on this mount. The owner saw it about 8° off on 2026-11-07 (F12), but that is not a measurement | 133.4: re-align, then the pointing map's lock solves (Phase 123) |
| Firmware versions | Not logged | Owed: log at connect |

## 3. Measured on the emulator
These rows test the app, not the hardware. They are kept so a regression shows against them.

| Date | What | Camera | Result | Source |
|---|---|---|---|---|
| 2026-10-17 | The write path at full frame, to the capture drive | ZWO ASI678MC [EMU] | 1280x960: 57 fps, 67 MB/s, 0 dropped. 3840x2160: 37 fps, 293 MB/s (the disk's measured ceiling is 292 MB/s), 556 of 556 frames written | `docs/CLEAR-NIGHT-CHECKLIST.md` item 13 (`9f886d6`) |
| 2026-11-16 | 30-minute soak with wide acquisition (157.0c) | ZWO ASI678MC [EMU] | 4 locks of 4; 7 runs, 0 unexplained frames. FAIL on memory (+65.4 MB/h, with the bench suite running alongside) and the reader (astropy missing) | `reports/soak_20261116_163526.txt` |
| 2026-11-16 | 4-hour soak, first try | ZWO ASI678MC [EMU] | FAIL: autofocus crossed a frame change in 28 sweeps of 30. Also private memory +105.3 MB/h, and 1 unexplained frame in 62 runs. 30 locks of 30; 62 of 62 recordings pass the independent reader | `reports/soak_20261116_184325.txt` |
| 2026-11-17 | **4-hour soak, with the fix (3.3.20)** | ZWO ASI678MC [EMU] | **PASS.** 26 locks of 26; 52 sweeps, 0 crossed. 51 of 51 recordings pass the independent reader; 0 unexplained frames. Private memory −15.6 MB/h; handles and threads flat | `soak_20261117_055136` (run from the 157 worktree; summary in `docs/Phase157-Plan-Reliability-Qualification.md`) |
| every push | SER and FITS written by the app, read back by independent readers (157.2) | — | 8 of 8 files pass, plus a crashed SER | CI, `tests/run-file-checks.ps1` |

## How to add a row
1. **A new camera, mount or focuser:** add a row to table 1 with its SDK and firmware. Phases 159 and 160 add QHY and Player One here when one has been run.
2. **A frame-rate figure:** run `ser_drop_audit` on the SER, or copy the run's `[CAPTURE] Frames:` line, and give the file name.
3. **A sky figure:** cite the 156 bundle's `results.json` or the session log, with the time.
4. **Never** copy a figure from a spec sheet or a forum into the measured columns. Put it in the notes, marked as the vendor's figure.
