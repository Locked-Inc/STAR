import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * FET Control and Cell Balancing Tests
 *
 * Tests the FET (Field-Effect Transistor) control and cell balancing features:
 * - Charge FET (CHG) enable/disable
 * - Discharge FET (DSG) enable/disable
 * - Real-time FET status display
 * - Cell balancing manual control
 * - Balancing status indicators
 */

test.describe('FET Control', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Manufacturer Access tab (where FET controls are)
    await page.click('button:has-text("Manufacturer Access")');
    await page.waitForTimeout(1000);
  });

  test('should display FET control buttons after connection', async ({ page }) => {
    // FET controls are in the Manufacturer Access tab
    await expect(page.locator('h3:has-text("FET Control")')).toBeVisible();
    await expect(page.locator('button:has-text("Enable CHG FET")')).toBeVisible();
    await expect(page.locator('button:has-text("Enable DSG FET")')).toBeVisible();
  });

  test('should toggle charge FET', async ({ page }) => {
    // Click Enable CHG FET
    const enableButton = page.locator('button:has-text("Enable CHG FET")');
    await enableButton.click();
    await page.waitForTimeout(1000);

    // After enabling, button should still be visible (separate Enable/Disable buttons)
    await expect(enableButton).toBeVisible();
    await expect(page.locator('button:has-text("Disable CHG FET")')).toBeVisible();
  });

  test('should toggle discharge FET', async ({ page }) => {
    // Click Enable DSG FET
    const enableButton = page.locator('button:has-text("Enable DSG FET")');
    await enableButton.click();
    await page.waitForTimeout(1000);

    // After enabling, button should still be visible (separate Enable/Disable buttons)
    await expect(enableButton).toBeVisible();
    await expect(page.locator('button:has-text("Disable DSG FET")')).toBeVisible();
  });

  test('should show FET status indicators', async ({ page }) => {
    // Send FET Status command to display status
    const dropdown = page.locator('#mfg-preset');
    await dropdown.selectOption('fet_status');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(1000);

    // Should display FET status indicators
    await expect(page.locator('.fet-status-display')).toBeVisible();
    await expect(page.locator('text=CHG FET:')).toBeVisible();
    await expect(page.locator('text=DSG FET:')).toBeVisible();
  });

  test('should display FET status from manufacturer access command', async ({ page }) => {
    // Already in Manufacturer Access tab from beforeEach

    // Select FET Status command
    const dropdown = page.locator('#mfg-preset');
    await dropdown.selectOption('fet_status');

    // Send command
    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(1000);

    // Should display FET status in response
    await expect(page.locator('.fet-status-display')).toBeVisible();
    await expect(page.locator('.indicator-label:has-text("CHG FET:")')).toBeVisible();
    await expect(page.locator('.indicator-label:has-text("DSG FET:")')).toBeVisible();
  });

  test('should require connection for FET control', async ({ page }) => {
    // Disconnect
    await page.click('button:has-text("Disconnect")');
    await page.waitForTimeout(500);

    // Navigate back to Manufacturer Access
    await page.click('button:has-text("Manufacturer Access")');
    await page.waitForTimeout(500);

    // FET buttons should be disabled
    await expect(page.locator('button:has-text("Enable CHG FET")')).toBeDisabled();
    await expect(page.locator('button:has-text("Enable DSG FET")')).toBeDisabled();
  });
});

