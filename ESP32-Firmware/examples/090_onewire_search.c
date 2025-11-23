/**
 * @file 090_onewire_search.c
 * @brief One-Wire Search Algorithm Detailed Example
 *
 * Demonstrates:
 * - Standard ROM search algorithm implementation
 * - Family code filtering during search
 * - Search state management
 * - Handling search conflicts
 * - Incremental device discovery
 * - Search optimization techniques
 * - Identifying newly added/removed devices
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_SEARCH";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* Print ROM code in detailed format */
static void priv_print_rom_details(star_onewire_rom_t rom)
{
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);

  uint8_t family = star_bus_onewire_get_family(rom);
  uint8_t crc    = star_bus_onewire_get_crc(rom);

  ESP_LOGI(s_tag, "    Full ROM: 0x%016llX", (unsigned long long)rom);
  ESP_LOGI(s_tag, "    Bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
           rom_bytes[0], rom_bytes[1], rom_bytes[2], rom_bytes[3],
           rom_bytes[4], rom_bytes[5], rom_bytes[6], rom_bytes[7]);
  ESP_LOGI(s_tag, "    Family Code: 0x%02X", family);
  ESP_LOGI(s_tag, "    Serial: %02X:%02X:%02X:%02X:%02X:%02X",
           rom_bytes[1], rom_bytes[2], rom_bytes[3],
           rom_bytes[4], rom_bytes[5], rom_bytes[6]);
  ESP_LOGI(s_tag, "    CRC: 0x%02X (%s)", crc, star_bus_onewire_verify_rom(rom) ? "VALID" : "INVALID");
}

/* Get device family name */
static const char *priv_get_family_name(uint8_t family)
{
  switch (family) {
    case STAR_ONEWIRE_FAMILY_DS18S20:
      return "DS18S20";
    case STAR_ONEWIRE_FAMILY_DS18B20:
      return "DS18B20";
    case STAR_ONEWIRE_FAMILY_DS1822:
      return "DS1822";
    case STAR_ONEWIRE_FAMILY_DS2431:
      return "DS2431";
    case STAR_ONEWIRE_FAMILY_DS2433:
      return "DS2433";
    case STAR_ONEWIRE_FAMILY_DS2401:
      return "DS2401/DS2411";
    default:
      return "Unknown";
  }
}

/* Search for specific family code */
static esp_err_t priv_search_by_family(star_bus_manager_t *manager,
                                       const char         *bus_name,
                                       uint8_t             family_code,
                                       star_onewire_rom_t *rom_codes,
                                       size_t             *count)
{
  /* Perform full search first */
  star_onewire_rom_t all_roms[STAR_ONEWIRE_MAX_DEVICES];
  size_t             all_count = STAR_ONEWIRE_MAX_DEVICES;

  esp_err_t ret = star_bus_onewire_search(manager, bus_name, all_roms, &all_count);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Filter by family code */
  size_t filtered_count = 0;
  for (size_t i = 0; i < all_count; i++) {
    if (star_bus_onewire_get_family(all_roms[i]) == family_code) {
      if (filtered_count < *count) {
        rom_codes[filtered_count++] = all_roms[i];
      }
    }
  }

  *count = filtered_count;
  return ESP_OK;
}

