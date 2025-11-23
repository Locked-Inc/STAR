/**
 * @file 031_i2c_repeated_start.c
 * @brief I2C repeated start condition examples
 *
 * This example demonstrates:
 * - Using repeated start (Sr) vs stop-start sequences
 * - Combined write-read transactions
 * - When to use repeated start
 * - Performance comparison: repeated start vs stop-start
 * - Atomic read-after-write operations
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_REPEATED_START";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address - EEPROM or sensor */
#define DEVICE_ADDR     (0x50)

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

void demonstrate_repeated_start_benefit(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Why Use Repeated Start? ===");
  ESP_LOGI(s_tag, "1. Atomic transactions - no interruption between write and read");
  ESP_LOGI(s_tag, "2. Faster - no stop/start overhead");
  ESP_LOGI(s_tag, "3. Bus remains owned - prevents multi-master conflicts");
  ESP_LOGI(s_tag, "4. Required by some devices for register reads");

  ESP_LOGI(s_tag, "\n=== Repeated Start Sequence ===");
  ESP_LOGI(s_tag, "S - Addr(W) - Reg - Sr - Addr(R) - Data - P");
  ESP_LOGI(s_tag, "  S  = Start condition");
  ESP_LOGI(s_tag, "  Sr = Repeated start (no stop before)");
  ESP_LOGI(s_tag, "  P  = Stop condition");

  ESP_LOGI(s_tag, "\n=== Stop-Start Sequence ===");
  ESP_LOGI(s_tag, "S - Addr(W) - Reg - P, S - Addr(R) - Data - P");
  ESP_LOGI(s_tag, "  Two separate transactions with stop between");
}

void test_repeated_start_read(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 1: Repeated Start Read ===");
  ESP_LOGI(s_tag, "Reading register using repeated start condition");

  uint8_t reg_addr = 0x00;
  uint8_t data[4];
  size_t  bytes_read;

  /* This internally uses repeated start:
   * START - ADDR(W) - REG - REPEATED_START - ADDR(R) - DATA - STOP
   */
  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "device", data, sizeof(data),
                                     reg_addr, &bytes_read);
  int64_t elapsed_us = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read %u bytes in %lld us using repeated start",
             (unsigned)bytes_read, elapsed_us);
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X 0x%02X",
             data[0], data[1], data[2], data[3]);
  } else {
    ESP_LOGW(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }
}

void test_write_read_atomic(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 2: Atomic Write-Read ===");
  ESP_LOGI(s_tag, "Setting register pointer and reading in one transaction");

  /* Write register address, then immediately read without releasing bus */
  uint8_t reg_addr = 0x00;
  uint8_t read_data[2];
  size_t  bytes_read;

  ESP_LOGI(s_tag, "Performing atomic write-read with repeated start...");

  /* First write register address */
  size_t bytes_written;
  int64_t start = esp_timer_get_time();

  esp_err_t ret = star_bus_i2c_write(manager, "device", &reg_addr, 1,
                                      reg_addr, &bytes_written);

  if (ret == ESP_OK) {
    /* Immediately read using repeated start */
    ret = star_bus_i2c_read(manager, "device", read_data, sizeof(read_data),
                            reg_addr, &bytes_read);
  }

  int64_t elapsed_us = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Atomic operation completed in %lld us", elapsed_us);
    ESP_LOGI(s_tag, "Read data: 0x%02X 0x%02X", read_data[0], read_data[1]);
  } else {
    ESP_LOGW(s_tag, "Operation failed: %s", esp_err_to_name(ret));
  }
}

void test_sequential_register_reads(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 3: Sequential Register Reads ===");
  ESP_LOGI(s_tag, "Reading multiple registers with repeated start");

  /* Read multiple consecutive registers efficiently */
  for (uint8_t reg = 0x00; reg < 0x04; reg++) {
    uint8_t data;
    size_t  bytes_read;

    int64_t start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_read(manager, "device", &data, 1,
                                       reg, &bytes_read);
    int64_t elapsed_us = esp_timer_get_time() - start;

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Reg 0x%02X: 0x%02X (read in %lld us)",
               reg, data, elapsed_us);
    } else {
      ESP_LOGW(s_tag, "Reg 0x%02X: Read failed", reg);
    }
  }
}