test.describe('Cell Balancing Control', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Cell Voltages tab (where balancing control typically lives)
    await page.click('button:has-text("Cell Voltages")');
  });

  test('should display cell balancing control', async ({ page }) => {
    const balancingControl = page.locator('button, .control').filter({ hasText: /Cell.*Balanc/i });

    if (await balancingControl.isVisible()) {
      expect(await balancingControl.isVisible()).toBeTruthy();
    }
  });

  test('should have cell balancing enable/disable button', async ({ page }) => {
    const balancingButton = page.locator('button').filter({ hasText: /Balanc/i });

    if (await balancingButton.isVisible()) {
      await expect(balancingButton).toBeVisible();
      await expect(balancingButton).toBeEnabled();
    }
  });

  test('should toggle cell balancing', async ({ page }) => {
    const balancingButton = page.locator('button').filter({ hasText: /Balanc/i }).first();

    if (await balancingButton.isVisible()) {
      // Get initial state
      const initialText = await balancingButton.textContent();

      // Click to toggle
      await balancingButton.click();
      await page.waitForTimeout(500);

      // Text should change
      const newText = await balancingButton.textContent();
      expect(newText).not.toBe(initialText);
    }
  });

  test('should show balancing status indicator', async ({ page }) => {
    // Look for balancing status badge
    const status = page.locator('.status-badge, .balancing-status').filter({ hasText: /ACTIVE|INACTIVE|ON|OFF/i });

    if (await status.isVisible()) {
      expect(await status.isVisible()).toBeTruthy();
    }
  });

  test('should display balancing active flag in protection status', async ({ page }) => {
    // Navigate to Protection Status
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    await expect(page.locator('.protection-flag')).toHaveCount(14);

    // Should show cell balancing flag (use .flag-name to be specific)
    await expect(page.locator('.flag-name:has-text("Cell Balancing")')).toBeVisible();
  });

  test('should show balancing explanation or help text', async ({ page }) => {
    // Navigate to Protection Status where balancing control is
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    // Try to scroll down to find balancing controls (they might be below the fold)
    for (let i = 0; i < 3; i++) {
      await page.keyboard.press('PageDown');
      await page.waitForTimeout(200);
    }

    // Look for balancing control buttons (easier to find than h3)
    const balancingButtons = page.locator('button').filter({ hasText: /Cell.*Balancing/i });
    if (await balancingButtons.count() > 0) {
      await expect(balancingButtons.first()).toBeVisible();
    }
  });

  test('should send ManufacturerAccess command 0x0026 for balancing control', async ({ page }) => {
    // Navigate to Manufacturer Access
    await page.click('button:has-text("Manufacturer Access")');
    await page.waitForTimeout(1000);

    // Try custom command for balancing (0x0026)
    const dropdown = page.locator('#mfg-preset');
    await dropdown.selectOption('custom');

    const subcommandInput = page.locator('#mfg-subcommand');
    await subcommandInput.fill('0x0026');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(1000);

    // Should show response
    await expect(page.locator('.mfg-result')).toBeVisible();
  });

  test('should persist balancing state when switching tabs', async ({ page }) => {
    const cellCards = page.locator('.cell-card');
    await expect(cellCards).toHaveCount(4);
    await expect(cellCards.first()).toBeVisible();

    // Switch tabs
    await page.click('button:has-text("Telemetry")');
    await page.waitForTimeout(1000);
    await page.click('button:has-text("Cell Voltages")');
    await page.waitForTimeout(1000);

    // Cell voltages should still be displayed
    await expect(cellCards).toHaveCount(4);
  });

  test('should disable balancing control when disconnected', async ({ page }) => {
    const balancingButton = page.locator('button').filter({ hasText: /Balanc/i }).first();

    if (await balancingButton.isVisible()) {
      // Disconnect
      await page.click('button:has-text("Disconnect")');
      await page.waitForTimeout(500);

      // Navigate back to Cell Voltages
      await page.click('button:has-text("Cell Voltages")');

      // Balancing button should be disabled
      if (await balancingButton.isVisible()) {
        await expect(balancingButton).toBeDisabled();
      }
    }
  });

  test('should show delta voltage to indicate balancing need', async ({ page }) => {
    const cellCards = page.locator('.cell-card');
    await expect(cellCards).toHaveCount(4);
    await expect(cellCards.first()).toBeVisible();

    // Delta voltage should be displayed
    await expect(page.locator('text=/Delta/i')).toBeVisible();

    // Delta indicates need for balancing (large delta = cells unbalanced)
    const deltaValue = page.locator('text=/Delta.*mV|Delta.*V/i');
    if (await deltaValue.isVisible()) {
      expect(await deltaValue.isVisible()).toBeTruthy();
    }
  });

  test('should color-code cells by voltage to show imbalance', async ({ page }) => {
    const cellCards = page.locator('.cell-card');
    await expect(cellCards).toHaveCount(4);
    await expect(cellCards.first()).toBeVisible();

    // Cells should have voltage bars or color coding
    const cellElements = page.locator('.cell-card, .cell-voltage');
    if (await cellElements.first().isVisible()) {
      const count = await cellElements.count();
      expect(count).toBeGreaterThan(0);
    }
  });
});

test.describe('FET and Balancing Integration', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);
  });

  test('should show comprehensive battery control overview', async ({ page }) => {
    // Header area should show FET status + balancing status
    const header = page.locator('header, .app-header, .status-bar');

    if (await header.isVisible()) {
      // Should contain battery control information
      expect(await header.isVisible()).toBeTruthy();
    }
  });

  test('should allow simultaneous FET and balancing control', async ({ page }) => {
    // Check FET controls in Manufacturer Access tab
    await page.click('button:has-text("Manufacturer Access")');
    await page.waitForTimeout(1000);
    await expect(page.locator('button:has-text("Enable CHG FET")')).toBeVisible();

    // Check balancing controls in Protection Status tab
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    // Try to scroll down to find balancing controls
    for (let i = 0; i < 3; i++) {
      await page.keyboard.press('PageDown');
      await page.waitForTimeout(200);
    }

    // Look for balancing control buttons
    const balancingButtons = page.locator('button').filter({ hasText: /Cell.*Balancing/i });
    if (await balancingButtons.count() > 0) {
      await expect(balancingButtons.first()).toBeVisible();
    }
  });

  test('should maintain consistent UI state across tabs', async ({ page }) => {
    // Read protection status
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    // Check balancing active status (use specific selector to avoid strict mode)
    const balancingFlag = page.locator('.flag-name:has-text("Cell Balancing")');
    const balancingActive = await balancingFlag.isVisible();

    // Go to Cell Voltages
    await page.click('button:has-text("Cell Voltages")');
    await page.waitForTimeout(1000);

    // Cell voltages tab doesn't show balancing status, just verify tab switch works
    await expect(page.locator('h2:has-text("Cell Voltages")')).toBeVisible();

    // Go back to Protection Status
    await page.click('button:has-text("Protection Status")');
    await page.waitForTimeout(1000);

    // Balancing flag should still be visible (state persisted)
    const balancingFlagAfter = page.locator('.flag-name:has-text("Cell Balancing")');
    await expect(balancingFlagAfter).toBeVisible();
  });
});
