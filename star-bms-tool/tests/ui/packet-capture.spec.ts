import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Packet Capture Integration Tests
 * Tests the packet capture backend integration in the bottom panel
 */

test.describe('Packet Capture Integration', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should capture packets when enabled', async ({ page }) => {
    // Navigate to bottom panel RAW tab (default)
    const rawTab = page.locator('.tabs button.tab:has-text("RAW")');
    await expect(rawTab).toHaveClass(/active/);

    // Verify capture is enabled by default
    const captureBtn = page.locator('.controls button.control-btn:has-text("Capturing")');
    await expect(captureBtn).toBeVisible();
    await expect(captureBtn).toHaveClass(/active/);
  });

  test('should clear packets when Clear button is clicked', async ({ page }) => {
    // Ensure RAW tab is active
    await page.click('.tabs button.tab:has-text("RAW")');

    // Click Clear button
    const clearBtn = page.locator('.controls button.control-btn:has-text("Clear")');
    await clearBtn.click();

    // Verify empty state is shown (since mock returns empty array after clear)
    const emptyState = page.locator('.packet-list .empty-state');
    await expect(emptyState).toBeVisible();
    await expect(emptyState).toContainText('No packets captured yet');
  });

  test('should toggle auto-scroll setting', async ({ page }) => {
    const autoScrollBtn = page.locator('.controls button.control-btn:has-text("Auto-scroll")');

    // Initially active
    await expect(autoScrollBtn).toHaveClass(/active/);

    // Toggle off
    await autoScrollBtn.click();
    await expect(autoScrollBtn).not.toHaveClass(/active/);

    // Toggle back on
    await autoScrollBtn.click();
    await expect(autoScrollBtn).toHaveClass(/active/);
  });

  test('should pause capture when Capturing button is clicked', async ({ page }) => {
    // Get capture button
    let captureBtn = page.locator('.controls button.control-btn:has-text("Capturing")');
    await expect(captureBtn).toHaveClass(/active/);

    // Click to pause
    await captureBtn.click();

    // Now button should show "Paused" and not be active
    const pausedBtn = page.locator('.controls button.control-btn:has-text("Paused")');
    await expect(pausedBtn).toBeVisible();
    await expect(pausedBtn).not.toHaveClass(/active/);
  });

  test('should resume capture when Paused button is clicked', async ({ page }) => {
    // First pause
    let captureBtn = page.locator('.controls button.control-btn:has-text("Capturing")');
    await captureBtn.click();

    // Now resume
    const pausedBtn = page.locator('.controls button.control-btn:has-text("Paused")');
    await pausedBtn.click();

    // Should be back to Capturing
    captureBtn = page.locator('.controls button.control-btn:has-text("Capturing")');
    await expect(captureBtn).toBeVisible();
    await expect(captureBtn).toHaveClass(/active/);
  });
});

test.describe('RAW Packet Display', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("RAW")');
  });

  test('should have packet list container', async ({ page }) => {
    const packetList = page.locator('.packet-list');
    await expect(packetList).toBeVisible();
  });

  test('should show empty state message correctly', async ({ page }) => {
    // Use specific selector for RAW view empty state
    const emptyState = page.locator('.raw-view-enhanced .empty-state');
    await expect(emptyState).toBeVisible();
    await expect(emptyState).toContainText('No packets captured yet');
  });

  test('should have working filter buttons', async ({ page }) => {
    const allBtn = page.locator('.raw-toolbar .filter-group button.filter-btn:has-text("All")');
    const txBtn = page.locator('.raw-toolbar .filter-group button.filter-btn:has-text("TX")');
    const rxBtn = page.locator('.raw-toolbar .filter-group button.filter-btn:has-text("RX")');

    // All should be active by default
    await expect(allBtn).toHaveClass(/active/);

    // Click TX
    await txBtn.click();
    await expect(txBtn).toHaveClass(/active/);
    await expect(allBtn).not.toHaveClass(/active/);

    // Click RX
    await rxBtn.click();
    await expect(rxBtn).toHaveClass(/active/);
    await expect(txBtn).not.toHaveClass(/active/);

    // Click All to reset
    await allBtn.click();
    await expect(allBtn).toHaveClass(/active/);
  });

  test('should have search input with correct placeholder', async ({ page }) => {
    const searchInput = page.locator('.raw-toolbar .search-input');
    await expect(searchInput).toBeVisible();
    await expect(searchInput).toHaveAttribute('placeholder', /Search hex/);
  });

  test('should have Copy button disabled when no packet selected', async ({ page }) => {
    const copyBtn = page.locator('.raw-toolbar .action-group button.action-btn:has-text("Copy")');
    await expect(copyBtn).toBeDisabled();
  });

  test('should have Export button always enabled', async ({ page }) => {
    const exportBtn = page.locator('.raw-toolbar .action-group button.action-btn:has-text("Export")');
    await expect(exportBtn).toBeEnabled();
  });

  test('should filter search input value', async ({ page }) => {
    const searchInput = page.locator('.raw-toolbar .search-input');
    await searchInput.fill('FF 00 AB');
    await expect(searchInput).toHaveValue('FF 00 AB');
  });
});

