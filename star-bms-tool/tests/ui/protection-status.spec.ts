import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Protection Status Tab Tests
 *
 * Tests the Protection Status monitoring tab which displays 14 protection flags:
 * - Voltage protections (Cell OV/UV, Pack OV/UV)
 * - Current protections (Charge OC, Discharge OC, Short Circuit)
 * - Temperature protections (OT/UT Charge/Discharge)
 * - System status (Cell balancing, Permanent failure, Safety alert)
 */

test.describe('Protection Status Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Protection Status tab
    await page.click('button:has-text("Protection Status")');
  });

  test('should display protection status tab', async ({ page }) => {
    await expect(page.locator('h2:has-text("Protection Status")')).toBeVisible();
  });

  test('should read and display all protection flags', async ({ page }) => {
    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Check voltage protection section
    await expect(page.locator('h3:has-text("Voltage Protection")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Cell Overvoltage")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Cell Undervoltage")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Pack Overvoltage")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Pack Undervoltage")')).toBeVisible();

    // Check current protection section
    await expect(page.locator('h3:has-text("Current Protection")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Charge Overcurrent")').first()).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Discharge Overcurrent")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Short Circuit")')).toBeVisible();

    // Check temperature protection section
    await expect(page.locator('h3:has-text("Temperature Protection")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Overtemp Charge")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Overtemp Discharge")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Undertemp Charge")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Undertemp Discharge")')).toBeVisible();

    // Check system status section
    await expect(page.locator('h3:has-text("System Status")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Cell Balancing")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Permanent Failure")')).toBeVisible();
    await expect(page.locator('.flag-name:has-text("Safety Alert")')).toBeVisible();
  });

  test('should show inactive status for all protections (mock data)', async ({ page }) => {
    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Mock returns all false except cell_balancing_active
    const indicators = page.locator('.protection-flag:not(.active)');
    await expect(indicators).toHaveCount(13); // All except cell balancing

    const activeIndicators = page.locator('.protection-flag.active');
    await expect(activeIndicators).toHaveCount(1); // Only cell balancing
  });

  test('should color-code protection indicators', async ({ page }) => {
    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Inactive protections should have green/gray styling
    const inactive = page.locator('.protection-flag:not(.active)').first();
    await expect(inactive).toBeVisible();

    // Active protections should have red/orange styling
    const active = page.locator('.protection-flag.active').first();
    await expect(active).toBeVisible();
  });

  test('should organize protections into categories', async ({ page }) => {
    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Check that all 4 category headers exist
    const categories = page.locator('.protection-section h3');
    await expect(categories).toContainText(['Voltage', 'Current', 'Temperature', 'System']);
  });

  test('should handle disconnected state', async ({ page }) => {
    // Disconnect
    await page.click('button:has-text("Disconnect")');
    await page.waitForTimeout(500);

    // Data collection controls should be disabled when disconnected
    await expect(page.getByTestId('auto-refresh-toggle')).toBeDisabled();
    await expect(page.getByTestId('collection-toggle')).toBeDisabled();

    // UI should not crash
    await expect(page.locator('h2:has-text("Protection Status")')).toBeVisible();
  });

  test('should persist data when switching tabs', async ({ page }) => {
    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Switch to different tab
    await page.click('button:has-text("Telemetry")');
    await page.waitForTimeout(1000);

    // Switch back
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    // Data should still be visible
    await expect(page.locator('.protection-flag')).toHaveCount(14);
  });
});
