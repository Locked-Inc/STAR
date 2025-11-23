/**
 * @file 115_bms_charging.c
 * @brief Charge control and monitoring
 *
 * Demonstrates:
 * - Charging state management
 * - Charge termination detection
 * - Fast/trickle charge modes
 * - Charging safety
 * - Charging algorithms (CC/CV)
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_CHARGING";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Charging parameters */
#define CHARGE_VOLTAGE_TARGET    (4200)  /* mV per cell */
#define CHARGE_CURRENT_FAST      (3000)  /* mA */
#define CHARGE_CURRENT_TRICKLE   (300)   /* mA */
#define CHARGE_TEMP_MIN          (0)     /* C */
#define CHARGE_TEMP_MAX          (45)    /* C */
#define CHARGE_TERM_CURRENT      (150)   /* mA */
#define CHARGE_FULL_SOC          (95)    /* % */

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/* Charging states */
typedef enum {
  CHARGE_STATE_IDLE,
  CHARGE_STATE_PRECHARGE,
  CHARGE_STATE_FAST_CC,
  CHARGE_STATE_FAST_CV,
  CHARGE_STATE_TRICKLE,
  CHARGE_STATE_COMPLETE,
  CHARGE_STATE_FAULT
} charge_state_t;

static const char* charge_state_names[] = {
  "IDLE",
  "PRE-CHARGE",
  "FAST CC",
  "FAST CV",
  "TRICKLE",
  "COMPLETE",
  "FAULT"
};

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Charging Control Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "charge_demo", NULL, NULL);
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

  /* ===== CHARGING OVERVIEW ===== */
  ESP_LOGI(TAG, "===== Li-Ion Charging Overview =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Standard CC/CV charging profile:");
  ESP_LOGI(TAG, "1. Pre-charge (optional):");
  ESP_LOGI(TAG, "   - If battery deeply discharged (< 2.5V)");
  ESP_LOGI(TAG, "   - Low current (0.1C) to 3.0V");
  ESP_LOGI(TAG, "   - Safety measure");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Fast Charge - Constant Current (CC):");
  ESP_LOGI(TAG, "   - Constant current until target voltage");
  ESP_LOGI(TAG, "   - Typically 0.5C to 1C rate");
  ESP_LOGI(TAG, "   - Reaches ~70-80%% SOC");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Fast Charge - Constant Voltage (CV):");
  ESP_LOGI(TAG, "   - Hold target voltage");
  ESP_LOGI(TAG, "   - Current tapers naturally");
  ESP_LOGI(TAG, "   - Reaches ~90-95%% SOC");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Termination:");
  ESP_LOGI(TAG, "   - When current drops below threshold");
  ESP_LOGI(TAG, "   - Or SOC reaches target");
  ESP_LOGI(TAG, "   - Or timeout occurs");

  /* ===== PRE-CHARGE CHECK ===== */
  ESP_LOGI(TAG, "\n===== Pre-Charge Check =====");

  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&g_bms, &cells);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Checking cell voltages:");

    bool needs_precharge = false;
    for (int i = 0; i < cells.valid_cells; i++) {
      ESP_LOGI(TAG, "  Cell %d: %d mV", i + 1, cells.cell_mv[i]);
      if (cells.cell_mv[i] < 2500) {
        ESP_LOGW(TAG, "    Cell %d below 2.5V - pre-charge required", i + 1);
        needs_precharge = true;
      }
    }

    if (needs_precharge) {
      ESP_LOGW(TAG, "\nPre-charge phase required:");
      ESP_LOGW(TAG, "  - Limit current to 0.1C");
      ESP_LOGW(TAG, "  - Charge to 3.0V minimum");
      ESP_LOGW(TAG, "  - Monitor carefully");
    } else {
      ESP_LOGI(TAG, "\nAll cells above 2.5V - proceed with normal charging");
    }
  }

  /* ===== TEMPERATURE CHECK ===== */
  ESP_LOGI(TAG, "\n===== Temperature Check =====");

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret == ESP_OK) {
    float max_temp_c = temps.temp_c[0];
    float min_temp_c = temps.temp_c[0];
    for (int i = 1; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] > max_temp_c) max_temp_c = temps.temp_c[i];
      if (temps.temp_c[i] < min_temp_c) min_temp_c = temps.temp_c[i];
    }

    ESP_LOGI(TAG, "Temperature range: %.1f - %.1f C", min_temp_c, max_temp_c);

    bool temp_ok = (min_temp_c >= CHARGE_TEMP_MIN && max_temp_c <= CHARGE_TEMP_MAX);

    if (!temp_ok) {
      if (min_temp_c < CHARGE_TEMP_MIN) {
        ESP_LOGE(TAG, "Temperature too low for charging!");
        ESP_LOGE(TAG, "  NEVER charge Li-Ion below 0C");
        ESP_LOGE(TAG, "  Causes lithium plating and safety risk");
      }
      if (max_temp_c > CHARGE_TEMP_MAX) {
        ESP_LOGW(TAG, "Temperature too high for fast charging");
        ESP_LOGW(TAG, "  Use reduced current or wait for cooling");
      }
    } else {
      ESP_LOGI(TAG, "Temperature OK for charging");
    }
  }

  /* ===== CHARGING STATE MACHINE ===== */
  ESP_LOGI(TAG, "\n===== Charging State Machine Simulation =====");

  charge_state_t state = CHARGE_STATE_IDLE;

  /* Simulate charging process */
  for (int i = 0; i < 10; i++) {
    bq7850_battery_state_t batt_state;
    ret = star_bms_bq7850_read_battery_state(&g_bms, &batt_state);

    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "\n[%d] State: %s", i, charge_state_names[state]);
      ESP_LOGI(TAG, "  Voltage: %u mV", batt_state.cells.pack_mv);
      ESP_LOGI(TAG, "  Current: %d mA", batt_state.current.current_ma);
      ESP_LOGI(TAG, "  SOC: %d%%", batt_state.soc.relative_soc);
      ESP_LOGI(TAG, "  Temp: %.1f C", batt_state.temps.avg_temp_c);

      /* State transitions (simplified simulation) */
      switch (state) {
        case CHARGE_STATE_IDLE:
          if (batt_state.current.current_ma > 100) {
            /* Charging started */
            if (batt_state.cells.pack_mv < 2500 * cells.valid_cells) {
              state = CHARGE_STATE_PRECHARGE;
              ESP_LOGI(TAG, "  -> Entering PRE-CHARGE");
            } else {
              state = CHARGE_STATE_FAST_CC;
              ESP_LOGI(TAG, "  -> Entering FAST CHARGE (CC)");
            }
          }
          break;

        case CHARGE_STATE_PRECHARGE:
          if (batt_state.cells.pack_mv >= 3000 * cells.valid_cells) {
            state = CHARGE_STATE_FAST_CC;
            ESP_LOGI(TAG, "  -> Pre-charge complete, entering FAST CHARGE (CC)");
          }
          break;

        case CHARGE_STATE_FAST_CC:
          /* Transition to CV when voltage reaches target */
          {
            uint16_t max_cell = 0;
            for (int j = 0; j < cells.valid_cells; j++) {
              if (cells.cell_mv[j] > max_cell) max_cell = cells.cell_mv[j];
            }
            if (max_cell >= CHARGE_VOLTAGE_TARGET) {
              state = CHARGE_STATE_FAST_CV;
              ESP_LOGI(TAG, "  -> Voltage reached, entering FAST CHARGE (CV)");
            }
          }
          break;

        case CHARGE_STATE_FAST_CV:
          /* Transition to complete when current tapers */
          if (batt_state.current.current_ma < CHARGE_TERM_CURRENT) {
            state = CHARGE_STATE_COMPLETE;
            ESP_LOGI(TAG, "  -> Current tapered, CHARGING COMPLETE");
          } else if (batt_state.soc.relative_soc >= CHARGE_FULL_SOC) {
            state = CHARGE_STATE_COMPLETE;
            ESP_LOGI(TAG, "  -> SOC target reached, CHARGING COMPLETE");
          }
          break;

        case CHARGE_STATE_COMPLETE:
          /* Check if we need trickle charge */
          if (batt_state.soc.relative_soc < CHARGE_FULL_SOC - 5) {
            state = CHARGE_STATE_TRICKLE;
            ESP_LOGI(TAG, "  -> SOC dropped, entering TRICKLE CHARGE");
          }
          break;

        case CHARGE_STATE_TRICKLE:
          if (batt_state.soc.relative_soc >= CHARGE_FULL_SOC) {
            state = CHARGE_STATE_COMPLETE;
            ESP_LOGI(TAG, "  -> CHARGING COMPLETE");
          }
          break;

        default:
          break;
      }

      /* Check for fault conditions */
      bq7850_status_t status;
      star_bms_bq7850_read_status(&g_bms, &status);
      if (star_bms_bq7850_is_fault_active(&status)) {
        state = CHARGE_STATE_FAULT;
        ESP_LOGE(TAG, "  -> FAULT DETECTED - Charging aborted!");
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* ===== CHARGING TERMINATION CRITERIA ===== */
  ESP_LOGI(TAG, "\n===== Charging Termination Criteria =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Primary termination:");
  ESP_LOGI(TAG, "  - Current drops below %d mA (C/10 typical)", CHARGE_TERM_CURRENT);
  ESP_LOGI(TAG, "  - All cells reach %.2fV", CHARGE_VOLTAGE_TARGET / 1000.0f);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Secondary termination:");
  ESP_LOGI(TAG, "  - SOC reaches %d%%", CHARGE_FULL_SOC);
  ESP_LOGI(TAG, "  - Timer expires (safety timeout)");
  ESP_LOGI(TAG, "  - Temperature exceeds limits");
  ESP_LOGI(TAG, "  - Voltage rise slows (dV/dt detection)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Fault termination:");
  ESP_LOGI(TAG, "  - Any safety status flag");
  ESP_LOGI(TAG, "  - Cell imbalance > threshold");
  ESP_LOGI(TAG, "  - Communication loss");

  /* ===== CHARGING PROFILES ===== */
  ESP_LOGI(TAG, "\n===== Different Charging Profiles =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Fast Charge (1C):");
  ESP_LOGI(TAG, "   - Fastest charge time (~1 hour)");
  ESP_LOGI(TAG, "   - Higher stress on battery");
  ESP_LOGI(TAG, "   - Reduced cycle life");
  ESP_LOGI(TAG, "   - Use when time-critical");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Standard Charge (0.5C):");
  ESP_LOGI(TAG, "   - Balanced approach (~2 hours)");
  ESP_LOGI(TAG, "   - Good for daily use");
  ESP_LOGI(TAG, "   - Recommended default");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Slow Charge (0.2C):");
  ESP_LOGI(TAG, "   - Gentlest on battery (~5 hours)");
  ESP_LOGI(TAG, "   - Maximum cycle life");
  ESP_LOGI(TAG, "   - Best for overnight charging");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Trickle Charge (0.05C):");
  ESP_LOGI(TAG, "   - Maintain full charge");
  ESP_LOGI(TAG, "   - Offset self-discharge");
  ESP_LOGI(TAG, "   - Not recommended for Li-Ion long-term");

  /* ===== CHARGING BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Charging Best Practices =====");
  ESP_LOGI(TAG, "1. Never charge below 0C (Li-Ion)");
  ESP_LOGI(TAG, "2. Stop charging above 45C");
  ESP_LOGI(TAG, "3. Use balancing during top-off phase");
  ESP_LOGI(TAG, "4. Don't keep at 100%% SOC continuously");
  ESP_LOGI(TAG, "5. Charge to 80%% for daily use (longer life)");
  ESP_LOGI(TAG, "6. Charge to 100%% monthly (calibration)");
  ESP_LOGI(TAG, "7. Monitor temperature during fast charging");
  ESP_LOGI(TAG, "8. Implement timeout protection");
  ESP_LOGI(TAG, "9. Log charging events for analysis");
  ESP_LOGI(TAG, "10. Use lower current if temperature rises");

  /* ===== CHARGER COMPATIBILITY ===== */
  ESP_LOGI(TAG, "\n===== Charger Compatibility =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "The BQ7850 works with external chargers:");
  ESP_LOGI(TAG, "  - Controls FETs to enable/disable charging");
  ESP_LOGI(TAG, "  - Monitors voltage/current/temperature");
  ESP_LOGI(TAG, "  - Can communicate charge parameters (SMBus)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Smart charger communication:");
  ESP_LOGI(TAG, "  - Charging voltage request");
  ESP_LOGI(TAG, "  - Charging current request");
  ESP_LOGI(TAG, "  - Charge complete indication");
  ESP_LOGI(TAG, "  - Fault reporting");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nCharging control example complete");
}
