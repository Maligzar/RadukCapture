# Weather replies for `tests/sky_weather.cpp` (phase 165.0)

Captured 2026-11-19 from each service. They are for a public reference place, never the owner's site.

| File | Source | What it is |
|---|---|---|
| `open_meteo_current_greenwich.json` | [Open-Meteo](https://open-meteo.com/), `/v1/forecast?current=...&timezone=UTC` for 51.48 N, 0.0 (the Royal Observatory, Greenwich) | A real reply, unedited. |
| `open_meteo_daily_limit.json` | Open-Meteo | The real reply to an hourly request that hit the daily limit from a shared address, unedited. |
| `nws_alerts_none.json` | [NWS](https://www.weather.gov/documentation/services-web-api), `/alerts/active?point=` for a point in Washington, DC | A real reply with no alerts, unedited. |
| `nws_alerts_alaska.json` | NWS, `/alerts/active?area=AK` | A real reply with 13 active alerts, unedited. |
| `seventimer_astro_layout.json` | [7Timer!](https://www.7timer.info/doc.php?lang=en), ASTRO, JSON | **Not a real reply.** It copies the captured reply's layout exactly (tabs, `" : "`, the keys and their order), but the values were chosen by the test, to hit each class's ends, `-9999`, out-of-range values and a missing key. |

## Terms

- **Open-Meteo.** Its API data is licensed [CC BY 4.0](https://open-meteo.com/en/licence) ("Weather data by Open-Meteo.com"). The two replies are reproduced unchanged. Open-Meteo's terms also ask for a link to it wherever its data is shown, which Dash's weather card (165.4) has to carry.
- **NWS.** The National Weather Service is part of the US government, and its data is in the public domain. (Recalled, not checked on a page here.)
- **7Timer!** Its documentation states no terms for its data. So, as with IMCCE's Opale replies (phase 141.0), no reply is committed, and the file above holds the layout only.
