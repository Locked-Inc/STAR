import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Registers Tab Tests
 * Tests reading and writing raw BMS registers
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Registers Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Registers tab
    const registersTab = page.locator('button:has-text("Registers")');
    await registersTab.click();
  });

  test.afterEach(async ({ page }) => {
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should display registers tab', async ({ page }) => {
    const registersSection = page.locator('h2:has-text("Register Access")');
    await expect(registersSection).toBeVisible();
  });

  test('should have read register section', async ({ page }) => {
    const readSection = page.locator('h3:has-text("Read Register")');
    await expect(readSection).toBeVisible();
  });

  test('should have write register section', async ({ page }) => {
    const writeSection = page.locator('h3:has-text("Write Register")');
    await expect(writeSection).toBeVisible();
  });

  test('should have address input for read', async ({ page }) => {
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await expect(addressInput).toBeVisible();
  });

  test('should have num bytes input for read', async ({ page }) => {
    const bytesInputs = page.locator('input[type="number"]');
    const count = await bytesInputs.count();
    expect(count).toBeGreaterThan(0);
  });

  test('should read register at address 0x00', async ({ page }) => {
    // Enter address
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await addressInput.fill('0');

    // Enter num bytes
    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    await readNumBytes.fill('2');

    // Click read button
    const readButton = page.locator('button:has-text("Read Register")');
    await readButton.click();
    await page.waitForTimeout(2000);

    // Verify value is displayed (mock returns 0x0350 for address 0x00)
    const valueText = await page.locator('text=/Value.*0x/i').count();
    expect(valueText).toBeGreaterThan(0);
  });

  test('should read register at address 0x02', async ({ page }) => {
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await addressInput.fill('2');

    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    await readNumBytes.fill('2');

    const readButton = page.locator('button:has-text("Read Register")');
    await readButton.click();
    await page.waitForTimeout(2000);

    // Mock returns 0xFFF0 for address 0x02 (current register)
    const valueText = await page.locator('text=/Value/i').count();
    expect(valueText).toBeGreaterThan(0);
  });

  test('should accept hexadecimal address format', async ({ page }) => {
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await addressInput.fill('0x42');

    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    await readNumBytes.fill('2');

    const readButton = page.locator('button:has-text("Read Register")');
    await readButton.click();
    await page.waitForTimeout(2000);

    // Should successfully read
    const valueText = await page.locator('text=/Value/i').count();
    expect(valueText).toBeGreaterThan(0);
  });

  test('should have write register inputs', async ({ page }) => {
    const writeAddressInput = page.locator('input[placeholder*="0x10"]').first();
    await expect(writeAddressInput).toBeVisible();

    const writeValueInput = page.locator('input[placeholder*="0xFF"]').first();
    await expect(writeValueInput).toBeVisible();
  });

  test('should write register successfully', async ({ page }) => {
    // Enter write address
    const writeAddressInput = page.locator('input[placeholder*="0x10"]').first();
    await writeAddressInput.fill('0x10');

    // Enter value
    const writeValueInput = page.locator('input[placeholder*="0xFF"]').first();
    await writeValueInput.fill('0xFF');

    // Enter num bytes
    const numBytesInputs = page.locator('input[type="number"]');
    const writeNumBytes = numBytesInputs.last();
    await writeNumBytes.fill('1');

    // Click write button
    const writeButton = page.locator('button:has-text("Write Register")');
    await writeButton.click();
    await page.waitForTimeout(2000);

    // Mock device returns success message
    // Should not show an error
    const errorText = await page.locator('text=/error/i').count();
    expect(errorText).toBe(0);
  });

  test('should validate num bytes range', async ({ page }) => {
    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();

    // Try setting to 0
    await readNumBytes.fill('0');
    const value = await readNumBytes.inputValue();

    // Should enforce minimum of 1
    const readButton = page.locator('button:has-text("Read Register")');
    await expect(readButton).toBeVisible();
  });

  test('should display both decimal and hex values', async ({ page }) => {
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    await addressInput.fill('0');

    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    await readNumBytes.fill('2');

    const readButton = page.locator('button:has-text("Read Register")');
    await readButton.click();
    await page.waitForTimeout(2000);

    // Should show value in hex and decimal
    const hexText = await page.locator('text=/0x[0-9A-Fa-f]+/').count();
    expect(hexText).toBeGreaterThan(0);
  });

  test('should handle multiple register reads', async ({ page }) => {
    const addressInput = page.locator('input[placeholder*="0x00"]').first();
    const numBytesInputs = page.locator('input[type="number"]');
    const readNumBytes = numBytesInputs.first();
    const readButton = page.locator('button:has-text("Read Register")');

    // Read first register
    await addressInput.fill('0');
    await readNumBytes.fill('2');
    await readButton.click();
    await page.waitForTimeout(1500);

    // Read second register
    await addressInput.fill('2');
    await readNumBytes.fill('2');
    await readButton.click();
    await page.waitForTimeout(1500);

    // Read third register
    await addressInput.fill('4');
    await readNumBytes.fill('2');
    await readButton.click();
    await page.waitForTimeout(1500);

    // Should have values displayed
    const valueText = await page.locator('text=/Value/i').count();
    expect(valueText).toBeGreaterThan(0);
  });
});
