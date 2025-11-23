/**
 * @file 080_smbus_alert.c
 * @brief SMBALERT# signal handling concept demonstration
 *
 * Demonstrates:
 * - SMBALERT# signal monitoring concepts
 * - Alert Response Address (ARA) protocol
 * - Multiple device alert handling
 * - Alert source identification
 *
 * NOTE: This example demonstrates SMBALERT# CONCEPTS.
 * Actual implementation requires GPIO interrupt handling and
 * hardware SMBALERT# signal from devices.
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char* TAG = "SMBUS_ALERT";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

// Alert Response Address (ARA) - standard SMBus address
#define SMBUS_ARA_ADDR (0x0C)

// Example device addresses
#define DEVICE1_ADDR (0x50)
#define DEVICE2_ADDR (0x51)
#define DEVICE3_ADDR (0x52)

static star_bus_manager_t g_manager;

/**
 * @brief Demonstrate Alert Response Address (ARA) protocol
 */
static void demonstrate_ara_protocol(void) {
  ESP_LOGI(TAG, "\n=== Alert Response Address (ARA) Protocol ===");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "ARA Address: 0x%02X (0b0001100)", SMBUS_ARA_ADDR);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "When SMBALERT# is asserted (pulled low):");
  ESP_LOGI(TAG, "1. Master reads from ARA address (0x0C)");
  ESP_LOGI(TAG, "2. Alerting device responds with its address");
  ESP_LOGI(TAG, "3. Device address is in bits [7:1] of response byte");
  ESP_LOGI(TAG, "4. Bit 0 is don't care");
  ESP_LOGI(TAG, "5. Master queries device for alert details");
  ESP_LOGI(TAG, "6. Master clears alert condition");
  ESP_LOGI(TAG, "7. Device releases SMBALERT#");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "If multiple devices assert SMBALERT#:");
  ESP_LOGI(TAG, "- Devices participate in arbitration");
  ESP_LOGI(TAG, "- Lowest address wins (like I2C arbitration)");
  ESP_LOGI(TAG, "- Master must read ARA multiple times");
  ESP_LOGI(TAG, "- Continue until SMBALERT# is released");
}

/**
 * @brief Demonstrate alert status query
 */
static void demonstrate_alert_query(void) {
  ESP_LOGI(TAG, "\n=== Querying Device for Alert Status ===");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "After identifying alerting device via ARA:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Read device status register (typically 0x01):");
  ESP_LOGI(TAG, "   Example: star_smbus_read_byte(manager, bus, addr, 0x01, &status)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Decode status bits:");
  ESP_LOGI(TAG, "   Bit 0: Over temperature");
  ESP_LOGI(TAG, "   Bit 1: Under voltage");
  ESP_LOGI(TAG, "   Bit 2: Over current");
  ESP_LOGI(TAG, "   Bit 3: Communication error");
  ESP_LOGI(TAG, "   (Actual bits depend on device specification)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Take appropriate action based on alert type");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Clear alert by writing to status register:");
  ESP_LOGI(TAG, "   Example: star_smbus_write_byte(manager, bus, addr, 0x01, 0x00)");
}

/**
 * @brief Demonstrate example alert scenarios
 */
static void demonstrate_alert_scenarios(void) {
  ESP_LOGI(TAG, "\n=== Example Alert Scenarios ===");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "Scenario 1: Single Device Alert");
  ESP_LOGI(TAG, "- Device 0x50 detects over-temperature");
  ESP_LOGI(TAG, "- Device pulls SMBALERT# low");
  ESP_LOGI(TAG, "- Master reads ARA (0x0C), receives 0xA0 (0x50 << 1)");
  ESP_LOGI(TAG, "- Master reads status from 0x50");
  ESP_LOGI(TAG, "- Master clears alert");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "Scenario 2: Multiple Device Alerts");
  ESP_LOGI(TAG, "- Device 0x50 and 0x51 both assert SMBALERT#");
  ESP_LOGI(TAG, "- Master reads ARA first time: 0x50 wins arbitration");
  ESP_LOGI(TAG, "- Master services 0x50");
  ESP_LOGI(TAG, "- SMBALERT# still low (0x51 still asserting)");
  ESP_LOGI(TAG, "- Master reads ARA again: receives 0x51");
  ESP_LOGI(TAG, "- Master services 0x51");
  ESP_LOGI(TAG, "- SMBALERT# released");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "Scenario 3: Alert Timeout");
  ESP_LOGI(TAG, "- Device asserts SMBALERT#");
  ESP_LOGI(TAG, "- No device responds to ARA (device failure)");
  ESP_LOGI(TAG, "- Master polls all known devices");
  ESP_LOGI(TAG, "- Master resets bus if necessary");
}

