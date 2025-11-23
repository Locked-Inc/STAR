/**
 * @file 098_onewire_emulation.c
 * @brief One-Wire peripheral emulation concepts example
 *
 * Demonstrates:
 * - Conceptual peripheral emulation
 * - Device behavior simulation
 * - Protocol timing requirements
 * - State machine design
 * - Response generation
 * - Temperature sensor simulation
 *
 * Note: Full peripheral emulation requires precise timing control
 * not available through standard GPIO. This example shows the concepts.
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_EMULATION";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* Simulated DS18B20 state */
typedef struct {
  uint64_t rom_code;
  float    temperature;
  uint8_t  resolution;
  uint8_t  scratchpad[9];
  bool     conversion_in_progress;
} ds18b20_sim_t;

/**
 * @brief Initialize simulated DS18B20
 */
static void priv_init_simulated_sensor(ds18b20_sim_t *sensor, uint64_t rom)
{
  sensor->rom_code                = rom;
  sensor->temperature             = 25.0f;
  sensor->resolution              = 12; /* 12-bit resolution */
  sensor->conversion_in_progress  = false;

  /* Initialize scratchpad */
  memset(sensor->scratchpad, 0, 9);
  sensor->scratchpad[4] = 0x7F; /* Configuration: 12-bit */
}

/**
 * @brief Calculate simple CRC8 for demonstration
 */
static uint8_t priv_calc_crc8(const uint8_t *data, size_t len)
{
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    for (int bit = 0; bit < 8; bit++) {
      uint8_t mix = (crc ^ byte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0x8C;
      }
      byte >>= 1;
    }
  }
  return crc;
}

/**
 * @brief Update scratchpad with current temperature
 */
static void priv_update_scratchpad(ds18b20_sim_t *sensor)
{
  /* Convert temperature to raw value */
  int16_t raw_temp = (int16_t)(sensor->temperature * 16.0f);

  /* Store in scratchpad */
  sensor->scratchpad[0] = raw_temp & 0xFF;
  sensor->scratchpad[1] = (raw_temp >> 8) & 0xFF;

  /* High/low alarm (not set) */
  sensor->scratchpad[2] = 0x4B;
  sensor->scratchpad[3] = 0x46;

  /* Configuration register */
  sensor->scratchpad[4] = 0x7F; /* 12-bit resolution */

  /* Reserved bytes */
  sensor->scratchpad[5] = 0xFF;
  sensor->scratchpad[6] = 0x0C;
  sensor->scratchpad[7] = 0x10;

  /* Calculate CRC */
  sensor->scratchpad[8] = priv_calc_crc8(sensor->scratchpad, 8);
}

/**
 * @brief Simulate temperature conversion
 */
static void priv_simulate_conversion(ds18b20_sim_t *sensor)
{
  ESP_LOGI(s_tag, "Starting temperature conversion...");
  sensor->conversion_in_progress = true;

  /* Simulate conversion time based on resolution */
  uint32_t conversion_time_ms;
  switch (sensor->resolution) {
    case 9:
      conversion_time_ms = 94;
      break;
    case 10:
      conversion_time_ms = 188;
      break;
    case 11:
      conversion_time_ms = 375;
      break;
    case 12:
    default:
      conversion_time_ms = 750;
      break;
  }

  vTaskDelay(pdMS_TO_TICKS(conversion_time_ms));

  /* Simulate slight temperature variation */
  sensor->temperature += ((float)(esp_random() % 100) / 100.0f - 0.5f) * 0.1f;

  /* Update scratchpad */
  priv_update_scratchpad(sensor);

  sensor->conversion_in_progress = false;
  ESP_LOGI(s_tag, "Conversion complete: %.2f C", sensor->temperature);
}

