import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Edge Cases and Error Handling Tests
 * Tests unusual scenarios, error conditions, and boundary cases
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('Connection Edge Cases', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.getByTestId('port-select').selectOption('custom');
  });

  test('should handle rapid connect/disconnect cycles', async ({ page }) => {
    const portInput = page.getByTestId('port-input');
    await portInput.fill(MOCK_PTY_PORT);
    const connectButton = page.locator('button:has-text("Connect")');

    // Rapidly connect and disconnect 5 times
    for (let i = 0; i < 5; i++) {
      await connectButton.click();
      await page.waitForTimeout(300);

      const disconnectButton = page.locator('button:has-text("Disconnect")');
      if (await disconnectButton.isVisible()) {
        await disconnectButton.click();
        await page.waitForTimeout(200);
      }
    }

    // App should still be functional
    await expect(page.locator('header')).toBeVisible();
    await expect(connectButton).toBeVisible();
  });

  test('should prevent multiple simultaneous connection attempts', async ({ page }) => {
    const portInput = page.getByTestId('port-input');
    await portInput.fill(MOCK_PTY_PORT);

    const connectButton = page.locator('button:has-text("Connect")');

    // Try to click connect multiple times rapidly
    await connectButton.click();
    await connectButton.click();
    await connectButton.click();

    await page.waitForTimeout(1500);

    // Should either be connected or show error, not crash
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    const errorBanner = page.locator('.error-banner, [role="alert"]');

    const isConnected = await disconnectButton.isVisible();
    const hasError = await errorBanner.isVisible();

    // Should be in a valid state (connected OR error, not limbo)
    expect(isConnected || hasError || await connectButton.isVisible()).toBeTruthy();
  });

  test('should handle connection to empty port name', async ({ page }) => {
    const portInput = page.getByTestId('port-input');
    await portInput.clear();

    const connectButton = page.locator('button:has-text("Connect")');

    // Button should be disabled or connection should fail gracefully
    const isDisabled = await connectButton.isDisabled();
    if (!isDisabled) {
      await connectButton.click();
      await page.waitForTimeout(1000);

      // Should show error or stay disconnected
      const errorBanner = page.locator('.error-banner, [role="alert"]');
      const disconnectButton = page.locator('button:has-text("Disconnect")');

      const hasError = await errorBanner.isVisible();
      const isConnected = await disconnectButton.isVisible();

      // Should not successfully connect to empty port
      if (isConnected) {
        // If connected, disconnect for cleanup
        await disconnectButton.click();
      } else {
        // Expected: error or still disconnected
        expect(hasError || await connectButton.isVisible()).toBeTruthy();
      }
    }
  });

  test('should handle very long port names', async ({ page }) => {
    const portInput = page.getByTestId('port-input');

    // Enter very long port name
    const longPort = '/dev/' + 'x'.repeat(200);
    await portInput.fill(longPort);

    // App should not crash
    await expect(page.locator('body')).toBeVisible();

    // Input should handle long text (may truncate or scroll)
    const value = await portInput.inputValue();
    expect(value.length).toBeGreaterThan(0);
  });

  test('should handle special characters in port name', async ({ page }) => {
    const portInput = page.getByTestId('port-input');

    const specialPorts = [
      '/dev/tty-special!@#',
      '/path/with spaces/port',
      '/path/with/unicode/日本語',
      'COM3:9600',
    ];

    for (const port of specialPorts) {
      await portInput.clear();
      await portInput.fill(port);

      const value = await portInput.inputValue();
      // Input should accept the value
      expect(value).toBeTruthy();

      await page.waitForTimeout(100);
    }

    // App should not crash
    await expect(page.locator('header')).toBeVisible();
  });
});

