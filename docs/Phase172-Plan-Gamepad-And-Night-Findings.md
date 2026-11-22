# Phase 172: a game controller at the scope, and the findings of 2026-11-21

Written 2026-11-22 at the owner's direction, from the Saturn night. Status: `PLANNED`.

## Why

PLANET LOCK guides well (1.5" mean on a hazy night, 0 drops in 63,891 recorded frames), but **the hand control and the lock fight**. The guider re-sends both axis rates every 250 ms (`CentroidTracker`, `S3_RATE_MS`), so a hand-control press moves the mount for at most a quarter second. On release the hand control stops that axis, and the guider then pulls the planet back to the old lock point. The app cannot see the hand control at all.

The app's own jog (the arrow keys and the mount window's N/S/E/W) already does this properly. It suspends the guider, and on release restores both rates and takes the planet's new place as the centre (the 2026-11-08 fix, `CentroidTracker.cpp` "hand jog ended"). The owner uses the hand control to find the planet before the lock, which is fine. The trouble is only fine-tuning during a lock.

The owner has a Bluetooth Xbox controller. It is the easier and safer of the two fixes: it goes through the existing jog, so the lock's behaviour does not change.

## 172.0: the controller (size M)

- **Read it with XInput.** XInput ships with Windows: no driver, no install and nothing online (rule 7). The Xbox Wireless controller over Bluetooth appears to XInput on Windows 10 and 11. That is recalled, to be confirmed on StarMachine. It is polled in the render loop at about 60 Hz, and nothing is read when no controller is connected.
- **A pure mapping header, `core/PadMap.h`.** It turns a pad state (buttons, sticks, triggers) into actions, with a dead zone, edge detection (a press is one action, not sixty), and hold-to-move for jogs. It is tested with g++ (`tests/pad_map.cpp`), with no Windows.
- **Every action goes through an existing path,** never a new one:

| Pad | Action | Existing path |
|---|---|---|
| Left stick or D-pad | jog N/S/E/W while held | `window.SetJogHandler` (suspends and re-centres the lock) |
| LB / RB | jog rate down / up | `mountCtrl.SetJogRate` |
| A | PLANET LOCK on/off | the PLANET LOCK button |
| B | STOP (mount) | `doFullStop`, the same as ESC |
| X | RECORD start/stop | the RECORD button |
| Y | autofocus | AUTO FOCUS. **Disabled until Phase 167 closes** (the donut night) |
| Triggers | focus in / out, small steps | the focuser panel. **Waits on Phase 169** (manual focus rebuilt) |

- **Safety.** B always stops, whatever else is held. A lost controller (Bluetooth drop) mid-jog ends that jog, exactly as releasing the key does, so the mount never runs on. This is tested by a pad state that disappears while a jog is held.
- **Shown in the app.** The status bar shows "pad: connected" or "none". Help gets a picture of the mapping.
- **Tests.** `pad_map` (mapping, dead zone, edges, the drop mid-jog); a scenario (`gamepad-jog.scn`) feeding pad states through a scenario hook into the jog path, on the emulator with a lock held. It checks the "hand jog ended: rates restored" line, and that the lock holds at the new place.

## 172.1: the hand control, detected (size S, later; warning only)

The app compares the rates it commands with what the encoders report (polled every 0.5 s). When they disagree by far more than seeing and wind explain, it shows one notice: "the hand control moved the mount during the lock; use the arrows or the controller". It **never changes the guiding**: a misfire must cost a notice, not the lock. The threshold is to be taken from a real night's encoder log.

## Findings of 2026-11-21, to be scheduled

Each was read from the night's log and screenshots. Where noted, it was checked against the code.

