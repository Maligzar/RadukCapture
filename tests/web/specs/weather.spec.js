// Phase 165.4: Dash's weather. Needs no RadukCapture running: the app's
// telemetry is fed to the page, and any request to Open-Meteo is answered by
// the test, never the internet.
//   - No location, no app: no request, and the panel says what is missing.
//   - A location, no app: Dash asks Open-Meteo itself; a value the reply does
//     not give shows "--", never a guess.
//   - The app connected with a fresh reading: the panel shows the app's, and
//     Dash asks for nothing.
//   - The app offline (rig.ini Offline=1): nothing is asked, by the panel or
//     the forecast tab; the refresh button sends weather_refresh to the app.
const { test, expect } = require('@playwright/test');
const { dashUrl } = require('./dash');

// Records every request that tries to leave this PC. Open-Meteo is answered
// with `reply` (a function of the URL) when one is given; the rest refused.
async function network(page, reply) {
  const tried = [];
  await page.route('**/*', route => {
    const url = route.request().url();
    const local = url.startsWith('file:') || /^(wss?|https?):\/\/(localhost|127\.0\.0\.1|\[::1\])(:|\/)/.test(url);
    if (local) return route.continue();
    tried.push(url);
    if (reply && url.startsWith('https://api.open-meteo.com/')) {
      return route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify(reply(url)) });
    }
    return route.abort();
  });
  return tried;
}

// Dash's saved location, as its location dialog writes it. A public place.
async function withLocation(page) {
  await page.addInitScript(() => {
    localStorage.setItem('raduk_location_v1', JSON.stringify(
      { lat: 51.48, lon: 0.0, label: 'GREENWICH', tz: 'Europe/London', elev: '' }));
  });
}

// Open-Meteo's reply as Dash asks for it (Fahrenheit, mph).
const current = over => ({
  timezone: 'Europe/London',
  current: Object.assign({
    time: '2026-11-20T21:00', interval: 900, temperature_2m: 47.8, relative_humidity_2m: 82,
    wind_speed_10m: 2.7, wind_direction_10m: 342, wind_gusts_10m: 5.8, cloud_cover: 100,
    pressure_msl: 992.3, visibility: 10000, dew_point_2m: 42.6,
  }, over || {}),
  hourly: { time: [], cloud_cover: [] },
});

// The app's telemetry: its weather fields only (165.2's SiteWeather, in SI).
const appWx = over => Object.assign({
  rc_running: false,
  wx_configured: true, wx_fresh: true, wx_source: 'internal', wx_state: '',
  wx_age_min: 4, wx_fetched_unix: 1795200000,
  wx_cloud_pct: 100, wx_temp_c: 10.0, wx_humidity_pct: 82, wx_wind_ms: 4.4704, wx_gust_ms: null,
  wx_dew_c: 5.0, wx_msl_hpa: 1013.2, wx_seeing_7timer: null, wx_dew_risk: 'elevated',
  wx_verdict: '', wx_score: -1, net_offline: false,
}, over || {});

test.describe('Dash: the weather (165.4)', () => {
  test('no location and no app: nothing asked, and the panel says why', async ({ page }) => {
    const tried = await network(page);
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.goto(dashUrl(''));
    await expect(page.locator('#wxSourceNote')).toContainText('SET YOUR LOCATION');
    await expect(page.locator('#val-cloud')).toHaveText('--');
    await expect(page.locator('#val-temp')).toHaveText('--');
    await expect(page.locator('#val-aqi')).toHaveText('--');   // was a fixed "AQI 42"
    expect(await page.evaluate(() => window.apexLiveScore ?? null)).toBeNull();
    expect(tried, 'requests that tried to leave this PC').toEqual([]);
    expect(errors, 'script errors').toEqual([]);
  });

  test('a location: Dash asks Open-Meteo, and shows what it gave', async ({ page }) => {
    await withLocation(page);
    const tried = await network(page, () => current());
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.goto(dashUrl(''));
    await expect(page.locator('#val-cloud')).toHaveText('100%');
    await expect(page.locator('#val-temp')).toHaveText('48°F');
    await expect(page.locator('#wind-gust')).toHaveText('Gusts: 6 mph');
    await expect(page.locator('#wxSourceNote')).toContainText('OPEN-METEO');
    expect(tried.length).toBeGreaterThan(0);
    for (const u of tried) {
      expect(u.startsWith('https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0'),
             'only Open-Meteo, for the location set: ' + u).toBeTruthy();
    }
    expect(errors, 'script errors').toEqual([]);
  });

  test('a reply without a value: "--", not a guess', async ({ page }) => {
    await withLocation(page);
    await network(page, () => current({ cloud_cover: null, wind_gusts_10m: undefined }));
    await page.goto(dashUrl(''));
    await expect(page.locator('#val-temp')).toHaveText('48°F');
    await expect(page.locator('#val-cloud')).toHaveText('--');        // it showed 15%
    await expect(page.locator('#wind-gust')).toHaveText('Gusts: --');  // and 12 mph
    expect(await page.evaluate(() => window.apexLiveScore ?? null), 'no score without cloud').toBeNull();
  });

  test('the app connected: its reading, and nothing asked', async ({ page }) => {
    const tried = await network(page);
    await page.goto(dashUrl(''));
    await page.evaluate(d => rcHandleData(d), appWx());
    await expect(page.locator('#val-cloud')).toHaveText('100%');
    await expect(page.locator('#val-temp')).toHaveText('50°F');
    await expect(page.locator('#val-wind')).toHaveText('10 mph');
    await expect(page.locator('#wind-gust')).toHaveText('Gusts: --');
    await expect(page.locator('#wc-pressure')).toHaveText('29.92');
    await expect(page.locator('#wxSourceNote')).toHaveText('FROM RADUKCAPTURE (ITS OWN FETCH, 4 MIN)');
    await page.evaluate(d => rcHandleData(d), appWx({ wx_source: 'leoben', wx_fetched_unix: 1795200600 }));
    await expect(page.locator('#wxSourceNote')).toContainText('LEOBEN');
    expect(tried, 'requests that tried to leave this PC').toEqual([]);
  });

  test('the app offline: nothing asked anywhere, and refresh goes to the app', async ({ page }) => {
    await withLocation(page);
    const tried = await network(page, () => current());
    await page.goto(dashUrl(''));
    await expect(page.locator('#val-cloud')).toHaveText('100%');   // before the app: Dash's own
    tried.length = 0;
    await page.evaluate(d => rcHandleData(d), appWx({ wx_fresh: false, wx_state: 'offline', net_offline: true }));
    await page.evaluate(() => {
      window.__sent = [];
      rcWs = { readyState: WebSocket.OPEN, send() {}, close() {} };
      window.rcSendCmd = (cmd, arg) => window.__sent.push([cmd, arg]);
    });
    await page.locator('#wxRefresh').click();
    await expect(page.locator('#wxSourceNote')).toContainText('NO FORECAST OFFLINE');
    expect(await page.evaluate(() => window.__sent)).toEqual([['weather_refresh', '']]);
    await page.evaluate(() => loadForecast());
    await expect(page.locator('#fcastStrip')).toContainText('NO FORECAST OFFLINE');
    expect(tried, 'requests while the app is offline').toEqual([]);
    await page.evaluate(() => { rcWs = null; });
  });
});
