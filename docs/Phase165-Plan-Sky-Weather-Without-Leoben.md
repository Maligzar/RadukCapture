# Phase 165: the sky's weather without Leoben

Planned 2026-11-19, at the owner's request: "remove the second machine Leoben dependency by pulling the Leoben skills for astroweather into RadukCapture."

## Why

RadukCapture gets its weather from Leoben (`GET /weather/current`, `src/net/LeobenClient.h`). Leoben runs on Elminster, a gaming PC that is off, asleep or busy as often as not. So on a night at the scope, the ADC's live air, the dew risk on a fade, the seeing forecast and the observation record's weather all depend on a second machine being up. When Leoben is down, the app runs on (it was built to treat Leoben's absence as ordinary). But it runs blind to the weather, and nothing at the scope can fix that.

This phase moves the astroweather into RadukCapture itself, so the imaging PC fetches and scores the sky on its own. When it is done, Leoben is an optional extra and no longer a source the app relies on.

## Two rules this phase must keep

- **Rule 7 (airplane mode).** Nothing ever *needs* the internet. Moving the weather in means the imaging PC asks Open-Meteo, 7Timer and NWS directly. Today it asks Leoben on the LAN, and Leoben asks the internet. So every fetch goes through `NetPolicy`, the same way `SatDesk::HttpGet` and SNTP do:
  - `Offline=1` means no request is ever made;
  - a name that fails to resolve is backed off;
  - without the internet, the app shows the last reading with its age, then "no forecast offline", and nothing waits on it.

  What the camera measures (`SeeingMonitor`, `FadeWatch`) stays the authority on the sky. A forecast only advises.
- **Phase 149 (no rig in code).** Leoben's scoring has the rig and the site written into it:
  - the owner's site: its coordinates, elevation, Bortle class and NWS forecast zone, as constants (deliberately not repeated here);
  - a "Celestron 8SE @ f/10 (2032mm) with heavy payload" penalty inside the wind score.

  The port takes the site from `RigConfig` (`siteLatDeg`, `siteLonDeg`, `siteElevationM`) and the focal length from the imaging train (149.0). No site, rig or camera constant is copied in.

## What Leoben does today, read from its source (2026-11-19, `maligzar/leoben` at `164566e`)

- **`/weather/current`** (`routers/anthem_astro.py`) returns `weather_live.get_current_weather_live()`. The values are:
  - one Open-Meteo `current` reading: cloud, humidity, wind, gust, temperature, dew point, sea-level pressure and visibility;
  - an imaging score from `weather.compute_score`;
  - a wind stability score from `compute_wind_stability_score`;
  - a dew risk from the temperature/dew-point spread: 4 °F or under is "warning", 8 °F or under is "elevated";
  - GO, CAUTION or NO-GO at score 75 and 55.

  Despite the module's header, the *current* reading is not an ensemble: 7Timer and GFS feed only the hourly forecast and the best-nights list.
- **The forecast side** (`weather.py`):
  - an hourly ensemble (Open-Meteo, GFS) and 7Timer's seeing and transparency;
  - Open-Meteo cloud layers (low, mid and high) with an imaging classification;
  - a wind-shear seeing estimate for when 7Timer is down;
  - the best imaging nights;
  - NWS hourly and active alerts (US only).
- **Elsewhere:** `dust_storm.py` gives a 0 to 5 dust category from visibility, gusts and NWS dust warnings. `sky_ledger.py` records forecast against actual per night, and needs SQLite.

### What reading it found

1. **(tested) On a clear night, `/weather/current` most likely returns 503.** `fetch_current_weather` writes visibility as the string `">10 mi"` when Open-Meteo reports 16 km or more. `get_current_weather_live` then does `float(current["visibility"].split()[0])`, which is `float(">10")`. That raises (checked here: `ValueError: could not convert string to float: '>10'`), the exception is caught, the snapshot is `None`, and the endpoint answers 503.
   - **Inferred:** in desert air, visibility of 16 km or more is the usual case, so Leoben's weather is likely missing most of exactly the nights that matter. This would explain readings that are rarely fresh.
   - **Confirm on Leoben** before relying on this: `curl` its `/weather/current` on a clear evening.
   - This is Leoben's bug to fix in its own repository, whatever this phase does.
2. **Phase 138's three keys were never sent.** Leoben's source has no `seeing_7timer`, `wind_200hpa` or `wind_250hpa`. So `SeeingTally`'s hourly SEEING line has said "none from Leoben" every night since 3.1.1, and the forecast-against-measured comparison Phase 138 set up has no data. This phase fills it.
3. **Missing values are made up.** Every field missing from the Open-Meteo reply is filled with a plausible guess:
   - cloud 15%, humidity 22%, wind 8 mph, gust 12 mph, temperature 62 °F, dew point 28 °F, pressure 1013 hPa, visibility 16 km;
   - 7Timer seeing and transparency 4.

   A reply with no cloud field scores as a fairly clear sky. The port treats a missing value as absent, never as a default.
4. **The score is tuned for deep sky, not planets.**
   - The Moon costs up to 40 points whether or not it is up. That matters for a nebula but means little for Jupiter.
   - Seeing enters only as a surface-wind proxy (`max(0, wind - 8) * 0.8`). The jet stream, which is what usually limits planetary detail, is not used at all.
   - RadukCapture is planetary until 4.0, so the score is re-derived here (165.3), not copied.
6. **(tested) Leoben's 7Timer seeing is always the fallback.** `fetch_seeing_forecast` reads `pt.get("seeing2", 4)`. 7Timer's ASTRO JSON has no `seeing2`: a reply captured 2026-11-19 has the key `seeing` on all 24 timepoints, and no `seeing2`. So every night's seeing score has been class 4 (1" to 1.25"), whatever 7Timer forecast.
7. **(sourced) Leoben reads 7Timer's wind class as knots.** In ASTRO's output `wind10m.speed` is a class from 1 to 8, not a speed. [7Timer's documentation](https://www.7timer.info/doc.php?lang=en) defines it: "1, Below 0.3m/s (calm); 2, 0.3-3.4m/s (light); 3, 3.4-8.0m/s (moderate)", up to "8, Over 32.6m/s (hurricane)". Leoben multiplies the class by 1.15078 as if it were knots, so a gale (class 6) reads as about 7 mph. The same page gives seeing classes 1 to 8 as `<0.5"` to `>2.5"`, transparency 1 to 8 as `<0.3` to `>1` mag per air mass, cloud cover 1 to 9 as `0%-6%` to `94%-100%`, relative humidity -4 to 16 in 5% steps, and `-9999` as undefined. 165.0's tables are taken from that page.
5. **Copied as-is, it would sit outside the rules.** The rig and site are fixed, `datetime.now()` is local time, and 7Timer's "local" hour is a hard-coded UTC offset.

## The plan

Each stage is its own PR and bumps PATCH.

### 165.0 The pure core: `src/core/SkyWeather.h`

This stage is pure, has no I/O, and is tested with g++.

- **Parsers**, for the replies saved as files under `tests/data/sky_weather/`:
  - Open-Meteo `current` and `hourly`, including the cloud layers and the 200 and 250 hPa winds (Phase 138's keys);
  - 7Timer `astro`: seeing and transparency classes, by UTC timepoint;
  - NWS `alerts/active?point=`.

  These are **real replies, captured once from the services and committed**, never written by hand. A missing field stays absent.
- **SI units inside.** Leoben's °F, mph and inHg become conversions at the edge only. `StationPressureHpa` (the ADC's pressure trap, already tested) is reused.
- **Ported and parameterised:**
  - **Dew risk.** Leoben's 4 °F and 8 °F spreads become 2.2 °C and 4.4 °C. They are `UNVERIFIED(item N)`, against the dew on the corrector on a real night.
  - **Wind tiers.** Leoben's sustained and gust bands are kept. The f/10 and payload deductions become a function of the train's focal length, with no "8SE" anywhere. Tagged UNVERIFIED.
  - **Cloud-layer classification.** It is not tied to the rig, so it ports as it is.
  - **The 7Timer tables**, from 7Timer's own documentation rather than Leoben's code: seeing, transparency, cloud cover, relative humidity and the wind class. `-9999` is absent.
  - **The wind-shear seeing estimate**, used only when 7Timer is absent, and always labelled as an estimate.
- **Dust.** The 0 to 5 dust category from visibility and gusts. NWS dust-warning floors apply only where NWS covers the site (inside the US); elsewhere the category comes from visibility and wind alone.
- **Left in Leoben:** `sky_ledger` (SQLite, forecast against actual) and the multi-location weather. Phase 138's `SeeingTally` already keeps the measured-against-forecast record in the session log, which is the comparison that matters here.

### 165.1 The fetcher: `src/net/SkyWeatherClient.h`

- **One background thread with the same timing as `LeobenClient`:** current conditions every 10 minutes, every 2 after a failure, and the hourly forecast every 30.
- **Every request goes through `SatDesk::HttpGet` under `NetPolicy`.** HTTPS only: 7Timer moved to a 302 redirect (Phase 138), so the client follows it to HTTPS or treats 7Timer as absent.
- **No request without a site.** With no site in `rig.ini` there is no request: the app does not ask for latitude 0, longitude 0. That is the mistake 151.4 found in Dash.
- **A disk cache** (`weather_cache.json` beside the session data) holds the last good reading and forecast with their times. After a restart offline, the app has something to show, with its age.
- **The struct is renamed and the consumers keep their shape.** `LeobenConditions` becomes `SkyConditions`, with a `source` field: `internal`, `leoben` or `cache`. The ADC, `FadeWatch`, `SeeingTally`, the observation record, the status window and the AI context read it as before. Only `wxSource` changes value.

### 165.2 Choosing the source, and Leoben's retirement

- **`rig.ini WeatherSource=`** takes four values:
  - `internal` (the default for a new rig);
  - `leoben` (today's behaviour);
  - `both`: internal first, with Leoben as a LAN fallback;
  - `off`.

  An existing `rig.ini` with a `LeobenUrl` starts on `both`, so nobody loses weather on upgrade.
- **A parity check, for about five nights on `both`.** Every 10 minutes, the session log writes a `WXCMP` line with both readings side by side, from which the AI context and the owner can judge. It covers cloud, temperature, dew point, pressure, wind and the dew risk.
- **Once those nights agree** (or the differences are explained, as Leoben's 503s would be), Leoben becomes a choice the owner can still make, not one the app ever needs. `LeobenClient` is removed in a later release, by the owner's decision.

### 165.3 A planetary sky verdict

This replaces Leoben's deep-sky score, for the planetary use.

- **Inputs:**
  - cloud: total, and by layer;
  - the jet stream: Open-Meteo 200 and 250 hPa wind, with Phase 138's 126 km/h line as the first threshold;
  - 7Timer seeing, when present;
  - wind on the tube, through 165.0's tiers;
  - dew risk;
  - the dust category.
- **The Moon counts only for the Moon project's own sessions** (Phase 162), and as a target, not a penalty. A deep-sky score with the Moon's penalty returns with deep sky after 4.0.
- **Every weight starts `UNVERIFIED`.** `SeeingTally`'s nightly pairing of forecast and measured seeing is what tunes them, and after about ten nights the log says which input, if any, predicts this site's seeing. The verdict is advice: Autorun still stops on the measured sky (fade, lost target, cloud on the frame), never on a forecast.

### 165.4 Dash and the status window

- **Dash stops fetching weather itself.** It reads `SkyConditions` from the app's telemetry. With that, `api.open-meteo.com` disappears from Dash, 151.4 is closed, and `tests/web/specs/offline.spec.js` goes green.
- **Dash gets a weather card:** the source, the age, the verdict and the dew risk. A `weather_refresh` command forces a fetch, with Playwright specs for desktop and phone. This keeps to the owner's rule that every control at the telescope works from Dash and is tested through it.
- **The status window's weather line names the source and the age**, for example "internal, 4 min" or "cache, 3 h, offline".

## 165.0a and 165.1, as built (2026-11-20, 3.3.34)

- **165.0a, `src/core/SkyWeather.h`.** It parses Open-Meteo's current conditions, 7Timer's ASTRO forecast and NWS active alerts. Each reading is absent when it is missing, null, out of range or in an unknown unit, and set-aside readings are listed for the log. A reply that is not in UTC is refused. The tables are dew risk, wind tier, cloud layers and dust, plus 7Timer's classes taken from its documentation. The JSON reader moved unchanged from `sky/MutualEvents.h` to `core/MiniJson.h`.
- **165.1, `src/core/SkyWeatherClient.h`, not `src/net/` as planned.** The client is pure C++: the request is a function passed in, so it is tested whole under g++ (ASan, UBSan, TSan) rather than only on CI. In the app that function is `SatDesk::HttpGet`, wired in 165.2.
  - Cadence: Open-Meteo every 10 minutes, or 2 after a failure; 7Timer every 60, or 10 after a failure; NWS every 10, or 30 after a failure.
  - States: ok, not reachable, offline, not readable (with the service's reason), and no site.
  - A raw-reply cache is written by temporary file and rename, and read back through the same parsers.
  - Dew, wind and dust come from readings under 30 minutes old only.
- **Test data** is in `tests/data/sky_weather/`, with a README on its terms:
  - real Open-Meteo replies (CC BY 4.0) for Greenwich: a current reading, and its daily-limit error;
  - real NWS replies (public domain): none at a point, and 13 alerts in Alaska;
  - a 7Timer file that copies a real reply's layout with test values, because 7Timer states no terms.
- **Checklist items:** 62 for the dew spreads and 63 for the wind bands, both `UNVERIFIED`.
- **Owed:** Open-Meteo's hourly parser (cloud layers, the 200 and 250 hPa winds). Its capture hit the daily limit from a shared address on 2026-11-19, and the host reset connections on 2026-11-20. It is written once a real reply is captured.

## 165.2, as built (2026-11-20, 3.3.35)

- **`src/core/SiteWeather.h`** holds the choice and is pure.
  - `WeatherSource=` takes `internal`, `leoben`, `both` or `off`. Left unset, it is `both` when a `LeobenUrl` is set and `internal` otherwise; text it does not understand gets the default, and the log says so.
  - `FromSky` and `FromLeoben` turn either source into one `SiteWeather`: optional fields, the source, and the reason when nothing is fresh.
  - `PickWeather` chooses. In `both`, internal wins when it is fresh and Leoben stands in when it is not; with neither fresh, both reasons are shown.
  - It also writes the status row, the AI context line and the `WXCMP` line.
- **`LeobenConditions` was not renamed** to `SkyConditions`, as the plan had it. Both sources map into `SiteWeather` instead, which leaves Leoben's parser and its tests untouched.
- **`main.cpp`.** Leoben's client starts only in `leoben` or `both`, and `SkyWeatherClient` only in `internal` or `both`, fetching through `SatDesk::HttpGet`. The cache is `weather_cache.txt` under the data root. Each consumer now reads `weatherNow()`:
  - the ADC's live air, set only when both pressure and temperature are present;
  - the dew risk in the fade warning;
  - the SEEING forecast: 7Timer's class while it covers the hour, so Phase 138's pairing finally gets data;
  - the observation record, which writes only the fields given;
  - the status `WX` row, which now names the source and shows the reading's freshness where there is no score;
  - the AI context.

  In `both`, a `WXCMP` line goes to the session log every 10 minutes.
- **`SeeingTally`** says "none given" rather than "none from Leoben".
- **Checks:** `tests/site_weather.cpp` (40 checks). CI builds `main.cpp` and `StatusWindow.h` but cannot run them; the run on the PC and five parity nights are checklist item 64.

## 165.4, as built (2026-11-20, 3.3.36)

Built ahead of 165.3: the verdict's jet-stream input waits on Open-Meteo's hourly reply, while Dash's fetch broke rule 7 today.

- **Telemetry** gains the weather the app uses: `wx_configured`, `wx_fresh`, `wx_source`, `wx_state`, `wx_age_min`, `wx_fetched_unix`, the readings in SI (null when not given), `wx_dew_risk`, `wx_verdict`, `wx_score` and `net_offline` (rig.ini `Offline=1`). There are no coordinates. `telemetry_night` checks the fields.
- **`weather_refresh`** calls `SkyWeatherClient::RefreshNow()`, so `Offline=1` still asks nothing.
- **Dash:**
  - **The gate.** `dashMayFetch()` allows an outside request only with a location set and live telemetry not saying `net_offline`. It covers the conditions panel, the forecast tab (Open-Meteo, GFS, 7Timer) and the address lookups (OpenStreetMap). The lookups are asked only on request, and never while offline.
  - **The app's reading.** While the app is connected with a fresh reading, the conditions panel shows it, with its source, and fetches nothing.
  - **Missing values** show `--`. Gone: the defaults (15% cloud, 22% humidity, 8 mph wind, 62 F, a 28 F dew point), the made-up score after a failed fetch, the fixed AQI 42, transparency 8.2/10 and seeing 2.8", the score 50 with no reading, and the hourly fallback's guessed values.
  - **A bug the old fallback hid.** `renderConditions` wrote to an `adcAlt` element the page does not have, so whenever Jupiter was up the old code threw inside its `try`, and the catch then showed made-up conditions.
  - **Open-Meteo** is credited under the panel, as its CC BY 4.0 terms ask.
- **Checks:** `specs/weather.spec.js` (5 cases) and `offline.spec.js` pass on desktop and phone, as do the existing no-app specs (16 of 16).
- **Still made up in Dash, owed:** the NASA events and FAA notices panels are fixed lists, including "AQI 42 — Good" and a nearby airport's notice. The wind stability score keeps Leoben's 8SE-specific deductions (phase 149's rule: the rig belongs in rig.ini).
- **Leoben serves a copy of Dash** (its Phase 95, `third_party/radukcapture/`, copied byte for byte). That copy changes only when Leoben re-syncs it.

## Checks

- **g++, with ASan and UBSan:**
  - `tests/sky_weather.cpp` covers the parsers on the captured replies, missing fields staying absent, the unit edges, the dew, wind and cloud-layer tables against Leoben's own values (converted), the dust floors, and a "16 km or more" visibility parsing as a number;
  - `tests/leoben_weather.cpp` keeps passing while Leoben remains a source.
- **CI (Windows):**
  - `tests/sky_weather_client.cpp` runs with `NetPolicy` offline: no request leaves, the cache is read, and the source and age are right. No test touches the internet.
  - `unverified_report --check` passes with the new tags.
- **Bench:** the emulator with `WeatherSource=internal` and `Offline=1`. The status line reads "offline", the ADC is on the standard atmosphere, the scenarios pass, and Dash's offline spec passes.
- **Nights:** 165.2's parity lines, then `SeeingTally` pairs for 165.3. These are added to `CLEAR-NIGHT-CHECKLIST.md` as new items.

## Order and size

| Stage | Size | Needs |
|---|---|---|
| 165.0 | M | Real replies captured once, for a public reference location, never the owner's site. Open-Meteo `current`, 7Timer and NWS were captured 2026-11-19 (Greenwich, and a US point for NWS); Open-Meteo's hourly hit its daily limit from the shared address and waits a day |
| 165.1 | S-M | 165.0 |
| 165.2 | S | 165.1, then about five nights on `both` |
| 165.3 | S, then nights | 165.0; ten nights of `SeeingTally` pairs to tune |
| 165.4 | S | 165.1; closes 151.4 |

It suggests Release 3.4, because it takes a second machine out of the night's failure paths. 165.3's tuning carries into 3.5, since it is planetary seeing. Which release it belongs to is the owner's decision.

## Not in this phase

- **Leoben's other skills**: chat, the assistant, the night's history UI and the 10-night forecast tab. The forecast tab could follow later from 165.0's hourly data, if wanted.
- **Changes to Leoben.** Its visibility bug (finding 1) and the 138 keys (finding 2) are reported here for Leoben's own repository and are not fixed from this one.
- **A local weather station or a dew sensor on the scope.** That would be the measured answer to dew and wind, and is a separate phase if the hardware arrives.
