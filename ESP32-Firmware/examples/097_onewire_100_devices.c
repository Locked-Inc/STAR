/**
 * @file 097_onewire_100_devices.c
 * @brief Large One-Wire network (100+ devices) example
 *
 * Demonstrates:
 * - Handling large device networks
 * - Efficient device scanning
 * - Device grouping and management
 * - Performance optimization
 * - Network topology considerations
 * - Alarm search for large networks
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_LARGE_NET";

#define ONEWIRE_GPIO      (GPIO_NUM_4)
#define MAX_DEVICES (128)
#define SEARCH_BATCH_SIZE (32)

typedef struct {
  uint64_t rom;
  uint8_t  family;
  bool     responding;
  uint32_t last_read_ms;
} device_info_t;

static device_info_t s_device_registry[MAX_DEVICES];
static size_t        s_device_count = 0;

/**
 * @brief Add device to registry
 */
static void priv_register_device(uint64_t rom)
{
  if (s_device_count >= MAX_DEVICES) {
    ESP_LOGW(s_tag, "Device registry full!");
    return;
  }

  /* Check if already registered */
  for (size_t i = 0; i < s_device_count; i++) {
    if (s_device_registry[i].rom == rom) {
      return; /* Already registered */
    }
  }

  /* Add new device */
  s_device_registry[s_device_count].rom          = rom;
  s_device_registry[s_device_count].family       = star_bus_onewire_get_family(rom);
  s_device_registry[s_device_count].responding   = true;
  s_device_registry[s_device_count].last_read_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

  s_device_count++;
}

/**
 * @brief Search and register all devices
 */
static esp_err_t priv_discover_all_devices(star_bus_manager_t *manager, const char *bus_name)
{
  uint64_t  batch[SEARCH_BATCH_SIZE];
  size_t    batch_count;
  esp_err_t ret;
  size_t    total_found = 0;

  ESP_LOGI(s_tag, "Starting comprehensive device search...");

  /* Perform search */
  batch_count = SEARCH_BATCH_SIZE;
  ret         = star_bus_onewire_search(manager, bus_name, batch, &batch_count);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Search failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Register devices */
  for (size_t i = 0; i < batch_count; i++) {
    priv_register_device(batch[i]);
    total_found++;
  }

  ESP_LOGI(s_tag, "Discovered %zu total devices", total_found);
  return ESP_OK;
}

/**
 * @brief Print device registry statistics
 */
static void priv_print_registry_stats(void)
{
  ESP_LOGI(s_tag, "\n--- Device Registry Statistics ---");
  ESP_LOGI(s_tag, "Total registered devices: %zu", s_device_count);

  /* Count by family */
  int family_count[256] = {0};

  for (size_t i = 0; i < s_device_count; i++) {
    family_count[s_device_registry[i].family]++;
  }

  ESP_LOGI(s_tag, "\nDevices by family code:");
  for (int f = 0; f < 256; f++) {
    if (family_count[f] > 0) {
      const char *family_name = "Unknown";

      switch (f) {
        case STAR_ONEWIRE_FAMILY_DS18B20:
          family_name = "DS18B20 (Temp)";
          break;
        case STAR_ONEWIRE_FAMILY_DS18S20:
          family_name = "DS18S20 (Temp)";
          break;
        case STAR_ONEWIRE_FAMILY_DS1822:
          family_name = "DS1822 (Temp)";
          break;
        case STAR_ONEWIRE_FAMILY_DS2431:
          family_name = "DS2431 (EEPROM)";
          break;
        case STAR_ONEWIRE_FAMILY_DS2433:
          family_name = "DS2433 (EEPROM)";
          break;
        case STAR_ONEWIRE_FAMILY_DS2401:
          family_name = "DS2401 (Serial)";
          break;
      }

      ESP_LOGI(s_tag, "  0x%02X (%s): %d device(s)", f, family_name, family_count[f]);
    }
  }

  /* Response status */
  int responding = 0;
  for (size_t i = 0; i < s_device_count; i++) {
    if (s_device_registry[i].responding) {
      responding++;
    }
  }

  ESP_LOGI(s_tag, "\nResponse status:");
  ESP_LOGI(s_tag, "  Responding: %d", responding);
  ESP_LOGI(s_tag, "  Not responding: %zu", s_device_count - responding);
}

