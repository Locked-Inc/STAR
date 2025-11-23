/**
 * @file 103_bms_balancing.c
 * @brief Cell balancing strategies and control
 *
 * Demonstrates:
 * - Detecting cell imbalance
 * - Enable/disable cell balancing
 * - Monitoring balancing status
 * - Configuring balancing thresholds
 * - Different balancing strategies
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_BALANCING";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Balancing thresholds */
#define BALANCE_THRESHOLD_MV  (10)   /* Balance if cell is 10mV above min */
#define BALANCE_MIN_VOLTAGE   (3000) /* Don't balance below 3.0V */
#define BALANCE_DURATION_MS   (2000) /* Balance for 2 seconds in demo */

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/**
 * @brief Calculate which cells need balancing
 */
static uint16_t calculate_balance_mask(const bq7850_cell_data_t* cells, uint16_t threshold_mv)
{
  uint16_t mask = 0;

  /* Find minimum voltage (reference for balancing) */
  uint16_t min_voltage = 0xFFFF;
  for (int i = 0; i < cells->valid_cells; i++) {
    if (cells->cell_mv[i] < min_voltage) {
      min_voltage = cells->cell_mv[i];
    }
  }

  /* Don't balance if pack voltage is too low */
  if (min_voltage < BALANCE_MIN_VOLTAGE) {
    ESP_LOGW(TAG, "Pack voltage too low for balancing (%d mV < %d mV)",
             min_voltage, BALANCE_MIN_VOLTAGE);
    return 0;
  }

  /* Mark cells above threshold for balancing */
  for (int i = 0; i < cells->valid_cells; i++) {
    if (cells->cell_mv[i] > min_voltage + threshold_mv) {
      mask |= (1 << i);
      ESP_LOGI(TAG, "  Cell %d needs balancing: %d mV (min: %d mV, delta: %d mV)",
               i + 1, cells->cell_mv[i], min_voltage,
               cells->cell_mv[i] - min_voltage);
    }
  }

  return mask;
}

/**
 * @brief Display balancing status
 */
