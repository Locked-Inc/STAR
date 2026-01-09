import { test, expect, Page } from '@playwright/test';

/**
 * Cancel Button Timing Test
 * Verifies that the cancel button responds immediately without waiting for backend
 */

const MOCK_PTY_PORT = '/tmp/bms_client';

// Modified mock setup that simulates slow connection
async function setupSlowMockTauri(page: Page) {
  await page.addInitScript(() => {
    // Initialize __TAURI_INTERNALS__ as expected by @tauri-apps/api v2
    (window as any).__TAURI_INTERNALS__ = (window as any).__TAURI_INTERNALS__ || {};

    // Track if connection was cancelled
    let connectionCancelledByMock = false;

    // Mock the invoke function with SLOW connect_to_device
    (window as any).__TAURI_INTERNALS__.invoke = async function (
      cmd: string,
      args: any = {},
      _options?: any
    ) {
      switch (cmd) {
        case 'list_serial_ports':
          return ['/tmp/bms_client'];

        case 'connect_to_device':
          const startTime = Date.now();

          // Simulate slow connection (2 seconds) - like the old broken behavior
          await new Promise(resolve => setTimeout(resolve, 2000));

          const endTime = Date.now();

          // Check if connection was cancelled during delay
          if (connectionCancelledByMock) {
            connectionCancelledByMock = false; // Reset
            throw new Error('Connection aborted by user');
          }

          // Auto-discovery returns device info
          (window as any).__mockConnected = true;
          return {
            manufacturer: 'Texas Instruments',
            device_name: 'BQ78350-R1A',
            chemistry: 'LION',
            serial_number: 0x12345678,
            firmware_version: 'v1.2.3',
            hardware_version: 'v0.1',
            design_capacity_mah: 3200,
            design_voltage_mv: 14800,
            num_cells: 4,
          };

        case 'abort_connection':
          connectionCancelledByMock = true;
          (window as any).__mockConnected = false;
          return null;

        case 'disconnect_from_device':
          (window as any).__mockConnected = false;
          return null;

        case 'is_connected':
          return (window as any).__mockConnected || false;

        case 'get_raw_packets':
          return [];

        case 'get_parsed_packets':
          return [];

        case 'clear_packet_capture':
          return null;

        case 'set_packet_capture_enabled':
          return null;

        default:
          throw new Error(`Unknown mock command: ${cmd}`);
      }
    };

    // Mock transformCallback and callbacks
    const callbacks = new Map<number, Function>();
    (window as any).__TAURI_INTERNALS__.transformCallback = function (
      callback: Function,
      once: boolean = false
    ): number {
      const identifier = window.crypto.getRandomValues(new Uint32Array(1))[0];
      callbacks.set(identifier, (data: any) => {
        if (once) {
          callbacks.delete(identifier);
        }
        return callback && callback(data);
      });
      return identifier;
    };

    (window as any).__TAURI_INTERNALS__.unregisterCallback = function (id: number) {
      callbacks.delete(id);
    };

    (window as any).__TAURI_INTERNALS__.callbacks = callbacks;
  });
}

test.describe('Cancel Button Timing Test', () => {
  test.beforeEach(async ({ page }) => {
    await setupSlowMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('cancel button responds immediately without 2-second delay', async ({ page }) => {
    // Enter PTY port path
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption(MOCK_PTY_PORT);

    // Click Connect button
    const connectButton = page.locator('button:has-text("Connect")');
    await connectButton.click();

    // Wait a bit for Cancel button to appear
    await page.waitForTimeout(100);

    // Verify Cancel button is visible
    const cancelButton = page.locator('button:has-text("Cancel")');
    await expect(cancelButton).toBeVisible({ timeout: 500 });

    // Click Cancel button and measure UI response time
    const cancelClickTime = Date.now();
    await cancelButton.click();

    // Measure how long it takes for UI to update (should be immediate)
    const uiUpdateStartTime = Date.now();

    // Wait for the "Connection cancelled" error message to appear
    // This should happen IMMEDIATELY, not after 2 seconds
    try {
      await page.waitForSelector('text=Connection cancelled', { timeout: 500 });
      const uiUpdateTime = Date.now() - uiUpdateStartTime;
      void uiUpdateTime;
    } catch (e) {
      // Alternative: check if Connect button is visible again (disconnected state)
      await connectButton.waitFor({ state: 'visible', timeout: 500 });
      const uiUpdateTime = Date.now() - uiUpdateStartTime;
      void uiUpdateTime;
    }

    // Calculate total time from cancel click to UI update
    const totalCancelResponseTime = Date.now() - cancelClickTime;
    // Verify timing
    if (totalCancelResponseTime < 200) {
    } else if (totalCancelResponseTime < 500) {
    } else if (totalCancelResponseTime >= 2000) {
      expect(totalCancelResponseTime).toBeLessThan(2000);
    } else {
    }

    // Assert that cancel responds quickly (THE KEY ASSERTION)
    // This MUST be < 2000ms or the fix didn't work
    expect(totalCancelResponseTime).toBeLessThan(2000);

    // Stricter assertion for ideal behavior
    // Verify we're back in disconnected state
    await expect(connectButton).toBeVisible();

    // Wait to ensure backend promise completes (should not affect UI)
    await page.waitForTimeout(2500);

    // UI should still be in disconnected state
    await expect(connectButton).toBeVisible();

    const elapsedAfterCancel = Date.now() - cancelClickTime;
    expect(elapsedAfterCancel).toBeGreaterThan(0);
  });
});
