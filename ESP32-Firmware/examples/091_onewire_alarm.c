/**
 * @file 091_onewire_alarm.c
 * @brief One-Wire Alarm Search Example
 *
 * Demonstrates:
 * - Alarm search for devices with active alarms
 * - DS18B20 temperature alarm configuration
 * - Setting alarm thresholds
 * - Conditional search for alarming devices
 * - Alarm vs normal search comparison
 * - Practical temperature monitoring with alarms
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_ALARM";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)
#define DS18B20_CMD_WRITE_SCRATCHPAD (0x4E)
#define DS18B20_CMD_COPY_SCRATCHPAD (0x48)
#define DS18B20_CMD_RECALL_E2 (0xB8)

/* DS18B20 scratchpad structure */
typedef struct {
  uint8_t temp_lsb;        /* Byte 0: Temperature LSB */
  uint8_t temp_msb;        /* Byte 1: Temperature MSB */
  uint8_t th_register;     /* Byte 2: TH (high alarm) */
  uint8_t tl_register;     /* Byte 3: TL (low alarm) */
  uint8_t config;          /* Byte 4: Configuration */
  uint8_t reserved[3];     /* Bytes 5-7: Reserved */
  uint8_t crc;             /* Byte 8: CRC */
} __attribute__((packed)) ds18b20_scratchpad_t;

/* Read DS18B20 scratchpad */
static esp_err_t priv_read_scratchpad(star_bus_manager_t   *manager,
                                      const char           *bus_name,
                                      star_onewire_rom_t    rom,
                                      ds18b20_scratchpad_t *scratchpad)
{
  esp_err_t ret;

  uint8_t cmd = DS18B20_CMD_READ_SCRATCHPAD;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = star_bus_onewire_read_bytes(manager, bus_name, (uint8_t*)scratchpad, 9);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Verify CRC */
  uint8_t crc = star_bus_onewire_crc8((uint8_t*)scratchpad, 8);
  if (crc != scratchpad->crc) {
    ESP_LOGE(s_tag, "Scratchpad CRC mismatch: expected 0x%02X, got 0x%02X", scratchpad->crc, crc);
    return ESP_ERR_INVALID_CRC;
  }

  return ESP_OK;
}

/* Write alarm thresholds to DS18B20 */
static esp_err_t priv_set_alarm_thresholds(star_bus_manager_t *manager,
                                           const char         *bus_name,
                                      star_onewire_rom_t  rom,
                                      int8_t              th_celsius,
                                      int8_t              tl_celsius)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "Setting alarm thresholds: TH=%d°C, TL=%d°C", th_celsius, tl_celsius);

  /* Write scratchpad (TH, TL, Config) */
  uint8_t cmd[4] = {
    DS18B20_CMD_WRITE_SCRATCHPAD,
    (uint8_t)th_celsius,  /* TH register */
    (uint8_t)tl_celsius,  /* TL register */
    0x7F                  /* Config: 12-bit resolution */
  };

  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, cmd, 4);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Copy scratchpad to EEPROM to make persistent */
  uint8_t copy_cmd = DS18B20_CMD_COPY_SCRATCHPAD;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &copy_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for copy to complete */
  vTaskDelay(pdMS_TO_TICKS(15));

  ESP_LOGI(s_tag, "Alarm thresholds saved to EEPROM");

  return ESP_OK;
}

/* Read temperature and check alarm status */
static esp_err_t priv_read_temperature_with_alarm(star_bus_manager_t *manager,
                                             const char         *bus_name,
                                             star_onewire_rom_t  rom,
                                                  float              *temperature,
                                                  bool               *alarm_triggered)
{
  esp_err_t ret;

  /* Start temperature conversion */
  uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for conversion */
  vTaskDelay(pdMS_TO_TICKS(750));

  /* Read scratchpad */
  ds18b20_scratchpad_t scratchpad;
  ret = priv_read_scratchpad(manager, bus_name, rom, &scratchpad);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Calculate temperature */
  int16_t raw_temp = (scratchpad.temp_msb << 8) | scratchpad.temp_lsb;
  *temperature     = (float)raw_temp / 16.0f;

  /* Check alarm conditions */
  int8_t th = (int8_t)scratchpad.th_register;
  int8_t tl = (int8_t)scratchpad.tl_register;

  *alarm_triggered = (*temperature > th) || (*temperature < tl);

  return ESP_OK;
}