static void display_balancing_status(uint16_t status_mask, int cell_count)
{
  int active_count = 0;

  for (int i = 0; i < cell_count; i++) {
    if (status_mask & (1 << i)) {
      ESP_LOGI(TAG, "    Cell %2d: BALANCING", i + 1);
      active_count++;
    }
  }

  if (active_count == 0) {
    ESP_LOGI(TAG, "    No cells currently balancing");
  } else {
    ESP_LOGI(TAG, "    Total cells balancing: %d", active_count);
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Cell Balancing Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "balancing_demo", NULL, NULL);
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

  ESP_LOGI(TAG, "BMS initialized successfully");

  /* ===== CHECK INITIAL CELL VOLTAGES ===== */
  ESP_LOGI(TAG, "\n===== Initial Cell Voltage Analysis =====");

  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&g_bms, &cells);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read cells: %s", esp_err_to_name(ret));
    goto cleanup;
  }

  ESP_LOGI(TAG, "Cell voltages:");
  uint16_t min_v = 0xFFFF, max_v = 0;
  int min_idx = -1, max_idx = -1;
  for (int i = 0; i < cells.valid_cells; i++) {
    if (cells.cell_mv[i] < min_v) {
      min_v = cells.cell_mv[i];
      min_idx = i;
    }
    if (cells.cell_mv[i] > max_v) {
      max_v = cells.cell_mv[i];
      max_idx = i;
    }
    char marker = ' ';
    if (i == min_idx) marker = 'L';  /* Lowest */
    if (i == max_idx) marker = 'H';  /* Highest */
    ESP_LOGI(TAG, "  Cell %2d: %4d mV [%c]", i + 1, cells.cell_mv[i], marker);
  }

  ESP_LOGI(TAG, "\nCell statistics:");
  ESP_LOGI(TAG, "  Minimum: %d mV (cell %d)", min_v, min_idx + 1);
  ESP_LOGI(TAG, "  Maximum: %d mV (cell %d)", max_v, max_idx + 1);
  ESP_LOGI(TAG, "  Delta: %d mV", max_v - min_v);

  if (max_v - min_v < 5) {
    ESP_LOGI(TAG, "  Status: Well balanced (delta < 5 mV)");
  } else if (max_v - min_v < 50) {
    ESP_LOGI(TAG, "  Status: Acceptable balance (delta < 50 mV)");
  } else {
    ESP_LOGW(TAG, "  Status: Poor balance (delta >= 50 mV) - balancing recommended");
  }

  /* ===== STRATEGY 1: FIXED THRESHOLD BALANCING ===== */
  ESP_LOGI(TAG, "\n===== Strategy 1: Fixed Threshold Balancing =====");
  ESP_LOGI(TAG, "Balance any cell %d mV above minimum", BALANCE_THRESHOLD_MV);

  uint16_t balance_mask = calculate_balance_mask(&cells, BALANCE_THRESHOLD_MV);

  if (balance_mask == 0) {
    ESP_LOGI(TAG, "No cells need balancing with current threshold");
  } else {
    ESP_LOGI(TAG, "Balance mask: 0x%04X", balance_mask);

    /* Enable balancing */
    ret = star_bms_bq7850_enable_cell_balancing(&g_bms, balance_mask);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "Cell balancing enabled");

      /* Monitor balancing for a short period */
      for (int i = 0; i < 5; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));

        uint16_t status_mask;
        ret = star_bms_bq7850_get_balancing_status(&g_bms, &status_mask);
        if (ret == ESP_OK) {
          ESP_LOGI(TAG, "  Balancing status [%d]: 0x%04X", i + 1, status_mask);
          display_balancing_status(status_mask, cells.valid_cells);
        }
      }

      /* Disable balancing */
      ret = star_bms_bq7850_disable_cell_balancing(&g_bms);
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Cell balancing disabled");
      }
    } else {
      ESP_LOGW(TAG, "Failed to enable balancing: %s", esp_err_to_name(ret));
    }
  }

  /* ===== STRATEGY 2: AGGRESSIVE BALANCING ===== */
  ESP_LOGI(TAG, "\n===== Strategy 2: Aggressive Balancing =====");
  ESP_LOGI(TAG, "Balance any cell above minimum (0 mV threshold)");

  balance_mask = calculate_balance_mask(&cells, 0);  /* Any difference */

  if (balance_mask == 0) {
    ESP_LOGI(TAG, "All cells perfectly balanced");
  } else {
    ESP_LOGI(TAG, "Aggressive balance mask: 0x%04X", balance_mask);
    ESP_LOGI(TAG, "(Not applied - example only)");
  }

  /* ===== STRATEGY 3: TOP-BALANCING ===== */
  ESP_LOGI(TAG, "\n===== Strategy 3: Top-Balancing =====");
  ESP_LOGI(TAG, "Balance cells only when pack is near full charge");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current SOC: %d%%", soc.relative_soc);

    if (soc.relative_soc >= 95) {
      ESP_LOGI(TAG, "Pack is near full - top-balancing recommended");
      balance_mask = calculate_balance_mask(&cells, BALANCE_THRESHOLD_MV);
      ESP_LOGI(TAG, "Top-balance mask: 0x%04X", balance_mask);
    } else {
      ESP_LOGI(TAG, "Pack SOC too low for top-balancing (wait until > 95%%)");
    }
  }

  /* ===== STRATEGY 4: SELECTIVE BALANCING ===== */
  ESP_LOGI(TAG, "\n===== Strategy 4: Selective Balancing =====");
  ESP_LOGI(TAG, "Balance only outlier cells (>25mV above average)");

  /* Calculate average voltage */
  uint32_t sum = 0;
  for (int i = 0; i < cells.valid_cells; i++) {
    sum += cells.cell_mv[i];
  }
  uint16_t avg_voltage = sum / cells.valid_cells;

  ESP_LOGI(TAG, "Average cell voltage: %d mV", avg_voltage);

  balance_mask = 0;
  for (int i = 0; i < cells.valid_cells; i++) {
    if (cells.cell_mv[i] > avg_voltage + 25) {
      balance_mask |= (1 << i);
      ESP_LOGI(TAG, "  Cell %d is outlier: %d mV (avg: %d mV, delta: %d mV)",
               i + 1, cells.cell_mv[i], avg_voltage,
               cells.cell_mv[i] - avg_voltage);
    }
  }

  if (balance_mask == 0) {
    ESP_LOGI(TAG, "No outlier cells detected");
  } else {
    ESP_LOGI(TAG, "Selective balance mask: 0x%04X", balance_mask);
  }

  /* ===== BALANCING EFFECTIVENESS MONITORING ===== */
  ESP_LOGI(TAG, "\n===== Balancing Effectiveness Test =====");

  /* Read initial state */
  bq7850_cell_data_t cells_before;
  star_bms_bq7850_read_cells(&g_bms, &cells_before);

  uint16_t min_before = 0xFFFF, max_before = 0;
  for (int i = 0; i < cells_before.valid_cells; i++) {
    if (cells_before.cell_mv[i] < min_before) min_before = cells_before.cell_mv[i];
    if (cells_before.cell_mv[i] > max_before) max_before = cells_before.cell_mv[i];
  }
  uint16_t delta_before = max_before - min_before;

  ESP_LOGI(TAG, "Before balancing: delta = %d mV", delta_before);

  /* Enable balancing */
  balance_mask = calculate_balance_mask(&cells_before, BALANCE_THRESHOLD_MV);
  if (balance_mask != 0) {
    star_bms_bq7850_enable_cell_balancing(&g_bms, balance_mask);
    ESP_LOGI(TAG, "Balancing for %d ms...", BALANCE_DURATION_MS);
    vTaskDelay(pdMS_TO_TICKS(BALANCE_DURATION_MS));
    star_bms_bq7850_disable_cell_balancing(&g_bms);

    /* Read state after balancing */
    bq7850_cell_data_t cells_after;
    star_bms_bq7850_read_cells(&g_bms, &cells_after);

    uint16_t min_after = 0xFFFF, max_after = 0;
    for (int i = 0; i < cells_after.valid_cells; i++) {
      if (cells_after.cell_mv[i] < min_after) min_after = cells_after.cell_mv[i];
      if (cells_after.cell_mv[i] > max_after) max_after = cells_after.cell_mv[i];
    }
    uint16_t delta_after = max_after - min_after;

    ESP_LOGI(TAG, "After balancing: delta = %d mV", delta_after);

    if (delta_after < delta_before) {
      ESP_LOGI(TAG, "Improvement: %d mV reduction", delta_before - delta_after);
    } else {
      ESP_LOGI(TAG, "Note: Significant balancing requires extended time");
    }
  }

  /* ===== BALANCING BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Balancing Best Practices =====");
  ESP_LOGI(TAG, "1. Balance only during charging (safer thermal conditions)");
  ESP_LOGI(TAG, "2. Top-balancing (95-100%% SOC) is most effective");
  ESP_LOGI(TAG, "3. Don't balance below minimum safe voltage");
  ESP_LOGI(TAG, "4. Monitor temperature during balancing");
  ESP_LOGI(TAG, "5. Balancing current is typically 50-200mA per cell");
  ESP_LOGI(TAG, "6. Allow sufficient time (hours for significant correction)");
  ESP_LOGI(TAG, "7. Re-check balance after each cycle");
  ESP_LOGI(TAG, "8. Persistent imbalance may indicate faulty cell");

cleanup:
  /* Ensure balancing is disabled before exit */
  star_bms_bq7850_disable_cell_balancing(&g_bms);

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nCell balancing example complete");
}
