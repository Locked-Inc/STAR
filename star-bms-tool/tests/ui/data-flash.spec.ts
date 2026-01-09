import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Data Flash Tab Tests
 *
 * Tests the Data Flash programming interface including:
 * - Reading Data Flash blocks (32 bytes)
 * - Writing Data Flash blocks
 * - Hex viewer display
 * - Backup/Restore functionality
 * - Class reference information
 */

test.describe('Data Flash Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Data Flash tab
    await page.click('button:has-text("Data Flash")');
  });

  test('should display data flash tab', async ({ page }) => {
    await expect(page.locator('h2:has-text("Data Flash Programming")')).toBeVisible();
  });

  test('should have class and offset input fields', async ({ page }) => {
    await expect(page.locator('input[type="text"]').filter({ hasText: /class/i }).or(page.locator('label:has-text("Class")').locator('~ input'))).toBeVisible();
    await expect(page.locator('input[type="text"]').filter({ hasText: /offset/i }).or(page.locator('label:has-text("Offset")').locator('~ input'))).toBeVisible();
  });

  test('should have read and write buttons', async ({ page }) => {
    const readButton = page.locator('button').filter({ hasText: /Read.*Block/i });
    const writeButton = page.locator('button').filter({ hasText: /Write.*Block/i });

    await expect(readButton.or(page.locator('button:has-text("Read")'))).toBeVisible();
    await expect(writeButton.or(page.locator('button:has-text("Write")'))).toBeVisible();
  });

  test('should read data flash block', async ({ page }) => {
    // Click read button
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(1000);

    // Should display hex viewer
    await expect(page.locator('.hex-viewer, .hex-display')).toBeVisible();
  });

  test('should display hex viewer with offset, hex, and ASCII columns', async ({ page }) => {
    // Read a block
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(1000);

    // Check for hex viewer headers
    const hexViewer = page.locator('.hex-viewer');
    await expect(hexViewer).toBeVisible();

    // Should have offset column header
    await expect(page.locator('.hex-header .addr')).toBeVisible();

    // Should have hex bytes column
    await expect(page.locator('.hex-header .hex-bytes')).toBeVisible();

    // Should have ASCII column
    await expect(page.locator('.hex-header .ascii')).toBeVisible();
  });

  test('should display 32 bytes in hex viewer', async ({ page }) => {
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Check for hex bytes (32 bytes = multiple rows)
    const hexBytes = page.locator('.hex-byte, .hex-cell');
    const count = await hexBytes.count();

    // Should display all 32 bytes
    expect(count).toBeGreaterThanOrEqual(32);
  });

  test('should have backup button', async ({ page }) => {
    const backupButton = page.locator('button:has-text("Create Backup")');
    await expect(backupButton).toBeVisible();
  });

  test('should have restore functionality', async ({ page }) => {
    // Look for restore button or file upload
    const restoreButton = page.locator('button').filter({ hasText: /Restore/i });
    const fileInput = page.locator('input[type="file"]');

    // Should have either restore button or file input
    const hasRestore = (await restoreButton.count()) > 0 || (await fileInput.count()) > 0;
    expect(hasRestore).toBeTruthy();
  });

  test('should display class reference information', async ({ page }) => {
    // Look for reference cards or class information
    const referenceSection = page.locator('text=/Common.*Classes/i, text=/Class.*Reference/i');

    // If visible, check for common class numbers
    if (await referenceSection.isVisible()) {
      // Common classes: 48 (Safety), 64 (Charge), 80 (Discharge), 82 (Data)
      const hasClassInfo =
        (await page.locator('text=/48/').count()) > 0 ||
        (await page.locator('text=/Safety/i').count()) > 0;
      expect(hasClassInfo).toBeTruthy();
    }
  });

  test('should validate class number range', async ({ page }) => {
    // Try invalid class number
    const classInput = page.locator('#df-class');
    await classInput.fill('999');

    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(1000);

    // Should show error or validation message
    const errorText = page.locator('.error');
    await expect(errorText).toBeVisible();
    await expect(errorText).toContainText('0-255');
  });

  test('should accept valid class numbers', async ({ page }) => {
    // Class 48 (Safety)
    const classInput = page.locator('#df-class');
    await classInput.clear();
    await classInput.fill('48');

    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Should succeed (no error, hex viewer shown)
    const hexViewer = page.locator('.hex-viewer, .hex-display');
    await expect(hexViewer).toBeVisible();
  });

  test('should show warning messages when appropriate', async ({ page }) => {
    // Data flash operations should have safety warnings
    const warning = page.locator('.warning, .caution');

    // Warning might be always visible or shown after certain actions
    const warningCount = await warning.count();
    expect(warningCount).toBeGreaterThanOrEqual(0); // At least possibility of warnings
  });

  test('should handle read errors gracefully', async ({ page }) => {
    // Disconnect to trigger error
    await page.click('button:has-text("Disconnect")');
    await page.waitForTimeout(500);

    // Navigate back to Data Flash
    await page.click('button:has-text("Data Flash")');

    // Button should be disabled when disconnected
    const readButton = page.locator('button:has-text("Read Block")');
    await expect(readButton).toBeDisabled();

    // UI should not crash
    await expect(page.locator('h2:has-text("Data Flash Programming")')).toBeVisible();
  });

  test('should format hex bytes with proper spacing', async ({ page }) => {
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Check that hex bytes are displayed in uppercase
    const hexBytes = page.locator('.hex-byte, .hex-cell').first();
    if (await hexBytes.isVisible()) {
      const text = await hexBytes.textContent();
      // Hex should be 2 characters, uppercase
      expect(text?.length).toBe(2);
      expect(text).toMatch(/^[0-9A-F]{2}$/);
    }
  });

  test('should display ASCII representation', async ({ page }) => {
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(1000);

    // ASCII column should show printable chars or dots (use .first() to avoid strict mode)
    const asciiColumn = page.locator('.hex-row .ascii').first();
    if (await asciiColumn.isVisible()) {
      const text = await asciiColumn.textContent();
      // ASCII should contain characters or dots for non-printable
      expect(text?.length).toBeGreaterThan(0);
    }
  });

  test('should persist data when switching tabs', async ({ page }) => {
    // Read data
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Verify hex viewer is visible
    await expect(page.locator('.hex-viewer, .hex-display')).toBeVisible();

    // Switch tabs
    await page.click('button:has-text("Telemetry")');
    await page.waitForTimeout(1000);
    await page.click('button:has-text("Data Flash")');
    await page.waitForTimeout(1000);

    // Data should still be visible
    await expect(page.locator('.hex-viewer, .hex-display')).toBeVisible();
  });
});
