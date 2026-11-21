// Phase 156.2a: Dash's SKY RUN card. The app runs the night (core/SkyRun.h,
// app/SkyRunController.h); Dash starts it, shows the plan with each group's
// status, the step now running and why it stopped, and STOP parks. Needs no
// RadukCapture running: the page is fed the app's telemetry and its sender is
// recorded. The live witness of a whole night is a later, @live spec.
const { test, expect } = require('@playwright/test');
const { dashUrl, blockInternet } = require('./dash');

const base = {
  rc_running: true, park_state: 'idle', park_set: true, park_reason: '',
  skyrun_phase: 'idle', skyrun_step: '', skyrun_refusals: '', skyrun_rows: '', skyrun_reason: '',
};

test.describe('Dash: the SKY RUN card', () => {
  test.beforeEach(async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(''));
    await expect(page.locator('#rcHeaderStatus')).toHaveText('DISCONNECTED');
    await page.locator('#tab-fc').click();
    await page.evaluate(() => {
      window.__sent = [];
      window.rcSendCmd = (cmd, arg) => window.__sent.push([cmd, arg]);
    });
  });

  test('hidden for an app that has no sky run', async ({ page }) => {
    const older = { ...base };
    for (const k of Object.keys(older)) if (k.startsWith('skyrun_')) delete older[k];
    await page.evaluate(d => rcHandleData(d), older);
    await expect(page.locator('#rcSkyRunSection')).toBeHidden();
  });

  test('idle: SKY RUN sends skyrun_start all; STOP is off', async ({ page }) => {
    await page.evaluate(d => rcHandleData(d), base);
    await expect(page.locator('#rcSkyRunSection')).toBeVisible();
    await expect(page.locator('#rcSkyRunState')).toHaveText('not started');
    await expect(page.locator('#rcSkyRunStop')).toBeDisabled();
    await page.locator('#rcSkyRunStart').click();
    expect(await page.evaluate(() => window.__sent)).toEqual([['skyrun_start', 'all']]);
  });

  test('refused: every reason is shown', async ({ page }) => {
    await page.evaluate(d => rcHandleData(d), { ...base, skyrun_phase: 'refused',
      skyrun_refusals: 'the park position is not set (rig.ini SkyParkAltAz); the mount is not aligned' });
    await expect(page.locator('#rcSkyRunRefusals')).toContainText('SkyParkAltAz');
    await expect(page.locator('#rcSkyRunRefusals')).toContainText('not aligned');
  });

  test('running: the step, the plan with statuses, and STOP sends skyrun_stop', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.evaluate(d => rcHandleData(d), { ...base, skyrun_phase: 'running',
      skyrun_step: 'jog: re-centre within 25 s',
      skyrun_rows: 'tracking|Saturn|running|;planet|Saturn|not-available|its steps are not built yet;solve|Vega|skipped-horizon|<b>under the trees</b>' });
    await expect(page.locator('#rcSkyRunState')).toHaveText('running');
    await expect(page.locator('#rcSkyRunStep')).toHaveText('jog: re-centre within 25 s');
    const rows = page.locator('#rcSkyRunRows .rc-skyrun-row');
    await expect(rows).toHaveCount(3);
    await expect(rows.nth(0)).toHaveText('tracking on Saturn: running');
    await expect(rows.nth(1)).toContainText('not built yet');
    await expect(rows.nth(2)).toContainText('<b>under the trees</b>');   // shown as text, never as HTML
    await expect(page.locator('#rcSkyRunStart')).toBeDisabled();
    await page.locator('#rcSkyRunStop').click();
    expect(await page.evaluate(() => window.__sent)).toEqual([['skyrun_stop', '']]);
    expect(errors, 'script errors').toEqual([]);
  });

  test('done: why it stopped, and it can start again', async ({ page }) => {
    await page.evaluate(d => rcHandleData(d), { ...base, skyrun_phase: 'done',
      skyrun_reason: 'the dark window ended', skyrun_rows: 'tracking|Saturn|passed|' });
    await expect(page.locator('#rcSkyRunState')).toHaveText('done: the dark window ended');
    await expect(page.locator('#rcSkyRunRows')).toContainText('tracking on Saturn: passed');
    await expect(page.locator('#rcSkyRunStart')).toBeEnabled();
    await expect(page.locator('#rcSkyRunStop')).toBeDisabled();
  });
});
