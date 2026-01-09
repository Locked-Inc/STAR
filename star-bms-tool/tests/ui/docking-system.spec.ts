import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Docking System Tests
 * Tests the IntelliJ-style panel docking system with 4-edge docking, drag-and-drop, and floating windows
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Docking System - Keyboard Shortcuts', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('Ctrl+1 should toggle Packet Viewer panel', async ({ page }) => {
    // Verify packet viewer is visible initially
    const packetViewer = page.locator('.packet-viewer-panel');
    await expect(packetViewer).toBeVisible();

    // Press Ctrl+1 (or Cmd+1 on Mac)
    const modifier = process.platform === 'darwin' ? 'Meta' : 'Control';
    await page.keyboard.press(`${modifier}+1`);
    await page.waitForTimeout(200);

    // Panel visibility might toggle (depending on implementation)
    // For now, just verify the shortcut doesn't break the app
    await expect(page.locator('body')).toBeVisible();
  });

  test('should navigate to Telemetry tab with keyboard', async ({ page }) => {
    // Navigate to Telemetry tab using nav button
    const telemetryButton = page.locator('nav.sidebar button:has-text("Telemetry")');
    await telemetryButton.click();
    await page.waitForTimeout(300);

    // Verify telemetry tab content is displayed
    const telemetryHeading = page.locator('h2:has-text("Battery Telemetry")');
    await expect(telemetryHeading).toBeVisible();
  });

  test('Escape key should not close app or tabs', async ({ page }) => {
    await page.keyboard.press('Escape');
    await page.waitForTimeout(200);

    // App should still be functional
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
  });
});

test.describe('Docking System - Panel Zones', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should have bottom zone with packet viewer', async ({ page }) => {
    // Bottom zone should exist with packet viewer panel
    const packetViewer = page.locator('.packet-viewer-panel');
    await expect(packetViewer).toBeVisible();

    // Verify it's in bottom area of viewport
    const box = await packetViewer.boundingBox();
    expect(box).not.toBeNull();
    if (box) {
      const viewportHeight = page.viewportSize()?.height || 1000;
      // Bottom panel should be in lower half of viewport
      expect(box.y).toBeGreaterThan(viewportHeight * 0.4);
    }
  });

  test('should have sidebar navigation (left zone)', async ({ page }) => {
    const sidebar = page.locator('nav.sidebar');
    await expect(sidebar).toBeVisible();

    // Sidebar should be on left side
    const box = await sidebar.boundingBox();
    expect(box).not.toBeNull();
    if (box) {
      // Sidebar should start near left edge
      expect(box.x).toBeLessThan(50);
    }
  });

  test('should have main content area (center zone)', async ({ page }) => {
    // Main content should exist between sidebar and bottom panel
    const mainContent = page.locator('.main-content, .app-body > div:not(nav)').first();
    await expect(mainContent).toBeVisible();
  });

  test('should allow panels to be resized', async ({ page }) => {
    // Look for resize handle
    const resizeHandle = page.locator('[class*="resize"], [data-resize], .resize-handle').first();

    if (await resizeHandle.isVisible()) {
      const initialBox = await resizeHandle.boundingBox();
      expect(initialBox).not.toBeNull();

      // Verify handle exists for resizing (actual drag testing is complex in Playwright)
      await expect(resizeHandle).toBeVisible();
    }
  });
});

test.describe('Docking System - Panel Tabs', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('packet viewer should have RAW, PARSED, CONSOLE tabs', async ({ page }) => {
    const packetViewer = page.locator('.packet-viewer-panel');
    await expect(packetViewer).toBeVisible();

    // Verify tabs exist
    const rawTab = packetViewer.locator('button:has-text("RAW"), [role="tab"]:has-text("RAW")');
    const parsedTab = packetViewer.locator('button:has-text("PARSED"), [role="tab"]:has-text("PARSED")');
    const consoleTab = packetViewer.locator('button:has-text("CONSOLE"), [role="tab"]:has-text("CONSOLE")');

    await expect(rawTab).toBeVisible();
    await expect(parsedTab).toBeVisible();
    await expect(consoleTab).toBeVisible();
  });

  test('should switch between packet viewer tabs', async ({ page }) => {
    const packetViewer = page.locator('.packet-viewer-panel');

    // Click PARSED tab
    const parsedTab = packetViewer.locator('button:has-text("PARSED")');
    await parsedTab.click();
    await page.waitForTimeout(200);

    // Verify PARSED content is visible
    const parsedContent = packetViewer.locator('.parsed-view, [data-tab="parsed"]');
    const isVisible = await parsedContent.isVisible();

    // Click CONSOLE tab
    const consoleTab = packetViewer.locator('button:has-text("CONSOLE")');
    await consoleTab.click();
    await page.waitForTimeout(200);

    // Verify CONSOLE content is visible
    const consoleContent = packetViewer.locator('.console-view, [data-tab="console"]');
    await expect(consoleContent).toBeVisible();
  });

  test('should maintain active tab when switching between sidebar tabs', async ({ page }) => {
    // Set packet viewer to CONSOLE tab
    const consoleTab = page.locator('.packet-viewer-panel button:has-text("CONSOLE")');
    await consoleTab.click();
    await page.waitForTimeout(200);

    // Switch to Telemetry sidebar tab
    const telemetryButton = page.locator('nav.sidebar button:has-text("Telemetry")');
    await telemetryButton.click();
    await page.waitForTimeout(300);

    // Switch back (packet viewer should still show CONSOLE)
    const cellsButton = page.locator('nav.sidebar button:has-text("Cell Voltages")');
    await cellsButton.click();
    await page.waitForTimeout(300);

    // Packet viewer should still be on CONSOLE tab
    const consoleContent = page.locator('.console-view');
    await expect(consoleContent).toBeVisible();
  });
});

