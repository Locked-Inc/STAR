/**
 * @file 033_i2c_multi_port.c
 * @brief Using I2C_NUM_0 and I2C_NUM_1 simultaneously
 *
 * This example demonstrates:
 * - Configuring multiple I2C ports
 * - Operating I2C_NUM_0 and I2C_NUM_1 independently
 * - Different clock speeds per port
 * - Managing devices on different buses
 * - Concurrent operations on both ports
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_MULTI_PORT";

/* I2C Port 0 pins */
#define I2C0_SDA_PIN    (GPIO_NUM_21)
#define I2C0_SCL_PIN    (GPIO_NUM_22)

/* I2C Port 1 pins */
#define I2C1_SDA_PIN    (GPIO_NUM_25)
#define I2C1_SCL_PIN    (GPIO_NUM_26)

/* Device addresses */
#define PORT0_DEVICE_ADDR  (0x48)  /* Temperature sensor on port 0 */
#define PORT1_DEVICE_ADDR  (0x50)  /* EEPROM on port 1 */

/* Clock speeds - can be different per port */
#define I2C0_CLK_SPEED  (100000)   /* 100 kHz for port 0 */
#define I2C1_CLK_SPEED  (400000)   /* 400 kHz for port 1 (Fast mode) */

void demonstrate_multi_port_benefits(void)
{
  ESP_LOGI(s_tag, "\n=== Multi-Port I2C Benefits ===");
  ESP_LOGI(s_tag, "1. Independent operation - no bus contention");
  ESP_LOGI(s_tag, "2. Different clock speeds for different device requirements");
  ESP_LOGI(s_tag, "3. Electrical isolation - separate pull-ups");
  ESP_LOGI(s_tag, "4. More total devices (duplicate addresses on different buses)");
  ESP_LOGI(s_tag, "5. Concurrent transactions on both buses");
  ESP_LOGI(s_tag, "6. Better fault isolation");

  ESP_LOGI(s_tag, "\n=== ESP32 I2C Ports ===");
  ESP_LOGI(s_tag, "Port 0: GPIO%d (SDA), GPIO%d (SCL) @ %d Hz",
           I2C0_SDA_PIN, I2C0_SCL_PIN, I2C0_CLK_SPEED);
  ESP_LOGI(s_tag, "Port 1: GPIO%d (SDA), GPIO%d (SCL) @ %d Hz",
           I2C1_SDA_PIN, I2C1_SCL_PIN, I2C1_CLK_SPEED);
}

void test_port0_operations(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Port 0 Operations ===");
  ESP_LOGI(s_tag, "Operating on I2C_NUM_0 at %d Hz", I2C0_CLK_SPEED);

  uint8_t data[2];
  size_t  bytes_read;

  esp_err_t ret = star_bus_i2c_read(manager, "port0_device", data, 2,
                                     0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 0: Read %u bytes: 0x%02X 0x%02X",
             (unsigned)bytes_read, data[0], data[1]);
  } else {
    ESP_LOGW(s_tag, "Port 0: Read failed - %s (device may not be present)",
             esp_err_to_name(ret));
  }

  /* Write operation */
  uint8_t write_data = 0xA5;
  size_t  bytes_written;
  ret = star_bus_i2c_write(manager, "port0_device", &write_data, 1,
                           0x01, &bytes_written);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 0: Wrote %u bytes", (unsigned)bytes_written);
  } else {
    ESP_LOGW(s_tag, "Port 0: Write failed - %s", esp_err_to_name(ret));
  }
}