/* Print device information */
static void print_device_info(star_onewire_rom_t rom, int index)
{
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);

  ESP_LOGI(s_tag, "  [%d] ROM: 0x%016llX", index, (unsigned long long)rom);
  ESP_LOGI(s_tag, "      Serial: %02X:%02X:%02X:%02X:%02X:%02X",
           rom_bytes[1], rom_bytes[2], rom_bytes[3],
           rom_bytes[4], rom_bytes[5], rom_bytes[6]);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Alarm Search Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "onewire_alarm", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Configure One-Wire bus */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* --- Alarm Search Explained --- */
  ESP_LOGI(s_tag, "--- Alarm Search Concept ---");
  ESP_LOGI(s_tag, "Alarm search (0xEC) finds only devices with active alarms");
  ESP_LOGI(s_tag, "Benefits:");
  ESP_LOGI(s_tag, "  - Faster than regular search when monitoring many devices");
  ESP_LOGI(s_tag, "  - Reduces bus traffic");
  ESP_LOGI(s_tag, "  - Only reads devices that need attention");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Alarm conditions:");
  ESP_LOGI(s_tag, "  - Temperature > TH (high threshold)");
  ESP_LOGI(s_tag, "  - Temperature < TL (low threshold)");
  ESP_LOGI(s_tag, "");

  /* Search for all devices first */
  ESP_LOGI(s_tag, "--- Initial Device Search ---");

  star_onewire_rom_t all_devices[10];
  size_t             all_count = 10;

  ret = star_bus_onewire_search(&manager, "onewire0", all_devices, &all_count);
  if (ret != ESP_OK || all_count == 0) {
    ESP_LOGW(s_tag, "No devices found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu total device(s)\n", all_count);

  /* Filter DS18B20 temperature sensors */
  star_onewire_rom_t temp_sensors[10];
  size_t             sensor_count = 0;

  for (size_t i = 0; i < all_count; i++) {
    uint8_t family = star_bus_onewire_get_family(all_devices[i]);
    if (family == STAR_ONEWIRE_FAMILY_DS18B20 || family == STAR_ONEWIRE_FAMILY_DS18S20) {
      temp_sensors[sensor_count++] = all_devices[i];
    }
  }

  if (sensor_count == 0) {
    ESP_LOGW(s_tag, "No DS18B20 temperature sensors found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu temperature sensor(s)", sensor_count);
  for (size_t i = 0; i < sensor_count; i++) {
    print_device_info(temp_sensors[i], i);
  }
  ESP_LOGI(s_tag, "");

  /* --- Configure Alarm Thresholds --- */
  ESP_LOGI(s_tag, "--- Configuring Alarm Thresholds ---");

  /* Set different thresholds for each sensor */
  for (size_t i = 0; i < sensor_count; i++) {
    ESP_LOGI(s_tag, "Sensor %zu:", i);

    /* Example thresholds (adjust based on your environment) */
    int8_t th = 30;  /* High alarm at 30°C */
    int8_t tl = 15;  /* Low alarm at 15°C */

    ret = priv_set_alarm_thresholds(&manager, "onewire0", temp_sensors[i], th, tl);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to set thresholds: %s", esp_err_to_name(ret));
    } else {
      ESP_LOGI(s_tag, "Configured: TH=%d°C, TL=%d°C", th, tl);
    }

    ESP_LOGI(s_tag, "");
  }

  /* --- Read Current Temperatures --- */
  ESP_LOGI(s_tag, "--- Current Temperature Readings ---");

  float temperatures[10];
  bool  alarm_status[10];

  for (size_t i = 0; i < sensor_count; i++) {
    ret = priv_read_temperature_with_alarm(&manager, "onewire0", temp_sensors[i],
                                      &temperatures[i], &alarm_status[i]);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Sensor %zu: %.2f°C %s", i, temperatures[i],
               alarm_status[i] ? "[ALARM!]" : "[Normal]");
    } else {
      ESP_LOGE(s_tag, "Sensor %zu: Read failed", i);
      temperatures[i] = 0.0f;
      alarm_status[i] = false;
    }
  }
  ESP_LOGI(s_tag, "");

  /* --- Alarm Search --- */
  ESP_LOGI(s_tag, "--- Alarm Search ---");

  star_onewire_rom_t alarm_devices[10];
  size_t             alarm_count = 10;

  ret = star_bus_onewire_search_alarms(&manager, "onewire0", alarm_devices, &alarm_count);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Alarm search failed: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI(s_tag, "Alarm search found %zu device(s) with active alarms", alarm_count);

    if (alarm_count > 0) {
      for (size_t i = 0; i < alarm_count; i++) {
        print_device_info(alarm_devices[i], i);

        /* Find temperature for this device */
        for (size_t j = 0; j < sensor_count; j++) {
          if (temp_sensors[j] == alarm_devices[i]) {
            ESP_LOGI(s_tag, "      Temperature: %.2f°C (ALARM)", temperatures[j]);
            break;
          }
        }
      }
    } else {
      ESP_LOGI(s_tag, "No devices in alarm state");
    }
  }
  ESP_LOGI(s_tag, "");

  /* --- Monitoring Loop --- */
  ESP_LOGI(s_tag, "--- Continuous Alarm Monitoring ---");
  ESP_LOGI(s_tag, "Monitoring for 30 seconds...");
  ESP_LOGI(s_tag, "Change sensor temperature to trigger alarms\n");

  for (int cycle = 0; cycle < 10; cycle++) {
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(s_tag, "Cycle %d: Checking for alarms...", cycle + 1);

    /* First trigger conversions on all sensors */
    for (size_t i = 0; i < sensor_count; i++) {
      uint8_t cmd = DS18B20_CMD_CONVERT_T;
      star_bus_onewire_write_bytes(&manager, "onewire0", temp_sensors[i], &cmd, 1);
    }

    vTaskDelay(pdMS_TO_TICKS(750)); /* Wait for all conversions */

    /* Perform alarm search */
    alarm_count = 10;
    ret = star_bus_onewire_search_alarms(&manager, "onewire0", alarm_devices, &alarm_count);

    if (ret == ESP_OK) {
      if (alarm_count > 0) {
        ESP_LOGW(s_tag, "  ALARM: %zu device(s) triggered!", alarm_count);

        for (size_t i = 0; i < alarm_count; i++) {
          /* Read temperature from alarming device */
          float temp;
          bool  alarm;
          ret = priv_read_temperature_with_alarm(&manager, "onewire0", alarm_devices[i], &temp, &alarm);

          if (ret == ESP_OK) {
            ESP_LOGW(s_tag, "    Device 0x%016llX: %.2f°C",
                     (unsigned long long)alarm_devices[i], temp);
          }
        }
      } else {
        ESP_LOGI(s_tag, "  All sensors within normal range");
      }
    } else {
      ESP_LOGE(s_tag, "  Alarm search failed: %s", esp_err_to_name(ret));
    }
  }

  ESP_LOGI(s_tag, "");

  /* --- Performance Comparison --- */
  ESP_LOGI(s_tag, "--- Performance Comparison ---");
  ESP_LOGI(s_tag, "Search Type        | Time (10 sensors) | Use Case");
  ESP_LOGI(s_tag, "-------------------|-------------------|---------------------------");
  ESP_LOGI(s_tag, "Normal Search      | ~100 ms           | Finding all devices");
  ESP_LOGI(s_tag, "Alarm Search       | ~10-100 ms        | Finding alarming devices");
  ESP_LOGI(s_tag, "Individual Read    | ~800 ms           | Reading all temperatures");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Best practice: Use alarm search for monitoring systems");
  ESP_LOGI(s_tag, "  1. Configure thresholds once at startup");
  ESP_LOGI(s_tag, "  2. Periodically check for alarms (fast)");
  ESP_LOGI(s_tag, "  3. Only read alarming devices (efficient)");
  ESP_LOGI(s_tag, "  4. Log or alert on alarm conditions");
  ESP_LOGI(s_tag, "");

  /* --- Use Cases --- */
  ESP_LOGI(s_tag, "--- Practical Use Cases ---");
  ESP_LOGI(s_tag, "1. Temperature Monitoring:");
  ESP_LOGI(s_tag, "   - Server room cooling");
  ESP_LOGI(s_tag, "   - Food storage");
  ESP_LOGI(s_tag, "   - Industrial processes");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Threshold Alerts:");
  ESP_LOGI(s_tag, "   - High temperature warnings");
  ESP_LOGI(s_tag, "   - Low temperature warnings");
  ESP_LOGI(s_tag, "   - Freeze detection");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. System Optimization:");
  ESP_LOGI(s_tag, "   - Reduce unnecessary reads");
  ESP_LOGI(s_tag, "   - Focus on critical sensors");
  ESP_LOGI(s_tag, "   - Lower power consumption");
  ESP_LOGI(s_tag, "");

  /* Display statistics */
  ESP_LOGI(s_tag, "--- Bus Statistics ---");
  star_onewire_stats_t stats;
  ret = star_bus_onewire_get_stats(&manager, "onewire0", &stats);
  if (ret == ESP_OK) {
    star_bus_onewire_print_stats("onewire0", &stats);
  }

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire alarm search example complete");
}
