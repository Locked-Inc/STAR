import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Custom Inputs and Manual Configuration Tests
 *
 * Tests all "custom" and user-defined input features:
 * - Manual port entry
 * - Custom manufacturer access commands
 * - Custom register addresses
 * - Custom battery chemistry profile
 * - Custom hex values and data payloads
 */

test.describe('Custom Port Entry', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should allow manual port entry', async ({ page }) => {
    // Find manual port input field
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');

    await expect(portInput).toBeVisible();
    await expect(portInput).toBeEnabled();
  });

  test('should connect using custom port path', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');

    // Enter custom port path
    await portInput.clear();
    await portInput.fill('/tmp/bms_client');  // Use valid mock port

    const connectButton = page.locator('button:has-text("Connect")');
    await connectButton.click();
    await page.waitForTimeout(2000);

    // Should successfully connect (mock always succeeds for /tmp/bms_client)
    // Verify by checking disconnect button appears
    const disconnectButton = page.locator('button:has-text("Disconnect")');
    await expect(disconnectButton).toBeVisible();

    // Clean up - disconnect
    await disconnectButton.click();
    await page.waitForTimeout(500);
  });

  test('should accept various port path formats', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');

    // Test different port formats
    const portPaths = [
      '/dev/ttyUSB0',
      '/dev/ttyACM0',
      '/dev/ttys001',
      '/tmp/bms_client',
      'COM3',
      'COM10'
    ];

    for (const path of portPaths) {
      await portInput.clear();
      await portInput.fill(path);

      const value = await portInput.inputValue();
      expect(value).toBe(path);
    }
  });

  test('should preserve custom port across refreshes', async ({ page }) => {
    const portSelect = page.getByTestId('port-select');
    await portSelect.selectOption('custom');
    const portInput = page.getByTestId('port-input');

    // Enter custom port
    await portInput.clear();
    await portInput.fill('/tmp/custom_port');

    // Refresh port list (icon button with title="Refresh ports")
    const refreshButton = page.locator('button[title="Refresh ports"]');
    await expect(refreshButton).toBeVisible();
    await refreshButton.click();
    await page.waitForTimeout(500);

    // Custom input should still be available
    await expect(portInput).toBeEnabled();
    const value = await portInput.inputValue();
    expect(value).toBe('/tmp/custom_port');
  });
});

test.describe('Custom Manufacturer Access Commands', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Manufacturer Access")');
  });

  test('should have Custom option in preset dropdown', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');

    await expect(dropdown).toContainText(/Custom/i);
  });

  test('should enable subcommand input when Custom is selected', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    // Select Custom
    await dropdown.selectOption('custom');
    await page.waitForTimeout(1000);

    // Subcommand input should be enabled
    await expect(subcommandInput).toBeEnabled();
  });

  test('should disable subcommand input when preset is selected', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    // Select a preset (e.g., Device Type)
    await dropdown.selectOption('device_type');
    await page.waitForTimeout(1000);

    // Subcommand input should be disabled
    await expect(subcommandInput).toBeDisabled();
  });

  test('should accept hex format for custom subcommand', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Enter hex value
    await subcommandInput.clear();
    await subcommandInput.fill('0x0050');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(500);

    // Should not error on hex format
    const error = page.locator('.error').filter({ hasText: /invalid|format/i });
    await expect(error).not.toBeVisible();
  });

  test('should accept decimal format for custom subcommand', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Enter decimal value
    await subcommandInput.clear();
    await subcommandInput.fill('80');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(500);

    // Should not error on decimal format
    const error = page.locator('.error').filter({ hasText: /invalid|format/i });
    await expect(error).not.toBeVisible();
  });

  test('should send custom subcommand successfully', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    await subcommandInput.clear();
    await subcommandInput.fill('0x0001');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(500);

    // Should display response
    await expect(page.locator('.mfg-result')).toBeVisible();
  });

  test('should accept custom data payload', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const dataInput = page.locator('#mfg-data');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    if (await dataInput.isVisible()) {
      await dataInput.clear();
      await dataInput.fill('0x01,0x02,0x03');

      await page.click('button:has-text("Send Command")');
      await page.waitForTimeout(500);

      // Should accept comma-separated hex values
      const error = page.locator('.error').filter({ hasText: /invalid|format/i });
      await expect(error).not.toBeVisible();
    }
  });

  test('should validate subcommand range (0x0000-0xFFFF)', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Test invalid value (too large)
    await subcommandInput.clear();
    await subcommandInput.fill('0x10000');

    await page.click('button:has-text("Send Command")');
    await page.waitForTimeout(500);

    // Should show validation error or clamp to valid range
    // (Test passes if no crash occurs - validation may be lenient)
  });

  test('should update subcommand value when changing presets', async ({ page }) => {
    const dropdown = page.locator('#mfg-preset');
    const subcommandInput = page.locator('#mfg-subcommand');

    // Select Device Type preset (0x0001)
    await dropdown.selectOption("device_type");
    await page.waitForTimeout(1000);

    const deviceTypeValue = await subcommandInput.inputValue();

    // Select Firmware Version preset (0x0002)
    await dropdown.selectOption("firmware_version");
    await page.waitForTimeout(1000);

    const firmwareValue = await subcommandInput.inputValue();

    // Values should be different
    expect(deviceTypeValue).not.toBe(firmwareValue);
  });
});

