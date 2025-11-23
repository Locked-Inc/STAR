/**
 * @file 084_smbus_host_notify.c
 * @brief SMBus Host Notify protocol concept demonstration
 *
 * Demonstrates:
 * - Host Notify protocol concepts
 * - Device-initiated notifications
 * - Status and data word retrieval
 * - Comparison with SMBALERT#
 *
 * NOTE: This example demonstrates Host Notify CONCEPTS.
 * Actual implementation requires multi-master I2C support.
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char *s_tag = "SMBUS_HOST_NOTIFY";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

#define HOST_NOTIFY_ADDR     (0x08)  // Standard Host Notify address

// Example device addresses
#define DEVICE1_ADDR (0x50)
#define DEVICE2_ADDR (0x51)
#define DEVICE3_ADDR (0x52)

static star_bus_manager_t s_manager;

/**
 * @brief Demonstrate Host Notify protocol
 */
static void priv_demonstrate_host_notify_protocol(void)
{
  ESP_LOGI(s_tag, "\n=== Host Notify Protocol ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Host Notify Address: 0x%02X (0b0001000)", HOST_NOTIFY_ADDR);
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "How Host Notify Works:");
  ESP_LOGI(s_tag, "1. Device becomes master (multi-master required)");
  ESP_LOGI(s_tag, "2. Device writes to Host Notify address (0x08)");
  ESP_LOGI(s_tag, "3. Write includes 3 bytes:");
  ESP_LOGI(s_tag, "   - Byte 0: Device address (left-shifted)");
  ESP_LOGI(s_tag, "   - Byte 1: Status/data low byte");
  ESP_LOGI(s_tag, "   - Byte 2: Status/data high byte");
  ESP_LOGI(s_tag, "4. Host receives notification");
  ESP_LOGI(s_tag, "5. Host queries device for details");
  ESP_LOGI(s_tag, "6. Host takes appropriate action");
}

/**
 * @brief Demonstrate notification message format
 */
static void priv_demonstrate_message_format(void)
{
  ESP_LOGI(s_tag, "\n=== Host Notify Message Format ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Example notification from device 0x50:");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Byte 0: 0xA0  (Device address 0x50 << 1)");
  ESP_LOGI(s_tag, "Byte 1: 0x02  (Status low byte)");
  ESP_LOGI(s_tag, "Byte 2: 0x00  (Status high byte)");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Status word 0x0002 might indicate:");
  ESP_LOGI(s_tag, "  Bit 0: Alert condition");
  ESP_LOGI(s_tag, "  Bit 1: Over temperature");
  ESP_LOGI(s_tag, "  Bit 2: Under voltage");
  ESP_LOGI(s_tag, "  Bit 3: Over current");
  ESP_LOGI(s_tag, "  (Bits defined by device specification)");
}

/**
 * @brief Demonstrate notification handling
 */
static void priv_demonstrate_notification_handling(void)
{
  ESP_LOGI(s_tag, "\n=== Handling Notifications ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Host Implementation (as slave at 0x08):");
  ESP_LOGI(s_tag, "1. Configure as I2C slave at address 0x08");
  ESP_LOGI(s_tag, "2. Register slave write callback");
  ESP_LOGI(s_tag, "3. In callback:");
  ESP_LOGI(s_tag, "   - Extract device address (byte 0 >> 1)");
  ESP_LOGI(s_tag, "   - Extract status word (bytes 1-2)");
  ESP_LOGI(s_tag, "   - Queue notification for processing");
  ESP_LOGI(s_tag, "4. In task:");
  ESP_LOGI(s_tag, "   - Process queued notifications");
  ESP_LOGI(s_tag, "   - Query device for details");
  ESP_LOGI(s_tag, "   - Take appropriate action");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Note: Cannot do I2C master operations in slave callback!");
}

/**
 * @brief Demonstrate device perspective
 */
static void priv_demonstrate_device_perspective(void)
{
  ESP_LOGI(s_tag, "\n=== Device Sending Notification ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Device Implementation:");
  ESP_LOGI(s_tag, "1. Detect event requiring notification");
  ESP_LOGI(s_tag, "2. Become I2C master (claim bus)");
  ESP_LOGI(s_tag, "3. Prepare notification:");
  ESP_LOGI(s_tag, "   uint8_t data[3];");
  ESP_LOGI(s_tag, "   data[0] = my_address << 1;");
  ESP_LOGI(s_tag, "   data[1] = status & 0xFF;");
  ESP_LOGI(s_tag, "   data[2] = (status >> 8) & 0xFF;");
  ESP_LOGI(s_tag, "4. Write to Host Notify address:");
  ESP_LOGI(s_tag, "   star_smbus_send_byte(manager, bus, 0x08, data)");
  ESP_LOGI(s_tag, "5. Release bus");
  ESP_LOGI(s_tag, "6. Return to slave mode");
}

/**
 * @brief Compare Host Notify with SMBALERT#
 */
