/**
 * @file 108_bms_cycles.c
 * @brief Cycle counting and battery lifetime tracking
 *
 * Demonstrates:
 * - Reading charge/discharge cycles
 * - Monitoring cycle accumulation
 * - Lifetime cycle tracking
 * - Cycle-based battery health
 * - Maintenance scheduling
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char* TAG = "BMS_CYCLES";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Cycle Counting Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "cycle_demo", NULL, NULL);
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

  /* ===== READ CYCLE COUNT ===== */
  ESP_LOGI(TAG, "===== Cycle Count Information =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Cycle count: %d", soc.cycle_count);

    /* Estimate battery age */
    ESP_LOGI(TAG, "\nBattery lifecycle stage:");

    if (soc.cycle_count < 50) {
      ESP_LOGI(TAG, "  Status: NEW (< 50 cycles)");
      ESP_LOGI(TAG, "  Condition: Break-in period");
    } else if (soc.cycle_count < 300) {
      ESP_LOGI(TAG, "  Status: YOUNG (50-300 cycles)");
      ESP_LOGI(TAG, "  Condition: Optimal performance");
    } else if (soc.cycle_count < 500) {
      ESP_LOGI(TAG, "  Status: MATURE (300-500 cycles)");
      ESP_LOGI(TAG, "  Condition: Good performance, monitor capacity");
    } else if (soc.cycle_count < 800) {
      ESP_LOGW(TAG, "  Status: AGING (500-800 cycles)");
      ESP_LOGW(TAG, "  Condition: Noticeable capacity reduction");
    } else {
      ESP_LOGW(TAG, "  Status: END-OF-LIFE (> 800 cycles)");
      ESP_LOGW(TAG, "  Condition: Consider replacement");
    }

    /* Expected remaining cycles (assume 1000 cycle life) */
    const int expected_total_cycles = 1000;
    int remaining_cycles = expected_total_cycles - soc.cycle_count;

    if (remaining_cycles > 0) {
      ESP_LOGI(TAG, "\nRemaining lifecycle:");
      ESP_LOGI(TAG, "  Expected total: %d cycles", expected_total_cycles);
      ESP_LOGI(TAG, "  Completed: %d cycles (%d%%)",
               soc.cycle_count,
               (soc.cycle_count * 100) / expected_total_cycles);
      ESP_LOGI(TAG, "  Remaining: %d cycles (%d%%)",
               remaining_cycles,
               (remaining_cycles * 100) / expected_total_cycles);
    } else {
      ESP_LOGW(TAG, "\nBattery has exceeded expected lifecycle");
    }
  } else {
    ESP_LOGE(TAG, "Failed to read SOC data: %s", esp_err_to_name(ret));
  }

  /* ===== CAPACITY VS CYCLES ===== */
  ESP_LOGI(TAG, "\n===== Capacity vs Cycle Count =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Capacity analysis:");
    ESP_LOGI(TAG, "  Current capacity: %d mAh", soc.full_capacity_mah);
    ESP_LOGI(TAG, "  Note: Design capacity requires config data");

    if (soc.cycle_count > 0) {
      ESP_LOGI(TAG, "  Cycles: %d", soc.cycle_count);
    }
  }

  /* ===== CYCLE-BASED MAINTENANCE ===== */
  ESP_LOGI(TAG, "\n===== Maintenance Schedule =====");

  if (ret == ESP_OK) {
    /* Define maintenance intervals */
    const int calibration_interval = 30;
    const int inspection_interval = 100;
    const int capacity_test_interval = 200;

    int cycles_since_calibration = soc.cycle_count % calibration_interval;
    int cycles_until_calibration = calibration_interval - cycles_since_calibration;

    int cycles_since_inspection = soc.cycle_count % inspection_interval;
    int cycles_until_inspection = inspection_interval - cycles_since_inspection;

    int cycles_since_capacity_test = soc.cycle_count % capacity_test_interval;
    int cycles_until_capacity_test = capacity_test_interval - cycles_since_capacity_test;

    ESP_LOGI(TAG, "Recommended maintenance:");

    ESP_LOGI(TAG, "  Calibration (every %d cycles):", calibration_interval);
    if (cycles_since_calibration == 0) {
      ESP_LOGW(TAG, "    DUE NOW!");
    } else {
      ESP_LOGI(TAG, "    Due in %d cycles", cycles_until_calibration);
    }

    ESP_LOGI(TAG, "  Inspection (every %d cycles):", inspection_interval);
    if (cycles_since_inspection == 0) {
      ESP_LOGW(TAG, "    DUE NOW!");
    } else {
      ESP_LOGI(TAG, "    Due in %d cycles", cycles_until_inspection);
    }

    ESP_LOGI(TAG, "  Capacity test (every %d cycles):", capacity_test_interval);
    if (cycles_since_capacity_test == 0) {
      ESP_LOGW(TAG, "    DUE NOW!");
    } else {
      ESP_LOGI(TAG, "    Due in %d cycles", cycles_until_capacity_test);
    }
  }

  /* ===== CYCLE COUNTING METHODOLOGY ===== */
  ESP_LOGI(TAG, "\n===== Cycle Counting Methodology =====");
  ESP_LOGI(TAG, "What counts as a cycle?");
  ESP_LOGI(TAG, "  - One full discharge (100%% to 0%%) = 1 cycle");
  ESP_LOGI(TAG, "  - Two 50%% discharges = 1 cycle");
  ESP_LOGI(TAG, "  - Four 25%% discharges = 1 cycle");
  ESP_LOGI(TAG, "\nThe BQ7850 uses coulomb counting to track partial cycles");
  ESP_LOGI(TAG, "and accumulates them into full cycle equivalents.");

  /* ===== CYCLE LIFE OPTIMIZATION ===== */
  ESP_LOGI(TAG, "\n===== Cycle Life Optimization Tips =====");
  ESP_LOGI(TAG, "1. Avoid deep discharges (< 20%% SOC)");
  ESP_LOGI(TAG, "2. Avoid storing at full charge");
  ESP_LOGI(TAG, "3. Keep temperature moderate (15-25C)");
  ESP_LOGI(TAG, "4. Limit high C-rate charging/discharging");
  ESP_LOGI(TAG, "5. Perform periodic full cycles for calibration");
  ESP_LOGI(TAG, "6. Monitor capacity degradation trends");
  ESP_LOGI(TAG, "7. Replace battery before critical failure");
  ESP_LOGI(TAG, "8. Store at 40-60%% SOC for long periods");

  /* ===== CYCLE COUNT HISTORY (SIMULATED) ===== */
  ESP_LOGI(TAG, "\n===== Cycle Count Trending =====");
  ESP_LOGI(TAG, "In a real application, log cycle count periodically:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  Date         | Cycles | Capacity | Retention");
  ESP_LOGI(TAG, "  -------------|--------|----------|----------");
  ESP_LOGI(TAG, "  2024-01-01   |    0   | 5000 mAh | 100.0%%");
  ESP_LOGI(TAG, "  2024-03-01   |   50   | 4950 mAh |  99.0%%");
  ESP_LOGI(TAG, "  2024-06-01   |  150   | 4850 mAh |  97.0%%");
  ESP_LOGI(TAG, "  2024-09-01   |  300   | 4700 mAh |  94.0%%");
  ESP_LOGI(TAG, "  2024-12-01   |  500   | 4500 mAh |  90.0%%");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "This helps identify accelerated degradation patterns.");

  /* ===== WARRANTY AND SPECIFICATIONS ===== */
  ESP_LOGI(TAG, "\n===== Typical Li-Ion Specifications =====");
  ESP_LOGI(TAG, "Cycle life (to 80%% capacity):");
  ESP_LOGI(TAG, "  Standard cells: 500-1000 cycles");
  ESP_LOGI(TAG, "  High-quality cells: 1000-2000 cycles");
  ESP_LOGI(TAG, "  LiFePO4 cells: 2000-5000 cycles");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Factors affecting cycle life:");
  ESP_LOGI(TAG, "  - Depth of discharge (deeper = fewer cycles)");
  ESP_LOGI(TAG, "  - Charge/discharge rate (higher = fewer cycles)");
  ESP_LOGI(TAG, "  - Operating temperature (extreme = fewer cycles)");
  ESP_LOGI(TAG, "  - Cell quality and manufacturing");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nCycle counting example complete");
}
