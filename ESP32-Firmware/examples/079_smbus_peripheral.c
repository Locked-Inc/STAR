/**
 * @file 079_smbus_peripheral.c
 * @brief SMBus peripheral (slave) mode concept demonstration
 *
 * Demonstrates:
 * - SMBus peripheral implementation concepts
 * - Register-based device emulation approach
 * - Command handling patterns
 * - PEC support in peripheral mode
 *
 * NOTE: This example demonstrates the CONCEPT of SMBus peripheral mode.
 * Actual peripheral implementation requires hardware I2C slave support
 * and additional low-level driver code not shown in this example.
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "SMBUS_PERIPHERAL";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define PERIPHERAL_ADDR (0x50)

// Register map for emulated peripheral device
#define REG_DEVICE_ID (0x00)
#define REG_STATUS (0x01)
#define REG_CONTROL (0x02)
#define REG_TEMPERATURE (0x10)
#define REG_VOLTAGE (0x11)
#define REG_CURRENT (0x12)
#define NUM_REGISTERS (64)

/**
 * @brief Peripheral device state
 */
typedef struct {
  uint8_t registers[NUM_REGISTERS];
  uint8_t last_command;
  bool initialized;
} peripheral_state_t;

static peripheral_state_t g_peripheral = {0};

/**
 * @brief Initialize peripheral registers with default values
 */
static void init_peripheral_registers(void) {
  memset(&g_peripheral, 0, sizeof(g_peripheral));

  // Set default register values
  g_peripheral.registers[REG_DEVICE_ID] = 0x42;
  g_peripheral.registers[REG_STATUS] = 0x00;
  g_peripheral.registers[REG_CONTROL] = 0x00;
  g_peripheral.registers[REG_TEMPERATURE] = 0x19;  // 25°C
  g_peripheral.registers[REG_TEMPERATURE + 1] = 0x00;
  g_peripheral.registers[REG_VOLTAGE] = 0xDC;      // 3.3V * 100 = 330
  g_peripheral.registers[REG_VOLTAGE + 1] = 0x01;
  g_peripheral.registers[REG_CURRENT] = 0x64;      // 100mA
  g_peripheral.registers[REG_CURRENT + 1] = 0x00;

  g_peripheral.initialized = true;

  ESP_LOGI(TAG, "Peripheral registers initialized");
}

/**
 * @brief Print register map
 */
static void print_register_map(void) {
  ESP_LOGI(TAG, "\n=== Emulated Peripheral Register Map ===");
  ESP_LOGI(TAG, "0x%02X: Device ID    = 0x%02X",
           REG_DEVICE_ID, g_peripheral.registers[REG_DEVICE_ID]);
  ESP_LOGI(TAG, "0x%02X: Status       = 0x%02X",
           REG_STATUS, g_peripheral.registers[REG_STATUS]);
  ESP_LOGI(TAG, "0x%02X: Control      = 0x%02X",
           REG_CONTROL, g_peripheral.registers[REG_CONTROL]);

  int16_t temp = g_peripheral.registers[REG_TEMPERATURE] |
                 (g_peripheral.registers[REG_TEMPERATURE + 1] << 8);
  ESP_LOGI(TAG, "0x%02X: Temperature  = %d (%.1f°C)",
           REG_TEMPERATURE, temp, temp / 10.0f);

  uint16_t voltage = g_peripheral.registers[REG_VOLTAGE] |
                     (g_peripheral.registers[REG_VOLTAGE + 1] << 8);
  ESP_LOGI(TAG, "0x%02X: Voltage      = %u (%.2fV)",
           REG_VOLTAGE, voltage, voltage / 100.0f);

  uint16_t current = g_peripheral.registers[REG_CURRENT] |
                     (g_peripheral.registers[REG_CURRENT + 1] << 8);
  ESP_LOGI(TAG, "0x%02X: Current      = %u mA",
           REG_CURRENT, current);
}

/**
 * @brief Demonstrate peripheral command handling concepts
 */