/**
 * @brief Demonstrate GPIO interrupt handling approach
 */
static void demonstrate_gpio_handling(void) {
  ESP_LOGI(TAG, "\n=== SMBALERT# GPIO Interrupt Handling ===");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Hardware Connection:");
  ESP_LOGI(TAG, "- SMBALERT# is open-drain, active low");
  ESP_LOGI(TAG, "- Connect SMBALERT# to ESP32 GPIO pin");
  ESP_LOGI(TAG, "- Enable pull-up resistor (typically 10kΩ)");
  ESP_LOGI(TAG, "- Multiple devices can wire-AND SMBALERT#");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Software Implementation:");
  ESP_LOGI(TAG, "1. Configure GPIO as input with pull-up");
  ESP_LOGI(TAG, "2. Enable falling-edge interrupt");
  ESP_LOGI(TAG, "3. In ISR: Set flag, don't do I2C operations");
  ESP_LOGI(TAG, "4. In task: Check flag, read ARA, query devices");
  ESP_LOGI(TAG, "5. Clear alert conditions");
  ESP_LOGI(TAG, "6. Wait for SMBALERT# to go high");
}

/**
 * @brief Compare SMBALERT# with other alert methods
 */
static void compare_alert_methods(void) {
  ESP_LOGI(TAG, "\n=== Comparison: SMBALERT# vs Other Methods ===");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "SMBALERT# Advantages:");
  ESP_LOGI(TAG, "- Hardware interrupt (low latency)");
  ESP_LOGI(TAG, "- Standard SMBus feature");
  ESP_LOGI(TAG, "- Single wire for multiple devices");
  ESP_LOGI(TAG, "- No polling required");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "SMBALERT# Disadvantages:");
  ESP_LOGI(TAG, "- Requires extra GPIO pin");
  ESP_LOGI(TAG, "- Requires ARA polling to identify device");
  ESP_LOGI(TAG, "- Wire-AND means sequential handling");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "Host Notify (Alternative):");
  ESP_LOGI(TAG, "- No GPIO required");
  ESP_LOGI(TAG, "- Device address included in notification");
  ESP_LOGI(TAG, "- Requires multi-master support");
  ESP_LOGI(TAG, "- Device must be master-capable");
  ESP_LOGI(TAG, "");

  ESP_LOGI(TAG, "Polling (Alternative):");
  ESP_LOGI(TAG, "- No extra hardware");
  ESP_LOGI(TAG, "- Simple to implement");
  ESP_LOGI(TAG, "- Higher latency");
  ESP_LOGI(TAG, "- Continuous bus traffic");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== SMBus SMBALERT# Signal Handling Concept Example ===\n");
  ESP_LOGI(TAG, "NOTE: This demonstrates SMBALERT# protocol CONCEPTS.");
  ESP_LOGI(TAG, "Actual implementation requires hardware SMBALERT# signal.\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "smbus_alert_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t* smbus_config = star_bus_config_create_i2c(
    "smbus_device",
    I2C_NUM_0,
    DEVICE1_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (smbus_config == NULL) {
    ESP_LOGE(TAG, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&g_manager, smbus_config);
  ret = star_bus_config_init(smbus_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init SMBus");
    return;
  }

  ESP_LOGI(TAG, "SMBus initialized");

  /* Demonstrate concepts */
  demonstrate_ara_protocol();
  demonstrate_alert_query();
  demonstrate_alert_scenarios();
  demonstrate_gpio_handling();
  compare_alert_methods();

  /* Implementation guidance */
  ESP_LOGI(TAG, "\n=== Implementation Checklist ===");
  ESP_LOGI(TAG, "1. Connect SMBALERT# to GPIO with pull-up");
  ESP_LOGI(TAG, "2. Configure GPIO interrupt (falling edge)");
  ESP_LOGI(TAG, "3. Create alert handling task");
  ESP_LOGI(TAG, "4. In ISR: Signal task, don't do I2C");
  ESP_LOGI(TAG, "5. In task: Read ARA repeatedly until no more alerts");
  ESP_LOGI(TAG, "6. Query each alerting device");
  ESP_LOGI(TAG, "7. Clear alert conditions");
  ESP_LOGI(TAG, "8. Verify SMBALERT# returns high");

  /* Cleanup */
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nSMBus SMBALERT# concept example complete");
}