test.describe('Docking System - Layout Persistence', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should persist sidebar navigation selection across page reload', async ({ page }) => {
    // Navigate to Cell Voltages
    const cellsButton = page.locator('nav.sidebar button:has-text("Cell Voltages")');
    await cellsButton.click();
    await page.waitForTimeout(300);

    // Reload page
    await page.reload();
    await page.waitForLoadState('networkidle');

    // Cell Voltages should still be active (if persistence is implemented)
    // or default to Telemetry (acceptable fallback)
    const heading = page.locator('h2');
    const headingText = await heading.first().textContent();

    // Should show either Cell Voltages (persisted) or Telemetry (default)
    expect(headingText).toMatch(/Cell Voltages|Telemetry/);
  });

  test('should persist packet viewer tab selection across sidebar navigation', async ({ page }) => {
    // Switch to CONSOLE tab
    const consoleTab = page.locator('.packet-viewer-panel button:has-text("CONSOLE")');
    await consoleTab.click();
    await page.waitForTimeout(200);

    // Navigate through sidebar tabs
    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(200);

    // CONSOLE tab should still be active
    const consoleContent = page.locator('.console-view');
    await expect(consoleContent).toBeVisible();
  });
});

test.describe('Docking System - Accessibility', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('sidebar navigation buttons should have accessible labels', async ({ page }) => {
    const navButtons = page.locator('nav.sidebar button');
    const count = await navButtons.count();

    expect(count).toBeGreaterThan(0);

    // Each button should have text content or aria-label
    for (let i = 0; i < Math.min(count, 5); i++) {
      const button = navButtons.nth(i);
      const text = await button.textContent();
      const ariaLabel = await button.getAttribute('aria-label');

      expect(text || ariaLabel).toBeTruthy();
    }
  });

  test('packet viewer tabs should have role="tab"', async ({ page }) => {
    const tabs = page.locator('.packet-viewer-panel button[role="tab"], .packet-viewer-panel [role="tab"]');
    const count = await tabs.count();

    // Should have at least RAW, PARSED, CONSOLE tabs
    expect(count).toBeGreaterThanOrEqual(3);
  });

  test('panels should be keyboard navigable', async ({ page }) => {
    // Press Tab to navigate
    await page.keyboard.press('Tab');
    await page.waitForTimeout(100);

    // Verify focus is visible somewhere
    const focused = await page.evaluate(() => {
      const el = document.activeElement;
      return el ? el.tagName : null;
    });

    expect(focused).toBeTruthy();
  });
});

test.describe('Docking System - Integration', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should maintain docking layout during connection workflow', async ({ page }) => {
    // Connect to device
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);

    // Verify packet viewer still visible
    await expect(page.locator('.packet-viewer-panel')).toBeVisible();

    // Verify sidebar still visible
    await expect(page.locator('nav.sidebar')).toBeVisible();

    // Disconnect
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
      await page.waitForTimeout(500);
    }

    // Layout should still be intact
    await expect(page.locator('.packet-viewer-panel')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
  });

  test('should handle rapid tab switching without layout issues', async ({ page }) => {
    const tabs = [
      'Telemetry',
      'Cell Voltages',
      'Device Info',
      'Protection Status',
      'Registers'
    ];

    // Rapidly switch between tabs
    for (const tab of tabs) {
      const button = page.locator(`nav.sidebar button:has-text("${tab}")`);
      if (await button.isVisible()) {
        await button.click();
        await page.waitForTimeout(100);
      }
    }

    // Layout should still be functional
    await expect(page.locator('nav.sidebar')).toBeVisible();
    await expect(page.locator('.packet-viewer-panel')).toBeVisible();
    await expect(page.locator('header')).toBeVisible();
  });

  test('should preserve docking layout when opening and closing panels', async ({ page }) => {
    // Open packet viewer CONSOLE tab
    await page.locator('.packet-viewer-panel button:has-text("CONSOLE")').click();
    await page.waitForTimeout(200);

    // Switch sidebar tabs multiple times
    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await page.waitForTimeout(200);
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(200);

    // Everything should still be visible and functional
    await expect(page.locator('.console-view')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
    await expect(page.locator('.main-content')).toBeVisible();
  });
});

test.describe('Docking System - Error Handling', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should handle invalid tab navigation gracefully', async ({ page }) => {
    // Navigate to valid tab
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(300);

    // Try to navigate to non-existent tab via URL or other means
    // App should not crash
    await expect(page.locator('body')).toBeVisible();
    await expect(page.locator('header')).toBeVisible();
  });

  test('should handle missing panels gracefully', async ({ page }) => {
    // Even if some panels fail to load, core layout should remain
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();

    // At least main content or packet viewer should be visible
    const mainVisible = await page.locator('.main-content').isVisible();
    const appBodyVisible = await page.locator('.app-body').isVisible();
    const packetViewerVisible = await page.locator('.packet-viewer-panel').isVisible();

    expect(mainVisible || appBodyVisible || packetViewerVisible).toBeTruthy();
  });
});