static void demonstrate_command_handlers(void) {
  ESP_LOGI(TAG, "\n=== SMBus Peripheral Command Handlers ===");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "In a real peripheral implementation, you would:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Quick Command Handler:");
  ESP_LOGI(TAG, "   - Respond to address-only transaction");
  ESP_LOGI(TAG, "   - Use for simple on/off control");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Send/Receive Byte Handler:");
  ESP_LOGI(TAG, "   - Handle single byte without command code");
  ESP_LOGI(TAG, "   - Return status or accept command");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Write/Read Byte Handler:");
  ESP_LOGI(TAG, "   - Parse command byte");
  ESP_LOGI(TAG, "   - Write: Store data to register");
  ESP_LOGI(TAG, "   - Read: Return data from register");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Write/Read Word Handler:");
  ESP_LOGI(TAG, "   - Handle 16-bit data (little-endian)");
  ESP_LOGI(TAG, "   - Atomic read/write of word values");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "5. Block Write/Read Handler:");
  ESP_LOGI(TAG, "   - Handle variable-length data (up to 32 bytes)");
  ESP_LOGI(TAG, "   - First byte is length");
  ESP_LOGI(TAG, "   - Followed by data bytes");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "6. Process Call Handler:");
  ESP_LOGI(TAG, "   - Write word, immediately read word");
  ESP_LOGI(TAG, "   - No STOP between write and read");
  ESP_LOGI(TAG, "   - Useful for command/response operations");
}

/**
 * @brief Demonstrate peripheral with PEC support
 */
static void demonstrate_pec_support(void) {
  ESP_LOGI(TAG, "\n=== Peripheral PEC Support ===");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "PEC in Peripheral Mode:");
  ESP_LOGI(TAG, "- Calculate PEC for outgoing data");
  ESP_LOGI(TAG, "- Verify PEC on incoming data");
  ESP_LOGI(TAG, "- Send NACK if PEC mismatch detected");
  ESP_LOGI(TAG, "");

  // Example: Calculate PEC for a read operation
  uint8_t pec_data[] = {
    (PERIPHERAL_ADDR << 1) | 0x00,  // Write address + command
    0x10,                            // Command
    (PERIPHERAL_ADDR << 1) | 0x01,  // Read address
    0x19                             // Data (temperature)
  };
  uint8_t pec = star_smbus_calculate_pec(pec_data, sizeof(pec_data), 0);

  ESP_LOGI(TAG, "Example PEC calculation for read byte:");
  ESP_LOGI(TAG, "  Address: 0x%02X", PERIPHERAL_ADDR);
  ESP_LOGI(TAG, "  Command: 0x10");
  ESP_LOGI(TAG, "  Data: 0x19");
  ESP_LOGI(TAG, "  PEC: 0x%02X", pec);
}

void app_main(void)
{
  ESP_LOGI(TAG, "=== SMBus Peripheral Mode Concept Example ===\n");
  ESP_LOGI(TAG, "NOTE: This demonstrates peripheral mode CONCEPTS.");
  ESP_LOGI(TAG, "Actual implementation requires hardware I2C slave support.\n");

  /* Initialize peripheral state */
  init_peripheral_registers();

  /* Print register map */
  print_register_map();

  /* Demonstrate command handlers */
  demonstrate_command_handlers();

  /* Demonstrate PEC support */
  demonstrate_pec_support();

  /* Information about implementation */
  ESP_LOGI(TAG, "\n=== Implementation Notes ===");
  ESP_LOGI(TAG, "To implement a real SMBus peripheral:");
  ESP_LOGI(TAG, "1. Configure I2C hardware in slave mode");
  ESP_LOGI(TAG, "2. Set peripheral address (typically 0x50-0x5F)");
  ESP_LOGI(TAG, "3. Register interrupt handlers for I2C events");
  ESP_LOGI(TAG, "4. Implement state machine for command parsing");
  ESP_LOGI(TAG, "5. Handle clock stretching when needed");
  ESP_LOGI(TAG, "6. Optionally implement PEC calculation/verification");
  ESP_LOGI(TAG, "7. Respond within SMBus timeout requirements (25-35ms)");

  ESP_LOGI(TAG, "\n=== Supported SMBus Commands ===");
  ESP_LOGI(TAG, "- Quick Command (R/W)");
  ESP_LOGI(TAG, "- Send Byte / Receive Byte");
  ESP_LOGI(TAG, "- Write Byte Data / Read Byte Data");
  ESP_LOGI(TAG, "- Write Word Data / Read Word Data");
  ESP_LOGI(TAG, "- Write Block Data / Read Block Data");
  ESP_LOGI(TAG, "- Process Call");
  ESP_LOGI(TAG, "- Block Write-Block Read Process Call");

  ESP_LOGI(TAG, "\nSMBus peripheral concept example complete");
}
