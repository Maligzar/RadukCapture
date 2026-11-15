// Dash against a running RadukCapture (the emulator build, or the rig).
// Start it first: launch_emulator.bat. Tagged @live.
const { test, expect } = require('@playwright/test');
const { dashUrl, wsToken, blockInternet } = require('./dash');

test.describe('@live Dash drives RadukCapture', () => {
  test.skip(!wsToken(), 'no WsToken: set RADUK_WS_TOKEN or RADUK_RIG_INI');

  test('connects with the token and shows live telemetry', async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(wsToken()));
    await expect(page.locator('#rcHeaderStatus')).not.toHaveText('DISCONNECTED', { timeout: 20_000 });
    await expect(page.locator('#rcFPS')).toHaveText(/\d/, { timeout: 20_000 });
  });

  test('starts and stops a capture from the browser', async ({ page }) => {
    await page.goto(dashUrl(wsToken()));
    await expect(page.locator('#rcHeaderStatus')).not.toHaveText('DISCONNECTED', { timeout: 20_000 });
    const rec = page.locator('#rcRecBtn');
    await rec.click();
    await expect(rec).toHaveText(/STOP/i, { timeout: 15_000 });
    await page.waitForTimeout(3000);
    await rec.click();
    await expect(rec).toHaveText(/START/i, { timeout: 30_000 });
  });
});