void test_port1_operations(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Port 1 Operations ===");
  ESP_LOGI(s_tag, "Operating on I2C_NUM_1 at %d Hz", I2C1_CLK_SPEED);

  uint8_t data[4];
  size_t  bytes_read;

  esp_err_t ret = star_bus_i2c_read(manager, "port1_device", data, 4,
                                     0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 1: Read %u bytes: 0x%02X 0x%02X 0x%02X 0x%02X",
             (unsigned)bytes_read, data[0], data[1], data[2], data[3]);
  } else {
    ESP_LOGW(s_tag, "Port 1: Read failed - %s (device may not be present)",
             esp_err_to_name(ret));
  }

  /* Write operation */
  uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
  size_t  bytes_written;
  ret = star_bus_i2c_write(manager, "port1_device", write_data, 4,
                           0x00, &bytes_written);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 1: Wrote %u bytes", (unsigned)bytes_written);
  } else {
    ESP_LOGW(s_tag, "Port 1: Write failed - %s", esp_err_to_name(ret));
  }
}

void test_concurrent_operations(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Concurrent Operations ===");
  ESP_LOGI(s_tag, "Demonstrating simultaneous access to both ports");

  /* In a real application, these could be in separate tasks */

  ESP_LOGI(s_tag, "\nIteration-based concurrent test:");
  for (int i = 0; i < 5; i++) {
    ESP_LOGI(s_tag, "Iteration %d:", i + 1);

    /* Read from port 0 */
    uint8_t   port0_data;
    size_t    bytes_read;
    int64_t   start = esp_timer_get_time();
    esp_err_t ret0  = star_bus_i2c_read(manager, "port0_device", &port0_data, 1,
                                        i, &bytes_read);
    int64_t   port0_time = esp_timer_get_time() - start;

    /* Read from port 1 */
    uint8_t port1_data;
    start = esp_timer_get_time();
    esp_err_t ret1 = star_bus_i2c_read(manager, "port1_device", &port1_data, 1,
                                        i, &bytes_read);
    int64_t port1_time = esp_timer_get_time() - start;

    if (ret0 == ESP_OK) {
      ESP_LOGI(s_tag, "  Port 0: 0x%02X (%lld us)", port0_data, port0_time);
    }

    if (ret1 == ESP_OK) {
      ESP_LOGI(s_tag, "  Port 1: 0x%02X (%lld us)", port1_data, port1_time);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void test_same_address_different_ports(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Same Address on Different Ports ===");
  ESP_LOGI(s_tag, "Demonstrating ability to use same address on different buses");

  /* Create devices with same address on both ports */
  star_bus_config_t *port0_dup = star_bus_config_create_i2c(
    "port0_duplicate",
    I2C_NUM_0,
    0x48,  /* Same address */
    I2C0_SDA_PIN,
    I2C0_SCL_PIN,
    I2C0_CLK_SPEED
  );

  star_bus_config_t *port1_dup = star_bus_config_create_i2c(
    "port1_duplicate",
    I2C_NUM_1,
    0x48,  /* Same address */
    I2C1_SDA_PIN,
    I2C1_SCL_PIN,
    I2C1_CLK_SPEED
  );

  if (port0_dup && port1_dup) {
    star_bus_manager_add_bus(manager, port0_dup);
    star_bus_config_init(port0_dup, manager);

    star_bus_manager_add_bus(manager, port1_dup);
    star_bus_config_init(port1_dup, manager);

    ESP_LOGI(s_tag, "Created two devices at address 0x48:");
    ESP_LOGI(s_tag, "  - One on I2C_NUM_0");
    ESP_LOGI(s_tag, "  - One on I2C_NUM_1");

    /* Access both independently */
    uint8_t data0, data1;
    size_t  bytes_read;

    esp_err_t ret0 = star_bus_i2c_read(manager, "port0_duplicate", &data0, 1,
                                        0x00, &bytes_read);
    esp_err_t ret1 = star_bus_i2c_read(manager, "port1_duplicate", &data1, 1,
                                        0x00, &bytes_read);

    if (ret0 == ESP_OK && ret1 == ESP_OK) {
      ESP_LOGI(s_tag, "Successfully accessed both devices independently");
      ESP_LOGI(s_tag, "  Port 0 device: 0x%02X", data0);
      ESP_LOGI(s_tag, "  Port 1 device: 0x%02X", data1);
    }

    /* Cleanup */
    star_bus_manager_remove_bus(manager, "port0_duplicate");
    star_bus_manager_remove_bus(manager, "port1_duplicate");
    star_bus_config_destroy(port0_dup);
    star_bus_config_destroy(port1_dup);
  }

  ESP_LOGI(s_tag, "This is impossible with single-port I2C!");
}

void test_performance_comparison(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Performance Comparison ===");
  ESP_LOGI(s_tag, "Comparing different clock speeds");

  /* Burst read on Port 0 (100 kHz) */
  uint8_t data0[16];
  size_t  bytes_read;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "port0_device", data0, 16,
                                     0x00, &bytes_read);
  int64_t port0_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 0 (100 kHz): 16 bytes in %lld us (%.1f us/byte)",
             port0_time, (float)port0_time / 16);
  }

  /* Burst read on Port 1 (400 kHz) */
  uint8_t data1[16];

  start = esp_timer_get_time();
  ret = star_bus_i2c_read(manager, "port1_device", data1, 16,
                          0x00, &bytes_read);
  int64_t port1_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Port 1 (400 kHz): 16 bytes in %lld us (%.1f us/byte)",
             port1_time, (float)port1_time / 16);

    if (port0_time > 0 && port1_time > 0) {
      ESP_LOGI(s_tag, "Speedup: %.1fx faster with 400 kHz",
               (float)port0_time / port1_time);
    }
  }
}

