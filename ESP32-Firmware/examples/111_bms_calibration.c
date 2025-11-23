/**
 * @file 111_bms_calibration.c
 * @brief Calibration mode and procedures
 *
 * Demonstrates:
 * - Entering calibration mode
 * - Calibrating voltage/current readings
 * - Verifying calibration accuracy
 * - Calibration best practices
 *
 * WARNING: Calibration mode requires device to be unsealed
 * and should only be performed with proper equipment!
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char* TAG = "BMS_CALIBRATION";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Calibration Example ===\n");
  ESP_LOGW(TAG, "WARNING: This example is informational only");
  ESP_LOGW(TAG, "Actual calibration requires precision equipment\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "calibration_demo", NULL, NULL);
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

  /* ===== CALIBRATION OVERVIEW ===== */
  ESP_LOGI(TAG, "===== Calibration Overview =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "BQ7850 requires calibration for:");
  ESP_LOGI(TAG, "  1. Voltage measurement accuracy");
  ESP_LOGI(TAG, "  2. Current sense resistor tolerance");
  ESP_LOGI(TAG, "  3. Temperature sensor offsets");
  ESP_LOGI(TAG, "  4. Coulomb counter accuracy");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Factory calibration is usually sufficient");
  ESP_LOGI(TAG, "Field calibration needed when:");
  ESP_LOGI(TAG, "  - Using custom current sense resistor");
  ESP_LOGI(TAG, "  - Requiring high accuracy (< 1%%)");
  ESP_LOGI(TAG, "  - After component replacement");
  ESP_LOGI(TAG, "  - For certification/compliance");

  /* ===== REQUIRED EQUIPMENT ===== */
  ESP_LOGI(TAG, "\n===== Required Equipment =====");
  ESP_LOGI(TAG, "For proper calibration:");
  ESP_LOGI(TAG, "  - Precision voltage source (0.01%% accuracy)");
  ESP_LOGI(TAG, "  - Precision current source (0.1%% accuracy)");
  ESP_LOGI(TAG, "  - Calibrated multimeter (6.5 digit minimum)");
  ESP_LOGI(TAG, "  - Temperature chamber (+-0.1C stability)");
  ESP_LOGI(TAG, "  - Stable power supply");
  ESP_LOGI(TAG, "  - Cell simulator or actual cells");

  /* ===== ENTERING CALIBRATION MODE ===== */
  ESP_LOGI(TAG, "\n===== Entering Calibration Mode =====");
  ESP_LOGI(TAG, "Procedure:");
  ESP_LOGI(TAG, "  1. Unseal device (requires unseal keys)");
  ESP_LOGI(TAG, "  2. Write 0x0040 to ManufacturerAccess (0x00)");
  ESP_LOGI(TAG, "  3. Device enters calibration mode");
  ESP_LOGI(TAG, "  4. Calibration registers become writable");
  ESP_LOGI(TAG, "");
  ESP_LOGW(TAG, "WARNING: Device must be unsealed first!");
  ESP_LOGW(TAG, "Calibration mode is NOT entered in this example");

  /* ===== VOLTAGE CALIBRATION ===== */
  ESP_LOGI(TAG, "\n===== Voltage Calibration Procedure =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Cell Voltage Calibration:");
  ESP_LOGI(TAG, "   a. Connect precision voltage source to cell inputs");
  ESP_LOGI(TAG, "   b. Apply known voltage (e.g., 3.700V)");
  ESP_LOGI(TAG, "   c. Read BMS voltage measurement");
  ESP_LOGI(TAG, "   d. Calculate offset/gain correction");
  ESP_LOGI(TAG, "   e. Write correction to calibration registers");
  ESP_LOGI(TAG, "   f. Verify corrected reading");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Pack Voltage Calibration:");
  ESP_LOGI(TAG, "   - Similar procedure for pack voltage");
  ESP_LOGI(TAG, "   - Use full-scale voltage");
  ESP_LOGI(TAG, "   - Verify across voltage range");

  /* Read current voltage (uncalibrated example) */
  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&g_bms, &cells);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "\nCurrent voltage readings (uncalibrated):");
    for (int i = 0; i < cells.valid_cells && i < 4; i++) {
      ESP_LOGI(TAG, "  Cell %d: %d mV", i + 1, cells.cell_mv[i]);
    }
  }

  /* ===== CURRENT CALIBRATION ===== */
  ESP_LOGI(TAG, "\n===== Current Calibration Procedure =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Current measurement depends on sense resistor:");
  ESP_LOGI(TAG, "  V_sense = I * R_sense");
  ESP_LOGI(TAG, "  I = V_sense / R_sense");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Calibration steps:");
  ESP_LOGI(TAG, "  1. Measure actual sense resistor value");
  ESP_LOGI(TAG, "     - Use 4-wire Kelvin measurement");
  ESP_LOGI(TAG, "     - Account for temperature coefficient");
  ESP_LOGI(TAG, "     - Typical values: 1-10 mOhm");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  2. Apply known current:");
  ESP_LOGI(TAG, "     - Use precision current source");
  ESP_LOGI(TAG, "     - Test at multiple currents (1A, 5A, 10A)");
  ESP_LOGI(TAG, "     - Both charge and discharge directions");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  3. Calculate correction factor:");
  ESP_LOGI(TAG, "     - Factor = I_actual / I_measured");
  ESP_LOGI(TAG, "     - Write to current calibration register");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  4. Verify linearity:");
  ESP_LOGI(TAG, "     - Test across expected current range");
  ESP_LOGI(TAG, "     - Check for offset errors");

  /* Read current measurement */
  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "\nCurrent readings (uncalibrated):");
    ESP_LOGI(TAG, "  Instantaneous: %d mA", current.current_ma);
    ESP_LOGI(TAG, "  Average: %d mA", current.avg_current_ma);
  }

  /* ===== TEMPERATURE CALIBRATION ===== */
  ESP_LOGI(TAG, "\n===== Temperature Calibration Procedure =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Temperature sensors (typically NTC thermistors):");
  ESP_LOGI(TAG, "  1. Place battery/sensors in temperature chamber");
  ESP_LOGI(TAG, "  2. Set chamber to known temperature (e.g., 25.0C)");
  ESP_LOGI(TAG, "  3. Wait for thermal equilibrium (30+ minutes)");
  ESP_LOGI(TAG, "  4. Read BMS temperature measurement");
  ESP_LOGI(TAG, "  5. Calculate offset correction");
  ESP_LOGI(TAG, "  6. Write correction to registers");
  ESP_LOGI(TAG, "  7. Repeat at multiple temperatures (0C, 25C, 50C)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Note: Temperature calibration usually not needed");
  ESP_LOGI(TAG, "unless using custom thermistors");

  /* ===== VERIFICATION PROCEDURE ===== */
  ESP_LOGI(TAG, "\n===== Calibration Verification =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "After calibration:");
  ESP_LOGI(TAG, "  1. Exit calibration mode");
  ESP_LOGI(TAG, "  2. Reset device (power cycle or soft reset)");
  ESP_LOGI(TAG, "  3. Verify voltage accuracy:");
  ESP_LOGI(TAG, "     - Test at multiple voltages");
  ESP_LOGI(TAG, "     - Compare to reference multimeter");
  ESP_LOGI(TAG, "     - Target: < +-0.1%% error");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  4. Verify current accuracy:");
  ESP_LOGI(TAG, "     - Test at multiple currents");
  ESP_LOGI(TAG, "     - Both charge and discharge");
  ESP_LOGI(TAG, "     - Target: < +-1%% error");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  5. Document results:");
  ESP_LOGI(TAG, "     - Record calibration date");
  ESP_LOGI(TAG, "     - Save test data");
  ESP_LOGI(TAG, "     - Note environmental conditions");
  ESP_LOGI(TAG, "     - Serial number / unit ID");

  /* ===== CALIBRATION DATA STORAGE ===== */
  ESP_LOGI(TAG, "\n===== Calibration Data Storage =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "BQ7850 stores calibration in non-volatile memory:");
  ESP_LOGI(TAG, "  - Survives power cycles");
  ESP_LOGI(TAG, "  - Protected when device is sealed");
  ESP_LOGI(TAG, "  - Should not need recalibration");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Backup calibration data externally:");
  ESP_LOGI(TAG, "  - Read calibration registers after calibration");
  ESP_LOGI(TAG, "  - Store in secure database");
  ESP_LOGI(TAG, "  - Use for verification or restore");

  /* ===== FIELD CALIBRATION ===== */
  ESP_LOGI(TAG, "\n===== Field Calibration (Simplified) =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "If precision equipment not available:");
  ESP_LOGI(TAG, "  1. Use calibrated consumer multimeter");
  ESP_LOGI(TAG, "  2. Measure actual voltages/currents");
  ESP_LOGI(TAG, "  3. Apply software compensation");
  ESP_LOGI(TAG, "  4. Store correction factors in NVS");
  ESP_LOGI(TAG, "  5. Apply in firmware before display");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Limitations:");
  ESP_LOGI(TAG, "  - Less accurate than hardware calibration");
  ESP_LOGI(TAG, "  - Requires firmware modifications");
  ESP_LOGI(TAG, "  - May drift over time");

  /* ===== RECALIBRATION SCHEDULE ===== */
  ESP_LOGI(TAG, "\n===== Recalibration Schedule =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Recommended recalibration intervals:");
  ESP_LOGI(TAG, "  - Standard applications: Never (if factory calibrated)");
  ESP_LOGI(TAG, "  - High accuracy needs: Annually");
  ESP_LOGI(TAG, "  - After repair: Always");
  ESP_LOGI(TAG, "  - After sense resistor change: Required");
  ESP_LOGI(TAG, "  - Certification/audit: As required");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Signs recalibration may be needed:");
  ESP_LOGI(TAG, "  - Measurements drift from reference");
  ESP_LOGI(TAG, "  - SOC accuracy degrades");
  ESP_LOGI(TAG, "  - Current integration errors accumulate");

  /* ===== CALIBRATION BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Calibration Best Practices =====");
  ESP_LOGI(TAG, "1. Perform calibration at 25C ambient temperature");
  ESP_LOGI(TAG, "2. Allow thermal stabilization before measurements");
  ESP_LOGI(TAG, "3. Use certified reference equipment");
  ESP_LOGI(TAG, "4. Calibrate across full operating range");
  ESP_LOGI(TAG, "5. Document all calibration parameters");
  ESP_LOGI(TAG, "6. Verify after writing calibration data");
  ESP_LOGI(TAG, "7. Seal device after successful calibration");
  ESP_LOGI(TAG, "8. Maintain calibration records");

  /* ===== TROUBLESHOOTING ===== */
  ESP_LOGI(TAG, "\n===== Calibration Troubleshooting =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Cannot enter calibration mode");
  ESP_LOGI(TAG, "  Solution: Ensure device is unsealed");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Calibration not persisting");
  ESP_LOGI(TAG, "  Solution: Verify write to correct registers");
  ESP_LOGI(TAG, "  Solution: Check for errors after write");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Problem: Poor accuracy after calibration");
  ESP_LOGI(TAG, "  Solution: Verify reference equipment accuracy");
  ESP_LOGI(TAG, "  Solution: Check for noise/interference");
  ESP_LOGI(TAG, "  Solution: Recalibrate with better equipment");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nCalibration example complete");
  ESP_LOGI(TAG, "Remember: This was informational only");
  ESP_LOGI(TAG, "Actual calibration requires proper equipment and unsealed device");
}