1. **TRK: OFF while the lock drove.** **Fixed** on the holding branch (`core/TrackingState.h`, test `tracking_state`): TRK: APP (lock), TRK: ? when unread, and the readiness, the assistant's state and auto-acquire's gates now count the lock as tracking. Display and gating only.
2. **An absolute focuser move while moving is dropped silently.** `EAFMove(154144) failed: 5` at 18:24:12 is `EAF_ERROR_MOVING` (`external/EAF_SDK/include/EAF_focuser.h`). `EAFFocuser::Command` sends absolute moves without checking `IsMoving()`; relative moves already chain onto the move in flight. **This goes into Phase 169:** wait for the move to finish, or stop and replace it, and say which.
3. **The seeing score reads blur as steadiness.** SEEING 9.4 to 9.7/10 on a hazy night, through a blurred disc. A blurred image barely changes between frames. **Phase 167:** the seeing score is not to be trusted while the focus readout is well below its reference.
4. **The base rates go stale during a long lock.** At 19:47 the guider added +8 to +9 "/s on azimuth to a 15.64 "/s base measured about 80 minutes earlier. Alt-az rates change as the target moves. It held 1.5", but a jump starts from a large integral. **Planned:** re-derive the base rates from the encoders during a long lock, without a new calibration.
5. **Altitude carries the guide error.** Azimuth stayed within ±4 px, while altitude swung ±15 px over 6 to 10 s. It is not backlash: the axis never reversed. The cause, seeing followed or loop overshoot, is to be told apart by comparing the altitude correction with the camera's own motion, and the altitude gain softened if it is the loop.
6. **`[PHEMU]` retries IMCCE often.** "could not reach IMCCE's prediction service; keeping what is cached" appears in a 2-second slice of the log. The count from the night's log is to be checked. Retries are to back off, and to go through NetPolicy only (rule 7). **Fixed in 3.3.50:** after a failure `PhemuDesk` waits 30 min, doubling to 6 h, and says when it will try again; a success ends the back-off.
7. **Untimestamped log lines.** `[SHARP]`, `[MOTION]`, `[ADC]` and `[PHEMU]` are printed without the time, so a night cannot be lined up against events. They are to go through `Log()`. **Fixed in 3.3.50:** `Log()` stamps every line that starts with a `[TAG]`, the same `HH:MM:SS.mmm` as CoreLog's.
8. **Layout.** In the mount window, the "...in about 79 min" line overlaps the 9x/FAST/MED/FINE row. In the focuser panel, the status lines are cut off at the right ("Ready to focu", "ADC: not fitte", "Filter: UV/IR cut (400-700nm"). Both are to be fixed through the layout scenario.
9. **Memory.** 9,977 MB of working set during recording. About 2.5 GB is accounted for by the write buffer and the preview ring. The trend (flat or rising) was not yet measured; the four-sample command is in the night's notes. If it rises, find what retains full-frame (3840x2160) buffers after the switch to 640.

10. **"tracking=REFUSED" while the mount tracked, and T then turned it off.** All night every `T 1` (ALT-AZ) was ACKed and every 't' readback 250 ms later failed ("ACKed but readback failed", from 17:42), while `T 0` read back every time; the mount tracked regardless (PLANET LOCK calibrated on it and held Saturn for two hours). `SetTrackingMode` returned false, ALIGN reported REFUSED, and the T key toggled a cache it could not trust, turning tracking OFF when the owner meant ON. **Fixed, first priority** (`core/TrackingConfirm.h`, test `tracking_confirm`): an ACK with no readback is "sent, confirming", the mode shows TRK: ? until the position poll (whose own 't' read never failed that night) reads it, then the log says "confirmed by the position poll" or "NOT RUNNING (mount likely unaligned)" as before; ALIGN says "tracking sent, confirming"; T and the TRACKING button ask for ON when the mode is unknown. **Owes:** the night's log on the next session to see "confirmed by the position poll", and why the readback right after `T 1` fails (it is not the poll's read, which works) - a serial-level NexStar emulator would let CI reproduce it; there is none today (`MountEmulator` sits above the protocol).

11. **After ★ ALIGN, the lock takes about 40 s to come back, and once gave up.** From the owner's log, 2026-11-22:
    - **22:52:07, ALIGN.** It ends the lock, as it must: a sync moves where the mount thinks it is.
    - **22:52:09, tracking confirmed** by the poll.
    - **22:52:57, PLANET LOCK pressed.**
    - **22:52:57 to 22:53:33, calibration: 36 s.** That is 3 s of hand-control rates, three drift windows of 3.4 s, and two 4 s offset moves. The drift windows disagreed: (5.3, -9.3), (5.6, -2.2) and (0.6, -5.4) px/s.
    - **22:53:49, "Guiding abandoned".** The planet ran to 197 px from centre after reaching 12 px.
    - **The owner restarted the lock at 22:55.**

    **Planned:** a sync changes neither the motor axes, nor their signs, nor the camera's rotation, so ALIGN should keep the calibration's geometry and re-measure only the base rates (about 3 s, not 36). Whether that holds on the NexStar is to be checked by comparing two calibrations either side of a sync. The abandonment is to be read from the night's `[GUIDE]` lines between 22:53:33 and 22:53:49. The candidates are a frame change mid-centring, or the drift not yet settled after the sync.

