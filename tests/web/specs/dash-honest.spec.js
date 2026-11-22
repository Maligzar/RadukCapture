// Phase 166.3: Dash shows only what the app knows (the owner's rule,
// 2026-11-21: the app is the instrument, Dash a remote). Dash once carried
// fixed lists made up in the page (an ISS pass "tonight", "no active TFRs",
// a nearby airfield's strobes), status lights for sources nothing asked,
// and a wind score with deductions written for one telescope. None of that
// may come back. Needs no RadukCapture running.
const { test, expect } = require('@playwright/test');
const { dashUrl, blockInternet } = require('./dash');

test.describe('Dash: nothing made up in the page', () => {
  test('no invented panels, status lights or one-telescope wording', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await blockInternet(page);
    await page.goto(dashUrl(''));
    await expect(page.locator('#rcHeaderStatus')).toHaveText('DISCONNECTED');
    const text = await page.locator('body').innerText();
    for (const made of ['NASA DONKI', 'FAA NOTAM', 'STELLARIUM', 'ISS Pass', 'TFR', 'DEER VALLEY', '8SE', 'f/10',
                        'STAR MACHINE PROTOCOL', 'CELESTRON'])
      expect(text, `"${made}" is in the page`).not.toContain(made);
    expect(await page.locator('#nasaEvents, #faaNotices').count()).toBe(0);
    expect(errors, 'script errors').toEqual([]);
  });

  test('the wind score has no focal-length or payload deduction, and no reading means no score', async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(''));
    const calm = await page.evaluate(() => computeWindStabilityScore(3, 5));
    expect(calm.score).toBe(100);   // it used to be 75: -15 for f/10 and -10 for the payload
    expect(calm.focalDeduct).toBeUndefined();
    expect(calm.payloadDeduct).toBeUndefined();
    const gusty = await page.evaluate(() => computeWindStabilityScore(8, 20));
    expect(gusty.gustGapDeduct).toBe(15);
  });
});
