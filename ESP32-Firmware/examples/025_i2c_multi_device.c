/**
 * @file 025_i2c_multi_device.c
 * @brief Multiple I2C devices on same bus example
 *
 * Demonstrates:
 * - Multiple devices with different addresses
 * - Concurrent access to different devices
 * - Bus sharing best practices
 * - Device priority management
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "I2C_MULTI_DEVICE_EXAMPLE";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED  (400000)

/* Device addresses */
#define TEMP_SENSOR_ADDR   (0x48)
#define HUMIDITY_ADDR      (0x40)
#define EEPROM_ADDR        (0x50)
#define RTC_ADDR           (0x68)
#define IO_EXPANDER_ADDR   (0x20)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Multiple I2C Devices Example ===\n");

  ret = star_bus_manager_init(&s_manager, "multi_device", NULL, NULL);
  if (ret != ESP_OK) {
    return;
  }

  /* --- Register All Devices --- */
  ESP_LOGI(s_tag, "--- Registering Devices ---");

  /* Temperature sensor */
  star_bus_config_t* temp_config = star_bus_config_create_i2c(
    "temp_sensor",
    I2C_NUM_0,
    TEMP_SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* Humidity sensor */
  star_bus_config_t* humidity_config = star_bus_config_create_i2c(
    "humidity_sensor",
    I2C_NUM_0,
    HUMIDITY_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* EEPROM */
  star_bus_config_t* eeprom_config = star_bus_config_create_i2c(
    "eeprom",
    I2C_NUM_0,
    EEPROM_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* RTC */
  star_bus_config_t* rtc_config = star_bus_config_create_i2c(
    "rtc",
    I2C_NUM_0,
    RTC_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* I/O Expander */
  star_bus_config_t* io_config = star_bus_config_create_i2c(
    "io_expander",
    I2C_NUM_0,
    IO_EXPANDER_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* Add all to manager */
  star_bus_manager_add_bus(&s_manager, temp_config);
  star_bus_manager_add_bus(&s_manager, humidity_config);
  star_bus_manager_add_bus(&s_manager, eeprom_config);
  star_bus_manager_add_bus(&s_manager, rtc_config);
  star_bus_manager_add_bus(&s_manager, io_config);

  /* Initialize all */
  star_bus_config_init(temp_config, &s_manager);
  star_bus_config_init(humidity_config, &s_manager);
  star_bus_config_init(eeprom_config, &s_manager);
  star_bus_config_init(rtc_config, &s_manager);
  star_bus_config_init(io_config, &s_manager);

  ESP_LOGI(s_tag, "5 devices registered on same bus:");
  ESP_LOGI(s_tag, "  0x%02X - Temperature sensor", TEMP_SENSOR_ADDR);
  ESP_LOGI(s_tag, "  0x%02X - Humidity sensor", HUMIDITY_ADDR);
  ESP_LOGI(s_tag, "  0x%02X - EEPROM", EEPROM_ADDR);
  ESP_LOGI(s_tag, "  0x%02X - RTC", RTC_ADDR);
  ESP_LOGI(s_tag, "  0x%02X - I/O Expander", IO_EXPANDER_ADDR);

  /* --- Sequential Access --- */
  ESP_LOGI(s_tag, "\n--- Sequential Access ---");

  uint8_t buffer[8];
  size_t bytes_read;

  /* Read from each device in sequence */
  ret = star_bus_i2c_read(&s_manager, "temp_sensor", buffer, 2, 0x00, &bytes_read);
  ESP_LOGI(s_tag, "Temp sensor: %s", esp_err_to_name(ret));

  ret = star_bus_i2c_read(&s_manager, "humidity_sensor", buffer, 2, 0x00, &bytes_read);
  ESP_LOGI(s_tag, "Humidity sensor: %s", esp_err_to_name(ret));

  ret = star_bus_i2c_read(&s_manager, "rtc", buffer, 7, 0x00, &bytes_read);
  ESP_LOGI(s_tag, "RTC: %s", esp_err_to_name(ret));

  /* --- Round-Robin Polling --- */
  ESP_LOGI(s_tag, "\n--- Round-Robin Polling ---");

  const char* devices[] = {"temp_sensor", "humidity_sensor", "rtc"};
  for (int round = 0; round < 3; round++) {
    ESP_LOGI(s_tag, "Round %d:", round + 1);
    for (int i = 0; i < 3; i++) {
      ret = star_bus_i2c_read(&s_manager, devices[i], buffer, 2, 0x00, &bytes_read);
      ESP_LOGI(s_tag, "  %s: %02X %02X", devices[i], buffer[0], buffer[1]);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* --- Concurrent Access (Thread-Safe) --- */
  ESP_LOGI(s_tag, "\n--- Concurrent Access ---");
  ESP_LOGI(s_tag, "Bus manager handles mutex locking automatically");
  ESP_LOGI(s_tag, "Multiple tasks can safely access different devices");

  /* --- Device Priority --- */
  ESP_LOGI(s_tag, "\n--- Device Priority Management ---");
  ESP_LOGI(s_tag, "Critical devices (RTC) should be read first");
  ESP_LOGI(s_tag, "Non-critical devices (EEPROM) can be deferred");
  ESP_LOGI(s_tag, "Use task priorities for time-critical sensors");

  /* --- Bus Sharing Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Bus Sharing Best Practices ---");
  ESP_LOGI(s_tag, "1. Use same clock speed for all devices");
  ESP_LOGI(s_tag, "2. Keep transactions short (hold mutex briefly)");
  ESP_LOGI(s_tag, "3. Use async operations for long transfers");
  ESP_LOGI(s_tag, "4. Handle NACK gracefully (device may be busy)");
  ESP_LOGI(s_tag, "5. Consider separate buses for high-speed devices");

  /* --- Address Conflict Prevention --- */
  ESP_LOGI(s_tag, "\n--- Address Conflict Prevention ---");
  ESP_LOGI(s_tag, "Common addresses to avoid:");
  ESP_LOGI(s_tag, "  0x48 - Many temp sensors");
  ESP_LOGI(s_tag, "  0x50 - EEPROMs");
  ESP_LOGI(s_tag, "  0x68 - RTCs");
  ESP_LOGI(s_tag, "Check device datasheets for alternate addresses");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nMultiple I2C devices example complete");
}
