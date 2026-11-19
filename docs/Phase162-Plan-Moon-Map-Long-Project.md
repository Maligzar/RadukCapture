# Phase 162c to 162g plan: the Moon map as a project of many months

Added 2026-11-18 from the owner's request. Status: `PLANNED`. See [`PHASE-STATUS.md`](PHASE-STATUS.md), row 162.

## The request

The owner, 2026-11-18: opening the Moon map should pick up the last session and carry on until the map is complete. The only thing that changes the project is a button that starts a new one. The Sky map and the Moon map should be tied together. A local copy of public Moon data should be explored. The aim is a map that is intuitive, easy, fun, and something to do over a long period.

The owner chose, the same day:
- **"Start map" means the Sky map.** Clicking the Moon there opens the Moon map, and the Sky map shows the project's progress.
- **All three kinds of Moon data:**
  - real imagery under the grid
  - the full IAU names
  - tonight's lighting drawn on the disc
- **All four ways of making it fun:**
  - captured tiles show the owner's own pictures
  - tonight's suggestion
  - a journal with milestones
  - a card for each section

## What is wrong with 162 as built

- **The active project is a guess.** `MoonMapController::Open` takes the newest file by modification time. PREV and NEXT make another project active without saving the choice. NEW, PREV and NEXT sit in the same row as GOTO and START.
- **Nothing says where the last session stopped**, or what to do tonight.
- **The disc is flat squares.** It has no surface and no names, and only about 70 features are stored (`LunarFeatures.h`).
- **The Sky map's Moon knows nothing of the project.** It is a 12 px disc with an unclipped fake shadow (`SkyMapWindow.h`, the Moon block).
- **Nothing shows progress across nights:** no pictures of the owner's own captures, no journal, no milestones.

## Stages

Each stage is its own branch, version and PR, and is useful on its own.

### 162c: always resume; a new project only by a confirmed button

- **The active project is stored.** `moonmaps\active.txt` names its file. `Open()` reads it, and falls back to the newest file only when it is missing. Only `NewProject` writes it.
- **PREV and NEXT become a read-only archive view.** The window draws the archived project, but GOTO, MARK, CLEAR and START act only on the active one. They are refused while an archived project is on screen ("viewing an archived map").
- **NEW asks to be confirmed in the window** before it acts: "Start a new map? 'Moon 2026-11-02' (31 of 84) is kept in the archive."
  - When the active map is complete, the header offers NEW PROJECT as the next thing to do.
- **The layout:** the main row is GOTO, MARK, CLEAR, START. A header strip holds NEW PROJECT and ARCHIVE ◂ ▸.
  - Scenario presses keep their names; `CONFIRM` is added.
- **A resume banner on opening:** "Night 5 of *Moon 2026-11-02*, 31 of 84, 12 ready now → START".
- **Gate (`tests/moon_map_run.cpp`):**
  - a reopen keeps the active project after another file is touched
  - the archive view does not change it, and START is refused there
  - NEW without CONFIRM does nothing

### 162d: the Moon's data, kept locally

Both downloads are asked of the owner first, with the source and the size.
- **Imagery:** the LRO WAC map from NASA SVS's CGI Moon Kit (public domain), the 4k equirectangular version, as an 8-bit grey PNG: `data/moon_wac_4k.png`.
- **Names:** the IAU/USGS Gazetteer of Planetary Nomenclature for the Moon (public domain), about 9,000 approved features, cut to `name,lat,lon,diameter_km,type`: `data/moon_gazetteer.csv`.
- Each file has a `SOURCE.txt` giving its URL, the date it was read and its licence.
- **Copying:** a `copy_moon_data` target in CMake, as `copy_knowledge_db`. The app finds the files through `Paths::Data()`.
- **`src/sky/LunarGazetteer.h`:**
  - It reads the CSV once.
  - `FeaturesIn(section)` lists the biggest features first. `Nearest(lat, lon)` and `Search(name)` complete it.
  - With no CSV it falls back to `kLunarFeatures`, which stays the GoTo panel's shortlist.
- **`src/gui/WicImage.h`:** a WIC PNG/JPEG decoder. The app encodes with WIC today and decodes nothing.
- **Gate (`tests/lunar_gazetteer.cpp`):**
  - Copernicus is in the right section on a 4 x 4 grid
  - quoted names parse
  - the fallback works when the file is absent

### 162e: tonight's Moon, with the owner's own captures on it