12. **PLANET LOCK overshot and gave up after a long centring (integral windup).** From the owner's log, 2026-11-22:
    - **22:53:34 to :42.** From 480 px off on the full frame, both corrections sat at the never-reverse limit (azimuth: a correction of -16.47 on a base of 17.47 "/s; altitude: +6.32 on -7.32), while the integral summed the large error to its 15 "/s clamp.
    - **22:53:44, at 2.4" from centre.** The corrections were still near full: az -14.21; alt 0.5 x -2.6 + 15 clipped to +6.32.
    - **The planet overshot** to 94, then 146 px. The runaway check abandoned the lock at 197 px.

    **Fixed** (`core/GuideLaw.h`, test `guide_windup`): conditional integration. An axis pinned at a limit by the error's own sign does not integrate. In a model of the mount fed the night's numbers, the old law overshoots about 24" (about 120 px) and the new one 3.7", settling in 22 s. A tracking rate 9 "/s stale (19:47 the same night) is still absorbed. **Owes:** a long centring on the sky (from a full-frame position more than 400 px off) that settles without abandoning.

13. **The status bar said `640x480 RAW8` while 16b was selected and 16-bit files were written.** The once-a-second update passed a literal `8` to `StatusBar::SetCameraStats` instead of `roiBits`, which the recording uses. **Fixed** in 3.3.47. It was display only.

14. **RGB ALIGN's label read `ALIGN +0 px` while RGB ALIGN was off**: the label is only updated while it is on, so it kept its first text. **Fixed in 3.3.50:** it says `ALIGN off`.

15. **The app hung at the end of a Jupiter run and Windows closed it (AppHangB1, 03:08).**
    - **The run:** 1280x960, 16-bit, at 114 fps to `S:`. That is about 280 MB/s, and `S:` could not keep up.
    - **The buffer filled:** the 9.6 GB write buffer filled, and the log said `Write buffer FULL ... frames are being dropped`.
    - **The stop blocked:** when the run ended, `StopRecording` waited on the window thread for the whole buffer to drain. The window froze, the mount's watchdog stopped the mount at 8.5 s, and the owner closed the frozen app.

    **Fixed in 3.3.51:** while the writer drains, `CaptureManager` calls a hook every 200 ms. It feeds the mount's heartbeat, keeps Windows' message check alive (`PeekMessage`, removing nothing, so no click reaches a half-finished run), and shows `finishing the recording: N frames still to write` in the title. The log says how many frames waited and how long the drain took. Gated by `capture_write_fail` section 1b.

    **Also fixed, in 3.3.52** (`core/DiskBudget.h`, test `disk_budget`):
    - **Measuring each disk.** The writer measures what each disk writes from its busy time and keeps it per drive in `rig.ini` `[DiskSpeed]`. The figure moves halfway to each new run's measurement.
    - **Warning before the run.** A run needing more than 95% of its disk's figure is warned at the start, with the rate, the disk's figure and when the buffer fills. Replayed on the night, that is 280 MB/s against S:, with the buffer full in about 54 s at 95 MB/s.

    **Still open:** say in the camera panel that 1280 is about 4.5 times 640's rate.

## What went right (for the record)

- Two derotation-window runs stopped on time at 440.5 s, with 0 drops each: run 1 at 39.9 fps (25 ms, gain 230) and run 2 at 105.1 fps (9.44 ms, gain 324).
- The WinJUPOS names' mid-times were correct.
- AE was held for the whole of each run.
- The mount link replied in 35 to 45 ms, with no misses seen.
- The camera stream had 0 SDK drops and 0 overruns all night; there were 3 timeouts, during AE mode changes between runs.