void i2c_multi_port_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_MultiPort", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config for Port 0 */
  star_bus_config_t *port0_config = star_bus_config_create_i2c(
    "port0_device",
    I2C_NUM_0,
    PORT0_DEVICE_ADDR,
    I2C0_SDA_PIN,
    I2C0_SCL_PIN,
    I2C0_CLK_SPEED
  );

  /* Create I2C config for Port 1 */
  star_bus_config_t *port1_config = star_bus_config_create_i2c(
    "port1_device",
    I2C_NUM_1,
    PORT1_DEVICE_ADDR,
    I2C1_SDA_PIN,
    I2C1_SCL_PIN,
    I2C1_CLK_SPEED
  );

  if (port0_config == NULL || port1_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C configs");
    if (port0_config) {
      star_bus_config_destroy(port0_config);
    }
    if (port1_config) {
      star_bus_config_destroy(port1_config);
    }
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Add both configs to manager */
  star_bus_manager_add_bus(&manager, port0_config);
  star_bus_manager_add_bus(&manager, port1_config);

  /* Initialize both ports */
  ret = star_bus_config_init(port0_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init Port 0: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI(s_tag, "Port 0 initialized successfully");
  }

  ret = star_bus_config_init(port1_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init Port 1: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI(s_tag, "Port 1 initialized successfully");
  }

  /* Run demonstrations */
  demonstrate_multi_port_benefits();
  vTaskDelay(pdMS_TO_TICKS(100));

  test_port0_operations(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_port1_operations(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_concurrent_operations(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_same_address_different_ports(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  test_performance_comparison(&manager);

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Multi-Port I2C Summary ===");
  ESP_LOGI(s_tag, "Use cases:");
  ESP_LOGI(s_tag, "  - Devices with same address");
  ESP_LOGI(s_tag, "  - Different speed requirements");
  ESP_LOGI(s_tag, "  - Electrical isolation");
  ESP_LOGI(s_tag, "  - More total devices");
  ESP_LOGI(s_tag, "  - Parallel operations");
  ESP_LOGI(s_tag, "\nConsiderations:");
  ESP_LOGI(s_tag, "  - Requires more GPIO pins");
  ESP_LOGI(s_tag, "  - Each port needs pull-up resistors");
  ESP_LOGI(s_tag, "  - Slightly higher power consumption");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nMulti-port example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Multi-Port Example ===\n");
  i2c_multi_port_example();
}
