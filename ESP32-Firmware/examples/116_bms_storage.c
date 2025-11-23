/**
 * @file 116_bms_storage.c
 * @brief Storage mode and long-term battery care
 *
 * Demonstrates:
 * - Enter/exit storage mode
 * - Configure storage parameters
 * - Monitor storage conditions
 * - Optimal storage voltage/temperature
 * - Maintenance during storage
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char* TAG = "BMS_STORAGE";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Storage parameters */
#define STORAGE_SOC_TARGET     (50)    /* % - optimal storage SOC */
#define STORAGE_SOC_MIN        (40)    /* % - minimum acceptable */
#define STORAGE_SOC_MAX        (60)    /* % - maximum acceptable */
#define STORAGE_TEMP_IDEAL     (15)    /* C - ideal storage temp */
#define STORAGE_TEMP_MAX       (25)    /* C - maximum recommended */
#define STORAGE_VOLTAGE_TARGET (3700)  /* mV per cell */

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Storage Mode Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "storage_demo", NULL, NULL);
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

  /* ===== STORAGE MODE OVERVIEW ===== */
  ESP_LOGI(TAG, "===== Li-Ion Battery Storage Overview =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Why proper storage matters:");
  ESP_LOGI(TAG, "  - Li-Ion batteries degrade over time even unused");
  ESP_LOGI(TAG, "  - Calendar aging depends on SOC and temperature");
  ESP_LOGI(TAG, "  - Improper storage can cause permanent damage");
  ESP_LOGI(TAG, "  - Optimal conditions maximize shelf life");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Calendar aging rates (capacity loss per year):");
  ESP_LOGI(TAG, "  At 100%% SOC, 25C: ~20%% per year");
  ESP_LOGI(TAG, "  At 100%% SOC, 40C: ~35%% per year");
  ESP_LOGI(TAG, "  At 50%% SOC, 25C:  ~4%% per year");
  ESP_LOGI(TAG, "  At 50%% SOC, 0C:   ~2%% per year");

  /* ===== CHECK CURRENT STATE ===== */
  ESP_LOGI(TAG, "\n===== Current Battery State =====");

  bq7850_battery_state_t state;
  ret = star_bms_bq7850_read_battery_state(&g_bms, &state);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Pack voltage: %u mV", state.cells.pack_mv);
    ESP_LOGI(TAG, "SOC: %d%%", state.soc.relative_soc);
    ESP_LOGI(TAG, "Temperature: %.1f C", state.temps.avg_temp_c);

    /* Check if ready for storage */
    ESP_LOGI(TAG, "\nStorage readiness check:");

    bool soc_ok = (state.soc.relative_soc >= STORAGE_SOC_MIN &&
                   state.soc.relative_soc <= STORAGE_SOC_MAX);
    if (soc_ok) {
      ESP_LOGI(TAG, "  SOC: OK (%d%%)", state.soc.relative_soc);
    } else if (state.soc.relative_soc < STORAGE_SOC_MIN) {
      ESP_LOGW(TAG, "  SOC: TOO LOW (%d%%) - Charge to %d%%",
               state.soc.relative_soc, STORAGE_SOC_TARGET);
    } else {
      ESP_LOGW(TAG, "  SOC: TOO HIGH (%d%%) - Discharge to %d%%",
               state.soc.relative_soc, STORAGE_SOC_TARGET);
    }

    float temp_c = state.temps.avg_temp_c;
    bool temp_ok = (temp_c <= STORAGE_TEMP_MAX);
    if (temp_ok) {
      ESP_LOGI(TAG, "  Temperature: OK (%.1f C)", temp_c);
    } else {
      ESP_LOGW(TAG, "  Temperature: HIGH (%.1f C) - Cool before storage", temp_c);
    }

    bool current_ok = (abs(state.current.current_ma) < 100);
    if (current_ok) {
      ESP_LOGI(TAG, "  Current: OK (idle)");
    } else {
      ESP_LOGW(TAG, "  Current: ACTIVE (%d mA) - Disconnect load/charger", state.current.current_ma);
    }
  }

  /* ===== PREPARE FOR STORAGE ===== */
  ESP_LOGI(TAG, "\n===== Preparing for Storage =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Step 1: Adjust SOC to target");

    if (state.soc.relative_soc < STORAGE_SOC_MIN) {
      ESP_LOGI(TAG, "  Action: Charge battery to %d%% SOC", STORAGE_SOC_TARGET);
      ESP_LOGI(TAG, "  Current SOC: %d%%", state.soc.relative_soc);
      ESP_LOGI(TAG, "  Needed: +%d%%", STORAGE_SOC_TARGET - state.soc.relative_soc);
    } else if (state.soc.relative_soc > STORAGE_SOC_MAX) {
      ESP_LOGI(TAG, "  Action: Discharge battery to %d%% SOC", STORAGE_SOC_TARGET);
      ESP_LOGI(TAG, "  Current SOC: %d%%", state.soc.relative_soc);
      ESP_LOGI(TAG, "  Needed: -%d%%", state.soc.relative_soc - STORAGE_SOC_TARGET);
    } else {
      ESP_LOGI(TAG, "  SOC already in optimal range");
    }

    ESP_LOGI(TAG, "\nStep 2: Configure FET control for storage");
    ESP_LOGI(TAG, "  Recommended configuration:");
    ESP_LOGI(TAG, "    - Charge FET: ENABLED (allows maintenance charging)");
    ESP_LOGI(TAG, "    - Discharge FET: DISABLED (prevents discharge)");
    ESP_LOGI(TAG, "  This protects against self-discharge while allowing top-up");

    /* Configure for storage - charge enabled, discharge disabled */
    ret = star_bms_bq7850_control_fets(&g_bms, true, false);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "  FETs configured for storage mode");
    }

    ESP_LOGI(TAG, "\nStep 3: Verify cell balance");

    bq7850_cell_data_t cells;
    if (star_bms_bq7850_read_cells(&g_bms, &cells) == ESP_OK) {
      uint16_t min_v = cells.cell_mv[0], max_v = cells.cell_mv[0];
      for (int i = 1; i < cells.valid_cells; i++) {
        if (cells.cell_mv[i] < min_v) min_v = cells.cell_mv[i];
        if (cells.cell_mv[i] > max_v) max_v = cells.cell_mv[i];
      }
      ESP_LOGI(TAG, "  Cell voltage range: %d - %d mV", min_v, max_v);
      ESP_LOGI(TAG, "  Delta: %d mV", max_v - min_v);

      if (max_v - min_v > 50) {
        ESP_LOGW(TAG, "  Cells not well balanced - perform balancing before storage");
      } else {
        ESP_LOGI(TAG, "  Cell balance acceptable");
      }
    }
  }

  /* ===== STORAGE MODE CONFIGURATION ===== */
  ESP_LOGI(TAG, "\n===== Storage Mode Configuration =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Optimal storage conditions:");
  ESP_LOGI(TAG, "  Target SOC: %d%%", STORAGE_SOC_TARGET);
  ESP_LOGI(TAG, "  SOC range: %d-%d%%", STORAGE_SOC_MIN, STORAGE_SOC_MAX);
  ESP_LOGI(TAG, "  Target voltage: %d mV per cell", STORAGE_VOLTAGE_TARGET);
  ESP_LOGI(TAG, "  Ideal temperature: %d C", STORAGE_TEMP_IDEAL);
  ESP_LOGI(TAG, "  Maximum temperature: %d C", STORAGE_TEMP_MAX);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Storage location:");
  ESP_LOGI(TAG, "  - Cool, dry place");
  ESP_LOGI(TAG, "  - Away from direct sunlight");
  ESP_LOGI(TAG, "  - Not in freezing temperatures");
  ESP_LOGI(TAG, "  - Fire-safe container recommended");

  /* ===== MONITORING DURING STORAGE ===== */
  ESP_LOGI(TAG, "\n===== Monitoring During Storage =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Short-term storage (< 3 months):");
  ESP_LOGI(TAG, "  - Check monthly");
  ESP_LOGI(TAG, "  - Verify SOC hasn't dropped below 40%%");
  ESP_LOGI(TAG, "  - No active monitoring needed");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Long-term storage (> 3 months):");
  ESP_LOGI(TAG, "  - Check every 3 months");
  ESP_LOGI(TAG, "  - Top up to 50%% if below 40%%");
  ESP_LOGI(TAG, "  - Verify no cell degradation");
  ESP_LOGI(TAG, "  - Exercise battery (full cycle) every 6 months");

  /* Check self-discharge rate */
  ESP_LOGI(TAG, "\nSelf-discharge estimation:");
  ESP_LOGI(TAG, "  Li-Ion self-discharge: 2-3%% per month at 25C");
  ESP_LOGI(TAG, "  Lower at cooler temperatures");
  ESP_LOGI(TAG, "");

  if (ret == ESP_OK) {
    bq7850_soc_data_t soc;
    if (star_bms_bq7850_read_soc(&g_bms, &soc) == ESP_OK) {
      /* Estimate time until maintenance needed */
      int soc_headroom = state.soc.relative_soc - STORAGE_SOC_MIN;
      int months_until_maintenance = soc_headroom / 3;  /* Assuming 3% per month */

      ESP_LOGI(TAG, "  Current SOC: %d%%", state.soc.relative_soc);
      ESP_LOGI(TAG, "  Headroom to minimum: %d%%", soc_headroom);
      ESP_LOGI(TAG, "  Estimated months until maintenance: ~%d", months_until_maintenance);

      if (months_until_maintenance < 1) {
        ESP_LOGW(TAG, "  WARNING: Charge soon!");
      } else if (months_until_maintenance < 2) {
        ESP_LOGW(TAG, "  Check within 1 month");
      }
    }
  }

  /* ===== EXITING STORAGE MODE ===== */
  ESP_LOGI(TAG, "\n===== Exiting Storage Mode =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Before returning to service:");
  ESP_LOGI(TAG, "1. Check battery state:");
  ESP_LOGI(TAG, "   - Read all voltages");
  ESP_LOGI(TAG, "   - Verify SOC");
  ESP_LOGI(TAG, "   - Check for swelling");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Re-enable FETs:");
  ESP_LOGI(TAG, "   - Enable both charge and discharge");
  ESP_LOGI(TAG, "   - Verify FET operation");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Perform conditioning:");
  ESP_LOGI(TAG, "   - Full charge/discharge cycle");
  ESP_LOGI(TAG, "   - Recalibrate SOC");
  ESP_LOGI(TAG, "   - Balance cells");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Test under load:");
  ESP_LOGI(TAG, "   - Apply normal load");
  ESP_LOGI(TAG, "   - Monitor voltage sag");
  ESP_LOGI(TAG, "   - Verify capacity");

  /* Re-enable normal operation */
  ESP_LOGI(TAG, "\nRestoring normal FET configuration...");
  star_bms_bq7850_control_fets(&g_bms, true, true);

  /* ===== STORAGE BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Storage Best Practices =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "DO:");
  ESP_LOGI(TAG, "  - Store at 40-60%% SOC");
  ESP_LOGI(TAG, "  - Keep in cool environment (15-20C ideal)");
  ESP_LOGI(TAG, "  - Check battery every 3 months");
  ESP_LOGI(TAG, "  - Top up if SOC drops below 40%%");
  ESP_LOGI(TAG, "  - Balance cells before long storage");
  ESP_LOGI(TAG, "  - Store in fire-safe container");
  ESP_LOGI(TAG, "  - Label with storage date");
  ESP_LOGI(TAG, "  - Exercise battery if stored > 6 months");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "DON'T:");
  ESP_LOGI(TAG, "  - Store fully charged (100%% SOC)");
  ESP_LOGI(TAG, "  - Store fully discharged (0%% SOC)");
  ESP_LOGI(TAG, "  - Leave in hot environment (> 30C)");
  ESP_LOGI(TAG, "  - Ignore for > 6 months");
  ESP_LOGI(TAG, "  - Store below freezing");
  ESP_LOGI(TAG, "  - Leave connected to load");
  ESP_LOGI(TAG, "  - Store damaged batteries");

  /* ===== STORAGE DAMAGE INDICATORS ===== */
  ESP_LOGI(TAG, "\n===== Storage Damage Indicators =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Signs of storage damage:");
  ESP_LOGI(TAG, "  - Significant capacity loss (> 10%%)");
  ESP_LOGI(TAG, "  - Cannot hold charge");
  ESP_LOGI(TAG, "  - Cell imbalance increased");
  ESP_LOGI(TAG, "  - Swollen or deformed cells");
  ESP_LOGI(TAG, "  - Unusual heating");
  ESP_LOGI(TAG, "  - Voltage too low to recover");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "If damage suspected:");
  ESP_LOGI(TAG, "  1. Do not use battery");
  ESP_LOGI(TAG, "  2. Measure open-circuit voltage");
  ESP_LOGI(TAG, "  3. Check for swelling");
  ESP_LOGI(TAG, "  4. Test capacity if safe");
  ESP_LOGI(TAG, "  5. Dispose properly if damaged");

  /* ===== TEMPERATURE EFFECTS ===== */
  ESP_LOGI(TAG, "\n===== Temperature Effects on Storage =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Capacity loss after 1 year storage:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  Temperature | 40%% SOC | 100%% SOC");
  ESP_LOGI(TAG, "  ------------|---------|----------");
  ESP_LOGI(TAG, "  0C          |   2%%    |    6%%");
  ESP_LOGI(TAG, "  25C         |   4%%    |   20%%");
  ESP_LOGI(TAG, "  40C         |  15%%    |   35%%");
  ESP_LOGI(TAG, "  60C         |  25%%    |   40%%");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Key takeaway: Cool + Low SOC = Longer shelf life");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nStorage mode example complete");
}