- **`src/sky/MoonRender.h`** (pure) draws the disc in the grid's frame: zero libration, north up, east right, the same frame as `MoonSection`.
  - **Brightness:** the WAC albedo at each pixel's latitude and longitude, times the body normal dotted with `LunarView::sunDir`, with a soft terminator.
  - **Past the limb tonight:** dimmed.
  - **Speed:** the per-pixel latitude, longitude and normal table is built once per size. The shading is redone on the existing 30 s sky refresh.
- **The window draws it under the grid.** The cells become tints and outlines: green for captured, an amber edge for ready now, dim for unlit. In night vision the states stay apart by brightness.
- **Zoom and pan** with the wheel and a drag. Gazetteer names appear once cells are big enough.
- **Tiles.** When a section is credited, a background job builds its tile:
  - it opens the SER (`SerReader.h`)
  - it takes the best frame by `SerScan::ScoreFrame`
  - it scales that frame to 256 px and writes `moonmaps\<project>\tiles\<section>.png` with the WIC encoder already used for snapshots
  - the capture line gains an optional `tile=` field, which older readers pass over

  A captured cell shows its tile, so the map slowly becomes the owner's own picture. A section marked by hand keeps the WAC look and a check.
  - The camera's angle to lunar north is not known, so a tile is drawn centred and clipped to its cell.
  - A tile is a progress picture and not the mosaic. Stitching stays another program's job.
- **Gate (`tests/moon_render.cpp`):**
  - a full Moon lights everything visible, and a new Moon nothing
  - at colongitude 0 the terminator lies near longitude 0
  - the pixel-to-section mapping agrees with `BuildMoonSections`

### 162f: tonight's suggestion, the journal, milestones and the section card

- **`src/core/MoonMapJournal.h`** (pure) works from the captures alone, so nothing new is stored.
  - **Nights:** captures grouped by a gap of 12 h or more.
  - **Milestones:** the first capture, each quadrant complete, 25, 50 and 75%, and the map complete.
- **Tonight's suggestion:** first the "last chance" sections, which are lit now but will be past the terminator by tomorrow (`SectionSkyAt` on tomorrow's `LunarView`); then the best Sun angle (20 to 50 degrees, `LUNAR_LOW/HIGH_SUN_DEG`). The header shows the top three. START still takes every ready section, in serpentine order.
- **A milestone** gets a toast and a short highlight. READY FOR MOSAIC becomes a completion banner with the number of nights and the first and last dates.
- **JOURNAL** opens a list of nights, each with its date, the sections added and the milestones reached.
- **The section card** replaces the one-line footer. It shows:
  - the section's name, latitude and longitude, and its largest features
  - every capture: date, Sun height, frames, file
  - its light tonight
  - **the next good light**: the next 30 days searched at 6 h steps for the Sun at 20 to 50 degrees over the aim point with the section in view
- **Gate (`tests/moon_map_journal.cpp`):**
  - nights are grouped correctly
  - milestones come in the right order
  - the last-chance ranking is right against a fixed JD

### 162g: the Sky map and the Moon map, tied together

- **`SkyMapWindow`:**
  - `SetMoonMapProgress(captured, total, readyNow, name)` puts a progress ring on the Moon, and the tooltip says "Moon map: 31/84, 12 ready".
  - The Moon's click popup gains MOON MAP beside SLEW, through a new `m_onMoonMap` callback, on both sky maps.
  - The fake shadow is replaced by a small `MoonRender` image (the real phase and libration), turned by `poleAngleDeg`.
- **`MoonMapWindow`** shows the Moon's altitude, azimuth and setting time, and gains a SKY MAP button that brings the Sky map up centred on the Moon.
- **Scenario:** `SKYMAP:MOON` is a new scenario press, used by `tests/scenarios/skymap-moon.scn`.

## How each stage is checked

1. **Bench tests:** `tests\run-tests.bat`, with the new tests added to its table. `moon_map`, `moon_map_run` and `lunar_features` must still pass.
2. **Emulator:** the `build_emu` build, then `.\tests\run-scenarios.ps1 moon`. The scenarios are `moon-map.scn` and `moon-map-buttons.scn`, extended, and `skymap-moon.scn`, which is new. Between them they check:
   - a reopen resumes
   - NEW asks for CONFIRM
   - the archive view refuses START
   - a `shot` of the drawn disc with `RADUK_EMU_MOONMAP_FULL=1`
   - tiles written for the sections an emulator run captured
3. **Release build:** run from `build\Release` and looked at on the five-screen layout.
4. **Clear night:** GoTo a section and check that its tile and the features its card lists match the frame. `MOON_MAP_PROVEN_ON_SKY` stays false until then (clear-night item 60).
