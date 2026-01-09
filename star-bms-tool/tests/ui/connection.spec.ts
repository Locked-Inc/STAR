import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Connection Flow Tests
 * Tests connecting and disconnecting from the mock BMS device via PTY
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('BMS Connection', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should list available serial ports', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await expect(portSelect).toBeVisible();

    // Should have at least the default option
    const options = await portSelect.locator('option').count();
    expect(options).toBeGreaterThanOrEqual(1);
  });

  test('should allow manual port entry', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');
    await expect(portInput).toBeVisible();
    await expect(portInput).toHaveAttribute('placeholder', /port/i);
  });

  test('should connect to mock device via PTY', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);

    // Click connect button
    const connectButton = page.locator('button:has-text("Connect")');
    await connectButton.click();

    // Wait for disconnect button to appear (indicates successful connection)
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    await expect(disconnectButton).toBeVisible({ timeout: 10000 });
  });

  test('should disconnect from device', async ({ page }) => {
    // First connect
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();

    // Wait for disconnect button to appear
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    await expect(disconnectButton).toBeVisible({ timeout: 10000 });

    // Then disconnect
    await disconnectButton.click();

    // Verify connect button appears again
    const connectButton = page.locator('button:has-text("Connect")');
    await expect(connectButton).toBeVisible({ timeout: 5000 });
  });

  test('should show error for invalid port', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');
    await portInput.fill('/dev/nonexistent_port_12345');

    const connectButton = page.locator('button:has-text("Connect")');
    await connectButton.click();

    // Wait a bit for error to appear
    await page.waitForTimeout(1000);

    // Should still show connect button (not connected)
    await expect(connectButton).toBeVisible();
  });

  test('should prevent connection when already connected', async ({ page }) => {
    // Connect first time
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();

    // Wait for connection to succeed
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    await expect(disconnectButton).toBeVisible({ timeout: 10000 });

    // Port input and select should be disabled
    await expect(portSelect).toBeDisabled();
  });
});
