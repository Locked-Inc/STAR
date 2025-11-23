/**
 * @file 085_smbus_udid.c
 * @brief SMBus UDID (Unique Device Identifier) handling example
 *
 * Demonstrates:
 * - UDID structure and format (128-bit identifier)
 * - UDID reading from SMBus devices
 * - Device capability identification
 * - Address Resolution Protocol (ARP) concepts
 * - Device enumeration using UDID
 * - Multi-master device identification
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SMBUS_UDID";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* SMBus ARP commands */
#define ARP_CMD_PREPARE_TO_ARP (0x01)
#define ARP_CMD_RESET_DEVICE (0x02)
#define ARP_CMD_GET_UDID_GENERAL (0x03)
#define ARP_CMD_ASSIGN_ADDRESS (0x04)
#define ARP_CMD_GET_UDID_DIRECTED (0x0B)
#define ARP_CMD_RESET_DEVICE_GEN (0x0C)

/* SMBus ARP address */
#define SMBUS_ARP_ADDRESS (0x61)

/* UDID structure (128 bits / 16 bytes) */
typedef struct {
  uint8_t  device_capabilities;   /* Byte 0 */
  uint8_t  version;               /* Byte 1 */
  uint16_t vendor_id;             /* Bytes 2-3 */
  uint16_t device_id;             /* Bytes 4-5 */
  uint16_t interface;             /* Bytes 6-7 */
  uint16_t subsystem_vendor_id;   /* Bytes 8-9 */
  uint16_t subsystem_device_id;   /* Bytes 10-11 */
  uint32_t vendor_specific;       /* Bytes 12-15 */
} smbus_udid_t;

static star_bus_manager_t s_manager;

/**
 * @brief Parse UDID from raw bytes
 */
static void priv_parse_udid(const uint8_t *udid_bytes, smbus_udid_t *udid)
{
  udid->device_capabilities = udid_bytes[0];
  udid->version             = udid_bytes[1];
  udid->vendor_id           = (udid_bytes[2] << 8) | udid_bytes[3];
  udid->device_id           = (udid_bytes[4] << 8) | udid_bytes[5];
  udid->interface           = (udid_bytes[6] << 8) | udid_bytes[7];
  udid->subsystem_vendor_id = (udid_bytes[8] << 8) | udid_bytes[9];
  udid->subsystem_device_id = (udid_bytes[10] << 8) | udid_bytes[11];
  udid->vendor_specific     = ((uint32_t)udid_bytes[12] << 24) |
                          ((uint32_t)udid_bytes[13] << 16) |
                          ((uint32_t)udid_bytes[14] << 8) | udid_bytes[15];
}

/**
 * @brief Print UDID information
 */
static void priv_print_udid(const smbus_udid_t *udid)
{
  ESP_LOGI(s_tag, "  Device Capabilities: 0x%02X", udid->device_capabilities);
  ESP_LOGI(s_tag, "    - PEC Support: %s", (udid->device_capabilities & 0x80) ? "Yes" : "No");
  ESP_LOGI(s_tag, "    - Fixed Address: %s", (udid->device_capabilities & 0x40) ? "Yes" : "No");
  ESP_LOGI(s_tag, "    - Persistent: %s", (udid->device_capabilities & 0x01) ? "Yes" : "No");

  ESP_LOGI(s_tag, "  Version: 0x%02X", udid->version);
  ESP_LOGI(s_tag, "  Vendor ID: 0x%04X", udid->vendor_id);
  ESP_LOGI(s_tag, "  Device ID: 0x%04X", udid->device_id);
  ESP_LOGI(s_tag, "  Interface: 0x%04X", udid->interface);
  ESP_LOGI(s_tag, "  Subsystem Vendor ID: 0x%04X", udid->subsystem_vendor_id);
  ESP_LOGI(s_tag, "  Subsystem Device ID: 0x%04X", udid->subsystem_device_id);
  ESP_LOGI(s_tag, "  Vendor Specific: 0x%08lX", (unsigned long)udid->vendor_specific);
}

/**
 * @brief Read UDID from device (simulated)
 */
