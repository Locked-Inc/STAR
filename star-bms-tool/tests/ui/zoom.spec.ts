import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Zoom Control Tests
 * Tests the zoom level setting in the Settings tab
 */

test.describe('Zoom Control', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Navigate to Settings tab
    await page.click('button:has-text("Settings")');
  });

  test('should display settings tab with zoom level dropdown', async ({ page }) => {
    const settingsHeading = page.locator('h2:has-text("Application Settings")');
    await expect(settingsHeading).toBeVisible();

    const zoomDropdown = page.locator('#zoom-level');
    await expect(zoomDropdown).toBeVisible();
  });

  test('should have zoom options from 80% to 140%', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');
    const options = await zoomDropdown.locator('option').allTextContents();

    expect(options).toContain('80%');
    expect(options).toContain('90%');
    expect(options).toContain('100% (Default)');
    expect(options).toContain('110%');
    expect(options).toContain('120%');
    expect(options).toContain('130%');
    expect(options).toContain('140%');
  });

  test('should default to 100%', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');
    const value = await zoomDropdown.inputValue();
    expect(value).toBe('100');
  });

  test('should change zoom value when selected', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');

    // Select 120%
    await zoomDropdown.selectOption('120');

    // Verify selection changed
    const value = await zoomDropdown.inputValue();
    expect(value).toBe('120');
  });

  test('should have Save Settings button', async ({ page }) => {
    const saveBtn = page.locator('button:has-text("Save Settings")');
    await expect(saveBtn).toBeVisible();
    await expect(saveBtn).toBeEnabled();
  });

  test('should apply zoom when settings are saved', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');

    // Select 120%
    await zoomDropdown.selectOption('120');

    // Click Save Settings
    const saveBtn = page.locator('button:has-text("Save Settings")');
    await saveBtn.click();

    // Check that font-size is applied to document root
    const fontSize = await page.evaluate(() => {
      return document.documentElement.style.fontSize;
    });
    expect(fontSize).toBe('120%');
  });

  test('should show success message after saving', async ({ page }) => {
    // Click Save Settings
    const saveBtn = page.locator('button:has-text("Save Settings")');
    await saveBtn.click();

    // Should show success message
    const successMsg = page.locator('.success:has-text("Settings saved")');
    await expect(successMsg).toBeVisible();
  });

  test('should apply different zoom levels correctly', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');
    const saveBtn = page.locator('button:has-text("Save Settings")');

    const zoomLevels = ['80', '90', '110', '130', '140'];

    for (const zoom of zoomLevels) {
      await zoomDropdown.selectOption(zoom);
      await saveBtn.click();

      const fontSize = await page.evaluate(() => {
        return document.documentElement.style.fontSize;
      });
      expect(fontSize).toBe(`${zoom}%`);
    }
  });

  test('should reset to 100% when selected', async ({ page }) => {
    const zoomDropdown = page.locator('#zoom-level');
    const saveBtn = page.locator('button:has-text("Save Settings")');

    // First change to 120%
    await zoomDropdown.selectOption('120');
    await saveBtn.click();

    // Then change back to 100%
    await zoomDropdown.selectOption('100');
    await saveBtn.click();

    const fontSize = await page.evaluate(() => {
      return document.documentElement.style.fontSize;
    });
    expect(fontSize).toBe('100%');
  });
});

test.describe('Other Settings', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Navigate to Settings tab
    await page.click('button:has-text("Settings")');
  });

  test('should have auto-refresh rate dropdown', async ({ page }) => {
    const refreshDropdown = page.locator('#auto-refresh-rate');
    await expect(refreshDropdown).toBeVisible();
  });

  test('should have auto-refresh rate options', async ({ page }) => {
    const refreshDropdown = page.locator('#auto-refresh-rate');
    const options = await refreshDropdown.locator('option').allTextContents();

    expect(options).toContain('0.5 Hz (2000ms)');
    expect(options).toContain('1 Hz (1000ms)');
    expect(options).toContain('2 Hz (500ms)');
    expect(options).toContain('5 Hz (200ms)');
    expect(options).toContain('10 Hz (100ms)');
  });

  test('should have chart history length dropdown', async ({ page }) => {
    const chartDropdown = page.locator('#chart-history');
    await expect(chartDropdown).toBeVisible();
  });

  test('should have chart history options', async ({ page }) => {
    const chartDropdown = page.locator('#chart-history');
    const options = await chartDropdown.locator('option').allTextContents();

    expect(options).toContain('25 points');
    expect(options).toContain('50 points');
    expect(options).toContain('100 points');
    expect(options).toContain('200 points');
  });

  test('should have help text for settings', async ({ page }) => {
    // Check for help text on settings
    const helpTexts = page.locator('.help-text');
    const count = await helpTexts.count();
    expect(count).toBeGreaterThan(0);
  });

  test('should have settings sections', async ({ page }) => {
    // Check for Telemetry Settings section
    const telemetrySection = page.locator('h3:has-text("Telemetry Settings")');
    await expect(telemetrySection).toBeVisible();

    // Check for Display section
    const displaySection = page.locator('h3:has-text("Display")');
    await expect(displaySection).toBeVisible();
  });
});

test.describe('Settings Persistence', () => {
  test('should persist zoom level across tab changes', async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Navigate to Settings tab
    await page.click('button:has-text("Settings")');

    const zoomDropdown = page.locator('#zoom-level');
    const saveBtn = page.locator('button:has-text("Save Settings")');

    // Change to 130%
    await zoomDropdown.selectOption('130');
    await saveBtn.click();

    // Switch to another tab
    await page.click('button:has-text("Telemetry")');

    // Switch back to Settings
    await page.click('button:has-text("Settings")');

    // Verify zoom is still 130%
    const value = await zoomDropdown.inputValue();
    expect(value).toBe('130');
  });

  test('should maintain zoom level after navigating tabs', async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Navigate to Settings and change zoom
    await page.click('button:has-text("Settings")');
    const zoomDropdown = page.locator('#zoom-level');
    await zoomDropdown.selectOption('110');
    await page.click('button:has-text("Save Settings")');

    // Navigate through multiple tabs (using actual nav text names)
    await page.click('button:has-text("Telemetry")');
    await page.click('button:has-text("Cell Voltages")');
    await page.click('nav.sidebar button.nav-item:has-text("Device Info")');

    // Check that zoom is still applied
    const fontSize = await page.evaluate(() => {
      return document.documentElement.style.fontSize;
    });
    expect(fontSize).toBe('110%');
  });
});