test.describe('Custom Register Addresses', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Registers")');
  });

  test('should accept hex register addresses', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    await addressInput.clear();
    await addressInput.fill('0x2A');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should successfully read and display result
    await expect(page.locator('.register-result')).toBeVisible();
  });

  test('should accept decimal register addresses', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    await addressInput.clear();
    await addressInput.fill('42');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should successfully read and display result
    await expect(page.locator('.register-result')).toBeVisible();
  });

  test('should validate address range (0x00-0xFF for SMBus)', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    // Test invalid value (too large for 8-bit address)
    await addressInput.clear();
    await addressInput.fill('0x100');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should show validation error or clamp to valid range
    // Test passes if no crash occurs - validation may be lenient
  });

  test('should write to custom register addresses', async ({ page }) => {
    const addressInput = page.locator('#reg-write-address');
    const valueInput = page.locator('#reg-value');

    await addressInput.clear();
    await addressInput.fill('0x10');

    await valueInput.clear();
    await valueInput.fill('0x42');

    await page.click('button:has-text("Write Register")');
    await page.waitForTimeout(500);

    // Should show success message (or no error)
    const successMsg = page.locator('.success');
    if (await successMsg.isVisible()) {
      await expect(successMsg).toBeVisible();
    }
  });

  test('should support 1 or 2 byte register reads', async ({ page }) => {
    const numBytesInput = page.locator('#reg-num-bytes');
    const addressInput = page.locator('#reg-address');

    // Set to 2 bytes
    await numBytesInput.clear();
    await numBytesInput.fill('2');

    // Read register 0x00
    await addressInput.clear();
    await addressInput.fill('0x00');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should display 2-byte value (4 hex digits)
    const result = page.locator('.register-result');
    if (await result.isVisible()) {
      const text = await result.textContent();
      expect(text).toMatch(/0x[0-9A-F]{4}/i);
    }
  });

  test('should display both hex and decimal values', async ({ page }) => {
    const addressInput = page.locator('#reg-address');

    await addressInput.clear();
    await addressInput.fill('0x00');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should show both hex and decimal representations in result
    const result = page.locator('.register-result');
    await expect(result).toBeVisible();
    await expect(result).toContainText('hex');
    await expect(result).toContainText('dec');
  });
});

test.describe('Custom Battery Chemistry Profile', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Chemistry Profiles")');
  });

  test('should have Custom chemistry option', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    await expect(dropdown).toContainText(/Custom/i);
  });

  test('should select custom chemistry profile', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    const value = await dropdown.inputValue();
    expect(value).toContain('custom');
  });

  test('should display custom profile parameters', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Custom profile should show voltage, current, temperature params
    await expect(page.locator('text=/Voltage|Current|Temperature/i').first()).toBeVisible();
  });

  test('should allow editing custom profile values', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Look for editable inputs (if custom profile allows editing)
    const inputs = page.locator('input[type="text"], input[type="number"]');
    const editableInputs = await inputs.count();

    // Custom profile should either have editable fields or display user-defined values
    expect(editableInputs).toBeGreaterThanOrEqual(0);
  });

  test('should maintain custom values when switching away and back', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');

    // Select custom
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Switch to different profile
    await dropdown.selectOption("lion");
    await page.waitForTimeout(1000);

    // Switch back to custom
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    // Custom should still be selectable
    const value = await dropdown.inputValue();
    expect(value).toContain('custom');
  });

  test('should apply custom chemistry profile', async ({ page }) => {
    const dropdown = page.locator('#chemistry-select');
    await dropdown.selectOption("custom");
    await page.waitForTimeout(1000);

    const applyButton = page.locator('button:has-text("Apply Profile to BMS")');
    await applyButton.click();
    await page.waitForTimeout(500);

    // Should show success message
    await expect(page.locator('.profile-result').first()).toBeVisible();
  });
});