void app_main(void)
{
  ESP_LOGI(s_tag, "=== One-Wire Emulation Concepts Example ===\n");

  /* --- Peripheral Emulation Requirements --- */
  ESP_LOGI(s_tag, "--- Peripheral Emulation Requirements ---");

  ESP_LOGI(s_tag, "\n1. Timing Requirements:");
  ESP_LOGI(s_tag, "   Reset pulse:");
  ESP_LOGI(s_tag, "     - Master: 480-960us low");
  ESP_LOGI(s_tag, "     - Slave presence: 60-240us low after 15-60us");
  ESP_LOGI(s_tag, "   Write 0: Master holds low 60-120us");
  ESP_LOGI(s_tag, "   Write 1: Master holds low 1-15us");
  ESP_LOGI(s_tag, "   Read: Slave responds within 15us after master low pulse");
  ESP_LOGI(s_tag, "   Slot duration: Minimum 60us + 1us recovery");

  ESP_LOGI(s_tag, "\n2. Hardware Requirements:");
  ESP_LOGI(s_tag, "   - Microsecond-precise GPIO control");
  ESP_LOGI(s_tag, "   - Edge detection and interrupts");
  ESP_LOGI(s_tag, "   - Open-drain output capability");
  ESP_LOGI(s_tag, "   - Real-time response to bus events");

  ESP_LOGI(s_tag, "\n3. Software Requirements:");
  ESP_LOGI(s_tag, "   - State machine for protocol handling");
  ESP_LOGI(s_tag, "   - ROM code storage and comparison");
  ESP_LOGI(s_tag, "   - Command parser");
  ESP_LOGI(s_tag, "   - CRC calculation");
  ESP_LOGI(s_tag, "   - Device-specific function implementation");

  /* --- Simulated Device Setup --- */
  ESP_LOGI(s_tag, "\n--- Simulated DS18B20 Setup ---");

  ds18b20_sim_t sensor;
  uint64_t      sim_rom = 0x28FF1234567890AB;  /* Family 0x28 = DS18B20 */

  priv_init_simulated_sensor(&sensor, sim_rom);

  ESP_LOGI(s_tag, "Simulated device initialized:");
  ESP_LOGI(s_tag, "  ROM: 0x%016llX", (unsigned long long)sensor.rom_code);
  ESP_LOGI(s_tag, "  Family: 0x%02X (DS18B20)",
           (uint8_t)(sensor.rom_code & 0xFF));
  ESP_LOGI(s_tag, "  Initial temperature: %.2f C", sensor.temperature);
  ESP_LOGI(s_tag, "  Resolution: %d-bit", sensor.resolution);

  /* --- Protocol State Machine --- */
  ESP_LOGI(s_tag, "\n--- One-Wire Protocol State Machine ---");

  ESP_LOGI(s_tag, "States:");
  ESP_LOGI(s_tag, "  1. IDLE - Waiting for reset pulse");
  ESP_LOGI(s_tag, "  2. PRESENCE - Responding to reset");
  ESP_LOGI(s_tag, "  3. ROM_COMMAND - Receiving ROM command byte");
  ESP_LOGI(s_tag, "  4. ROM_MATCH - Comparing ROM bytes");
  ESP_LOGI(s_tag, "  5. FUNCTION_COMMAND - Receiving function command");
  ESP_LOGI(s_tag, "  6. FUNCTION_DATA - Sending/receiving data");

  /* --- Simulation Examples --- */
  ESP_LOGI(s_tag, "\n--- Simulation Examples ---");

  /* Example 1: Temperature conversion */
  ESP_LOGI(s_tag, "\n1. Temperature Conversion:");
  sensor.temperature = 22.5f;
  priv_update_scratchpad(&sensor);
  ESP_LOGI(s_tag, "   Set temperature: %.2f C", sensor.temperature);
  ESP_LOGI(s_tag, "   Scratchpad bytes:");
  ESP_LOG_BUFFER_HEX_LEVEL(s_tag, sensor.scratchpad, 9, ESP_LOG_INFO);

  /* Example 2: Simulate conversion process */
  ESP_LOGI(s_tag, "\n2. Conversion Process:");
  priv_simulate_conversion(&sensor);

  /* Example 3: Different temperatures */
  ESP_LOGI(s_tag, "\n3. Temperature Range Test:");

  float test_temps[] = {-10.0f, 0.0f, 25.0f, 50.0f, 85.0f};

  for (int i = 0; i < 5; i++) {
    sensor.temperature = test_temps[i];
    priv_update_scratchpad(&sensor);

    int16_t raw = (sensor.scratchpad[1] << 8) | sensor.scratchpad[0];
    ESP_LOGI(s_tag, "   %.1f C -> Raw: 0x%04X -> Scratchpad: %02X %02X",
             test_temps[i],
             (uint16_t)raw,
             sensor.scratchpad[0],
             sensor.scratchpad[1]);
  }

  /* --- ROM Command Handling --- */
  ESP_LOGI(s_tag, "\n--- ROM Command Handling ---");

  ESP_LOGI(s_tag, "ROM Commands:");
  ESP_LOGI(s_tag, "  0x33 - Read ROM (single device only)");
  ESP_LOGI(s_tag, "  0xCC - Skip ROM (address all devices)");
  ESP_LOGI(s_tag, "  0x55 - Match ROM (address specific device)");
  ESP_LOGI(s_tag, "  0xF0 - Search ROM (device discovery)");
  ESP_LOGI(s_tag, "  0xEC - Alarm Search (find devices in alarm)");

  ESP_LOGI(s_tag, "\nSimulated Match ROM sequence:");
  uint8_t rom_bytes[8];
  /* Convert ROM to bytes */
  for (int i = 0; i < 8; i++) {
    rom_bytes[i] = (sensor.rom_code >> (i * 8)) & 0xFF;
  }

  ESP_LOGI(s_tag, "  1. Receive ROM command: 0x55");
  ESP_LOGI(s_tag, "  2. Receive 8 ROM bytes:");
  for (int i = 0; i < 8; i++) {
    ESP_LOGI(s_tag, "     [%d] 0x%02X %s",
             i,
             rom_bytes[i],
             (i == 0) ? "(Family)" : (i == 7) ? "(CRC)" : "");
  }
  ESP_LOGI(s_tag, "  3. Compare with stored ROM");
  ESP_LOGI(s_tag, "  4. If match: Respond to function commands");
  ESP_LOGI(s_tag, "  5. If no match: Go idle");

  /* --- Function Command Handling --- */
  ESP_LOGI(s_tag, "\n--- DS18B20 Function Commands ---");

  ESP_LOGI(s_tag, "0x44 - Convert T:");
  ESP_LOGI(s_tag, "  - Start temperature conversion");
  ESP_LOGI(s_tag, "  - Pull bus low during conversion (parasitic)");
  ESP_LOGI(s_tag, "  - Or release bus (external power)");
  ESP_LOGI(s_tag, "  - Duration: 750ms (12-bit)");

  ESP_LOGI(s_tag, "\n0xBE - Read Scratchpad:");
  ESP_LOGI(s_tag, "  - Send 9 bytes of scratchpad");
  ESP_LOGI(s_tag, "  - Bytes 0-1: Temperature");
  ESP_LOGI(s_tag, "  - Bytes 2-3: Alarm thresholds");
  ESP_LOGI(s_tag, "  - Byte 4: Configuration");
  ESP_LOGI(s_tag, "  - Bytes 5-7: Reserved");
  ESP_LOGI(s_tag, "  - Byte 8: CRC8");

  ESP_LOGI(s_tag, "\n0x4E - Write Scratchpad:");
  ESP_LOGI(s_tag, "  - Receive 3 bytes (TH, TL, Config)");
  ESP_LOGI(s_tag, "  - Update scratchpad");

  ESP_LOGI(s_tag, "\n0x48 - Copy Scratchpad:");
  ESP_LOGI(s_tag, "  - Copy scratchpad to EEPROM");
  ESP_LOGI(s_tag, "  - ~10ms write time");

  /* --- Emulation Challenges --- */
  ESP_LOGI(s_tag, "\n--- Emulation Challenges on ESP32 ---");

  ESP_LOGI(s_tag, "\n1. Timing Precision:");
  ESP_LOGI(s_tag, "   - ESP32 has interrupt latency");
  ESP_LOGI(s_tag, "   - FreeRTOS scheduling delays");
  ESP_LOGI(s_tag, "   - Cache misses affect timing");
  ESP_LOGI(s_tag, "   Solution: Use RMT or dedicated timer");

  ESP_LOGI(s_tag, "\n2. Interrupt Response:");
  ESP_LOGI(s_tag, "   - Must respond within 15us");
  ESP_LOGI(s_tag, "   - Cannot use high-level drivers");
  ESP_LOGI(s_tag, "   Solution: Direct register access");

  ESP_LOGI(s_tag, "\n3. Open-Drain I/O:");
  ESP_LOGI(s_tag, "   - ESP32 supports open-drain mode");
  ESP_LOGI(s_tag, "   - External pullup required");
  ESP_LOGI(s_tag, "   Solution: gpio_set_direction(GPIO_MODE_INPUT_OUTPUT_OD)");

  /* --- Practical Implementations --- */
  ESP_LOGI(s_tag, "\n--- Practical Implementation Approaches ---");

  ESP_LOGI(s_tag, "\n1. RMT Peripheral:");
  ESP_LOGI(s_tag, "   - Hardware-timed pulse generation");
  ESP_LOGI(s_tag, "   - Can handle precise timing");
  ESP_LOGI(s_tag, "   - Limited to TX functionality");

  ESP_LOGI(s_tag, "\n2. Dedicated Core:");
  ESP_LOGI(s_tag, "   - Run emulation on Core 0");
  ESP_LOGI(s_tag, "   - Higher priority interrupts");
  ESP_LOGI(s_tag, "   - Better timing consistency");

  ESP_LOGI(s_tag, "\n3. External IC:");
  ESP_LOGI(s_tag, "   - Use dedicated One-Wire peripheral chip");
  ESP_LOGI(s_tag, "   - DS2480B (serial to One-Wire bridge)");
  ESP_LOGI(s_tag, "   - ESP32 communicates via UART");

  ESP_LOGI(s_tag, "\n4. State Machine + GPIO:");
  ESP_LOGI(s_tag, "   - Edge-triggered interrupts");
  ESP_LOGI(s_tag, "   - Fast interrupt handlers");
  ESP_LOGI(s_tag, "   - Assembly-optimized critical sections");

  /* --- Use Cases --- */
  ESP_LOGI(s_tag, "\n--- Emulation Use Cases ---");
  ESP_LOGI(s_tag, "- Testing and development");
  ESP_LOGI(s_tag, "- Protocol analyzers");
  ESP_LOGI(s_tag, "- Device simulation");
  ESP_LOGI(s_tag, "- Educational purposes");
  ESP_LOGI(s_tag, "- Bridge implementations");
  ESP_LOGI(s_tag, "- Network debugging");

  ESP_LOGI(s_tag, "\nOne-Wire emulation concepts example complete");
}
