// Dash with no internet (rule 7, phase 151). Needs no RadukCapture running.
const { test, expect } = require('@playwright/test');
const { dashUrl, blockInternet } = require('./dash');

test('Dash loads and works with no internet connection', async ({ page }) => {
  const tried = await blockInternet(page);
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.goto(dashUrl(''));
  await page.waitForTimeout(2000);
  // The remote panel's tab may be closed; the page is up when its status reads.
  await expect(page.locator('#rcHeaderStatus')).toHaveText('DISCONNECTED');
  expect(errors, 'script errors').toEqual([]);
  // Found 2026-11-15 by this test: Dash fetched a weather forecast from
  // api.open-meteo.com for latitude 0, longitude 0. Fixed in 165.4 (closing
  // 151.4): with no location set, Dash asks nothing (specs/weather.spec.js).
  expect(tried, 'requests that tried to leave this PC').toEqual([]);
});
