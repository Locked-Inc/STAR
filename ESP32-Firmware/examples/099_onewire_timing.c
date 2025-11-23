/**
 * @file 099_onewire_timing.c
 * @brief One-Wire timing analysis and tuning example
 *
 * Demonstrates:
 * - Protocol timing specifications
 * - Standard vs overdrive timing
 * - Timing measurements
 * - Performance optimization
 * - Bus speed analysis
 * - Timing troubleshooting
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_TIMING";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/**
 * @brief Measure search operation time
 */
static void priv_measure_search_time(star_bus_manager_t *manager, const char *bus_name)
{
  ESP_LOGI(s_tag, "\n--- Search Operation Timing ---");

  uint64_t devices[16];
  size_t   device_count = 16;
  uint32_t start_time = esp_timer_get_time();

  esp_err_t ret = star_bus_onewire_search(manager, bus_name, devices, &device_count);

  uint32_t elapsed_us = esp_timer_get_time() - start_time;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Search completed:");
    ESP_LOGI(s_tag, "  Devices found: %zu", device_count);
    ESP_LOGI(s_tag, "  Total time: %lu us (%.2f ms)",
             (unsigned long)elapsed_us,
             elapsed_us / 1000.0f);

    if (device_count > 0) {
      ESP_LOGI(s_tag, "  Time per device: %lu us",
               (unsigned long)(elapsed_us / device_count));
    }
  } else {
    ESP_LOGW(s_tag, "Search failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Benchmark different operations
 */
static void priv_benchmark_operations(star_bus_manager_t *manager, const char *bus_name)
{
  ESP_LOGI(s_tag, "\n--- Operation Benchmarks ---");

  uint64_t devices[16];
  size_t   device_count = 16;
  uint32_t start_time;
  uint32_t elapsed_us;

  /* Search benchmark */
  start_time = esp_timer_get_time();
  star_bus_onewire_search(manager, bus_name, devices, &device_count);
  elapsed_us = esp_timer_get_time() - start_time;
  ESP_LOGI(s_tag, "Search: %lu us", (unsigned long)elapsed_us);

  ESP_LOGI(s_tag, "Note: Additional timing measurements require actual hardware");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Timing Analysis Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Standard Speed Timing --- */
  ESP_LOGI(s_tag, "--- Standard Speed Configuration ---");

  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;
  config.speed                 = STAR_ONEWIRE_SPEED_STANDARD;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire initialized (Standard Speed)");
  ESP_LOGI(s_tag, "GPIO: %d\n", ONEWIRE_GPIO);

  /* --- Timing Specifications --- */
  ESP_LOGI(s_tag, "--- One-Wire Timing Specifications ---");

  ESP_LOGI(s_tag, "\nStandard Speed (15.4 kbps):");
  ESP_LOGI(s_tag, "  Reset pulse:");
  ESP_LOGI(s_tag, "    Master low: 480-960 us (typically 480 us)");
  ESP_LOGI(s_tag, "    Presence pulse: 60-240 us low (after 15-60 us delay)");
  ESP_LOGI(s_tag, "  Write 0:");
  ESP_LOGI(s_tag, "    Master low: 60-120 us (typically 60 us)");
  ESP_LOGI(s_tag, "  Write 1:");
  ESP_LOGI(s_tag, "    Master low: 1-15 us (typically 6 us)");
  ESP_LOGI(s_tag, "  Read:");
  ESP_LOGI(s_tag, "    Master low: 1-15 us (typically 6 us)");
  ESP_LOGI(s_tag, "    Sample: within 15 us");
  ESP_LOGI(s_tag, "  Slot time: 60-120 us");
  ESP_LOGI(s_tag, "  Recovery: 1+ us between slots");

  ESP_LOGI(s_tag, "\nOverdrive Speed (142 kbps):");
  ESP_LOGI(s_tag, "  Reset pulse:");
  ESP_LOGI(s_tag, "    Master low: 48-80 us (typically 48 us)");
  ESP_LOGI(s_tag, "    Presence pulse: 6-24 us low (after 2-6 us delay)");
  ESP_LOGI(s_tag, "  Write 0:");
  ESP_LOGI(s_tag, "    Master low: 6-16 us (typically 8 us)");
  ESP_LOGI(s_tag, "  Write 1:");
  ESP_LOGI(s_tag, "    Master low: 1-2 us (typically 1 us)");
  ESP_LOGI(s_tag, "  Read:");
  ESP_LOGI(s_tag, "    Master low: 1-2 us (typically 1 us)");
  ESP_LOGI(s_tag, "    Sample: within 2 us");
  ESP_LOGI(s_tag, "  Slot time: 6-16 us");
  ESP_LOGI(s_tag, "  Recovery: 1+ us between slots");

  /* --- Perform Timing Measurements --- */
  priv_measure_search_time(&manager, "onewire0");
  priv_benchmark_operations(&manager, "onewire0");

  /* --- Overdrive Speed Test --- */
  ESP_LOGI(s_tag, "\n--- Overdrive Speed Test ---");
  ESP_LOGI(s_tag, "Note: Requires device support");

  star_bus_onewire_deinit(&manager, "onewire0");

  config.speed = STAR_ONEWIRE_SPEED_OVERDRIVE;
  ret          = star_bus_onewire_init(&manager, "onewire0", &config);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Overdrive mode initialized");
    priv_measure_search_time(&manager, "onewire0");
  } else {
    ESP_LOGW(s_tag, "Overdrive mode not available");
  }

  /* --- Performance Factors --- */
  ESP_LOGI(s_tag, "\n--- Performance Factors ---");

  ESP_LOGI(s_tag, "\n1. Bus Capacitance:");
  ESP_LOGI(s_tag, "   - Longer cables increase capacitance");
  ESP_LOGI(s_tag, "   - Higher capacitance slows rise time");
  ESP_LOGI(s_tag, "   - Typical: <400 pF for 100m");
  ESP_LOGI(s_tag, "   - Use lower value pullup for faster rise");

  ESP_LOGI(s_tag, "\n2. Pullup Resistor:");
  ESP_LOGI(s_tag, "   - 4.7kΩ: Standard value");
  ESP_LOGI(s_tag, "   - 2.2kΩ: Faster rise time (<25 devices)");
  ESP_LOGI(s_tag, "   - 1.0kΩ: Maximum speed (few devices)");
  ESP_LOGI(s_tag, "   - Trade-off: speed vs power consumption");

  ESP_LOGI(s_tag, "\n3. Cable Length:");
  ESP_LOGI(s_tag, "   - Short (<50m): Optimal performance");
  ESP_LOGI(s_tag, "   - Medium (50-100m): Good performance");
  ESP_LOGI(s_tag, "   - Long (>100m): May need tuning");
  ESP_LOGI(s_tag, "   - Use twisted pair to reduce interference");

  ESP_LOGI(s_tag, "\n4. Number of Devices:");
  ESP_LOGI(s_tag, "   - More devices = more capacitive load");
  ESP_LOGI(s_tag, "   - Search time increases linearly");
  ESP_LOGI(s_tag, "   - Consider multiple buses for >50 devices");

  /* --- Timing Troubleshooting --- */
  ESP_LOGI(s_tag, "\n--- Timing Troubleshooting ---");

  ESP_LOGI(s_tag, "\nSymptom: No presence pulse");
  ESP_LOGI(s_tag, "  - Check pullup resistor (should be 2.2k-4.7k)");
  ESP_LOGI(s_tag, "  - Verify device power");
  ESP_LOGI(s_tag, "  - Check bus for short to ground");
  ESP_LOGI(s_tag, "  - Measure reset pulse width (should be ~480us)");

  ESP_LOGI(s_tag, "\nSymptom: Intermittent reads");
  ESP_LOGI(s_tag, "  - Bus capacitance too high");
  ESP_LOGI(s_tag, "  - Slow rise time on bus");
  ESP_LOGI(s_tag, "  - Reduce pullup resistance");
  ESP_LOGI(s_tag, "  - Shorten cable length");
  ESP_LOGI(s_tag, "  - Add series resistor (100Ω)");

  ESP_LOGI(s_tag, "\nSymptom: CRC errors");
  ESP_LOGI(s_tag, "  - Timing violations");
  ESP_LOGI(s_tag, "  - Electrical noise");
  ESP_LOGI(s_tag, "  - Check with oscilloscope");
  ESP_LOGI(s_tag, "  - Shield cables if near noise sources");

  ESP_LOGI(s_tag, "\nSymptom: Slow performance");
  ESP_LOGI(s_tag, "  - Normal: ~560us per byte");
  ESP_LOGI(s_tag, "  - High capacitance slowing rise time");
  ESP_LOGI(s_tag, "  - Reduce pullup value");
  ESP_LOGI(s_tag, "  - Consider overdrive mode");

  /* --- Optimization Tips --- */
  ESP_LOGI(s_tag, "\n--- Optimization Tips ---");

  ESP_LOGI(s_tag, "\n1. Fast Search:");
  ESP_LOGI(s_tag, "   - Cache ROM codes to avoid repeated searches");
  ESP_LOGI(s_tag, "   - Use Skip ROM when only one device present");
  ESP_LOGI(s_tag, "   - Implement incremental search for hot-plug");

  ESP_LOGI(s_tag, "\n2. Efficient Communication:");
  ESP_LOGI(s_tag, "   - Batch operations when possible");
  ESP_LOGI(s_tag, "   - Use Skip ROM for broadcast commands");
  ESP_LOGI(s_tag, "   - Minimize reset pulses");

  ESP_LOGI(s_tag, "\n3. Hardware Optimization:");
  ESP_LOGI(s_tag, "   - Use quality twisted pair cable");
  ESP_LOGI(s_tag, "   - Keep connections short and direct");
  ESP_LOGI(s_tag, "   - Use appropriate pullup value");
  ESP_LOGI(s_tag, "   - Consider active pullup for large networks");

  ESP_LOGI(s_tag, "\n4. Software Optimization:");
  ESP_LOGI(s_tag, "   - Implement timeout handling");
  ESP_LOGI(s_tag, "   - Use DMA if available");
  ESP_LOGI(s_tag, "   - Cache frequently used data");
  ESP_LOGI(s_tag, "   - Minimize processing between operations");

  /* --- Measurement Tools --- */
  ESP_LOGI(s_tag, "\n--- Measurement Tools ---");

  ESP_LOGI(s_tag, "\nOscilloscope measurements:");
  ESP_LOGI(s_tag, "  - Reset pulse width and presence pulse");
  ESP_LOGI(s_tag, "  - Rise time (should be <1us at 1m)");
  ESP_LOGI(s_tag, "  - Fall time");
  ESP_LOGI(s_tag, "  - Slot timing");
  ESP_LOGI(s_tag, "  - Recovery time between slots");

  ESP_LOGI(s_tag, "\nLogic analyzer:");
  ESP_LOGI(s_tag, "  - Full protocol decode");
  ESP_LOGI(s_tag, "  - Timing analysis");
  ESP_LOGI(s_tag, "  - Search algorithm visualization");
  ESP_LOGI(s_tag, "  - Error detection");

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire timing analysis example complete");
}
