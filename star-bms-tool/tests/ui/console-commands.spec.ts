import { test, expect } from '@playwright/test';
import { setupMockTauri } from './test-setup';

/**
 * Console Command Tests
 * Comprehensive tests for all console commands in the BMS Tool CLI
 */

test.describe('Console Command Execution', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should execute help command and show all commands', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Available commands:');
    await expect(output).toContainText('connect');
    await expect(output).toContainText('disconnect');
    await expect(output).toContainText('status');
    await expect(output).toContainText('telemetry');
    await expect(output).toContainText('cells');
    await expect(output).toContainText('info');
    await expect(output).toContainText('read');
    await expect(output).toContainText('write');
    await expect(output).toContainText('clear');
  });

  test('should show error for unknown command', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('invalidcommand');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Unknown command: invalidcommand');
    await expect(output).toContainText('Type "help" for available commands');
  });

  test('should execute clear command', async ({ page }) => {
    const input = page.locator('.console-input');

    // Execute help first to add some output
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Execute clear
    await input.fill('clear');
    await input.press('Enter');

    // Console should be cleared with only success message
    const output = page.locator('.console-output');
    await expect(output).toContainText('Console cleared');
  });

  test('should show status error when not connected', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('status');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Not connected');
  });

  test('should show connect usage when no port specified', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('connect');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Usage: connect');
  });

  test('should show read usage when no address specified', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('read');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Usage: read');
  });

  test('should show write usage when insufficient args', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('write');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Usage: write');
  });

  test('should show write usage when only address provided', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('write 0x10');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Usage: write');
  });

  test('should clear input after command execution', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');

    await expect(input).toHaveValue('');
  });

  test('should display command in output with $ prefix', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');

    const commandLine = page.locator('.console-line.command').first();
    await expect(commandLine).toContainText('help');
  });

  test('should show timestamps for all output lines', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');

    const timestamps = page.locator('.console-timestamp');
    const count = await timestamps.count();
    expect(count).toBeGreaterThan(0);
  });
});

test.describe('Console Controls', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should have Clear button in controls', async ({ page }) => {
    const clearBtn = page.locator('.console-controls button.control-btn:has-text("Clear")');
    await expect(clearBtn).toBeVisible();
  });

  test('should have Copy button in controls', async ({ page }) => {
    const copyBtn = page.locator('.console-controls button.control-btn:has-text("Copy")');
    await expect(copyBtn).toBeVisible();
  });

  test('should have Export button in controls', async ({ page }) => {
    const exportBtn = page.locator('.console-controls button.control-btn:has-text("Export")');
    await expect(exportBtn).toBeVisible();
  });

  test('should have Auto-scroll button in controls', async ({ page }) => {
    const autoScrollBtn = page.locator('.console-controls button.control-btn:has-text("Auto-scroll")');
    await expect(autoScrollBtn).toBeVisible();
  });

  test('should clear console when Clear button clicked', async ({ page }) => {
    const input = page.locator('.console-input');

    // Add some content
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Click Clear button
    const clearBtn = page.locator('.console-controls button.control-btn:has-text("Clear")');
    await clearBtn.click();

    // Should show console cleared message
    const output = page.locator('.console-output');
    await expect(output).toContainText('Console cleared');
  });

  test('should toggle auto-scroll when button clicked', async ({ page }) => {
    const autoScrollBtn = page.locator('.console-controls button.control-btn:has-text("Auto-scroll")');

    // Initially active
    await expect(autoScrollBtn).toHaveClass(/active/);

    // Toggle off
    await autoScrollBtn.click();
    await expect(autoScrollBtn).not.toHaveClass(/active/);

    // Toggle on
    await autoScrollBtn.click();
    await expect(autoScrollBtn).toHaveClass(/active/);
  });
});

test.describe('Console Command History', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should navigate command history with arrow up', async ({ page }) => {
    const input = page.locator('.console-input');

    // Execute some commands
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    await input.fill('status');
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Press arrow up to get last command
    await input.press('ArrowUp');
    await expect(input).toHaveValue('status');

    // Press arrow up again to get previous command
    await input.press('ArrowUp');
    await expect(input).toHaveValue('help');
  });

  test('should navigate command history with arrow down', async ({ page }) => {
    const input = page.locator('.console-input');

    // Execute some commands
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    await input.fill('status');
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Go back in history
    await input.press('ArrowUp');
    await input.press('ArrowUp');
    await expect(input).toHaveValue('help');

    // Go forward in history
    await input.press('ArrowDown');
    await expect(input).toHaveValue('status');

    // Go to end (empty)
    await input.press('ArrowDown');
    await expect(input).toHaveValue('');
  });

  test('should not execute empty command', async ({ page }) => {
    const input = page.locator('.console-input');
    const initialLines = await page.locator('.console-line').count();

    // Press enter without input
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Should not add any new lines
    const finalLines = await page.locator('.console-line').count();
    expect(finalLines).toBe(initialLines);
  });
});

