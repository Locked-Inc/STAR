/**
 * @file 024_i2c_peripheral.c
 * @brief I2C peripheral (slave) mode example
 *
 * Demonstrates:
 * - I2C peripheral initialization
 * - Read callback (master reading from us)
 * - Write callback (master writing to us)
 * - 7-bit and 10-bit addressing
 * - General call address support
 */

#include "star_bus_i2c_peripheral.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_PERIPHERAL_EXAMPLE";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PERIPHERAL_ADDR (0x42)

/* Peripheral device state */
static uint8_t  s_registers[256];
static uint8_t  s_current_reg = 0;

/* Read callback - master is reading from us */
static esp_err_t priv_peripheral_read_cb(uint8_t *data, size_t *len, void *context)
{
  ESP_LOGI(s_tag, "Master reading from register 0x%02X", s_current_reg);

  /* Return register value */
  *data = s_registers[s_current_reg];
  *len = 1;

  /* Auto-increment for sequential reads */
  s_current_reg++;

  return ESP_OK;
}

/* Write callback - master is writing to us */
static esp_err_t priv_peripheral_write_cb(const uint8_t *data,
                                          size_t len,
                                          void *context)
{
  ESP_LOGI(s_tag, "Master writing %d bytes", len);

  if (len > 0) {
    /* First byte is register address */
    s_current_reg = data[0];
    ESP_LOGI(s_tag, "Register address set to 0x%02X", s_current_reg);

    /* Remaining bytes are data to write */
    for (size_t i = 1; i < len && s_current_reg < 256; i++) {
      s_registers[s_current_reg++] = data[i];
      ESP_LOGI(s_tag, "  Wrote 0x%02X to register 0x%02X", data[i], s_current_reg - 1);
    }
  }

  return ESP_OK;
}

/* Error callback */
static void priv_peripheral_error_cb(esp_err_t error, void *context)
{
  ESP_LOGE(s_tag, "Peripheral error: %s", esp_err_to_name(error));
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== I2C Peripheral Mode Example ===\n");

  /* Initialize register bank with test data */
  for (int i = 0; i < 256; i++) {
    s_registers[i] = i;
  }

  /* --- 7-bit Addressing Mode --- */
  ESP_LOGI(s_tag, "--- 7-bit Peripheral ---");

  star_i2c_peripheral_config_t config = STAR_I2C_PERIPHERAL_CONFIG_DEFAULT();
  config.address = PERIPHERAL_ADDR;
  config.addr_mode = STAR_I2C_ADDR_7BIT;
  config.enable_10bit_addr = false;
  config.enable_general_call = false;
  config.rx_buffer_size = 256;
  config.tx_buffer_size = 256;
  config.on_read  = priv_peripheral_read_cb;
  config.on_write = priv_peripheral_write_cb;
  config.on_error = priv_peripheral_error_cb;
  config.context = NULL;

  /* Note: Peripheral mode requires a bus manager and bus name */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "peripheral_demo", NULL, NULL);

  /* Create I2C bus configuration */
  star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
    "i2c_peripheral",
    I2C_NUM_0,
    PERIPHERAL_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    100000
  );
  star_bus_manager_add_bus(&manager, i2c_bus);
  star_bus_config_init(i2c_bus, &manager);

  ret = star_bus_i2c_peripheral_enable(&manager, "i2c_peripheral", &config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Peripheral initialized at address 0x%02X", PERIPHERAL_ADDR);
    ESP_LOGI(s_tag, "Waiting for master transactions...");
    ESP_LOGI(s_tag, "Run I2C master example on another device to test");

    /* In real application, would run forever */
    /* For demo, just show the configuration */
    vTaskDelay(pdMS_TO_TICKS(5000));

    star_bus_i2c_peripheral_disable(&manager, "i2c_peripheral");
  } else {
    ESP_LOGE(s_tag, "Peripheral init failed: %s", esp_err_to_name(ret));
  }

  /* --- 10-bit Addressing Mode --- */
  ESP_LOGI(s_tag, "\n--- 10-bit Peripheral ---");

  config.address = 0x123;  /* 10-bit address */
  config.addr_mode = STAR_I2C_ADDR_10BIT;
  config.enable_10bit_addr = true;

  ret = star_bus_i2c_peripheral_enable(&manager, "i2c_peripheral", &config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "10-bit peripheral at 0x%03X", config.address);
    vTaskDelay(pdMS_TO_TICKS(5000));
    star_bus_i2c_peripheral_disable(&manager, "i2c_peripheral");
  }

  /* --- General Call Support --- */
  ESP_LOGI(s_tag, "\n--- General Call Enabled ---");

  config.address = PERIPHERAL_ADDR;
  config.addr_mode = STAR_I2C_ADDR_7BIT;
  config.enable_10bit_addr = false;
  config.enable_general_call = true;

  ret = star_bus_i2c_peripheral_enable(&manager, "i2c_peripheral", &config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Peripheral with general call (0x00) support");
    vTaskDelay(pdMS_TO_TICKS(5000));
    star_bus_i2c_peripheral_disable(&manager, "i2c_peripheral");
  }

  /* --- Pre-loading Response Data --- */
  ESP_LOGI(s_tag, "\n--- Pre-loading Response ---");
  ESP_LOGI(s_tag, "Peripheral can pre-load data for next read");
  ESP_LOGI(s_tag, "Useful for status/ID registers that don't change");

  /* --- Statistics --- */
  ESP_LOGI(s_tag, "\n--- Peripheral Statistics ---");
  star_i2c_peripheral_stats_t stats;
  ret = star_bus_i2c_peripheral_get_stats(&manager, "i2c_peripheral", &stats);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Reads from master: %lu", (unsigned long)stats.total_read_requests);
    ESP_LOGI(s_tag, "Writes from master: %lu", (unsigned long)stats.total_write_requests);
    ESP_LOGI(s_tag, "Bytes sent: %lu", (unsigned long)stats.total_bytes_sent);
    ESP_LOGI(s_tag, "Bytes received: %lu", (unsigned long)stats.total_bytes_received);
  }

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nI2C peripheral example complete");
}
