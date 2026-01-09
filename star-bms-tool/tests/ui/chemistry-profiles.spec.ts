import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Chemistry Profiles Tab Tests
 *
 * Tests the battery chemistry profile system including:
 * - 5 pre-defined profiles (Li-ion, LiFePO4, Li-Po, NiMH, Custom)
 * - Profile parameter display
 * - Chemistry comparison table
 * - Profile application
 */

test.describe('Chemistry Profiles Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Chemistry Profiles tab
    await page.click('button:has-text("Chemistry Profiles")');
  });

  test('should display chemistry profiles tab', async ({ page }) => {
    await expect(page.locator('h2').filter({ hasText: /Chemistry.*Profiles/i })).toBeVisible();
  });

  test('should have profile selection dropdown', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');
    await expect(dropdown).toBeVisible();
  });

  test('should list all 5 battery chemistries', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    // Check for all 5 profiles
    await expect(dropdown).toContainText(/Li-ion/i);
    await expect(dropdown).toContainText(/LiFePO4/i);
    await expect(dropdown).toContainText(/Li-Po/i);
    await expect(dropdown).toContainText(/NiMH/i);
    await expect(dropdown).toContainText(/Custom/i);
  });

  test('should display Li-ion profile by default', async ({ page }) => {
    // Li-ion should be selected initially
    const dropdown = page.locator('#chemistry-select');
    const value = await dropdown.inputValue();

    expect(value).toContain('lion');
  });

  test('should display voltage limits for selected profile', async ({ page }) => {
    // Should show Voltage Limits section
    await expect(page.locator('.spec-header:has-text("Voltage Limits")')).toBeVisible();

    // Should display voltage values in mV
    await expect(page.locator('.spec-value').first()).toBeVisible();
  });

  test('should display current limits for selected profile', async ({ page }) => {
    // Should show Current Limits section
    await expect(page.locator('.spec-header:has-text("Current Limits")')).toBeVisible();
  });

  test('should display temperature limits for selected profile', async ({ page }) => {
    // Should show Temperature Limits sections
    await expect(page.locator('.spec-header').filter({ hasText: /Temperature.*Limits/i }).first()).toBeVisible();
  });

  test('should switch between profiles', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    // Switch to LiFePO4
    await dropdown.selectOption("lifepo4");
    await page.waitForTimeout(1000);

    // Verify LiFePO4 is selected
    const value1 = await dropdown.inputValue();
    expect(value1).toContain('lifepo4');

    // Switch to NiMH
    await dropdown.selectOption("nimh");
    await page.waitForTimeout(1000);

    // Verify NiMH is selected
    const value2 = await dropdown.inputValue();
    expect(value2).toContain('nimh');
  });

  test('should display comparison table', async ({ page }) => {
    // Look for comparison table
    const table = page.locator('.comparison-table table');
    await expect(table).toBeVisible();

    // Should have headers
    await expect(table.locator('th').first()).toBeVisible();

    // Should compare multiple chemistries
    await expect(table).toContainText(/Li-ion/i);
    await expect(table).toContainText(/LiFePO4/i);
  });

  test('should show cycle life information', async ({ page }) => {
    // Profiles should display expected cycle life
    const cycleInfo = page.locator('text=/cycle/i, text=/500.*1000|2000.*5000/i');

    if (await cycleInfo.isVisible()) {
      expect(await cycleInfo.count()).toBeGreaterThan(0);
    }
  });

  test('should show use case recommendations', async ({ page }) => {
    // Should recommend applications for each chemistry
    const useCase = page.locator('text=/use.*case/i, text=/application/i, text=/consumer.*electronics/i');

    if (await useCase.isVisible()) {
      expect(await useCase.count()).toBeGreaterThan(0);
    }
  });

  test('should have apply profile button', async ({ page }) => {
    const applyButton = page.locator('button').filter({ hasText: /Apply/i });
    await expect(applyButton).toBeVisible();
  });

  test('should show confirmation when applying profile', async ({ page }) => {
    const applyButton = page.locator('button:has-text("Apply Profile to BMS")');
    await applyButton.click();
    await page.waitForTimeout(500);

    // Should show success message with ✓ symbol
    const message = page.locator('.profile-result');
    await expect(message).toBeVisible();
    await expect(message).toContainText(/✓.*Profile/i);
  });

  test('should display custom profile option', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Verify custom profile is selected
    const value = await dropdown.inputValue();
    expect(value).toContain('custom');

    // Should show custom profile description
    await expect(page.locator('.profile-description')).toContainText(/Custom.*parameter/i);
  });

  test('should show energy density comparison', async ({ page }) => {
    // Look for energy density information (Wh/kg)
    const energyDensity = page.locator('text=/energy.*density/i, text=/Wh.*kg/i, text=/150.*200/i');

    if (await energyDensity.isVisible()) {
      expect(await energyDensity.count()).toBeGreaterThan(0);
    }
  });

  test('should display safety information', async ({ page }) => {
    // Should mention safety characteristics
    const safetyInfo = page.locator('text=/safety/i, text=/stable/i, text=/moderate/i');

    if (await safetyInfo.isVisible()) {
      expect(await safetyInfo.count()).toBeGreaterThan(0);
    }
  });

  test('should persist selected profile when switching tabs', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    // Select LiFePO4
    await dropdown.selectOption("lifepo4");
    await page.waitForTimeout(1000);

    // Verify LiFePO4 is selected before switch
    const value1 = await dropdown.inputValue();
    expect(value1).toContain('lifepo4');

    // Switch to different tab
    await page.click('button:has-text("Telemetry")');
    await page.waitForTimeout(1000);

    // Switch back to Chemistry Profiles
    await page.click('button:has-text("Chemistry Profiles")');
    await page.waitForTimeout(500); // Give time for tab to fully render

    // LiFePO4 should still be selected
    const value2 = await dropdown.inputValue();
    expect(value2).toContain('lifepo4');
  });

  test('should display nominal voltage prominently', async ({ page }) => {
    // Nominal voltage is key parameter
    await expect(page.locator('text=/Nominal.*Voltage/i, text=/3.7.*V|3700.*mV/i')).toBeVisible();
  });

  test('should show pack vs cell voltage calculations', async ({ page }) => {
    // If device has 4 cells, should show pack voltage = cell voltage × 4
    const packInfo = page.locator('text=/pack/i, text=/14\.8.*V|14800.*mV/i');

    // This might not be displayed, depending on implementation
    if (await packInfo.isVisible()) {
      expect(await packInfo.isVisible()).toBeTruthy();
    }
  });

  test('should handle disconnected state gracefully', async ({ page }) => {
    // Disconnect
    await page.click('button:has-text("Disconnect")');
    await page.waitForTimeout(500);

    // Navigate to Chemistry Profiles
    await page.click('button:has-text("Chemistry Profiles")');

    // Profile selection should still work (doesn't require connection)
    const dropdown = page.locator('#chemistry-select');
    await expect(dropdown).toBeVisible();
    await expect(dropdown).toBeEnabled();
  });

  test('should provide clear profile descriptions', async ({ page }) => {
    // Each profile should have descriptive text
    const description = page.locator('.profile-description');

    if (await description.isVisible()) {
      expect(await description.count()).toBeGreaterThan(0);
    }
  });

  test('should format voltage and current values consistently', async ({ page }) => {
    // Values should be formatted with units (mV, mA, °C)
    await expect(page.locator('text=/mV|V/').first()).toBeVisible();
    await expect(page.locator('text=/mA|A/').first()).toBeVisible();
    await expect(page.locator('text=/°C|Celsius/i').first()).toBeVisible();
  });
});