test.describe('Console Tab Completion', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should complete unique command prefix with Tab', async ({ page }) => {
    const input = page.locator('.console-input');

    // Type partial command that has unique completion
    await input.fill('sta');
    await input.press('Tab');

    // Should complete to 'status'
    await expect(input).toHaveValue('status');
  });

  test('should show possible completions when multiple matches', async ({ page }) => {
    const input = page.locator('.console-input');

    // Type partial command that has multiple matches
    await input.fill('c');
    await input.press('Tab');

    // Should show possible completions
    const output = page.locator('.console-output');
    await expect(output).toContainText('Possible completions:');
    await expect(output).toContainText('clear');
    await expect(output).toContainText('connect');
    await expect(output).toContainText('cells');
  });
});

test.describe('Console Output Styling', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should display command lines with command class', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    const commandLine = page.locator('.console-line.command');
    await expect(commandLine.first()).toBeVisible();
  });

  test('should display info lines with info class', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    const infoLines = page.locator('.console-line.info');
    const count = await infoLines.count();
    expect(count).toBeGreaterThan(0);
  });

  test('should display error lines with error class', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('unknowncommand');
    await input.press('Enter');
    await page.waitForTimeout(100);

    const errorLines = page.locator('.console-line.error');
    await expect(errorLines.first()).toBeVisible();
  });
});

test.describe('Console Welcome Message', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should show welcome message on initial load', async ({ page }) => {
    const welcome = page.locator('.console-welcome');
    await expect(welcome).toBeVisible();
    await expect(welcome).toContainText('BMS Tool Console');
    await expect(welcome).toContainText('Type "help" for available commands');
  });

  test('should hide welcome message after first command', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('help');
    await input.press('Enter');
    await page.waitForTimeout(100);

    // Welcome should be hidden now (console output replaces it)
    const welcome = page.locator('.console-welcome');
    await expect(welcome).not.toBeVisible();
  });
});

test.describe('Console Input UI', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should have console input with placeholder', async ({ page }) => {
    const input = page.locator('.console-input');
    await expect(input).toBeVisible();
    await expect(input).toHaveAttribute('placeholder', /Enter command/);
  });

  test('should have prompt indicator', async ({ page }) => {
    const promptIndicator = page.locator('.console-input-container .console-prompt-indicator');
    await expect(promptIndicator).toBeVisible();
    await expect(promptIndicator).toHaveText('$');
  });

  test('should have hint in toolbar', async ({ page }) => {
    const hint = page.locator('.console-hint');
    await expect(hint).toBeVisible();
    await expect(hint).toContainText('help');
  });

  test('should focus input when typing', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.focus();
    await page.keyboard.type('test');
    await expect(input).toHaveValue('test');
  });
});

test.describe('Console Command Arguments', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should handle cells command with count argument', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('cells 4');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Reading 4 cell voltages');
  });

  test('should handle cells command without argument (default)', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('cells');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText(/Reading.*cell/i);
  });

  test('should handle read command with address', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('read 0x09');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Reading');
    await expect(output).toContainText('0x09');
  });

  test('should handle read command with address and byte count', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('read 0x09 4');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Reading 4 byte(s)');
  });

  test('should show invalid address error for read command', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('read notanumber');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Invalid address');
  });

  test('should show invalid address error for write command', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('write notanumber 0xFF');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Invalid address');
  });
});

test.describe('Console Connection Commands', () => {
  test.beforeEach(async ({ page }) => {
    await setupMockTauri(page);
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    await page.click('.tabs button.tab:has-text("CONSOLE")');
  });

  test('should attempt connection with port argument', async ({ page }) => {
    const input = page.locator('.console-input');
    await input.fill('connect /tmp/bms_client');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Connecting to /tmp/bms_client');
  });

  test('should handle disconnect command', async ({ page }) => {
    const input = page.locator('.console-input');

    // First connect
    await input.fill('connect /tmp/bms_client');
    await input.press('Enter');
    await page.waitForTimeout(200);

    // Then disconnect
    await input.fill('disconnect');
    await input.press('Enter');

    const output = page.locator('.console-output');
    await expect(output).toContainText('Disconnecting');
  });
});
