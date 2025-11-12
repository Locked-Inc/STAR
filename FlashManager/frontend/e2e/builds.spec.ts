import { test, expect } from '@playwright/test';

test.describe('Builds', () => {
  test.beforeEach(async ({ page }) => {
    // Register and login before each test
    const testEmail = `test${Date.now()}@example.com`;
    const testPassword = 'password123';

    await page.goto('/login');
    await page.getByTestId('toggle-mode').click();
    await page.getByTestId('email-input').fill(testEmail);
    await page.getByTestId('password-input').fill(testPassword);
    await page.getByTestId('submit-button').click();

    await expect(page).toHaveURL('/dashboard');

    // Navigate to builds page
    await page.getByRole('link', { name: 'Builds' }).click();
    await expect(page).toHaveURL('/builds');
  });

  test('should display builds page', async ({ page }) => {
    await expect(page.getByText('Build Jobs')).toBeVisible();
    await expect(page.getByTestId('trigger-build-button')).toBeVisible();
  });

  test('should show trigger build form', async ({ page }) => {
    await page.getByTestId('trigger-build-button').click();

    const form = page.getByTestId('trigger-build-form');
    await expect(form).toBeVisible();

    await expect(page.getByTestId('component-select')).toBeVisible();
    await expect(page.getByTestId('branch-input')).toBeVisible();
    await expect(page.getByTestId('submit-trigger')).toBeVisible();
  });

  test('should cancel trigger build form', async ({ page }) => {
    await page.getByTestId('trigger-build-button').click();

    const form = page.getByTestId('trigger-build-form');
    await expect(form).toBeVisible();

    await page.getByRole('button', { name: 'Cancel' }).click();
    await expect(form).not.toBeVisible();
  });

  test('should trigger a build', async ({ page }) => {
    await page.getByTestId('trigger-build-button').click();

    // Select component
    await page.getByTestId('component-select').selectOption('ESP32_FIRMWARE');

    // Enter branch
    await page.getByTestId('branch-input').fill('main');

    // Submit
    await page.getByTestId('submit-trigger').click();

    // Wait for form to close and build to appear
    await expect(page.getByTestId('trigger-build-form')).not.toBeVisible();

    // Check that builds table is visible
    await page.waitForTimeout(2000); // Wait for data to load
    const table = page.getByTestId('builds-table');
    await expect(table).toBeVisible();

    // Verify the build appears in the table
    await expect(table.getByText('ESP32 FIRMWARE')).toBeVisible();
    await expect(table.getByText('main')).toBeVisible();
  });

  test('should display build status badges', async ({ page }) => {
    // Wait for data to load
    await page.waitForTimeout(1000);

    const table = page.getByTestId('builds-table');

    // Check if table has content
    const hasBuilds = await table.locator('tbody tr').count() > 0;

    if (hasBuilds) {
      // Check that status badges are displayed
      const firstRow = table.locator('tbody tr').first();
      const statusBadge = firstRow.locator('span[class*="rounded-full"]');
      await expect(statusBadge).toBeVisible();
    }
  });

  test('should display build durations', async ({ page }) => {
    // Wait for data to load
    await page.waitForTimeout(1000);

    const table = page.getByTestId('builds-table');

    // Check if table has content
    const hasBuilds = await table.locator('tbody tr').count() > 0;

    if (hasBuilds) {
      // Duration column should exist
      await expect(page.getByText('Duration')).toBeVisible();
    }
  });
});
