import { test, expect } from '@playwright/test';

test.describe('Dashboard', () => {
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
  });

  test('should display dashboard with stats', async ({ page }) => {
    await expect(page.getByText('Dashboard')).toBeVisible();

    // Check for stats cards
    const statsGrid = page.getByTestId('stats-grid');
    await expect(statsGrid).toBeVisible();

    // Check for stat labels
    await expect(page.getByText('Active Builds')).toBeVisible();
    await expect(page.getByText('Online Devices')).toBeVisible();
    await expect(page.getByText('Pending Flash Jobs')).toBeVisible();
  });

  test('should display user email in nav', async ({ page }) => {
    const userEmail = await page.getByTestId('user-email').textContent();
    expect(userEmail).toContain('@example.com');
  });

  test('should navigate to builds page', async ({ page }) => {
    await page.getByRole('link', { name: 'Builds' }).click();
    await expect(page).toHaveURL('/builds');
    await expect(page.getByText('Build Jobs')).toBeVisible();
  });

  test('should navigate to devices page', async ({ page }) => {
    await page.getByRole('link', { name: 'Devices' }).click();
    await expect(page).toHaveURL('/devices');
    await expect(page.getByText('Devices')).toBeVisible();
  });

  test('should navigate to flash jobs page', async ({ page }) => {
    await page.getByRole('link', { name: 'Flash Jobs' }).click();
    await expect(page).toHaveURL('/flash-jobs');
    await expect(page.getByText('Flash Jobs')).toBeVisible();
  });

  test('should display stats with correct counts', async ({ page }) => {
    // Wait for data to load
    await page.waitForTimeout(1000);

    // Check that stat counts are numbers
    const activeBuilds = await page.getByTestId('active-builds-count').textContent();
    expect(activeBuilds).toMatch(/^\d+$/);

    const onlineDevices = await page.getByTestId('online-devices-count').textContent();
    expect(onlineDevices).toMatch(/^\d+$/);

    const pendingFlash = await page.getByTestId('pending-flash-count').textContent();
    expect(pendingFlash).toMatch(/^\d+$/);
  });
});