void test_burst_read_with_repeated_start(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 4: Burst Read with Repeated Start ===");
  ESP_LOGI(s_tag, "Reading multiple bytes from consecutive registers");

  /* Many devices support auto-increment of register pointer
   * So we can read multiple registers in one transaction
   */
  uint8_t burst_data[16];
  size_t  bytes_read;

  ESP_LOGI(s_tag, "Attempting 16-byte burst read from register 0x00...");

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "device", burst_data,
                                     sizeof(burst_data), 0x00, &bytes_read);
  int64_t elapsed_us = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Burst read completed in %lld us", elapsed_us);
    ESP_LOGI(s_tag, "Read %u bytes:", (unsigned)bytes_read);

    /* Display in rows of 8 */
    for (int i = 0; i < bytes_read; i += 8) {
      if (i + 8 <= bytes_read) {
        ESP_LOGI(s_tag, "  %02X %02X %02X %02X %02X %02X %02X %02X",
                 burst_data[i], burst_data[i+1], burst_data[i+2],
                 burst_data[i+3], burst_data[i+4], burst_data[i+5],
                 burst_data[i+6], burst_data[i+7]);
      } else {
        /* Last row might have fewer bytes */
        printf("  ");
        for (int j = i; j < bytes_read; j++) {
          printf("%02X ", burst_data[j]);
        }
        printf("\n");
      }
    }

    ESP_LOGI(s_tag, "Efficiency: %lld us / %u bytes = %.2f us/byte",
             elapsed_us, (unsigned)bytes_read,
             (float)elapsed_us / bytes_read);
  } else {
    ESP_LOGW(s_tag, "Burst read failed: %s", esp_err_to_name(ret));
  }
}

void test_performance_comparison(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 5: Performance Comparison ===");

  /* Test 1: Single burst read (repeated start) */
  ESP_LOGI(s_tag, "\nMethod 1: Burst read (1 transaction with repeated start)");
  uint8_t burst[8];
  size_t  bytes_read;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "device", burst, 8,
                                     0x00, &bytes_read);
  int64_t burst_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Burst read: %lld us", burst_time);
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  /* Test 2: Individual reads (multiple stop-start sequences) */
  ESP_LOGI(s_tag, "\nMethod 2: Individual reads (8 separate transactions)");
  uint8_t individual[8];

  start = esp_timer_get_time();
  for (int i = 0; i < 8; i++) {
    ret = star_bus_i2c_read(manager, "device", &individual[i], 1,
                            i, &bytes_read);
    if (ret != ESP_OK) {
      break;
    }
  }
  int64_t individual_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Individual reads: %lld us", individual_time);
    ESP_LOGI(s_tag, "\nPerformance gain: %.1fx faster using burst read",
             (float)individual_time / burst_time);
    ESP_LOGI(s_tag, "Time saved: %lld us", individual_time - burst_time);
  }
}

void i2c_repeated_start_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_RepStart", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");

  /* Run demonstrations */
  demonstrate_repeated_start_benefit(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_repeated_start_read(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_write_read_atomic(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_sequential_register_reads(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_burst_read_with_repeated_start(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_performance_comparison(&manager);

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Repeated Start Summary ===");
  ESP_LOGI(s_tag, "Benefits:");
  ESP_LOGI(s_tag, "  - Atomic operations (no bus release)");
  ESP_LOGI(s_tag, "  - Better performance for burst operations");
  ESP_LOGI(s_tag, "  - Prevents multi-master conflicts");
  ESP_LOGI(s_tag, "  - Required by many I2C devices");
  ESP_LOGI(s_tag, "\nWhen to use:");
  ESP_LOGI(s_tag, "  - Register reads (write addr, read data)");
  ESP_LOGI(s_tag, "  - Burst reads from consecutive registers");
  ESP_LOGI(s_tag, "  - Any write-then-read sequence");
  ESP_LOGI(s_tag, "  - Multi-master environments");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nRepeated start example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Repeated Start Condition Example ===\n");
  i2c_repeated_start_example();
}
