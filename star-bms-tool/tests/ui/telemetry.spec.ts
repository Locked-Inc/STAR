import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Telemetry Tab Tests
 * Tests reading and displaying battery telemetry data
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Telemetry Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Telemetry tab
    const telemetryTab = page.locator('nav.sidebar button:has-text("Telemetry")');
    await telemetryTab.click();
  });

  test.afterEach(async ({ page }) => {
    // Disconnect after each test
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should display telemetry tab', async ({ page }) => {
    const telemetrySection = page.locator('h2:has-text("Battery Telemetry")');
    await expect(telemetrySection).toBeVisible();
  });

  test('should read and display telemetry data', async ({ page }) => {
    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify voltage is displayed (mock data: 14800 mV = 14.8V)
    const voltageText = await page.locator('text=/Voltage.*14\\.8.*V/').count();
    expect(voltageText).toBeGreaterThan(0);

    // Verify current is displayed (mock data: -1500 mA = -1.5A)
    const currentText = await page.locator('text=/Current.*-1\\.5.*A/').count();
    expect(currentText).toBeGreaterThan(0);

    // Verify SOC is displayed (mock data: 75%)
    const socText = await page.locator('text=/SOC.*75%/').count();
    expect(socText).toBeGreaterThan(0);

    // Verify temperature is displayed (mock data: 25C)
    // Note: UI uses "C" not "°C" for temperature
    const tempText = await page.locator('text=/Temperature.*25.*C/').count();
    expect(tempText).toBeGreaterThan(0);
  });

  test('should show capacity information', async ({ page }) => {
    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify remaining capacity (mock: 2250 mAh = 2.25 Ah)
    const remainingText = await page.locator('text=/Remaining.*2\\.25.*Ah/i').count();
    expect(remainingText).toBeGreaterThan(0);

    // Verify full capacity (mock: 3000 mAh = 3.0 Ah)
    const fullText = await page.locator('text=/Full.*3\\.0.*Ah/i').count();
    expect(fullText).toBeGreaterThan(0);
  });

  test('should show cycle count', async ({ page }) => {
    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify cycle count (mock: 42)
    const cycleText = await page.locator('text=/Cycle.*42/i').count();
    expect(cycleText).toBeGreaterThan(0);
  });

  test('should show time to empty', async ({ page }) => {
    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify time to empty (mock: 90 minutes)
    const timeText = await page.locator('text=/Empty.*90.*min/i').count();
    expect(timeText).toBeGreaterThan(0);
  });

  test('should indicate not charging status', async ({ page }) => {
    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify charging status (mock: false)
    const chargingText = await page.locator('text=/Charging.*No/i').count();
    expect(chargingText).toBeGreaterThan(0);
  });

});