test.describe('PARSED Packet Display', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("PARSED")');
    // Wait for view to render
    await page.waitForSelector('.parsed-view-enhanced', { timeout: 2000 });
  });

  test('should display PARSED tab content', async ({ page }) => {
    const parsedView = page.locator('.parsed-view-enhanced');
    await expect(parsedView).toBeVisible();
  });

  test('should have toolbar in PARSED view', async ({ page }) => {
    const toolbar = page.locator('.parsed-view-enhanced .parsed-toolbar');
    await expect(toolbar).toBeVisible();
  });

  test('should have filter buttons in PARSED view', async ({ page }) => {
    const allBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("All")');
    const txBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("TX")');
    const rxBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("RX")');

    await expect(allBtn).toBeVisible();
    await expect(txBtn).toBeVisible();
    await expect(rxBtn).toBeVisible();
  });

  test('should show empty state in PARSED view', async ({ page }) => {
    const emptyState = page.locator('.parsed-view-enhanced .empty-state');
    await expect(emptyState).toBeVisible();
    await expect(emptyState).toContainText('No parsed packets captured yet');
  });

  test('should have search input for fields in PARSED view', async ({ page }) => {
    const searchInput = page.locator('.parsed-toolbar .search-input');
    await expect(searchInput).toBeVisible();
    await expect(searchInput).toHaveAttribute('placeholder', /Search fields/);
  });

  test('should have Copy button disabled when no packet selected', async ({ page }) => {
    const copyBtn = page.locator('.parsed-toolbar .action-group button.action-btn:has-text("Copy")');
    await expect(copyBtn).toBeDisabled();
  });

  test('should have Export button always enabled', async ({ page }) => {
    const exportBtn = page.locator('.parsed-toolbar .action-group button.action-btn:has-text("Export")');
    await expect(exportBtn).toBeEnabled();
  });

  test('All filter should be active by default in PARSED view', async ({ page }) => {
    const allBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("All")');
    await expect(allBtn).toHaveClass(/active/);
  });

  test('should filter packets by direction in PARSED view', async ({ page }) => {
    const txBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("TX")');
    const rxBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("RX")');
    const allBtn = page.locator('.parsed-toolbar .filter-group button.filter-btn:has-text("All")');

    // Click TX
    await txBtn.click();
    await expect(txBtn).toHaveClass(/active/);

    // Click RX
    await rxBtn.click();
    await expect(rxBtn).toHaveClass(/active/);

    // Click All
    await allBtn.click();
    await expect(allBtn).toHaveClass(/active/);
  });

  test('should allow typing in search input', async ({ page }) => {
    const searchInput = page.locator('.parsed-toolbar .search-input');
    await searchInput.fill('voltage');
    await expect(searchInput).toHaveValue('voltage');
  });
});

test.describe('Packet Capture State Persistence', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should maintain capture state across tab switches', async ({ page }) => {
    // Pause capture
    let captureBtn = page.locator('.controls button.control-btn:has-text("Capturing")');
    await captureBtn.click();

    // Verify paused
    let pausedBtn = page.locator('.controls button.control-btn:has-text("Paused")');
    await expect(pausedBtn).toBeVisible();

    // Switch to PARSED tab
    await page.click('.tabs button.tab:has-text("PARSED")');

    // Capture state should persist (still paused)
    pausedBtn = page.locator('.controls button.control-btn:has-text("Paused")');
    await expect(pausedBtn).toBeVisible();

    // Switch back to RAW
    await page.click('.tabs button.tab:has-text("RAW")');

    // Still paused
    pausedBtn = page.locator('.controls button.control-btn:has-text("Paused")');
    await expect(pausedBtn).toBeVisible();
  });

  test('should maintain auto-scroll state across tab switches', async ({ page }) => {
    // Disable auto-scroll
    const autoScrollBtn = page.locator('.controls button.control-btn:has-text("Auto-scroll")');
    await autoScrollBtn.click();
    await expect(autoScrollBtn).not.toHaveClass(/active/);

    // Switch to PARSED tab
    await page.click('.tabs button.tab:has-text("PARSED")');

    // Auto-scroll should still be off
    const autoScrollAfterSwitch = page.locator('.controls button.control-btn:has-text("Auto-scroll")');
    await expect(autoScrollAfterSwitch).not.toHaveClass(/active/);
  });
});
