/**
 * @file 087_onewire_overdrive.c
 * @brief One-Wire Overdrive Speed Mode Example
 *
 * Demonstrates:
 * - Standard vs Overdrive speed comparison
 * - Switching between speed modes
 * - Overdrive mode timing and constraints
 * - Performance measurement
 * - Device compatibility checking
 * - Speed mode impact on operations
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_OVERDRIVE";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* Overdrive mode command (device-specific) */
#define ONEWIRE_CMD_OVERDRIVE_SKIP_ROM (0x3C)
#define ONEWIRE_CMD_OVERDRIVE_MATCH_ROM (0x69)

/* DS18B20 commands for testing */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)

/* Measure operation time in microseconds */
static int64_t priv_measure_operation_time(star_bus_manager_t *manager,
                                           const char         *bus_name,
                                      star_onewire_rom_t  rom,
                                      bool                is_temperature_read)
{
  int64_t start = esp_timer_get_time();

  if (is_temperature_read) {
    /* Temperature conversion and read */
    uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
    star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); /* Reduced wait time for demo */

    uint8_t read_cmd = DS18B20_CMD_READ_SCRATCHPAD;
    star_bus_onewire_write_bytes(manager, bus_name, rom, &read_cmd, 1);

    uint8_t scratchpad[9];
    star_bus_onewire_read_bytes(manager, bus_name, scratchpad, 9);
  } else {
    /* Simple reset and presence detection */
    bool present;
    star_bus_onewire_reset(manager, bus_name, &present);
  }

  int64_t end = esp_timer_get_time();
  return end - start;
}

