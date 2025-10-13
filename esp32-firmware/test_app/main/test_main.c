/* esp32-firmware/test_app/main/test_main.c */

/**
 * @file test_main.c
 * @brief Main test application for running STAR tests on ESP32 hardware.
 *
 * This application runs all component tests and reports results.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_system.h"
#include "star_test.h"

static const char* TAG = "test_main";

/**
 * @brief Main application entry point
 */
void app_main(void)
{
  /* Wait a bit for serial monitor to connect */
  vTaskDelay(pdMS_TO_TICKS(2000));

  ESP_LOGI(TAG, "====================================");
  ESP_LOGI(TAG, "   STAR Test Application Started");
  ESP_LOGI(TAG, "====================================");

  /* Tests are auto-registered via constructor attribute */

  /* Run all tests */
  star_test_run_all();

  /* Print results */
  star_test_print_results();

  /* Get results */
  star_test_results_t results = star_test_get_results();

  if (results.failed == 0) {
    ESP_LOGI(TAG, "All %" PRIu32 " tests passed!", results.total);
  } else {
    ESP_LOGE(TAG, "%" PRIu32 " of %" PRIu32 " tests failed!", results.failed, results.total);
  }

  /* Print completion marker for CI/CD detection */
  printf("\n");
  printf("================================================================================\n");
  if (results.failed == 0) {
    printf("===TEST_EXECUTION_COMPLETE:PASS===\n");
  } else {
    printf("===TEST_EXECUTION_COMPLETE:FAIL===\n");
  }
  printf("================================================================================\n");
  printf("\n");

  ESP_LOGI(TAG, "Test execution complete.");

  /* Allow time for logs to flush to serial console */
  vTaskDelay(pdMS_TO_TICKS(1000));

  /* Idle forever - monitoring script will detect completion and exit */
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