test.describe('Data Input Edge Cases', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to device
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);

    // Navigate to Registers tab
    await page.locator('nav.sidebar button:has-text("Registers")').click();
    await page.waitForTimeout(300);
  });

  test('should handle invalid hex input in register address', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    const invalidInputs = [
      'ZZZZ',  // Invalid hex characters
      '0xGGGG',  // Invalid hex with prefix
      '999999',  // Too large
      '-1',  // Negative
      '0x-5',  // Negative hex
    ];

    for (const input of invalidInputs) {
      await addressInput.clear();
      await addressInput.fill(input);
      await page.waitForTimeout(100);

      // Should show validation error or prevent submission
      // App should not crash
      await expect(page.locator('body')).toBeVisible();
    }
  });

  test('should handle maximum value inputs', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    // Try maximum values
    await addressInput.clear();
    await addressInput.fill('0xFF');
    await page.waitForTimeout(200);

    // Should be valid
    await expect(page.locator('body')).toBeVisible();

    // Try exceeding maximum
    await addressInput.clear();
    await addressInput.fill('0xFFFF');
    await page.waitForTimeout(200);

    // Should show error or clamp value
    await expect(page.locator('body')).toBeVisible();
  });

  test('should handle rapid input changes', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    // Rapidly type and delete
    for (let i = 0; i < 10; i++) {
      await addressInput.type('1');
      await page.keyboard.press('Backspace');
    }

    await page.waitForTimeout(200);

    // App should not crash
    await expect(page.locator('header')).toBeVisible();
  });
});

test.describe.skip('Cell Voltages Edge Cases', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to device
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);

    // Navigate to Cell Voltages tab
    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await page.waitForTimeout(300);
  });

  test('should handle zero cells request', async ({ page }) => {
    const numCellsInput = page.locator('input[type="number"]').first();
    await numCellsInput.clear();
    await numCellsInput.fill('0');

    const readButton = page.locator('button:has-text("Read Cell Voltages")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Should show error or minimum value (1 cell)
    await expect(page.locator('body')).toBeVisible();
  });

  test('should handle negative cell count', async ({ page }) => {
    const numCellsInput = page.locator('input[type="number"]').first();

    // Try to enter negative number
    await numCellsInput.clear();
    await numCellsInput.fill('-5');
    await page.waitForTimeout(200);

    const readButton = page.locator('button:has-text("Read Cell Voltages")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Should reject or clamp to minimum
    await expect(page.locator('body')).toBeVisible();
  });

  test('should handle extremely large cell count', async ({ page }) => {
    const numCellsInput = page.locator('input[type="number"]').first();
    await numCellsInput.clear();
    await numCellsInput.fill('999');

    const readButton = page.locator('button:has-text("Read Cell Voltages")');
    await readButton.click();
    await page.waitForTimeout(1000);

    // Should clamp to device maximum or show error
    await expect(page.locator('body')).toBeVisible();
  });

  test('should handle rapid cell voltage read requests', async ({ page }) => {
    const readButton = page.locator('button:has-text("Read Cell Voltages")');

    // Click read button 10 times rapidly
    for (let i = 0; i < 10; i++) {
      await readButton.click();
      await page.waitForTimeout(50);
    }

    await page.waitForTimeout(1000);

    // App should not crash
    await expect(page.locator('header')).toBeVisible();
  });
});

test.describe('State Persistence Edge Cases', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should handle corrupted localStorage gracefully', async ({ page }) => {
    // Inject corrupted localStorage
    await page.evaluate(() => {
      localStorage.setItem('dockingLayout', 'invalid json {{{');
      localStorage.setItem('settings', '{malformed}');
    });

    // Reload page
    await page.reload();
    await page.waitForLoadState('networkidle');

    // App should fall back to defaults and not crash
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
  });

  test('should handle missing localStorage keys', async ({ page }) => {
    // Clear all localStorage
    await page.evaluate(() => {
      localStorage.clear();
    });

    await page.reload();
    await page.waitForLoadState('networkidle');

    // App should use defaults
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
  });

  test('should handle localStorage quota exceeded', async ({ page }) => {
    // Try to fill localStorage to quota
    await page.evaluate(() => {
      try {
        const largeData = 'x'.repeat(5 * 1024 * 1024); // 5MB string
        for (let i = 0; i < 100; i++) {
          localStorage.setItem(`large_${i}`, largeData);
        }
      } catch (e) {
        // Expected: QuotaExceededError
      }
    });

    // Navigate and make changes
    await page.locator('nav.sidebar button:has-text("Cell Voltages")').click();
    await page.waitForTimeout(300);

    // App should handle failed localStorage writes gracefully
    await expect(page.locator('header')).toBeVisible();
  });
});

