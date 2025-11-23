/**
 * @file 096_onewire_multi_bus.c
 * @brief Multiple One-Wire buses example
 *
 * Demonstrates:
 * - Managing multiple One-Wire buses
 * - Independent bus operations
 * - Bus isolation
 * - Parallel device scanning
 * - Cross-bus comparisons
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_MULTI_BUS";

#define ONEWIRE_BUS1_GPIO (GPIO_NUM_4)
#define ONEWIRE_BUS2_GPIO (GPIO_NUM_5)
#define ONEWIRE_BUS3_GPIO (GPIO_NUM_18)

typedef struct {
  const char            *name;
  gpio_num_t            gpio;
  uint64_t              devices[16];
  size_t                device_count;
} onewire_bus_t;

/**
 * @brief Initialize a One-Wire bus
 */
static esp_err_t priv_init_bus(star_bus_manager_t *manager, onewire_bus_t *bus)
{
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = bus->gpio;

  esp_err_t ret = star_bus_onewire_init(manager, bus->name, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init %s: %s", bus->name, esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_tag, "%s initialized on GPIO %d", bus->name, bus->gpio);
  return ESP_OK;
}

/**
 * @brief Search devices on a bus
 */
static esp_err_t priv_search_bus(star_bus_manager_t *manager, onewire_bus_t *bus)
{
  bus->device_count = 16;
  esp_err_t ret =
    star_bus_onewire_search(manager, bus->name, bus->devices, &bus->device_count);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "%s: Found %zu device(s)", bus->name, bus->device_count);
  } else {
    ESP_LOGW(s_tag, "%s: Search failed or no devices", bus->name);
    bus->device_count = 0;
  }

  return ret;
}

/**
 * @brief Print bus devices
 */
static void priv_print_bus_devices(onewire_bus_t *bus)
{
  ESP_LOGI(s_tag, "\n--- %s Devices ---", bus->name);

  if (bus->device_count == 0) {
    ESP_LOGI(s_tag, "  No devices found");
    return;
  }

  for (size_t i = 0; i < bus->device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(bus->devices[i]);
    uint8_t crc    = star_bus_onewire_get_crc(bus->devices[i]);

    ESP_LOGI(s_tag, "  [%zu] ROM: 0x%016llX", i, (unsigned long long)bus->devices[i]);
    ESP_LOGI(s_tag, "      Family: 0x%02X, CRC: 0x%02X", family, crc);
  }
}

/**
 * @brief Get statistics for a bus
 */
