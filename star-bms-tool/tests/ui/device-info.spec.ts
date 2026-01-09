import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Device Info Tab Tests
 * Tests reading and displaying BMS device information
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Device Info Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Device Info tab
    const infoTab = page.locator('nav.sidebar button.nav-item:has-text("Device Info")');
    await infoTab.click();
  });

  test.afterEach(async ({ page }) => {
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should display device info tab', async ({ page }) => {
    const infoSection = page.locator('h2:has-text("Device Information")');
    await expect(infoSection).toBeVisible();
  });

  test('should read and display manufacturer', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify manufacturer (mock: "Texas Instruments")
    const mfgText = await page.locator('text=/Manufacturer.*Texas Instruments/i').count();
    expect(mfgText).toBeGreaterThan(0);
  });

  test('should display device name', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify device name (mock: "BQ78350-R1A")
    const nameText = await page.locator('text=/Device.*BQ78350-R1A/i').count();
    expect(nameText).toBeGreaterThan(0);
  });

  test('should display chemistry type', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify chemistry (mock: "LION")
    const chemText = await page.locator('text=/Chemistry.*LION/i').count();
    expect(chemText).toBeGreaterThan(0);
  });

  test('should display serial number', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify serial number (mock: 0x12345678)
    const serialText = await page.locator('text=/Serial/i').count();
    expect(serialText).toBeGreaterThan(0);
  });

  test('should display firmware version', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify firmware version (mock: "v1.2.3")
    const fwText = await page.locator('text=/Firmware.*v1\\.2\\.3/i').count();
    expect(fwText).toBeGreaterThan(0);
  });

  test('should display hardware version', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify hardware version (mock: "v0.1")
    const hwText = await page.locator('text=/Hardware.*v0\\.1/i').count();
    expect(hwText).toBeGreaterThan(0);
  });

  test('should display design capacity', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify design capacity (mock: 3200 mAh = 3.2 Ah)
    const capText = await page.locator('text=/Design.*Capacity.*3\\.2.*Ah/i').count();
    expect(capText).toBeGreaterThan(0);
  });

  test('should display design voltage', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify design voltage (mock: 14800 mV = 14.8 V)
    const voltText = await page.locator('text=/Design.*Voltage.*14\\.8.*V/i').count();
    expect(voltText).toBeGreaterThan(0);
  });

  test('should display number of cells', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Verify num cells (mock: 4)
    const cellsText = await page.locator('text=/Cells.*4/i').count();
    expect(cellsText).toBeGreaterThan(0);
  });

  test('should persist data after tab switch', async ({ page }) => {
    await expect(page.locator('.device-info')).toBeVisible();

    // Switch to another tab
    const telemetryTab = page.locator('button:has-text("Telemetry")');
    await telemetryTab.click();
    await page.waitForTimeout(500);

    // Switch back to device info
    const infoTab = page.locator('nav.sidebar button.nav-item:has-text("Device Info")');
    await infoTab.click();
    await page.waitForTimeout(500);

    // Data should still be visible
    const mfgText = await page.locator('text=/Manufacturer.*Texas Instruments/i').count();
    expect(mfgText).toBeGreaterThan(0);
  });
});