/* Switch to overdrive mode for a device */
static esp_err_t priv_enter_overdrive_mode(star_bus_manager_t *manager,
                                           const char         *bus_name,
                                      star_onewire_rom_t  rom)
{
  esp_err_t ret;

  /* Send overdrive skip ROM or match ROM command */
  uint8_t cmd = ONEWIRE_CMD_OVERDRIVE_SKIP_ROM;
  ret         = star_bus_onewire_write_bytes(manager, bus_name, rom, &cmd, 1);

  return ret;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Overdrive Speed Mode Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "onewire_overdrive", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Standard Speed Mode --- */
  ESP_LOGI(s_tag, "--- Standard Speed Mode ---");

  star_onewire_config_t config_std = STAR_ONEWIRE_CONFIG_DEFAULT();
  config_std.gpio_pin              = ONEWIRE_GPIO;
  config_std.speed                 = STAR_ONEWIRE_SPEED_STANDARD;

  ret = star_bus_onewire_init(&manager, "onewire_std", &config_std);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init standard mode: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Standard mode initialized on GPIO %d", ONEWIRE_GPIO);
  ESP_LOGI(s_tag, "Timings: Reset=480us, Write1=10us, Read=15us");

  /* Search for devices in standard mode */
  star_onewire_rom_t rom_codes[10];
  size_t             device_count = 10;

  ret = star_bus_onewire_search(&manager, "onewire_std", rom_codes, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGW(s_tag, "No devices found in standard mode");
    star_bus_onewire_deinit(&manager, "onewire_std");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu device(s)", device_count);
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    ESP_LOGI(s_tag, "  [%zu] ROM: 0x%016llX (Family: 0x%02X)", i, (unsigned long long)rom_codes[i], family);
  }

  /* Measure standard speed performance */
  ESP_LOGI(s_tag, "\n--- Standard Speed Performance ---");

  int64_t std_reset_time = priv_measure_operation_time(&manager, "onewire_std", 0, false);
  ESP_LOGI(s_tag, "Reset & presence: %lld us", std_reset_time);

  if (star_bus_onewire_get_family(rom_codes[0]) == STAR_ONEWIRE_FAMILY_DS18B20) {
    int64_t std_read_time = priv_measure_operation_time(&manager, "onewire_std", rom_codes[0], true);
    ESP_LOGI(s_tag, "Temperature read: %lld us", std_read_time);
  }

  /* Get standard mode statistics */
  star_onewire_stats_t std_stats;
  star_bus_onewire_get_stats(&manager, "onewire_std", &std_stats);
  ESP_LOGI(s_tag, "Bytes written: %llu, Bytes read: %llu",
           (unsigned long long)std_stats.bytes_written,
           (unsigned long long)std_stats.bytes_read);

  /* --- Overdrive Speed Mode --- */
  ESP_LOGI(s_tag, "\n--- Overdrive Speed Mode ---");

  star_onewire_config_t config_od = STAR_ONEWIRE_CONFIG_DEFAULT();
  config_od.gpio_pin              = ONEWIRE_GPIO;
  config_od.speed                 = STAR_ONEWIRE_SPEED_OVERDRIVE;

  /* Deinit standard mode first */
  star_bus_onewire_deinit(&manager, "onewire_std");

  ret = star_bus_onewire_init(&manager, "onewire_od", &config_od);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init overdrive mode: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Overdrive mode initialized on GPIO %d", ONEWIRE_GPIO);
  ESP_LOGI(s_tag, "Timings: Reset=48us, Write1=1us, Read=1.5us");
  ESP_LOGI(s_tag, "Speed increase: ~10x faster than standard");

  /* Important notes about overdrive mode */
  ESP_LOGI(s_tag, "\n--- Overdrive Mode Notes ---");
  ESP_LOGI(s_tag, "- Not all devices support overdrive mode");
  ESP_LOGI(s_tag, "- Must enter overdrive with special command");
  ESP_LOGI(s_tag, "- Cable length limited to ~30 meters");
  ESP_LOGI(s_tag, "- More susceptible to noise");
  ESP_LOGI(s_tag, "- Reset pulse returns to standard speed");

  /* Attempt overdrive search */
  size_t od_device_count = 10;
  star_onewire_stats_t od_stats = {0};
  ret = star_bus_onewire_search(&manager, "onewire_od", rom_codes, &od_device_count);
  if (ret == ESP_OK && od_device_count > 0) {
    ESP_LOGI(s_tag, "\nFound %zu overdrive-capable device(s)", od_device_count);

    /* Measure overdrive performance */
    ESP_LOGI(s_tag, "\n--- Overdrive Speed Performance ---");

    int64_t od_reset_time = priv_measure_operation_time(&manager, "onewire_od", 0, false);
    ESP_LOGI(s_tag, "Reset & presence: %lld us", od_reset_time);

    if (star_bus_onewire_get_family(rom_codes[0]) == STAR_ONEWIRE_FAMILY_DS18B20) {
      int64_t od_read_time = priv_measure_operation_time(&manager, "onewire_od", rom_codes[0], true);
      ESP_LOGI(s_tag, "Temperature read: %lld us", od_read_time);

      /* Calculate speedup */
      if (std_reset_time > 0 && od_reset_time > 0) {
        float speedup = (float)std_reset_time / (float)od_reset_time;
        ESP_LOGI(s_tag, "Speedup factor: %.2fx", speedup);
      }
    }

    /* Get overdrive mode statistics */
    star_bus_onewire_get_stats(&manager, "onewire_od", &od_stats);
    ESP_LOGI(s_tag, "Bytes written: %llu, Bytes read: %llu",
             (unsigned long long)od_stats.bytes_written,
             (unsigned long long)od_stats.bytes_read);
  } else {
    ESP_LOGW(s_tag, "No overdrive-capable devices found");
  }

  /* --- Speed Mode Comparison --- */
  ESP_LOGI(s_tag, "\n--- Speed Mode Comparison ---");
  ESP_LOGI(s_tag, "┌─────────────────┬──────────┬──────────┐");
  ESP_LOGI(s_tag, "│ Operation       │ Standard │ Overdrive│");
  ESP_LOGI(s_tag, "├─────────────────┼──────────┼──────────┤");
  ESP_LOGI(s_tag, "│ Reset pulse     │  480 us  │   48 us  │");
  ESP_LOGI(s_tag, "│ Presence detect │   60 us  │    6 us  │");
  ESP_LOGI(s_tag, "│ Write 1 slot    │   70 us  │    7 us  │");
  ESP_LOGI(s_tag, "│ Write 0 slot    │   70 us  │    7 us  │");
  ESP_LOGI(s_tag, "│ Read slot       │   70 us  │    7 us  │");
  ESP_LOGI(s_tag, "│ Recovery time   │    1 us  │    1 us  │");
  ESP_LOGI(s_tag, "└─────────────────┴──────────┴──────────┘");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Overdrive Best Practices ---");
  ESP_LOGI(s_tag, "1. Test device compatibility before deployment");
  ESP_LOGI(s_tag, "2. Use shorter cables (<30m) for reliability");
  ESP_LOGI(s_tag, "3. Add proper termination for long cables");
  ESP_LOGI(s_tag, "4. Fall back to standard speed on errors");
  ESP_LOGI(s_tag, "5. Account for increased noise sensitivity");
  ESP_LOGI(s_tag, "6. Document which devices support overdrive");

  /* --- Device Compatibility --- */
  ESP_LOGI(s_tag, "\n--- Known Device Support ---");
  ESP_LOGI(s_tag, "Supports Overdrive:");
  ESP_LOGI(s_tag, "  - DS18B20 (with limitations)");
  ESP_LOGI(s_tag, "  - DS1994");
  ESP_LOGI(s_tag, "  - DS2430A");
  ESP_LOGI(s_tag, "  - DS2890");
  ESP_LOGI(s_tag, "\nNo Overdrive Support:");
  ESP_LOGI(s_tag, "  - DS18S20");
  ESP_LOGI(s_tag, "  - DS2401");
  ESP_LOGI(s_tag, "  - Many older devices");

  /* Display final statistics */
  ESP_LOGI(s_tag, "\n--- Final Statistics ---");
  star_bus_onewire_print_stats("onewire_od", &od_stats);

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire_od");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire overdrive example complete");
}