static void priv_compare_notification_methods(void)
{
  ESP_LOGI(s_tag, "\n=== Host Notify vs SMBALERT# ===");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "HOST NOTIFY:");
  ESP_LOGI(s_tag, "Advantages:");
  ESP_LOGI(s_tag, "  + No extra GPIO pin required");
  ESP_LOGI(s_tag, "  + Device address included in message");
  ESP_LOGI(s_tag, "  + Status word included in message");
  ESP_LOGI(s_tag, "  + Lower latency (no ARA polling)");
  ESP_LOGI(s_tag, "  + Can distinguish multiple alerts");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Disadvantages:");
  ESP_LOGI(s_tag, "  - Requires multi-master support");
  ESP_LOGI(s_tag, "  - Device must be master-capable");
  ESP_LOGI(s_tag, "  - More complex protocol");
  ESP_LOGI(s_tag, "  - Bus arbitration required");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SMBALERT#:");
  ESP_LOGI(s_tag, "Advantages:");
  ESP_LOGI(s_tag, "  + Simpler hardware (just open-drain pin)");
  ESP_LOGI(s_tag, "  + Standard I2C slave operation");
  ESP_LOGI(s_tag, "  + No multi-master required");
  ESP_LOGI(s_tag, "  + Well-established protocol");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Disadvantages:");
  ESP_LOGI(s_tag, "  - Requires extra GPIO pin");
  ESP_LOGI(s_tag, "  - Requires ARA polling");
  ESP_LOGI(s_tag, "  - Higher latency");
  ESP_LOGI(s_tag, "  - Wire-AND limits parallelism");
}

/**
 * @brief Demonstrate typical scenarios
 */
static void priv_demonstrate_scenarios(void)
{
  ESP_LOGI(s_tag, "\n=== Example Scenarios ===");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Scenario 1: Over Temperature");
  ESP_LOGI(s_tag, "- Device 0x50 detects temperature > threshold");
  ESP_LOGI(s_tag, "- Device sends Host Notify: [0xA0, 0x02, 0x00]");
  ESP_LOGI(s_tag, "- Host extracts: device=0x50, status=0x0002");
  ESP_LOGI(s_tag, "- Host reads temperature from device 0x50");
  ESP_LOGI(s_tag, "- Host takes cooling action");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Scenario 2: Data Ready");
  ESP_LOGI(s_tag, "- Device 0x51 completes measurement");
  ESP_LOGI(s_tag, "- Device sends Host Notify: [0xA2, 0x10, 0x00]");
  ESP_LOGI(s_tag, "- Host extracts: device=0x51, status=0x0010");
  ESP_LOGI(s_tag, "- Host reads measurement data from 0x51");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Scenario 3: Multiple Devices");
  ESP_LOGI(s_tag, "- Device 0x50 sends notification");
  ESP_LOGI(s_tag, "- Host processes 0x50");
  ESP_LOGI(s_tag, "- Device 0x51 sends notification");
  ESP_LOGI(s_tag, "- Host processes 0x51");
  ESP_LOGI(s_tag, "- Notifications queued if host busy");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SMBus Host Notify Protocol Concept Example ===\n");
  ESP_LOGI(s_tag, "NOTE: This demonstrates Host Notify CONCEPTS.");
  ESP_LOGI(s_tag, "Actual implementation requires multi-master I2C support.\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "host_notify_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t *notify_config = star_bus_config_create_i2c(
    "notify_bus",
    I2C_NUM_0,
    HOST_NOTIFY_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (notify_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, notify_config);
  ret = star_bus_config_init(notify_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init Host Notify interface");
    return;
  }

  ESP_LOGI(s_tag, "Host Notify interface initialized");

  /* Demonstrate concepts */
  priv_demonstrate_host_notify_protocol();
  priv_demonstrate_message_format();
  priv_demonstrate_notification_handling();
  priv_demonstrate_device_perspective();
  priv_compare_notification_methods();
  priv_demonstrate_scenarios();

  /* Implementation notes */
  ESP_LOGI(s_tag, "\n=== Implementation Checklist ===");
  ESP_LOGI(s_tag, "For Host (receiving notifications):");
  ESP_LOGI(s_tag, "  1. Configure as I2C slave at address 0x08");
  ESP_LOGI(s_tag, "  2. Register slave write callback");
  ESP_LOGI(s_tag, "  3. Parse notification in callback");
  ESP_LOGI(s_tag, "  4. Queue for processing (don't do I2C in callback)");
  ESP_LOGI(s_tag, "  5. Process notifications in task");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "For Device (sending notifications):");
  ESP_LOGI(s_tag, "  1. Support multi-master operation");
  ESP_LOGI(s_tag, "  2. Claim bus when notification needed");
  ESP_LOGI(s_tag, "  3. Send 3-byte message to 0x08");
  ESP_LOGI(s_tag, "  4. Handle arbitration loss gracefully");
  ESP_LOGI(s_tag, "  5. Return to slave mode");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSMBus Host Notify concept example complete");
}