static esp_err_t priv_read_device_udid(uint8_t device_addr, uint8_t *udid_bytes)
{
  esp_err_t ret;
  uint8_t   block_len;

  /* Note: Real ARP command would be sent to SMBUS_ARP_ADDRESS */
  /* This is a simplified demonstration */

  ESP_LOGI(s_tag, "Reading UDID from device at 0x%02X", device_addr);

  /* Attempt to read UDID using block read */
  /* In real ARP, this would use specific ARP commands */
  ret = star_smbus_block_read(&s_manager, "device", device_addr, 0xFA, udid_bytes, 16, &block_len);

  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Block read failed, UDID not available");
    return ret;
  }

  if (block_len != 16) {
    ESP_LOGW(s_tag, "Invalid UDID length: %d (expected 16)", block_len);
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

/**
 * @brief Create example UDID for demonstration
 */
static void priv_create_example_udid(smbus_udid_t *udid, uint8_t device_num)
{
  /* Example: Temperature sensor with ARP support */
  udid->device_capabilities = 0x80;  /* PEC supported, volatile address */
  udid->version             = 0x11;  /* SMBus spec version 1.1 */
  udid->vendor_id           = 0x1234 + device_num;
  udid->device_id           = 0x5678;
  udid->interface           = 0x0001; /* SMBus interface */
  udid->subsystem_vendor_id = 0xABCD;
  udid->subsystem_device_id = 0xEF00 + device_num;
  udid->vendor_specific     = 0x12345678 + device_num;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SMBus UDID Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "udid_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t *config = star_bus_config_create_i2c(
    "device", I2C_NUM_0, SMBUS_ARP_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLK_SPEED);

  if (config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, config);
  ret = star_bus_config_init(config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SMBus interface");
    return;
  }

  ESP_LOGI(s_tag, "SMBus interface initialized\n");

  /* --- UDID Structure --- */
  ESP_LOGI(s_tag, "--- UDID Structure (128 bits / 16 bytes) ---");

  ESP_LOGI(s_tag, "\nByte  0: Device Capabilities");
  ESP_LOGI(s_tag, "  Bit 7: PEC support");
  ESP_LOGI(s_tag, "  Bit 6: Fixed address (non-ARP)");
  ESP_LOGI(s_tag, "  Bit 1: Persistent (survives reset)");
  ESP_LOGI(s_tag, "  Bit 0: AR flag (Address Resolved)");

  ESP_LOGI(s_tag, "\nByte  1: Version (SMBus spec version)");
  ESP_LOGI(s_tag, "Bytes 2-3: Vendor ID (big-endian)");
  ESP_LOGI(s_tag, "Bytes 4-5: Device ID");
  ESP_LOGI(s_tag, "Bytes 6-7: Interface (protocol version)");
  ESP_LOGI(s_tag, "Bytes 8-9: Subsystem Vendor ID");
  ESP_LOGI(s_tag, "Bytes 10-11: Subsystem Device ID");
  ESP_LOGI(s_tag, "Bytes 12-15: Vendor Specific Data");

  /* --- Example UDIDs --- */
  ESP_LOGI(s_tag, "\n--- Example UDID Demonstration ---");

  for (int i = 0; i < 3; i++) {
    smbus_udid_t udid;
    priv_create_example_udid(&udid, i);

    ESP_LOGI(s_tag, "\nDevice %d UDID:", i);
    priv_print_udid(&udid);
  }

  /* --- UDID Comparison --- */
  ESP_LOGI(s_tag, "\n--- UDID Comparison ---");

  smbus_udid_t udid1, udid2;
  priv_create_example_udid(&udid1, 0);
  priv_create_example_udid(&udid2, 1);

  ESP_LOGI(s_tag, "Comparing two UDIDs:");
  ESP_LOGI(s_tag, "  Vendor IDs: 0x%04X vs 0x%04X - %s",
           udid1.vendor_id,
           udid2.vendor_id,
           (udid1.vendor_id == udid2.vendor_id) ? "SAME" : "DIFFERENT");

  ESP_LOGI(s_tag, "  Device IDs: 0x%04X vs 0x%04X - %s",
           udid1.device_id,
           udid2.device_id,
           (udid1.device_id == udid2.device_id) ? "SAME" : "DIFFERENT");

  /* --- ARP Protocol Overview --- */
  ESP_LOGI(s_tag, "\n--- Address Resolution Protocol (ARP) ---");

  ESP_LOGI(s_tag, "\nARP Address: 0x61 (fixed)");
  ESP_LOGI(s_tag, "Purpose: Automatic address assignment for SMBus devices");

  ESP_LOGI(s_tag, "\nARP Commands:");
  ESP_LOGI(s_tag, "  0x01 - Prepare to ARP");
  ESP_LOGI(s_tag, "         Device prepares for address assignment");
  ESP_LOGI(s_tag, "  0x02 - Reset Device (Directed)");
  ESP_LOGI(s_tag, "         Reset specific device by UDID");
  ESP_LOGI(s_tag, "  0x03 - Get UDID (General)");
  ESP_LOGI(s_tag, "         Discover devices needing address");
  ESP_LOGI(s_tag, "  0x04 - Assign Address");
  ESP_LOGI(s_tag, "         Assign address to device with specific UDID");
  ESP_LOGI(s_tag, "  0x0B - Get UDID (Directed)");
  ESP_LOGI(s_tag, "         Read UDID from specific device");
  ESP_LOGI(s_tag, "  0x0C - Reset Device (General)");
  ESP_LOGI(s_tag, "         Reset all ARP-capable devices");

  /* --- ARP Process --- */
  ESP_LOGI(s_tag, "\n--- ARP Address Assignment Process ---");

  ESP_LOGI(s_tag, "\n1. Prepare to ARP:");
  ESP_LOGI(s_tag, "   Host: Send Prepare to ARP to 0x61");
  ESP_LOGI(s_tag, "   Devices: Clear AR flag, prepare for enumeration");

  ESP_LOGI(s_tag, "\n2. Get UDID:");
  ESP_LOGI(s_tag, "   Host: Send Get UDID (General) to 0x61");
  ESP_LOGI(s_tag, "   Device: Respond with 16-byte UDID (if AR=0)");

  ESP_LOGI(s_tag, "\n3. Assign Address:");
  ESP_LOGI(s_tag, "   Host: Send Assign Address with UDID + new address");
  ESP_LOGI(s_tag, "   Device: Accept address, set AR flag");

  ESP_LOGI(s_tag, "\n4. Verify:");
  ESP_LOGI(s_tag, "   Host: Communication with newly assigned address");
  ESP_LOGI(s_tag, "   Device: Respond on new address");

  ESP_LOGI(s_tag, "\n5. Repeat:");
  ESP_LOGI(s_tag, "   Continue steps 2-4 until no more devices respond");

  /* --- Use Cases --- */
  ESP_LOGI(s_tag, "\n--- UDID Use Cases ---");

  ESP_LOGI(s_tag, "\n1. Plug-and-Play:");
  ESP_LOGI(s_tag, "   - Automatic device discovery");
  ESP_LOGI(s_tag, "   - Dynamic address assignment");
  ESP_LOGI(s_tag, "   - No manual configuration");

  ESP_LOGI(s_tag, "\n2. Hot-Plug Support:");
  ESP_LOGI(s_tag, "   - Detect new devices at runtime");
  ESP_LOGI(s_tag, "   - Reassign addresses after reset");
  ESP_LOGI(s_tag, "   - Handle device removal");

  ESP_LOGI(s_tag, "\n3. Multi-Master Systems:");
  ESP_LOGI(s_tag, "   - Resolve address conflicts");
  ESP_LOGI(s_tag, "   - Coordinate between masters");
  ESP_LOGI(s_tag, "   - Unique device identification");

  ESP_LOGI(s_tag, "\n4. Device Inventory:");
  ESP_LOGI(s_tag, "   - Track connected devices");
  ESP_LOGI(s_tag, "   - Identify device capabilities");
  ESP_LOGI(s_tag, "   - Verify device authenticity");

  /* --- Implementation Considerations --- */
  ESP_LOGI(s_tag, "\n--- Implementation Considerations ---");

  ESP_LOGI(s_tag, "\n1. Not all SMBus devices support ARP");
  ESP_LOGI(s_tag, "   - Check device datasheet");
  ESP_LOGI(s_tag, "   - Fixed-address devices don't need ARP");
  ESP_LOGI(s_tag, "   - Legacy devices may not support UDID");

  ESP_LOGI(s_tag, "\n2. PEC (Packet Error Checking):");
  ESP_LOGI(s_tag, "   - Recommended for ARP commands");
  ESP_LOGI(s_tag, "   - Check device capabilities byte");
  ESP_LOGI(s_tag, "   - Ensures data integrity");

  ESP_LOGI(s_tag, "\n3. Timing:");
  ESP_LOGI(s_tag, "   - ARP process can take time");
  ESP_LOGI(s_tag, "   - Multiple devices require multiple cycles");
  ESP_LOGI(s_tag, "   - Allow timeout for device response");

  ESP_LOGI(s_tag, "\n4. Address Management:");
  ESP_LOGI(s_tag, "   - Maintain address pool");
  ESP_LOGI(s_tag, "   - Track assigned addresses");
  ESP_LOGI(s_tag, "   - Avoid conflicts with fixed addresses");

  /* --- Device Examples --- */
  ESP_LOGI(s_tag, "\n--- Devices Supporting ARP/UDID ---");

  ESP_LOGI(s_tag, "\nCommon devices with ARP support:");
  ESP_LOGI(s_tag, "  - SMBus ARP Controllers");
  ESP_LOGI(s_tag, "  - Intelligent battery systems");
  ESP_LOGI(s_tag, "  - Power management ICs");
  ESP_LOGI(s_tag, "  - System management peripherals");

  ESP_LOGI(s_tag, "\nFixed-address devices (no ARP):");
  ESP_LOGI(s_tag, "  - Most temperature sensors");
  ESP_LOGI(s_tag, "  - EEPROMs");
  ESP_LOGI(s_tag, "  - A/D converters");
  ESP_LOGI(s_tag, "  - GPIO expanders");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Best Practices ---");

  ESP_LOGI(s_tag, "1. Document UDID format for custom devices");
  ESP_LOGI(s_tag, "2. Use standard vendor IDs when available");
  ESP_LOGI(s_tag, "3. Implement ARP only if dynamic addressing needed");
  ESP_LOGI(s_tag, "4. Test hot-plug scenarios thoroughly");
  ESP_LOGI(s_tag, "5. Handle address conflicts gracefully");
  ESP_LOGI(s_tag, "6. Store UDID for device tracking");
  ESP_LOGI(s_tag, "7. Implement timeout and retry logic");
  ESP_LOGI(s_tag, "8. Use PEC for critical ARP operations");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSMBus UDID example complete");
}
