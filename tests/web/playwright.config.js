// Playwright for Dash (RadukCaptureDashboard.html). See docs/TESTING.md.
// Two projects: the capture PC's own browser, and a phone held outside.
const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './specs',
  timeout: 60_000,
  retries: 0,
  // One at a time: the live specs share one running RadukCapture.
  workers: 1,
  reporter: [['list'], ['json', { outputFile: 'results/dash-results.json' }]],
  use: { trace: 'off', screenshot: 'only-on-failure' },
  projects: [
    { name: 'desktop', use: { ...devices['Desktop Chrome'] } },
    { name: 'phone',   use: { ...devices['Pixel 7'] } },
  ],
});