/**
 * @brief Simulate large network performance test
 */
static void priv_performance_test(star_bus_manager_t *manager, const char *bus_name, size_t num_devices)
{
  ESP_LOGI(s_tag, "\n--- Performance Test (Simulated %zu devices) ---", num_devices);

  uint32_t start_time = xTaskGetTickCount();

  /* Search time */
  uint64_t devices[SEARCH_BATCH_SIZE];
  size_t   count = SEARCH_BATCH_SIZE;
  star_bus_onewire_search(manager, bus_name, devices, &count);

  uint32_t search_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
  ESP_LOGI(s_tag, "Search time: %lu ms", (unsigned long)search_time);
  ESP_LOGI(s_tag, "Time per device: %.1f ms", (float)search_time / (float)count);

  /* Estimate for 100 devices */
  if (count > 0) {
    uint32_t estimated_100 = (search_time * 100) / count;
    ESP_LOGI(s_tag, "Estimated time for 100 devices: %.1f seconds", estimated_100 / 1000.0f);
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Large One-Wire Network Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Initialize One-Wire */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* --- Device Discovery --- */
  ESP_LOGI(s_tag, "--- Device Discovery ---");

  ret = priv_discover_all_devices(&manager, "onewire0");
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Discovery failed");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* --- Registry Statistics --- */
  priv_print_registry_stats();

  /* --- List First 10 Devices --- */
  ESP_LOGI(s_tag, "\n--- Device List (first 10) ---");

  size_t list_count = s_device_count < 10 ? s_device_count : 10;
  for (size_t i = 0; i < list_count; i++) {
    ESP_LOGI(s_tag, "[%zu] ROM: 0x%016llX (Family: 0x%02X)",
             i,
             (unsigned long long)s_device_registry[i].rom,
             s_device_registry[i].family);
  }

  if (s_device_count > 10) {
    ESP_LOGI(s_tag, "... and %zu more devices", s_device_count - 10);
  }

  /* --- Performance Analysis --- */
  priv_performance_test(&manager, "onewire0", s_device_count);

  /* --- Network Topology Recommendations --- */
  ESP_LOGI(s_tag, "\n--- Large Network Design Guidelines ---");

  ESP_LOGI(s_tag, "\n1. Topology:");
  ESP_LOGI(s_tag, "   - Star topology preferred");
  ESP_LOGI(s_tag, "   - Avoid long stubs (>3m)");
  ESP_LOGI(s_tag, "   - Use junction boxes for branches");
  ESP_LOGI(s_tag, "   - Total network length: <200m");

  ESP_LOGI(s_tag, "\n2. Electrical:");
  ESP_LOGI(s_tag, "   - 4.7kΩ pullup (standard load)");
  ESP_LOGI(s_tag, "   - 2.2kΩ for <25 devices");
  ESP_LOGI(s_tag, "   - Add 100Ω series resistor for >25 devices");
  ESP_LOGI(s_tag, "   - Use twisted pair cable");
  ESP_LOGI(s_tag, "   - Minimize capacitance");

  ESP_LOGI(s_tag, "\n3. Power:");
  ESP_LOGI(s_tag, "   - External power preferred for >10 devices");
  ESP_LOGI(s_tag, "   - Parasitic power limited to ~10 devices");
  ESP_LOGI(s_tag, "   - Consider separate power lines");
  ESP_LOGI(s_tag, "   - Use strong pullup for conversions");

  ESP_LOGI(s_tag, "\n4. Performance:");
  ESP_LOGI(s_tag, "   - Search time increases linearly");
  ESP_LOGI(s_tag, "   - Cache ROM codes to avoid repeated searches");
  ESP_LOGI(s_tag, "   - Use alarm search to find active devices");
  ESP_LOGI(s_tag, "   - Group devices by function");
  ESP_LOGI(s_tag, "   - Consider multiple buses for >50 devices");

  ESP_LOGI(s_tag, "\n5. Software:");
  ESP_LOGI(s_tag, "   - Implement device registry");
  ESP_LOGI(s_tag, "   - Handle device addition/removal");
  ESP_LOGI(s_tag, "   - Monitor device health");
  ESP_LOGI(s_tag, "   - Use timeout/retry logic");
  ESP_LOGI(s_tag, "   - Log CRC errors for diagnosis");

  /* --- Scaling Strategies --- */
  ESP_LOGI(s_tag, "\n--- Scaling Strategies ---");

  ESP_LOGI(s_tag, "\nFor 50-100 devices:");
  ESP_LOGI(s_tag, "  - Single bus possible with good design");
  ESP_LOGI(s_tag, "  - 30-60 second scan time expected");
  ESP_LOGI(s_tag, "  - Use device caching");
  ESP_LOGI(s_tag, "  - Implement incremental search");

  ESP_LOGI(s_tag, "\nFor 100+ devices:");
  ESP_LOGI(s_tag, "  - Consider multiple buses (2-4)");
  ESP_LOGI(s_tag, "  - Parallel scanning reduces total time");
  ESP_LOGI(s_tag, "  - Physical separation improves reliability");
  ESP_LOGI(s_tag, "  - Easier troubleshooting");

  /* --- Troubleshooting --- */
  ESP_LOGI(s_tag, "\n--- Troubleshooting Large Networks ---");

  ESP_LOGI(s_tag, "\nSymptom: Devices not found");
  ESP_LOGI(s_tag, "  - Check pullup resistor value");
  ESP_LOGI(s_tag, "  - Verify power supply");
  ESP_LOGI(s_tag, "  - Measure bus capacitance");
  ESP_LOGI(s_tag, "  - Check for shorts/opens");

  ESP_LOGI(s_tag, "\nSymptom: CRC errors");
  ESP_LOGI(s_tag, "  - Reduce cable length");
  ESP_LOGI(s_tag, "  - Add series resistor");
  ESP_LOGI(s_tag, "  - Shield cables");
  ESP_LOGI(s_tag, "  - Check for EMI sources");

  ESP_LOGI(s_tag, "\nSymptom: Intermittent devices");
  ESP_LOGI(s_tag, "  - Check connections");
  ESP_LOGI(s_tag, "  - Verify device power");
  ESP_LOGI(s_tag, "  - Look for loose wires");
  ESP_LOGI(s_tag, "  - Test at temperature extremes");

  /* --- Real-World Applications --- */
  ESP_LOGI(s_tag, "\n--- Real-World Applications ---");

  ESP_LOGI(s_tag, "\nBuilding Automation:");
  ESP_LOGI(s_tag, "  - 100+ temperature sensors");
  ESP_LOGI(s_tag, "  - HVAC monitoring and control");
  ESP_LOGI(s_tag, "  - Energy management");

  ESP_LOGI(s_tag, "\nIndustrial Monitoring:");
  ESP_LOGI(s_tag, "  - Process temperature measurement");
  ESP_LOGI(s_tag, "  - Equipment tracking (iButtons)");
  ESP_LOGI(s_tag, "  - Asset management");

  ESP_LOGI(s_tag, "\nAgriculture:");
  ESP_LOGI(s_tag, "  - Greenhouse monitoring");
  ESP_LOGI(s_tag, "  - Soil temperature sensing");
  ESP_LOGI(s_tag, "  - Climate control");

  /* --- Benchmarks --- */
  if (s_device_count > 0) {
    ESP_LOGI(s_tag, "\n--- Current Network Benchmarks ---");
    ESP_LOGI(s_tag, "Devices detected: %zu", s_device_count);
    ESP_LOGI(s_tag, "Bus load: %.1f%%", (s_device_count * 100.0f) / 100.0f);

    if (s_device_count < 25) {
      ESP_LOGI(s_tag, "Status: Light load, optimal performance");
    } else if (s_device_count < 50) {
      ESP_LOGI(s_tag, "Status: Moderate load, good performance");
    } else if (s_device_count < 100) {
      ESP_LOGI(s_tag, "Status: Heavy load, consider optimization");
    } else {
      ESP_LOGI(s_tag, "Status: Maximum load, consider multiple buses");
    }
  }

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nLarge network example complete");
}