/* Compare device lists to detect changes */
static void priv_compare_device_lists(star_onewire_rom_t *old_list,
                                      size_t              old_count,
                                      star_onewire_rom_t *new_list,
                                 size_t              new_count)
{
  ESP_LOGI(s_tag, "Comparing device lists...");

  /* Check for removed devices */
  for (size_t i = 0; i < old_count; i++) {
    bool found = false;
    for (size_t j = 0; j < new_count; j++) {
      if (old_list[i] == new_list[j]) {
        found = true;
        break;
      }
    }

    if (!found) {
      ESP_LOGI(s_tag, "  REMOVED: 0x%016llX", (unsigned long long)old_list[i]);
    }
  }

  /* Check for added devices */
  for (size_t i = 0; i < new_count; i++) {
    bool found = false;
    for (size_t j = 0; j < old_count; j++) {
      if (new_list[i] == old_list[j]) {
        found = true;
        break;
      }
    }

    if (!found) {
      ESP_LOGI(s_tag, "  ADDED: 0x%016llX", (unsigned long long)new_list[i]);
    }
  }

  /* Summary */
  ESP_LOGI(s_tag, "Previous: %zu devices, Current: %zu devices", old_count, new_count);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Search Algorithm Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Configure One-Wire bus */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;
  config.search_timeout_ms     = 5000;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* --- Search Algorithm Explanation --- */
  ESP_LOGI(s_tag, "--- 1-Wire Search Algorithm ---");
  ESP_LOGI(s_tag, "The search algorithm discovers all devices on the bus");
  ESP_LOGI(s_tag, "by resolving ROM code conflicts bit by bit.");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Algorithm steps:");
  ESP_LOGI(s_tag, "1. Master sends Search ROM command (0xF0)");
  ESP_LOGI(s_tag, "2. For each bit position (0-63):");
  ESP_LOGI(s_tag, "   a. Read bit from all devices");
  ESP_LOGI(s_tag, "   b. Read complement bit from all devices");
  ESP_LOGI(s_tag, "   c. If conflict (both 0 and 1), choose path");
  ESP_LOGI(s_tag, "   d. Write chosen bit to select devices");
  ESP_LOGI(s_tag, "3. Repeat until all paths explored");
  ESP_LOGI(s_tag, "");

  /* --- Basic Search --- */
  ESP_LOGI(s_tag, "--- Basic Device Search ---");

  star_onewire_rom_t rom_codes[STAR_ONEWIRE_MAX_DEVICES];
  size_t             device_count = STAR_ONEWIRE_MAX_DEVICES;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Search failed: %s", esp_err_to_name(ret));
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Search complete: Found %zu device(s)\n", device_count);

  if (device_count == 0) {
    ESP_LOGW(s_tag, "No devices found on bus");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Display all found devices */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);

    ESP_LOGI(s_tag, "Device %zu: %s (0x%02X)", i, priv_get_family_name(family), family);
    priv_print_rom_details(rom_codes[i]);
    ESP_LOGI(s_tag, "");
  }

  /* --- Family Code Filtering --- */
  ESP_LOGI(s_tag, "--- Search by Family Code ---");

  /* Search for temperature sensors only */
  ESP_LOGI(s_tag, "Searching for DS18B20 temperature sensors (0x28)...");

  star_onewire_rom_t temp_sensors[10];
  size_t             temp_count = 10;

  ret = priv_search_by_family(&manager, "onewire0", STAR_ONEWIRE_FAMILY_DS18B20, temp_sensors, &temp_count);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Found %zu DS18B20 sensor(s)", temp_count);
    for (size_t i = 0; i < temp_count; i++) {
      ESP_LOGI(s_tag, "  [%zu] 0x%016llX", i, (unsigned long long)temp_sensors[i]);
    }
  }
  ESP_LOGI(s_tag, "");

  /* Search for EEPROM devices */
  ESP_LOGI(s_tag, "Searching for DS2431 EEPROM devices (0x2D)...");

  star_onewire_rom_t eeprom_devices[10];
  size_t             eeprom_count = 10;

  ret = priv_search_by_family(&manager, "onewire0", STAR_ONEWIRE_FAMILY_DS2431, eeprom_devices, &eeprom_count);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Found %zu DS2431 device(s)", eeprom_count);
    for (size_t i = 0; i < eeprom_count; i++) {
      ESP_LOGI(s_tag, "  [%zu] 0x%016llX", i, (unsigned long long)eeprom_devices[i]);
    }
  }
  ESP_LOGI(s_tag, "");

  /* --- Device Organization by Family --- */
  ESP_LOGI(s_tag, "--- Devices Organized by Family ---");

  /* Count devices by family */
  uint8_t families[256] = {0};
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    families[family]++;
  }

  /* Display summary */
  ESP_LOGI(s_tag, "Family Code Summary:");
  for (int i = 0; i < 256; i++) {
    if (families[i] > 0) {
      ESP_LOGI(s_tag, "  0x%02X (%s): %d device(s)", i, priv_get_family_name(i), families[i]);
    }
  }
  ESP_LOGI(s_tag, "");

  /* --- Search Performance --- */
  ESP_LOGI(s_tag, "--- Search Performance Analysis ---");

  star_onewire_stats_t stats;
  ret = star_bus_onewire_get_stats(&manager, "onewire0", &stats);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Search operations: %llu", (unsigned long long)stats.search_operations);
    ESP_LOGI(s_tag, "Devices found: %u", stats.devices_found);
    ESP_LOGI(s_tag, "Total resets: %llu", (unsigned long long)stats.total_resets);

    if (stats.search_operations > 0) {
      ESP_LOGI(s_tag, "Average devices per search: %.2f",
               (float)stats.devices_found / (float)stats.search_operations);
    }
  }
  ESP_LOGI(s_tag, "");

  /* --- Incremental Search Demo --- */
  ESP_LOGI(s_tag, "--- Incremental Search (Hot-Plug Detection) ---");
  ESP_LOGI(s_tag, "Monitoring for device changes...");
  ESP_LOGI(s_tag, "Add or remove devices to see detection");
  ESP_LOGI(s_tag, "");

  /* Save current device list */
  star_onewire_rom_t previous_roms[STAR_ONEWIRE_MAX_DEVICES];
  size_t             previous_count = device_count;
  memcpy(previous_roms, rom_codes, device_count * sizeof(star_onewire_rom_t));

  /* Perform periodic searches to detect changes */
  for (int scan = 0; scan < 3; scan++) {
    ESP_LOGI(s_tag, "Scan %d: Searching for changes...", scan + 1);

    size_t new_count = STAR_ONEWIRE_MAX_DEVICES;
    ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &new_count);

    if (ret == ESP_OK) {
      if (new_count != previous_count) {
        ESP_LOGI(s_tag, "Device count changed: %zu -> %zu", previous_count, new_count);
        priv_compare_device_lists(previous_roms, previous_count, rom_codes, new_count);

        /* Update previous list */
        memcpy(previous_roms, rom_codes, new_count * sizeof(star_onewire_rom_t));
        previous_count = new_count;
      } else {
        /* Check if any ROMs changed */
        bool changed = false;
        for (size_t i = 0; i < new_count; i++) {
          if (rom_codes[i] != previous_roms[i]) {
            changed = true;
            break;
          }
        }

        if (changed) {
          ESP_LOGI(s_tag, "Device list changed (same count)");
          priv_compare_device_lists(previous_roms, previous_count, rom_codes, new_count);

          memcpy(previous_roms, rom_codes, new_count * sizeof(star_onewire_rom_t));
        } else {
          ESP_LOGI(s_tag, "No changes detected");
        }
      }
    } else {
      ESP_LOGE(s_tag, "Search failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(s_tag, "");
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  /* --- Search Optimization Tips --- */
  ESP_LOGI(s_tag, "--- Search Optimization Tips ---");
  ESP_LOGI(s_tag, "1. Cache ROM codes - avoid repeated searches");
  ESP_LOGI(s_tag, "2. Use family code filtering when possible");
  ESP_LOGI(s_tag, "3. Set appropriate search timeout");
  ESP_LOGI(s_tag, "4. Implement incremental search for hot-plug");
  ESP_LOGI(s_tag, "5. Verify CRC before using ROM codes");
  ESP_LOGI(s_tag, "6. Handle search errors gracefully");
  ESP_LOGI(s_tag, "");

  /* --- Search Limitations --- */
  ESP_LOGI(s_tag, "--- Search Algorithm Limitations ---");
  ESP_LOGI(s_tag, "- Time complexity: O(n * 64) where n = device count");
  ESP_LOGI(s_tag, "- Maximum ~100 devices practical limit");
  ESP_LOGI(s_tag, "- Cable length affects reliability");
  ESP_LOGI(s_tag, "- Noise can cause false detections");
  ESP_LOGI(s_tag, "- Search time increases with device count");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Estimated search times:");
  ESP_LOGI(s_tag, "   1 device:  ~5-10 ms");
  ESP_LOGI(s_tag, "  10 devices: ~50-100 ms");
  ESP_LOGI(s_tag, "  50 devices: ~250-500 ms");
  ESP_LOGI(s_tag, " 100 devices: ~500-1000 ms");
  ESP_LOGI(s_tag, "");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "--- Search Best Practices ---");
  ESP_LOGI(s_tag, "1. Perform initial search at startup");
  ESP_LOGI(s_tag, "2. Cache device list for fast access");
  ESP_LOGI(s_tag, "3. Re-search periodically or on error");
  ESP_LOGI(s_tag, "4. Validate ROM CRC before use");
  ESP_LOGI(s_tag, "5. Handle zero devices gracefully");
  ESP_LOGI(s_tag, "6. Log ROM codes for debugging");
  ESP_LOGI(s_tag, "7. Consider family-specific searches");
  ESP_LOGI(s_tag, "8. Implement timeout protection");
  ESP_LOGI(s_tag, "");

  /* Display final statistics */
  ESP_LOGI(s_tag, "--- Final Statistics ---");
  star_bus_onewire_print_stats("onewire0", &stats);

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire search algorithm example complete");
}
