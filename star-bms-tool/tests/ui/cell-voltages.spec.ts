import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Cell Voltages Tab Tests
 * Tests reading and displaying individual cell voltages
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Cell Voltages Tab', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to mock device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1000);

    // Navigate to Cell Voltages tab
    const cellTab = page.locator('button:has-text("Cell Voltages")');
    await cellTab.click();
  });

  test.afterEach(async ({ page }) => {
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should display cell voltages tab', async ({ page }) => {
    const cellSection = page.locator('h2:has-text("Cell Voltages")');
    await expect(cellSection).toBeVisible();
  });

  test('should read and display 4 cell voltages', async ({ page }) => {
    await expect(page.locator('.cell-grid')).toBeVisible();

    // Verify cell voltages are displayed (mock: 3.70V, 3.71V, 3.72V, 3.73V)
    const cell1 = await page.locator('text=/Cell 1.*3\\.7[0-9].*V/i').count();
    const cell2 = await page.locator('text=/Cell 2.*3\\.7[0-9].*V/i').count();
    const cell3 = await page.locator('text=/Cell 3.*3\\.7[0-9].*V/i').count();
    const cell4 = await page.locator('text=/Cell 4.*3\\.7[0-9].*V/i').count();

    expect(cell1).toBeGreaterThan(0);
    expect(cell2).toBeGreaterThan(0);
    expect(cell3).toBeGreaterThan(0);
    expect(cell4).toBeGreaterThan(0);
  });

  test('should display pack voltage', async ({ page }) => {
    await expect(page.locator('.cell-grid')).toBeVisible();

    // Mock pack voltage should be sum of 4 cells (≈14.8V)
    const packText = await page.locator('text=/Pack.*14\\.8.*V/i').count();
    expect(packText).toBeGreaterThan(0);
  });

  test('should display min cell voltage', async ({ page }) => {
    await expect(page.locator('.cell-grid')).toBeVisible();

    // Min should be first cell (3.70V)
    const minText = await page.locator('text=/Min.*3\\.7.*V/i').count();
    expect(minText).toBeGreaterThan(0);
  });

  test('should display max cell voltage', async ({ page }) => {
    await expect(page.locator('.cell-grid')).toBeVisible();

    // Max should be last cell (3.73V or higher)
    const maxText = await page.locator('text=/Max.*3\\.7[3-9].*V/i').count();
    expect(maxText).toBeGreaterThan(0);
  });

  test('should display delta voltage', async ({ page }) => {
    await expect(page.locator('.cell-grid')).toBeVisible();

    // Delta should be present
    const deltaText = await page.locator('text=/Delta.*[0-9]+.*mV/i').count();
    expect(deltaText).toBeGreaterThan(0);
  });

});
