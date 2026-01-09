import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

test.describe('Floating Panels', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('floating a panel removes it from the main dock and can be docked back', async ({ page }) => {
    const packetPanel = page.locator('[data-panel-id="packet-viewer"]');
    await expect(packetPanel).toBeVisible();

    const floatButton = page.locator('.panel-header .header-action-btn[title="Float panel in new window"]');
    await floatButton.click();

    await expect(packetPanel).toBeHidden();

    const windowTab = page.locator('button.window-tab:has-text("Packet Viewer")');
    await expect(windowTab).toHaveClass(/floating/);

    await windowTab.click();
    await expect(packetPanel).toBeVisible();
    await expect(windowTab).not.toHaveClass(/floating/);
  });
});
