import { test, expect } from '@playwright/test';

test.describe('Authentication', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/login');
  });

  test('should display login page', async ({ page }) => {
    await expect(page.getByText('Sign in to FlashManager')).toBeVisible();
    await expect(page.getByTestId('email-input')).toBeVisible();
    await expect(page.getByTestId('password-input')).toBeVisible();
    await expect(page.getByTestId('submit-button')).toBeVisible();
  });

  test('should toggle between login and register modes', async ({ page }) => {
    // Should start in login mode
    await expect(page.getByText('Sign in to FlashManager')).toBeVisible();

    // Toggle to register mode
    await page.getByTestId('toggle-mode').click();
    await expect(page.getByText('Create your account')).toBeVisible();

    // Toggle back to login mode
    await page.getByTestId('toggle-mode').click();
    await expect(page.getByText('Sign in to FlashManager')).toBeVisible();
  });

  test('should show validation error for empty fields', async ({ page }) => {
    await page.getByTestId('submit-button').click();

    // Browser's built-in validation should prevent submission
    const emailInput = page.getByTestId('email-input');
    await expect(emailInput).toHaveAttribute('required');

    const passwordInput = page.getByTestId('password-input');
    await expect(passwordInput).toHaveAttribute('required');
  });

  test('should register a new user', async ({ page }) => {
    // Generate unique email for test
    const testEmail = `test${Date.now()}@example.com`;
    const testPassword = 'password123';

    // Toggle to register mode
    await page.getByTestId('toggle-mode').click();

    // Fill in form
    await page.getByTestId('email-input').fill(testEmail);
    await page.getByTestId('password-input').fill(testPassword);

    // Submit form
    await page.getByTestId('submit-button').click();

    // Should navigate to dashboard after successful registration
    await expect(page).toHaveURL('/dashboard');
    await expect(page.getByTestId('user-email')).toContainText(testEmail);
  });

  test('should login with existing user', async ({ page }) => {
    // First, register a user
    const testEmail = `test${Date.now()}@example.com`;
    const testPassword = 'password123';

    await page.getByTestId('toggle-mode').click();
    await page.getByTestId('email-input').fill(testEmail);
    await page.getByTestId('password-input').fill(testPassword);
    await page.getByTestId('submit-button').click();

    // Wait for dashboard to load
    await expect(page).toHaveURL('/dashboard');

    // Logout by clearing localStorage and navigating to login
    await page.evaluate(() => {
      localStorage.clear();
    });
    await page.goto('/login');

    // Login with the same credentials
    await page.getByTestId('email-input').fill(testEmail);
    await page.getByTestId('password-input').fill(testPassword);
    await page.getByTestId('submit-button').click();

    // Should navigate to dashboard after successful login
    await expect(page).toHaveURL('/dashboard');
    await expect(page.getByTestId('user-email')).toContainText(testEmail);
  });

  test('should show error for invalid credentials', async ({ page }) => {
    await page.getByTestId('email-input').fill('invalid@example.com');
    await page.getByTestId('password-input').fill('wrongpassword');
    await page.getByTestId('submit-button').click();

    // Should show error message
    await expect(page.getByTestId('error-message')).toBeVisible();
  });
});
