import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * End-to-End Workflow Tests
 * Tests the complete user journey through the application
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Complete E2E Workflow', () => {
  test('should complete full BMS testing workflow', async ({ page }) => {
    // Step 1: Setup mocks and navigate to application
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Step 2: Connect to device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);

    const connectButton = page.locator('button:has-text("Connect")');
    await connectButton.click();
    await page.waitForTimeout(1000);

    // Verify connected
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    await expect(disconnectButton).toBeVisible();

    // Step 3: Read telemetry data
    const telemetryTab = page.locator('nav.sidebar button:has-text("Telemetry")');
    await telemetryTab.click();

    await expect(page.locator('.telemetry-grid')).toBeVisible();

    // Verify telemetry data
    const voltageText = await page.locator('text=/Voltage.*14\\.8.*V/').count();
    expect(voltageText).toBeGreaterThan(0);

    // Step 4: Read cell voltages
    const cellTab = page.locator('button:has-text("Cell Voltages")');
    await cellTab.click();

    await expect(page.locator('.cell-grid')).toBeVisible();

    // Verify cell data
    const cell1Text = await page.locator('text=/Cell 1.*3\\.7/i').count();
    expect(cell1Text).toBeGreaterThan(0);

    // Step 5: Read device info
    const infoTab = page.locator('nav.sidebar button.nav-item:has-text("Device Info")');
    await infoTab.click();

    await expect(page.locator('.device-info')).toBeVisible();

    // Verify device info
    const mfgText = await page.locator('text=/Texas Instruments/i').count();
    expect(mfgText).toBeGreaterThan(0);

    // Step 6: Read a register
    const registersTab = page.locator('button:has-text("Registers")');
    await registersTab.click();

    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await addressInput.fill('0');

    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    await readNumBytes.fill('2');

    const readRegButton = page.locator('button:has-text("Read Register")');
    await readRegButton.click();
    await page.waitForTimeout(2000);

    // Verify register value
    const valueText = await page.locator('text=/Value/i').count();
    expect(valueText).toBeGreaterThan(0);

    // Step 7: Write a register
    const writeAddressInput = page.locator('input[placeholder*="0x10"]').first();
    await writeAddressInput.fill('0x10');

    const writeValueInput = page.locator('input[placeholder*="0xFF"]').first();
    await writeValueInput.fill('0xFF');

    const writeNumBytes = numBytesInputs.last();
    await writeNumBytes.fill('1');

    const writeButton = page.locator('button:has-text("Write Register")');
    await writeButton.click();
    await page.waitForTimeout(2000);

    // Step 8: Return to telemetry and verify data persists
    await telemetryTab.click();

    // Data should still be visible from earlier read
    const voltageStillVisible = await page.locator('text=/Voltage.*14\\.8.*V/').count();
    expect(voltageStillVisible).toBeGreaterThan(0);

    // Step 9: Disconnect
    await disconnectButton.click();

    // Verify connect button reappears
    await expect(connectButton).toBeVisible();

    // Port input should be enabled again
    await expect(portSelect).toBeEnabled();
  });

  test('should handle rapid tab switching', async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Rapidly switch tabs
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Device Info")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Registers")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();

    // Should still be functional
    await expect(page.locator('h2:has-text("Battery Telemetry")')).toBeVisible();

    // Disconnect
    await page.locator('button:has-text("Disconnect")').click();
  });

  test('should maintain connection across tab switches', async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Switch tabs and verify disconnect button is always visible
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await expect(page.locator('button:has-text("Disconnect")')).toBeVisible();

    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await expect(page.locator('button:has-text("Disconnect")')).toBeVisible();

    await page.locator('nav.sidebar button:has-text("Device Info")').click();
    await expect(page.locator('button:has-text("Disconnect")')).toBeVisible();

    await page.locator('nav.sidebar button:has-text("Registers")').click();
    await expect(page.locator('button:has-text("Disconnect")')).toBeVisible();

    // Disconnect
    await page.locator('button:has-text("Disconnect")').click();
  });
});
