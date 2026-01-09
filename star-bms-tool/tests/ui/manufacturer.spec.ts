import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Manufacturer Access Tab Tests
 * Tests BQ4050/BQ78350 manufacturer-specific commands
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Manufacturer Access Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Manufacturer Access tab
    const mfgTab = page.locator('nav.sidebar button:has-text("Manufacturer Access")');
    await mfgTab.click();
  });

  test.afterEach(async ({ page }) => {
    // Disconnect after each test
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should display manufacturer access tab', async ({ page }) => {
    const heading = page.locator('h2:has-text("Manufacturer Access Commands")');
    await expect(heading).toBeVisible();
  });

  test('should have preset command dropdown', async ({ page }) => {
    const dropdown = page.locator('select#mfg-preset');
    await expect(dropdown).toBeVisible();

    // Check that it has preset options
    const options = await dropdown.locator('option').count();
    expect(options).toBeGreaterThan(5); // At least 5 presets
  });

  test('should have Send Command button', async ({ page }) => {
    const sendButton = page.locator('button:has-text("Send Command")');
    await expect(sendButton).toBeVisible();
    await expect(sendButton).toBeEnabled();
  });

  test('should read Device Type command', async ({ page }) => {
    // Select Device Type preset (should be default)
    const dropdown = page.locator('select#mfg-preset');
    await dropdown.selectOption('device_type');

    // Send command
    const sendButton = page.locator('button:has-text("Send Command")');
    await sendButton.click();
    await page.waitForTimeout(2000);

    // Verify response is displayed
    const responseSection = page.locator('.mfg-result h4:has-text("Response")');
    await expect(responseSection).toBeVisible();

    // Verify subcommand code is shown (0x0001)
    const subcommandText = await page.locator('text=/Sub-command.*0x0001/i').count();
    expect(subcommandText).toBeGreaterThan(0);

    // Verify response data is shown
    const dataText = await page.locator('text=/Response Data.*0x50.*0x40/i').count();
    expect(dataText).toBeGreaterThan(0);
  });

  test('should read FET Status command', async ({ page }) => {
    // Select FET Status preset
    const dropdown = page.locator('select#mfg-preset');
    await dropdown.selectOption('fet_status');

    // Send command
    const sendButton = page.locator('button:has-text("Send Command")');
    await sendButton.click();
    await page.waitForTimeout(2000);

    // Verify response is displayed
    const responseSection = page.locator('.mfg-result h4:has-text("Response")');
    await expect(responseSection).toBeVisible();

    // Verify subcommand code (0x0023)
    const subcommandText = await page.locator('text=/Sub-command.*0x0023/i').count();
    expect(subcommandText).toBeGreaterThan(0);

    // Verify FET status decoding (both enabled = 0x03)
    // Look for the specific decoded text
    const fetDecoded = page.locator('.info-row:has-text("FET Status (decoded)")');
    await expect(fetDecoded).toBeVisible();
    const fetText = await fetDecoded.textContent();
    expect(fetText).toContain('CHG: Enabled');
    expect(fetText).toContain('DSG: Enabled');
  });

  test('should allow custom subcommand input', async ({ page }) => {
    // Select Custom preset
    const dropdown = page.locator('select#mfg-preset');
    await dropdown.selectOption('custom');

    // Subcommand input should be enabled
    const subcommandInput = page.locator('input#mfg-subcommand');
    await expect(subcommandInput).toBeEnabled();

    // Enter custom subcommand
    await subcommandInput.fill('0x00FF');

    // Send command
    const sendButton = page.locator('button:has-text("Send Command")');
    await sendButton.click();
    await page.waitForTimeout(2000);

    // Should get generic response (0x00 0x00)
    const dataText = await page.locator('text=/Response Data.*0x00.*0x00/i').count();
    expect(dataText).toBeGreaterThan(0);
  });

  test('should disable subcommand input when preset is selected', async ({ page }) => {
    // Select a non-custom preset
    const dropdown = page.locator('select#mfg-preset');
    await dropdown.selectOption('device_type');

    // Subcommand input should be disabled
    const subcommandInput = page.locator('input#mfg-subcommand');
    await expect(subcommandInput).toBeDisabled();
  });

  test('should show reference cards for common commands', async ({ page }) => {
    // Verify reference section exists
    const referenceHeading = page.locator('h3:has-text("Common Device Commands")');
    await expect(referenceHeading).toBeVisible();

    // Count reference cards (should be 8: 7 commands + 1 custom)
    const cards = page.locator('.reference-card');
    const count = await cards.count();
    expect(count).toBe(8);

    // Verify Device Type card
    const deviceTypeCard = page.locator('.reference-card:has-text("0x0001")');
    await expect(deviceTypeCard).toBeVisible();
  });

  test('should update subcommand when changing presets', async ({ page }) => {
    const dropdown = page.locator('select#mfg-preset');
    const subcommandInput = page.locator('input#mfg-subcommand');

    // Select Device Type
    await dropdown.selectOption('device_type');
    let value = await subcommandInput.inputValue();
    expect(value).toBe('0x0001');

    // Select FET Status
    await dropdown.selectOption('fet_status');
    value = await subcommandInput.inputValue();
    expect(value).toBe('0x0023');

    // Select Safety Status
    await dropdown.selectOption('safety_status');
    value = await subcommandInput.inputValue();
    expect(value).toBe('0x0070');
  });

  test('should show help text for presets', async ({ page }) => {
    const dropdown = page.locator('select#mfg-preset');

    // Select Device Type
    await dropdown.selectOption('device_type');
    let helpText = page.locator('text=/Read chip device ID/i');
    await expect(helpText).toBeVisible();

    // Select FET Status
    await dropdown.selectOption('fet_status');
    helpText = page.locator('text=/CHG\\/DSG FET enable status/i');
    await expect(helpText).toBeVisible();
  });

  test('should accept data payload input', async ({ page }) => {
    // Data payload input should exist
    const dataInput = page.locator('input#mfg-data');
    await expect(dataInput).toBeVisible();

    // Should accept hex input
    await dataInput.fill('0x01 0x02 0x03');
    const value = await dataInput.inputValue();
    expect(value).toBe('0x01 0x02 0x03');
  });

  test('should show response length', async ({ page }) => {
    // Select Device Type and send
    const dropdown = page.locator('select#mfg-preset');
    await dropdown.selectOption('device_type');

    const sendButton = page.locator('button:has-text("Send Command")');
    await sendButton.click();
    await page.waitForTimeout(2000);

    // Verify length is shown (Device Type returns 2 bytes)
    const lengthText = await page.locator('text=/Response Length.*2 bytes/i').count();
    expect(lengthText).toBeGreaterThan(0);
  });
});
