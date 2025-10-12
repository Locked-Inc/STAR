/* esp32-firmware/components/star_test/star_test.c */

/**
 * @file star_test.c
 * @brief STAR Testing Framework Implementation
 */

#include "star_test.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char* TAG = "STAR_TEST";

/* Global test results */
star_test_results_t g_star_test_results = {0};

/* Current test being executed */
star_test_case_t* g_star_test_current = NULL;

/* Head of linked list of registered tests */
static star_test_case_t* test_list_head = NULL;
static star_test_case_t* test_list_tail = NULL;

/* Verbose mode flag - when false, component logs are suppressed during tests */
static bool g_verbose_mode = false;

/**
 * @brief Register a test case
 */
void star_test_register(star_test_case_t* test)
{
  if (test == NULL) {
    return;
  }

  test->next = NULL;

  if (test_list_head == NULL) {
    test_list_head = test;
    test_list_tail = test;
  } else {
    test_list_tail->next = test;
    test_list_tail       = test;
  }

  ESP_LOGI(TAG, "Registered test: [%s] %s", test->group, test->name);
}

/**
 * @brief Set verbose mode for test execution
 *
 * When verbose mode is disabled (default), component logs are suppressed
 * during test execution to provide cleaner output. When enabled, all logs
 * are visible.
 *
 * @param verbose true to show all logs, false to suppress component logs
 */
void star_test_set_verbose(bool verbose)
{
  g_verbose_mode = verbose;
  ESP_LOGI(TAG, "Verbose mode %s", verbose ? "enabled" : "disabled");
}

/**
 * @brief Run all registered tests
 */
void star_test_run_all(void)
{
  /* Reset results */
  g_star_test_results.total  = 0;
  g_star_test_results.passed = 0;
  g_star_test_results.failed = 0;

  printf("\n");
  printf("================================================================================\n");
  printf("                      STAR Test Framework - Running Tests\n");
  printf("================================================================================\n");
  printf("\n");

  star_test_case_t* current = test_list_head;
  while (current != NULL) {
    g_star_test_current = current;
    g_star_test_results.total++;

    printf("[ RUN      ] [%s] %s\n", current->group, current->name);

    /* Run the test */
    uint32_t before_failed = g_star_test_results.failed;

    /* Suppress component logs during test unless in verbose mode */
    esp_log_level_t saved_level = ESP_LOG_INFO;
    if (!g_verbose_mode) {
      /* Save current default log level and suppress all logs except STAR_TEST */
      esp_log_level_set("*", ESP_LOG_NONE);
      esp_log_level_set(TAG, ESP_LOG_INFO);
    }

    current->func();

    /* Restore log level */
    if (!g_verbose_mode) {
      esp_log_level_set("*", saved_level);
    }

    /* Check if test passed */
    if (g_star_test_results.failed == before_failed) {
      g_star_test_results.passed++;
      printf("[       OK ] [%s] %s\n", current->group, current->name);
    } else {
      printf("[  FAILED  ] [%s] %s\n", current->group, current->name);
      if (!g_verbose_mode) {
        printf("[   NOTE   ] Component logs were suppressed during this test.\n");
        printf("[   NOTE   ] Re-run with verbose logging to see error details.\n");
      }
    }

    current = current->next;
  }

  g_star_test_current = NULL;
}

/**
 * @brief Run tests in a specific group
 */
void star_test_run_group(const char* group)
{
  /* Reset results */
  g_star_test_results.total  = 0;
  g_star_test_results.passed = 0;
  g_star_test_results.failed = 0;

  printf("\n");
  printf("================================================================================\n");
  printf("           STAR Test Framework - Running Tests in Group: %s\n", group);
  printf("================================================================================\n");
  printf("\n");

  star_test_case_t* current = test_list_head;
  while (current != NULL) {
    if (strcmp(current->group, group) == 0) {
      g_star_test_current = current;
      g_star_test_results.total++;

      printf("[ RUN      ] [%s] %s\n", current->group, current->name);

      /* Run the test */
      uint32_t before_failed = g_star_test_results.failed;

      /* Suppress component logs during test unless in verbose mode */
      esp_log_level_t saved_level = ESP_LOG_INFO;
      if (!g_verbose_mode) {
        /* Save current default log level and suppress all logs except STAR_TEST */
        esp_log_level_set("*", ESP_LOG_NONE);
        esp_log_level_set(TAG, ESP_LOG_INFO);
      }

      current->func();

      /* Restore log level */
      if (!g_verbose_mode) {
        esp_log_level_set("*", saved_level);
      }

      /* Check if test passed */
      if (g_star_test_results.failed == before_failed) {
        g_star_test_results.passed++;
        printf("[       OK ] [%s] %s\n", current->group, current->name);
      } else {
        printf("[  FAILED  ] [%s] %s\n", current->group, current->name);
        if (!g_verbose_mode) {
          printf("[   NOTE   ] Component logs were suppressed during this test.\n");
          printf("[   NOTE   ] Re-run with verbose logging to see error details.\n");
        }
      }
    }

    current = current->next;
  }

  g_star_test_current = NULL;
}

/**
 * @brief Get current test results
 */
star_test_results_t star_test_get_results(void)
{
  return g_star_test_results;
}

/**
 * @brief Print test results summary
 */
void star_test_print_results(void)
{
  printf("\n");
  printf("================================================================================\n");
  printf("                          STAR Test Results Summary\n");
  printf("================================================================================\n");
  printf("  Total Tests:  %lu\n", (unsigned long)g_star_test_results.total);
  printf("  Passed:       %lu\n", (unsigned long)g_star_test_results.passed);
  printf("  Failed:       %lu\n", (unsigned long)g_star_test_results.failed);
  printf("================================================================================\n");

  if (g_star_test_results.failed == 0) {
    printf("\n  ALL TESTS PASSED!\n\n");
    ESP_LOGI(TAG, "ALL TESTS PASSED");
  } else {
    printf("\n  SOME TESTS FAILED\n\n");
    ESP_LOGE(TAG, "%lu TEST(S) FAILED", (unsigned long)g_star_test_results.failed);
  }
}

/**
 * @brief Handle assertion failure
 */
void star_test_fail(const char* file, int line, const char* message)
{
  g_star_test_results.failed++;

  /* Extract just the filename from the full path */
  const char* filename = strrchr(file, '/');
  if (filename == NULL) {
    filename = file;
  } else {
    filename++; /* Skip the '/' */
  }

  printf("[  FAILED  ] %s:%d: %s\n", filename, line, message);

  if (g_star_test_current != NULL) {
    ESP_LOGE(TAG,
             "Test failed: [%s] %s - %s:%d: %s",
             g_star_test_current->group,
             g_star_test_current->name,
             filename,
             line,
             message);
  }
}