static void priv_print_bus_stats(star_bus_manager_t *manager, onewire_bus_t *bus)
{
  star_onewire_stats_t stats;
  esp_err_t            ret = star_bus_onewire_get_stats(manager, bus->name, &stats);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "%s Statistics:", bus->name);
    star_bus_onewire_print_stats(bus->name, &stats);
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Multiple One-Wire Buses Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Initialize Buses --- */
  ESP_LOGI(s_tag, "--- Initializing Buses ---");

  onewire_bus_t bus1 = {
    .name = "Bus1",
    .gpio = ONEWIRE_BUS1_GPIO,
    .device_count = 0
  };

  onewire_bus_t bus2 = {
    .name = "Bus2",
    .gpio = ONEWIRE_BUS2_GPIO,
    .device_count = 0
  };

  onewire_bus_t bus3 = {
    .name = "Bus3",
    .gpio = ONEWIRE_BUS3_GPIO,
    .device_count = 0
  };

  priv_init_bus(&manager, &bus1);
  priv_init_bus(&manager, &bus2);
  priv_init_bus(&manager, &bus3);

  ESP_LOGI(s_tag, "All buses initialized\n");

  /* --- Search All Buses --- */
  ESP_LOGI(s_tag, "--- Searching All Buses ---");

  priv_search_bus(&manager, &bus1);
  priv_search_bus(&manager, &bus2);
  priv_search_bus(&manager, &bus3);

  /* --- Display Devices --- */
  priv_print_bus_devices(&bus1);
  priv_print_bus_devices(&bus2);
  priv_print_bus_devices(&bus3);

  /* --- Summary --- */
  ESP_LOGI(s_tag, "\n--- Device Summary ---");

  size_t total_devices = bus1.device_count + bus2.device_count + bus3.device_count;
  ESP_LOGI(s_tag, "Total devices across all buses: %zu", total_devices);
  ESP_LOGI(s_tag, "  Bus1: %zu devices", bus1.device_count);
  ESP_LOGI(s_tag, "  Bus2: %zu devices", bus2.device_count);
  ESP_LOGI(s_tag, "  Bus3: %zu devices", bus3.device_count);

  /* --- Device Type Analysis --- */
  ESP_LOGI(s_tag, "\n--- Device Type Analysis ---");

  int temp_sensors = 0;
  int eeproms      = 0;
  int serial_nums  = 0;
  int others       = 0;

  onewire_bus_t* buses[] = {&bus1, &bus2, &bus3};

  for (int b = 0; b < 3; b++) {
    for (size_t i = 0; i < buses[b]->device_count; i++) {
      uint8_t family = star_bus_onewire_get_family(buses[b]->devices[i]);

      switch (family) {
        case STAR_ONEWIRE_FAMILY_DS18B20:
        case STAR_ONEWIRE_FAMILY_DS18S20:
        case STAR_ONEWIRE_FAMILY_DS1822:
          temp_sensors++;
          break;
        case STAR_ONEWIRE_FAMILY_DS2431:
        case STAR_ONEWIRE_FAMILY_DS2433:
          eeproms++;
          break;
        case STAR_ONEWIRE_FAMILY_DS2401:
          serial_nums++;
          break;
        default:
          others++;
          break;
      }
    }
  }

  ESP_LOGI(s_tag, "Temperature sensors: %d", temp_sensors);
  ESP_LOGI(s_tag, "EEPROMs: %d", eeproms);
  ESP_LOGI(s_tag, "Serial numbers: %d", serial_nums);
  ESP_LOGI(s_tag, "Other devices: %d", others);

  /* --- Find Duplicate ROMs --- */
  ESP_LOGI(s_tag, "\n--- Checking for Duplicate ROMs ---");

  bool duplicates = false;
  for (int b1 = 0; b1 < 3; b1++) {
    for (size_t i = 0; i < buses[b1]->device_count; i++) {
      for (int b2 = b1; b2 < 3; b2++) {
        size_t start = (b2 == b1) ? i + 1 : 0;
        for (size_t j = start; j < buses[b2]->device_count; j++) {
          if (buses[b1]->devices[i] == buses[b2]->devices[j]) {
            ESP_LOGW(s_tag,
                     "Duplicate ROM found: 0x%016llX on %s and %s",
                     (unsigned long long)buses[b1]->devices[i],
                     buses[b1]->name,
                     buses[b2]->name);
            duplicates = true;
          }
        }
      }
    }
  }

  if (!duplicates) {
    ESP_LOGI(s_tag, "No duplicate ROMs detected");
  }

  /* --- Bus Statistics --- */
  ESP_LOGI(s_tag, "\n--- Bus Statistics ---");

  priv_print_bus_stats(&manager, &bus1);
  ESP_LOGI(s_tag, "");
  priv_print_bus_stats(&manager, &bus2);
  ESP_LOGI(s_tag, "");
  priv_print_bus_stats(&manager, &bus3);

  /* --- Use Cases --- */
  ESP_LOGI(s_tag, "\n--- Multi-Bus Use Cases ---");
  ESP_LOGI(s_tag, "1. Physical Separation:");
  ESP_LOGI(s_tag, "   - Isolate sensors by location");
  ESP_LOGI(s_tag, "   - Reduce cable lengths");
  ESP_LOGI(s_tag, "   - Minimize electrical interference");

  ESP_LOGI(s_tag, "\n2. Device Organization:");
  ESP_LOGI(s_tag, "   - Separate by device type");
  ESP_LOGI(s_tag, "   - Group by function");
  ESP_LOGI(s_tag, "   - Critical vs non-critical");

  ESP_LOGI(s_tag, "\n3. Performance:");
  ESP_LOGI(s_tag, "   - Parallel bus scanning");
  ESP_LOGI(s_tag, "   - Reduced bus contention");
  ESP_LOGI(s_tag, "   - Independent timing");

  ESP_LOGI(s_tag, "\n4. Reliability:");
  ESP_LOGI(s_tag, "   - Fault isolation");
  ESP_LOGI(s_tag, "   - Redundant measurements");
  ESP_LOGI(s_tag, "   - Bus backup");

  ESP_LOGI(s_tag, "\n--- Design Considerations ---");
  ESP_LOGI(s_tag, "1. GPIO availability (1 pin per bus)");
  ESP_LOGI(s_tag, "2. Power budget (pullup resistors)");
  ESP_LOGI(s_tag, "3. Code complexity vs benefits");
  ESP_LOGI(s_tag, "4. Maximum devices per bus (64 typical, 100+ possible)");
  ESP_LOGI(s_tag, "5. Bus length and topology");
  ESP_LOGI(s_tag, "6. Parasitic vs external power");

  ESP_LOGI(s_tag, "\n--- Example Application ---");
  ESP_LOGI(s_tag, "HVAC System:");
  ESP_LOGI(s_tag, "  Bus1: Indoor temperature sensors");
  ESP_LOGI(s_tag, "  Bus2: Outdoor temperature sensors");
  ESP_LOGI(s_tag, "  Bus3: Configuration EEPROMs and serial IDs");

  /* Cleanup */
  star_bus_onewire_deinit(&manager, bus1.name);
  star_bus_onewire_deinit(&manager, bus2.name);
  star_bus_onewire_deinit(&manager, bus3.name);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nMulti-bus example complete");
}