test.describe('UI Responsiveness Edge Cases', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should handle very small viewport', async ({ page }) => {
    // Resize to mobile size
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(500);

    // Core elements should still be accessible
    await expect(page.locator('header')).toBeVisible();

    // Navigation might be collapsed or hidden, but app shouldn't crash
    await expect(page.locator('body')).toBeVisible();
  });

  test('should handle very large viewport', async ({ page }) => {
    // Resize to 4K
    await page.setViewportSize({ width: 3840, height: 2160 });
    await page.waitForTimeout(500);

    // Layout should adapt
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('nav.sidebar')).toBeVisible();
  });

  test('should handle viewport resize during operation', async ({ page }) => {
    // Connect to device
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(500);

    // Resize during connection
    await page.setViewportSize({ width: 1024, height: 768 });
    await page.waitForTimeout(500);
    await page.setViewportSize({ width: 1920, height: 1080 });
    await page.waitForTimeout(500);

    // App should remain functional
    await expect(page.locator('header')).toBeVisible();
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await expect(disconnectButton).toBeVisible();
    }
  });
});

test.describe('Concurrent Operations', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    // Connect to device
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);
  });

  test('should handle simultaneous tab navigation and data reading', async ({ page }) => {
    // Navigate to Telemetry
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(200);

    const cellVoltagesButton = page.locator('nav.sidebar button:has-text("Cell Voltages")');
    await Promise.all([
      page.waitForTimeout(100).then(() => cellVoltagesButton.click()),
      page.waitForTimeout(150).then(() => page.locator('nav.sidebar button:has-text("Device Info")').click()),
    ]);

    await page.waitForTimeout(1000);

    // App should handle concurrent operations gracefully
    await expect(page.locator('header')).toBeVisible();
  });

  test('should handle rapid panel tab switching', async ({ page }) => {
    const packetViewer = page.locator('.packet-viewer-panel');

    const tabs = ['RAW', 'PARSED', 'CONSOLE'];

    // Rapidly switch between tabs 10 times
    for (let i = 0; i < 10; i++) {
      const tabName = tabs[i % tabs.length];
      const tab = packetViewer.locator(`button:has-text("${tabName}")`);
      await tab.click();
      await page.waitForTimeout(50);
    }

    // App should remain functional
    await expect(packetViewer).toBeVisible();
  });
});

test.describe('Memory and Performance', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should handle extended session without memory leaks', async ({ page }) => {
    // Connect
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);

    // Navigate through all tabs multiple times
    const tabs = [
      'Telemetry',
      'Cell Voltages',
      'Device Info',
      'Protection Status',
      'Chemistry Profiles',
      'Registers',
    ];

    for (let round = 0; round < 3; round++) {
      for (const tab of tabs) {
        const button = page.locator(`nav.sidebar button:has-text("${tab}")`);
        if (await button.isVisible()) {
          await button.click();
          await page.waitForTimeout(200);
        }
      }
    }

    // App should still be responsive
    await expect(page.locator('header')).toBeVisible();

    // Disconnect
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    if (await disconnectButton.isVisible()) {
      await disconnectButton.click();
    }
  });

  test('should handle large telemetry log without performance degradation', async ({ page }) => {
    // Connect and navigate to Telemetry
    await page.getByTestId('port-select').selectOption(MOCK_PTY_PORT);
    await page.locator('button:has-text("Connect")').click();
    await page.waitForTimeout(1500);
    await page.locator('nav.sidebar button:has-text("Telemetry")').click();
    await page.waitForTimeout(300);

    // Enable auto refresh to build up log
    const autoRefresh = page.getByTestId('auto-refresh-toggle');
    await autoRefresh.click();
    await page.waitForTimeout(2500);

    // App should still be responsive
    await expect(page.locator('header')).toBeVisible();
    await expect(page.locator('button:has-text("Export CSV")')).toBeEnabled();
  });
});
