/**
 * @file 109_bms_seal.c
 * @brief Seal/unseal operations for BQ7850
 *
 * Demonstrates:
 * - Sealing device for production
 * - Unsealing for configuration
 * - Full access mode management
 * - Security key handling
 * - Sealed vs unsealed capabilities
 *
 * WARNING: This example demonstrates seal/unseal operations.
 * Sealing prevents configuration changes. Use with caution!
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char* TAG = "BMS_SEAL";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Default unseal keys for BQ7850 (check datasheet for actual values) */
#define UNSEAL_KEY_1 (0x0414)
#define UNSEAL_KEY_2 (0x3672)
#define FULL_ACCESS_KEY_1 (0xFFFF)
#define FULL_ACCESS_KEY_2 (0xFFFF)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Seal/Unseal Example ===\n");
  ESP_LOGW(TAG, "WARNING: This example modifies device security state");
  ESP_LOGW(TAG, "Ensure you have the correct unseal keys before proceeding!\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "seal_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus manager");
    return;
  }

  /* Create and configure I2C/SMBus bus first */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "bms_bus",
    I2C_NUM_0,
    BQ7850_DEFAULT_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  ret = star_bus_manager_add_bus(&g_manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add bus to manager");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ret = star_bus_config_init(i2c_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  /* Initialize BMS */
  bq7850_config_t bms_config = {
    .num_cells = 16,
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&g_bms, &g_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init BMS: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ESP_LOGI(TAG, "BMS initialized successfully\n");

  /* ===== SECURITY MODES EXPLAINED ===== */
  ESP_LOGI(TAG, "===== BQ7850 Security Modes =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. SEALED MODE:");
  ESP_LOGI(TAG, "   - Default production state");
  ESP_LOGI(TAG, "   - Read-only access to most registers");
  ESP_LOGI(TAG, "   - Cannot modify protection settings");
  ESP_LOGI(TAG, "   - Cannot modify configuration");
  ESP_LOGI(TAG, "   - Prevents accidental/malicious changes");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. UNSEALED MODE:");
  ESP_LOGI(TAG, "   - Requires 2-step unseal key sequence");
  ESP_LOGI(TAG, "   - Allows configuration changes");
  ESP_LOGI(TAG, "   - Can modify protection thresholds");
  ESP_LOGI(TAG, "   - Limited to safe operations");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. FULL ACCESS MODE:");
  ESP_LOGI(TAG, "   - Requires additional 2-step key sequence");
  ESP_LOGI(TAG, "   - Complete access to all features");
  ESP_LOGI(TAG, "   - Can modify manufacturer data");
  ESP_LOGI(TAG, "   - Use with extreme caution");

  /* ===== CHECK CURRENT SEAL STATUS ===== */
  ESP_LOGI(TAG, "\n===== Current Security Status =====");
  ESP_LOGI(TAG, "Note: Checking seal status (read operation status)");
  ESP_LOGI(TAG, "This example assumes device is in SEALED mode");

  /* ===== UNSEALING PROCEDURE ===== */
  ESP_LOGI(TAG, "\n===== Unsealing Procedure =====");
  ESP_LOGI(TAG, "To unseal the device:");
  ESP_LOGI(TAG, "1. Write first unseal key (0x%04X) to ManufacturerAccess (0x00)", UNSEAL_KEY_1);
  ESP_LOGI(TAG, "2. Write second unseal key (0x%04X) to ManufacturerAccess (0x00)", UNSEAL_KEY_2);
  ESP_LOGI(TAG, "3. Verify unsealed status");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "IMPORTANT: Unseal keys are device-specific!");
  ESP_LOGI(TAG, "Check BQ7850 datasheet or manufacturer documentation");
  ESP_LOGI(TAG, "");
  ESP_LOGW(TAG, "This example does NOT actually unseal the device");
  ESP_LOGW(TAG, "It only demonstrates the procedure");

  /* ===== OPERATIONS IN SEALED MODE ===== */
  ESP_LOGI(TAG, "\n===== Operations in Sealed Mode =====");
  ESP_LOGI(TAG, "Allowed operations:");
  ESP_LOGI(TAG, "  - Read all measurement data (voltage, current, temp)");
  ESP_LOGI(TAG, "  - Read SOC and capacity data");
  ESP_LOGI(TAG, "  - Read status registers");
  ESP_LOGI(TAG, "  - Read device information");
  ESP_LOGI(TAG, "  - Normal monitoring and control");

  /* Verify we can read in sealed mode */
  bq7850_battery_state_t state;
  ret = star_bms_bq7850_read_battery_state(&g_bms, &state);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "\nSealed mode read test SUCCESS:");
    ESP_LOGI(TAG, "  Pack voltage: %u mV", state.cells.pack_mv);
    ESP_LOGI(TAG, "  Current: %d mA", state.current.current_ma);
    ESP_LOGI(TAG, "  SOC: %d%%", state.soc.relative_soc);
  } else {
    ESP_LOGW(TAG, "Read operation failed (unexpected)");
  }

  ESP_LOGI(TAG, "\nBlocked operations:");
  ESP_LOGI(TAG, "  - Modify protection thresholds");
  ESP_LOGI(TAG, "  - Change configuration");
  ESP_LOGI(TAG, "  - Update calibration data");
  ESP_LOGI(TAG, "  - Modify manufacturer data");

  /* Try to modify protection (should fail in sealed mode) */
  ESP_LOGI(TAG, "\nTesting protection write (should fail if sealed):");
  bq7850_protection_t protection = {
    .overvoltage_mv = 4200,
    .undervoltage_mv = 2800,
    .overcharge_ma = 5000,
    .overdischarge_ma = 10000,
    .overtemp_c = 60,
    .undertemp_c = 0
  };

  ret = star_bms_bq7850_write_protection(&g_bms, &protection);
  if (ret != ESP_OK) {
    ESP_LOGI(TAG, "  Result: BLOCKED (as expected in sealed mode)");
  } else {
    ESP_LOGW(TAG, "  Result: SUCCESS (device may be unsealed)");
  }

  /* ===== SEALING PROCEDURE ===== */
  ESP_LOGI(TAG, "\n===== Sealing Procedure =====");
  ESP_LOGI(TAG, "To seal the device:");
  ESP_LOGI(TAG, "1. Ensure all configuration is complete");
  ESP_LOGI(TAG, "2. Verify protection settings");
  ESP_LOGI(TAG, "3. Write SEAL command (0x0030) to ManufacturerAccess");
  ESP_LOGI(TAG, "4. Device immediately enters sealed mode");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Once sealed:");
  ESP_LOGI(TAG, "  - Configuration is protected");
  ESP_LOGI(TAG, "  - Requires unseal keys to modify");
  ESP_LOGI(TAG, "  - Safe for end-user deployment");

  /* ===== FULL ACCESS MODE ===== */
  ESP_LOGI(TAG, "\n===== Full Access Mode =====");
  ESP_LOGI(TAG, "To enter full access mode:");
  ESP_LOGI(TAG, "1. First unseal the device");
  ESP_LOGI(TAG, "2. Write first full access key (0x%04X)", FULL_ACCESS_KEY_1);
  ESP_LOGI(TAG, "3. Write second full access key (0x%04X)", FULL_ACCESS_KEY_2);
  ESP_LOGI(TAG, "");
  ESP_LOGW(TAG, "Full access mode allows:");
  ESP_LOGW(TAG, "  - Modification of manufacturer data");
  ESP_LOGW(TAG, "  - Advanced calibration operations");
  ESP_LOGW(TAG, "  - Potentially dangerous operations");
  ESP_LOGW(TAG, "");
  ESP_LOGW(TAG, "USE EXTREME CAUTION!");
  ESP_LOGW(TAG, "Incorrect operations can brick the device");

  /* ===== PRODUCTION WORKFLOW ===== */
  ESP_LOGI(TAG, "\n===== Recommended Production Workflow =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Development/Testing Phase:");
  ESP_LOGI(TAG, "   - Keep device unsealed");
  ESP_LOGI(TAG, "   - Test all features thoroughly");
  ESP_LOGI(TAG, "   - Tune protection parameters");
  ESP_LOGI(TAG, "   - Calibrate if necessary");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Pre-Production:");
  ESP_LOGI(TAG, "   - Finalize all configuration");
  ESP_LOGI(TAG, "   - Document all settings");
  ESP_LOGI(TAG, "   - Verify protection thresholds");
  ESP_LOGI(TAG, "   - Test seal/unseal procedure");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Production:");
  ESP_LOGI(TAG, "   - Program final configuration");
  ESP_LOGI(TAG, "   - Verify settings");
  ESP_LOGI(TAG, "   - Seal device");
  ESP_LOGI(TAG, "   - Verify sealed operation");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Field Service:");
  ESP_LOGI(TAG, "   - Unseal only when necessary");
  ESP_LOGI(TAG, "   - Make required changes");
  ESP_LOGI(TAG, "   - Re-seal before return to service");
  ESP_LOGI(TAG, "   - Document all modifications");

  /* ===== SECURITY BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Security Best Practices =====");
  ESP_LOGI(TAG, "1. Protect unseal keys (use secure storage)");
  ESP_LOGI(TAG, "2. Limit access to unseal capability");
  ESP_LOGI(TAG, "3. Log all seal/unseal operations");
  ESP_LOGI(TAG, "4. Verify device is sealed before deployment");
  ESP_LOGI(TAG, "5. Test unsealing procedure before production");
  ESP_LOGI(TAG, "6. Document unseal keys securely");
  ESP_LOGI(TAG, "7. Consider changing default keys if supported");
  ESP_LOGI(TAG, "8. Never hardcode keys in production firmware");

  /* ===== KEY MANAGEMENT ===== */
  ESP_LOGI(TAG, "\n===== Unseal Key Management =====");
  ESP_LOGI(TAG, "Options for storing unseal keys:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. NVS (Non-Volatile Storage):");
  ESP_LOGI(TAG, "   - Encrypted NVS partition");
  ESP_LOGI(TAG, "   - Access controlled by firmware");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Secure Element:");
  ESP_LOGI(TAG, "   - Hardware security module");
  ESP_LOGI(TAG, "   - Keys never exposed to software");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Remote Server:");
  ESP_LOGI(TAG, "   - Keys stored on secure server");
  ESP_LOGI(TAG, "   - Retrieved over authenticated connection");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Documentation Only:");
  ESP_LOGI(TAG, "   - Keys in secure documentation");
  ESP_LOGI(TAG, "   - Manual unsealing procedure");

  /* ===== TROUBLESHOOTING ===== */
  ESP_LOGI(TAG, "\n===== Troubleshooting =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Cannot unseal device");
  ESP_LOGI(TAG, "  - Verify unseal keys are correct");
  ESP_LOGI(TAG, "  - Check key sequence (must be sent in order)");
  ESP_LOGI(TAG, "  - Ensure no I2C communication errors");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Configuration changes not taking effect");
  ESP_LOGI(TAG, "  - Check if device is sealed");
  ESP_LOGI(TAG, "  - Unseal before making changes");
  ESP_LOGI(TAG, "  - Verify write operations succeed");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Lost unseal keys");
  ESP_LOGI(TAG, "  - Check manufacturer documentation");
  ESP_LOGI(TAG, "  - Contact technical support");
  ESP_LOGI(TAG, "  - May require device replacement");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nSeal/unseal example complete");
  ESP_LOGI(TAG, "Remember: This example did NOT modify device security state");
}
