# Phase 173: RUN DARKS

Written 2026-11-22 at the owner's request, at the scope: "I need a dark file tracker meaning at the end of the night I click run darks and it runs my darks based on what is in the capture folders for the night." Status: `WRITTEN` (3.3.53).

## What it does
1. **First press** (`Ctrl+Shift+D`, or "darks" in the command palette) reads every observation record in:
   - the folders recorded to in this session;
   - the current output folder;
   - each folder's `Darks` subfolder.

   Records from before the local noon that began the night are ignored. Each light is reduced to the settings a dark has to match: frame (width, height, bin), bit depth, exposure (within 0.5%) and gain. Sets already covered by a dark taken tonight are dropped. The plan is shown and logged under `[DARKS]`: each set, how many runs it serves and their targets, the frames per set and the time.
2. **Second press within a minute,** with the scope capped, runs it. For each set it:
   - applies the frame and bit depth, exposure and gain, with AE off;
   - waits 3 s for frames already in flight;
   - records `DarkFrames` frames (`rig.ini` `[Rig]`, default 200) as frame type Dark into `<lights folder>\Darks\`, named `Dark_<exposure>ms_g<gain>_<w>x<h>_<bits>bit`.

   The next set starts when the file closes. Darks are never auto-stacked.
3. **At the end, on ESC or on a failure,** the camera is put back exactly as it was (frame, bit depth, exposure, gain, AE), and so is the capture config (frame type, prefix, folder).

## Code
- **`core/DarkPlan.h`** (pure): `DarkKeyOf`, `PlanDarks`, `DarkPrefix`, `DarkSetSeconds`, `DescribeDarkSet`.
- **`app/DarkRunController.h`** (pure): the two-press arm and confirm, the settle-record-next cycle, the stop, a failure with a stated reason, and the restore.
- **`main.cpp`:** reads the records, the hooks, the readout-tick `Tick`, ESC's stop, and `Window`'s `Ctrl+Shift+D`.
- **Tested by `dark_plan`,** with the night's own settings:
  - Saturn: 25 ms at gain 230, and 9.44 ms at 324;
  - Jupiter: 9.23 ms at 391, including a 9.2301 ms run from camera rounding that falls in the same set;
  - a record from the night before, which is ignored;
  - a record with no gain, which is counted as unusable.

## Not matched, on purpose for now
**Sensor temperature.** The observation record does not carry it. For exposures of milliseconds, dark current matters little next to the read pattern a planetary dark is for. To be added to the record, then to the key, if real nights show it matters.

## Owes
- One run at the scope.
- Native windows for the plan (today it is a notice and the log).