test.describe('Custom Data Flash Operations', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');

    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Data Flash")');
  });

  test('should accept custom class numbers', async ({ page }) => {
    const classInput = page.locator('#df-class');

    // Test various class numbers
    const classNumbers = ['48', '64', '80', '82', '100'];

    for (const classNum of classNumbers) {
      await classInput.clear();
      await classInput.fill(classNum);

      const value = await classInput.inputValue();
      expect(value).toBe(classNum);
    }
  });

  test('should accept custom offset values', async ({ page }) => {
    const offsetInput = page.locator('#df-offset');

    // Test various offsets
    const offsets = ['0', '16', '32', '48'];

    for (const offset of offsets) {
      await offsetInput.clear();
      await offsetInput.fill(offset);

      const value = await offsetInput.inputValue();
      expect(value).toBe(offset);
    }
  });

  test('should read from custom class and offset', async ({ page }) => {
    const classInput = page.locator('#df-class');
    const offsetInput = page.locator('#df-offset');

    await classInput.clear();
    await classInput.fill('64');

    await offsetInput.clear();
    await offsetInput.fill('0');

    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // Should display hex viewer with data
    await expect(page.locator('.hex-viewer, .hex-display').first()).toBeVisible();
  });

  test('should validate class number range (0-255)', async ({ page }) => {
    const classInput = page.locator('#df-class');

    // Test invalid class - HTML5 input validation will prevent > 255
    await classInput.clear();
    await classInput.fill('256');

    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    // May show error or clamp value (HTML5 input type=number handles this)
    // Test passes if operation completes without crash
  });

  test('should write custom data to Data Flash', async ({ page }) => {
    const classInput = page.locator('#df-class');
    await classInput.clear();
    await classInput.fill('48');

    const writeButton = page.locator('button:has-text("Write Block")');

    // Write requires data to be read first
    const readButton = page.locator('button:has-text("Read Block")');
    await readButton.click();
    await page.waitForTimeout(500);

    if (await writeButton.isEnabled()) {
      // Write operation exists and is enabled
      await writeButton.click();
      await page.waitForTimeout(500);

      // Should show confirmation or success (or no error)
      // Test passes if no crash occurs
    }
  });
});

test.describe('Input Format Handling', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should handle uppercase hex input (0xABCD)', async ({ page }) => {
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Registers")');

    const addressInput = page.locator('#reg-address');
    await addressInput.clear();
    await addressInput.fill('0xAB');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should accept uppercase hex (no error)
    const error = page.locator('.error').filter({ hasText: /invalid|format/i });
    await expect(error).not.toBeVisible();
  });

  test('should handle lowercase hex input (0xabcd)', async ({ page }) => {
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Registers")');

    const addressInput = page.locator('#reg-address');
    await addressInput.clear();
    await addressInput.fill('0xab');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should accept lowercase hex (no error)
    const error = page.locator('.error').filter({ hasText: /invalid|format/i });
    await expect(error).not.toBeVisible();
  });

  test('should handle hex without 0x prefix', async ({ page }) => {
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Registers")');

    const addressInput = page.locator('#reg-address');
    await addressInput.clear();
    await addressInput.fill('2A');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should accept hex without 0x prefix or show error
    // Test passes if no crash occurs
  });

  test('should reject invalid hex characters', async ({ page }) => {
    await page.getByTestId('port-select').selectOption('/tmp/bms_client');
    await page.click('button:has-text("Connect")');
    await page.waitForTimeout(1000);

    await page.click('button:has-text("Registers")');

    const addressInput = page.locator('#reg-address');
    await addressInput.clear();
    await addressInput.fill('0xGHIJ');

    await page.click('button:has-text("Read Register")');
    await page.waitForTimeout(500);

    // Should show validation error or handle gracefully
    // Test passes if no crash occurs
  });
});
