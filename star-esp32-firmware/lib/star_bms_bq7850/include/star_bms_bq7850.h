/* lib/star_bms_bq7850/include/star_bms_bq7850.h */

#ifndef STAR_BMS_BQ7850_H
#define STAR_BMS_BQ7850_H

/* System headers */
#include <stdbool.h>
#include <stdint.h>

/* FreeRTOS headers */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ESP-IDF headers */
#include "esp_err.h"

/* Project headers */
#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bms_bq7850.h
 * @brief TI BQ7850 Battery Management System driver
 *
 * This driver provides a high-level interface to the BQ7850 multi-cell
 * lithium-ion battery monitor and protector. It supports:
 * - Up to 16 series-connected cells
 * - Cell voltage monitoring
 * - Temperature monitoring
 * - Cell balancing
 * - Protection (OV, UV, OC, OT, UT)
 * - Coulomb counting
 * - SMBus communication
 *
 * Example Usage:
 * @code
 * // === Basic Setup ===
 *
 * #include "star_bms_bq7850.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // 1. Setup bus manager (see star_bus_manager.h for full setup)
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Create I2C/SMBus for BQ7850
 * star_bus_config_t* bms_bus = star_bus_config_create_i2c(
 *     "bms_smbus",
 *     I2C_NUM_0,
 *     k_bq7850_default_addr,    // 0x08
 *     GPIO_NUM_21,              // SDA
 *     GPIO_NUM_22,              // SCL
 *     100000                    // 100kHz (SMBus standard)
 * );
 * star_bus_manager_add_bus(&bus_manager, bms_bus);
 *
 * // 3. Configure and initialize BMS
 * bq7850_handle_t bms;
 * bq7850_config_t config = {
 *     .num_cells = 4,                   // 4S battery pack
 *     .num_temp = 2,                    // 2 temperature sensors
 *     .smbus_addr = k_bq7850_default_addr,
 *     .design_capacity = 5000,          // 5000 mAh
 *     .design_voltage = 14800           // 14.8V (4S * 3.7V)
 * };
 *
 * esp_err_t ret = star_bms_bq7850_init(
 *     &bms,
 *     &bus_manager,
 *     "bms_smbus",
 *     NULL,                   // Use default error handler
 *     &config
 * );
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init BMS: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Read Device Information ===
 *
 * bq7850_device_info_t info;
 * ret = star_bms_bq7850_get_device_info(&bms, &info);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Device: %s by %s", info.device_name, info.manufacturer);
 *     ESP_LOGI(TAG, "FW: %d, HW: %d, Serial: %lu",
 *              info.fw_version, info.hw_version, info.serial_number);
 *     ESP_LOGI(TAG, "Chemistry: %s", info.chemistry);
 * }
 *
 *
 * // === Read Complete Battery State ===
 *
 * bq7850_battery_state_t state;
 * ret = star_bms_bq7850_read_battery_state(&bms, &state);
 * if (ret == ESP_OK) {
 *     // Cell voltages
 *     for (int i = 0; i < state.cells.valid_cells; i++) {
 *         ESP_LOGI(TAG, "Cell %d: %d mV", i + 1, state.cells.cell_mv[i]);
 *     }
 *     ESP_LOGI(TAG, "Pack voltage: %d mV", state.cells.pack_mv);
 *
 *     // Temperature
 *     ESP_LOGI(TAG, "Temperature: %.1f°C",
 *              state.temps.avg_temp_c / 10.0f);
 *
 *     // Current and power
 *     ESP_LOGI(TAG, "Current: %d mA, Power: %ld mW",
 *              state.current.current_ma, state.current.power_mw);
 *
 *     // State of charge
 *     ESP_LOGI(TAG, "SOC: %d%% (%d/%d mAh), Cycles: %d",
 *              state.soc.relative_soc,
 *              state.soc.remaining_capacity_mah,
 *              state.soc.full_capacity_mah,
 *              state.soc.cycle_count);
 *
 *     // Status
 *     ESP_LOGI(TAG, "Status: %s%s%s",
 *              state.status.charging ? "Charging " : "",
 *              state.status.discharging ? "Discharging " : "",
 *              state.status.fully_charged ? "Full" : "");
 * }
 *
 *
 * // === Read Cell Voltages ===
 *
 * bq7850_cell_data_t cells;
 * star_bms_bq7850_read_cells(&bms, &cells);
 *
 * // Find min/max cell
 * uint16_t min_mv = 65535, max_mv = 0;
 * for (int i = 0; i < cells.valid_cells; i++) {
 *     if (cells.cell_mv[i] < min_mv) min_mv = cells.cell_mv[i];
 *     if (cells.cell_mv[i] > max_mv) max_mv = cells.cell_mv[i];
 * }
 * ESP_LOGI(TAG, "Cell delta: %d mV (min=%d, max=%d)",
 *          max_mv - min_mv, min_mv, max_mv);
 *
 * // Read single cell
 * uint16_t cell1_mv;
 * star_bms_bq7850_read_cell_voltage(&bms, 0, &cell1_mv);
 *
 *
 * // === Read Temperature ===
 *
 * bq7850_temp_data_t temps;
 * star_bms_bq7850_read_temperatures(&bms, &temps);
 * for (int i = 0; i < temps.valid_sensors; i++) {
 *     float temp_c = temps.temp_c[i] / 10.0f;
 *     ESP_LOGI(TAG, "Temp sensor %d: %.1f°C", i + 1, temp_c);
 * }
 *
 *
 * // === Read Current ===
 *
 * bq7850_current_data_t current;
 * star_bms_bq7850_read_current(&bms, &current);
 * ESP_LOGI(TAG, "Instant: %d mA, Average: %d mA",
 *          current.current_ma, current.avg_current_ma);
 * ESP_LOGI(TAG, "Power: %ld mW", current.power_mw);
 *
 *
 * // === Read State of Charge ===
 *
 * bq7850_soc_data_t soc;
 * star_bms_bq7850_read_soc(&bms, &soc);
 * ESP_LOGI(TAG, "Relative SOC: %d%%", soc.relative_soc);
 * ESP_LOGI(TAG, "Absolute SOC: %d%%", soc.absolute_soc);
 * ESP_LOGI(TAG, "Capacity: %d/%d mAh", soc.remaining_capacity_mah, soc.full_capacity_mah);
 *
 *
 * // === Check Status and Faults ===
 *
 * bq7850_status_t status;
 * star_bms_bq7850_read_status(&bms, &status);
 *
 * if (star_bms_bq7850_is_fault_active(&status)) {
 *     ESP_LOGW(TAG, "Battery fault active!");
 *
 *     char status_str[256];
 *     star_bms_bq7850_status_to_string(&status, status_str, sizeof(status_str));
 *     ESP_LOGW(TAG, "Status: %s", status_str);
 * }
 *
 *
 * // === Cell Balancing ===
 *
 * // Enable balancing on cells 1, 2, 3 (bitmask)
 * star_bms_bq7850_enable_cell_balancing(&bms, 0x0007);
 *
 * // Check which cells are balancing
 * uint16_t balancing_mask;
 * star_bms_bq7850_get_balancing_status(&bms, &balancing_mask);
 * ESP_LOGI(TAG, "Balancing cells: 0x%04X", balancing_mask);
 *
 * // Disable balancing
 * star_bms_bq7850_disable_cell_balancing(&bms);
 *
 *
 * // === Protection Thresholds ===
 *
 * // Read current protection settings
 * bq7850_protection_t protection;
 * star_bms_bq7850_read_protection(&bms, &protection);
 * ESP_LOGI(TAG, "OV: %d mV, UV: %d mV",
 *          protection.overvoltage_mv, protection.undervoltage_mv);
 *
 * // Modify protection (requires unsealed device)
 * // protection.overvoltage_mv = 4200;
 * // protection.undervoltage_mv = 2800;
 * // star_bms_bq7850_write_protection(&bms, &protection);
 *
 *
 * // === FET Control ===
 *
 * // Enable charging and discharging
 * star_bms_bq7850_control_fets(&bms, true, true);
 *
 * // Disable discharging (safe storage mode)
 * star_bms_bq7850_control_fets(&bms, true, false);
 *
 * // Disable all (emergency stop)
 * star_bms_bq7850_control_fets(&bms, false, false);
 *
 *
 * // === Monitoring Task ===
 *
 * void bms_monitor_task(void* arg) {
 *     bq7850_handle_t* bms = (bq7850_handle_t*)arg;
 *     bq7850_battery_state_t state;
 *
 *     while (1) {
 *         if (star_bms_bq7850_read_battery_state(bms, &state) == ESP_OK) {
 *             // Check for low battery
 *             if (state.soc.relative_soc < 10) {
 *                 ESP_LOGW(TAG, "Low battery: %d%%", state.soc.relative_soc);
 *             }
 *
 *             // Check for faults
 *             if (state.status.fault_active) {
 *                 ESP_LOGE(TAG, "Battery fault!");
 *             }
 *         }
 *         vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second
 *     }
 * }
 *
 *
 * // === Cleanup ===
 *
 * star_bms_bq7850_deinit(&bms);
 * star_bus_manager_remove_bus(&bus_manager, "bms_smbus");
 * @endcode
 */

/* --- Device Constants --- */

/**
 * @brief BQ7850 device configuration limits
 *
 * These constants define the hardware limits of the BQ7850 device.
 */
typedef enum {
  k_bq7850_default_addr           = 0x08, /**< Default SMBus address (configurable) */
  k_bq7850_max_cells              = 16,   /**< Maximum number of series cells */
  k_bq7850_max_temp_sensors       = 3,    /**< Maximum number of temperature sensors */
  k_bq7850_device_info_string_len = 32,   /**< Maximum length for device info strings */
} bq7850_device_limits_t;

/**
 * @brief BQ7850 timing constants
 *
 * Timing values for various BQ7850 operations.
 */
typedef enum {
  k_bq7850_mfr_access_delay_ms  = 10,   /**< Delay after manufacturer access command (ms) */
  k_bq7850_reset_delay_ms       = 1000, /**< Delay after device reset (ms) */
  k_bq7850_cmd_write_delay_ms   = 50,   /**< Delay between consecutive write commands (ms) */
  k_bq7850_kelvin_offset_deci_c = 2731, /**< 273.1K in 0.1K units (for K to C conversion) */
} bq7850_timing_t;

/* --- Register Map --- */

/**
 * @brief BQ7850 Standard SMBus Commands
 *
 * Standard commands accessible via SMBus protocol.
 */
typedef enum {
  k_bq7850_cmd_manufacturer_access   = 0x00, /**< Manufacturer access */
  k_bq7850_cmd_temperature           = 0x08, /**< Temperature (0.1K) */
  k_bq7850_cmd_voltage               = 0x09, /**< Pack voltage (mV) */
  k_bq7850_cmd_current               = 0x0A, /**< Pack current (mA) */
  k_bq7850_cmd_average_current       = 0x0B, /**< Average current (mA) */
  k_bq7850_cmd_relative_state_charge = 0x0D, /**< Relative SOC (%) */
  k_bq7850_cmd_absolute_state_charge = 0x0E, /**< Absolute SOC (%) */
  k_bq7850_cmd_remaining_capacity    = 0x0F, /**< Remaining capacity (mAh) */
  k_bq7850_cmd_full_charge_capacity  = 0x10, /**< Full charge capacity (mAh) */
  k_bq7850_cmd_charging_current      = 0x14, /**< Charging current (mA) */
  k_bq7850_cmd_charging_voltage      = 0x15, /**< Charging voltage (mV) */
  k_bq7850_cmd_battery_status        = 0x16, /**< Battery status flags */
  k_bq7850_cmd_cycle_count           = 0x17, /**< Cycle count */
  k_bq7850_cmd_design_capacity       = 0x18, /**< Design capacity (mAh) */
  k_bq7850_cmd_design_voltage        = 0x19, /**< Design voltage (mV) */
  k_bq7850_cmd_serial_number         = 0x1C, /**< Serial number */
  k_bq7850_cmd_manufacture_name      = 0x20, /**< Manufacturer name (block) */
  k_bq7850_cmd_device_name           = 0x21, /**< Device name (block) */
  k_bq7850_cmd_device_chemistry      = 0x22, /**< Device chemistry (block) */
  k_bq7850_cmd_manufacture_data      = 0x23, /**< Manufacturer data (block) */
  k_bq7850_cmd_cell_balance_ctrl     = 0x40, /**< Cell balancing control */
  k_bq7850_cmd_fet_control           = 0x46, /**< FET control */
  k_bq7850_cmd_alarm_warning         = 0x50, /**< Alarm/Warning flags */
  k_bq7850_cmd_safety_status         = 0x51, /**< Safety status */
  k_bq7850_cmd_pf_status             = 0x53, /**< Permanent fail status */
  k_bq7850_cmd_operation_status      = 0x54, /**< Operation status */
} bq7850_cmd_t;

/**
 * @brief BQ7850 Extended Subcommands (via Manufacturer Access 0x00)
 *
 * Subcommands written to manufacturer access register for extended functions.
 */
typedef enum {
  k_bq7850_subcmd_device_type           = 0x0001, /**< Device type */
  k_bq7850_subcmd_fw_version            = 0x0002, /**< Firmware version */
  k_bq7850_subcmd_hw_version            = 0x0003, /**< Hardware version */
  k_bq7850_subcmd_protection_ov_thresh  = 0x0010, /**< Overvoltage threshold */
  k_bq7850_subcmd_protection_uv_thresh  = 0x0011, /**< Undervoltage threshold */
  k_bq7850_subcmd_protection_oc_thresh  = 0x0012, /**< Overcurrent threshold */
  k_bq7850_subcmd_protection_ot_thresh  = 0x0013, /**< Over-temperature threshold */
  k_bq7850_subcmd_protection_ut_thresh  = 0x0014, /**< Under-temperature threshold */
  k_bq7850_subcmd_it_enable             = 0x0021, /**< Enable impedance track */
  k_bq7850_subcmd_seal                  = 0x0030, /**< Seal device */
  k_bq7850_subcmd_cal_mode              = 0x0040, /**< Enter calibration mode */
  k_bq7850_subcmd_reset                 = 0x0041, /**< Software reset */
  k_bq7850_subcmd_device_reset          = 0x0041, /**< Full device reset (same as reset) */
  k_bq7850_subcmd_cell_voltage_base     = 0x0070, /**< Cell 1 voltage base (Cell N = base + N-1) */
  k_bq7850_subcmd_temp_sensor_base      = 0x0078, /**< Temp sensor 1 base (Sensor N = base + N-1) */
} bq7850_subcmd_t;

/**
 * @brief BQ7850 Battery Status Flags (register 0x16)
 *
 * Bit flags for battery status register.
 */
typedef enum {
  k_bq7850_battery_status_oca  = (1 << 0),  /**< Over-current in charge */
  k_bq7850_battery_status_tca  = (1 << 1),  /**< Terminate charge alarm */
  k_bq7850_battery_status_fd   = (1 << 4),  /**< Fully discharged */
  k_bq7850_battery_status_fc   = (1 << 5),  /**< Fully charged */
  k_bq7850_battery_status_dsg  = (1 << 6),  /**< Discharging */
  k_bq7850_battery_status_init = (1 << 7),  /**< Initialized */
  k_bq7850_battery_status_rca  = (1 << 9),  /**< Remaining capacity alarm */
  k_bq7850_battery_status_rta  = (1 << 10), /**< Remaining time alarm */
  k_bq7850_battery_status_tda  = (1 << 14), /**< Terminate discharge alarm */
  k_bq7850_battery_status_ota  = (1 << 15), /**< Over-temperature alarm */
} bq7850_battery_status_t;

/**
 * @brief BQ7850 Safety Status Flags (register 0x51)
 *
 * Bit flags for safety status register.
 */
typedef enum {
  k_bq7850_safety_status_cuv = (1 << 0), /**< Cell undervoltage */
  k_bq7850_safety_status_cov = (1 << 1), /**< Cell overvoltage */
  k_bq7850_safety_status_occ = (1 << 2), /**< Overcurrent in charge */
  k_bq7850_safety_status_ocd = (1 << 3), /**< Overcurrent in discharge */
  k_bq7850_safety_status_otc = (1 << 4), /**< Over-temperature charge */
  k_bq7850_safety_status_otd = (1 << 5), /**< Over-temperature discharge */
  k_bq7850_safety_status_utc = (1 << 6), /**< Under-temperature charge */
  k_bq7850_safety_status_utd = (1 << 7), /**< Under-temperature discharge */
} bq7850_safety_status_t;

/**
 * @brief BQ7850 FET Control Bits (register 0x46)
 *
 * Bit flags for FET control register.
 */
typedef enum {
  k_bq7850_fet_control_chg_fet  = (1 << 0), /**< Charge FET enable */
  k_bq7850_fet_control_dsg_fet  = (1 << 1), /**< Discharge FET enable */
  k_bq7850_fet_control_pchg_fet = (1 << 2), /**< Pre-charge FET enable */
} bq7850_fet_control_t;

/* --- Types --- */

/**
 * @brief BQ7850 device configuration
 */
typedef struct {
  uint8_t  num_cells;       /**< Number of series cells (1-16) */
  uint8_t  num_temp;        /**< Number of temperature sensors (0-3) */
  uint8_t  smbus_addr;      /**< SMBus device address (default 0x08) */
  uint16_t design_capacity; /**< Design capacity in mAh */
  uint16_t design_voltage;  /**< Design voltage in mV */
} bq7850_config_t;

/**
 * @brief BQ7850 cell voltage data
 */
typedef struct {
  uint16_t cell_mv[k_bq7850_max_cells]; /**< Individual cell voltages (mV) */
  uint8_t  valid_cells;                 /**< Number of valid cell readings */
  uint16_t pack_mv;                     /**< Total pack voltage (mV) */
} bq7850_cell_data_t;

/**
 * @brief BQ7850 temperature data
 */
typedef struct {
  int16_t temp_c[k_bq7850_max_temp_sensors]; /**< Temperature sensors (0.1C) */
  uint8_t valid_sensors;                     /**< Number of valid sensors */
  int16_t avg_temp_c;                        /**< Average temperature (0.1C) */
} bq7850_temp_data_t;

/**
 * @brief BQ7850 current and power data
 */
typedef struct {
  int16_t  current_ma;     /**< Instantaneous current (mA, + = charge, - = discharge) */
  int16_t  avg_current_ma; /**< Average current (mA) */
  uint16_t voltage_mv;     /**< Pack voltage (mV) */
  int32_t  power_mw;       /**< Calculated power (mW) */
} bq7850_current_data_t;

/**
 * @brief BQ7850 state of charge data
 */
typedef struct {
  uint16_t remaining_capacity_mah; /**< Remaining capacity (mAh) */
  uint16_t full_capacity_mah;      /**< Full charge capacity (mAh) */
  uint8_t  relative_soc;           /**< Relative SOC (0-100%) */
  uint8_t  absolute_soc;           /**< Absolute SOC (0-100%) */
  uint16_t cycle_count;            /**< Charge/discharge cycles */
} bq7850_soc_data_t;

/**
 * @brief BQ7850 status flags
 */
typedef struct {
  uint16_t battery_status;   /**< Battery status register */
  uint16_t safety_status;    /**< Safety status register */
  uint16_t operation_status; /**< Operation status register */
  uint16_t alarm_warning;    /**< Alarm/warning register */
  bool     charging;         /**< True if charging */
  bool     discharging;      /**< True if discharging */
  bool     fully_charged;    /**< True if fully charged */
  bool     fault_active;     /**< True if any fault is active */
} bq7850_status_t;

/**
 * @brief BQ7850 protection thresholds
 */
typedef struct {
  uint16_t overvoltage_mv;   /**< Overvoltage threshold per cell (mV) */
  uint16_t undervoltage_mv;  /**< Undervoltage threshold per cell (mV) */
  uint16_t overcharge_ma;    /**< Overcharge current threshold (mA) */
  uint16_t overdischarge_ma; /**< Overdischarge current threshold (mA) */
  int16_t  overtemp_c;       /**< Over-temperature threshold (0.1C) */
  int16_t  undertemp_c;      /**< Under-temperature threshold (0.1C) */
} bq7850_protection_t;

/**
 * @brief BQ7850 complete battery state
 */
typedef struct {
  bq7850_cell_data_t    cells;   /**< Cell voltage data */
  bq7850_temp_data_t    temps;   /**< Temperature data */
  bq7850_current_data_t current; /**< Current and power data */
  bq7850_soc_data_t     soc;     /**< State of charge data */
  bq7850_status_t       status;  /**< Status flags */
} bq7850_battery_state_t;

/**
 * @brief BQ7850 device information
 */
typedef struct {
  uint16_t device_type;                                   /**< Device type ID */
  uint16_t fw_version;                                    /**< Firmware version */
  uint16_t hw_version;                                    /**< Hardware version */
  uint32_t serial_number;                                 /**< Serial number */
  char     manufacturer[k_bq7850_device_info_string_len]; /**< Manufacturer name */
  char     device_name[k_bq7850_device_info_string_len];  /**< Device name */
  char     chemistry[k_bq7850_device_info_string_len];    /**< Battery chemistry */
} bq7850_device_info_t;

/**
 * @brief BQ7850 device handle
 *
 * Maintains state and error handling for BMS operations.
 * Thread-safe: Initialization and deinitialization operations are protected
 * by an internal mutex. Read operations do not acquire mutex (consistent with
 * other STAR sensor drivers using const handle pointers).
 */
typedef struct bq7850_handle {
  star_bus_manager_t* manager;      /**< Pointer to bus manager */
  const char*         bus_name;     /**< Name of I2C/SMBus for this device */
  uint8_t             smbus_addr;   /**< SMBus device address */
  bq7850_config_t     config;       /**< Device configuration */
  SemaphoreHandle_t   mutex;        /**< Mutex for thread-safe operations */
  bool                initialized;  /**< Initialization state */
} bq7850_handle_t;

/* --- Core Functions --- */

/**
 * @brief Create and initialize BQ7850 BMS device handle
 *
 * Performs device detection, reads configuration, and verifies communication.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle   Pointer to handle structure (must be allocated by caller)
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C/SMBus configured for this device
 * @param[in]  config   Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_bms_bq7850_deinit() when done
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_bms_bq7850_init(bq7850_handle_t*       handle,
                               star_bus_manager_t*    manager,
                               const char*            bus_name,
                               const bq7850_config_t* config);

/**
 * @brief Deinitialize BQ7850 BMS device handle
 *
 * Cleans up resources associated with the handle.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_deinit(bq7850_handle_t* handle);

/**
 * @brief Read device information from BQ7850
 *
 * @param[in]  handle Pointer to initialized BMS handle
 * @param[out] info   Device information structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_get_device_info(const bq7850_handle_t* handle,
                                          bq7850_device_info_t*  info);

/**
 * @brief Software reset of BQ7850
 *
 * Issues a soft reset command. Device will take approximately 1 second to reset.
 *
 * @param[in] handle Pointer to initialized BMS handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_reset(const bq7850_handle_t* handle);

/* --- Cell Voltage Functions --- */

/**
 * @brief Read all cell voltages
 *
 * @param[in]  handle    Pointer to initialized BMS handle
 * @param[out] cell_data Cell voltage data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_cells(const bq7850_handle_t* handle, bq7850_cell_data_t* cell_data);

/**
 * @brief Read single cell voltage
 *
 * @param[in]  handle     Pointer to initialized BMS handle
 * @param[in]  cell_index Cell index (0-15)
 * @param[out] voltage_mv Cell voltage in millivolts
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if cell_index >= 16
 */
esp_err_t star_bms_bq7850_read_cell_voltage(const bq7850_handle_t* handle,
                                            uint8_t                cell_index,
                                            uint16_t*              voltage_mv);

/**
 * @brief Read pack voltage
 *
 * @param[in]  handle     Pointer to initialized BMS handle
 * @param[out] voltage_mv Pack voltage in millivolts
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_pack_voltage(const bq7850_handle_t* handle, uint16_t* voltage_mv);

/* --- Temperature Functions --- */

/**
 * @brief Read all temperature sensors
 *
 * @param[in]  handle    Pointer to initialized BMS handle
 * @param[out] temp_data Temperature data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_temperatures(const bq7850_handle_t* handle,
                                            bq7850_temp_data_t*    temp_data);

/**
 * @brief Read pack temperature
 *
 * @param[in]  handle Pointer to initialized BMS handle
 * @param[out] temp_c Temperature in 0.1C (e.g., 253 = 25.3C)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_temperature(const bq7850_handle_t* handle, int16_t* temp_c);

/* --- Current and Power Functions --- */

/**
 * @brief Read current measurements
 *
 * @param[in]  handle       Pointer to initialized BMS handle
 * @param[out] current_data Current and power data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_current(const bq7850_handle_t* handle,
                                       bq7850_current_data_t* current_data);

/* --- State of Charge Functions --- */

/**
 * @brief Read state of charge data
 *
 * @param[in]  handle   Pointer to initialized BMS handle
 * @param[out] soc_data SOC data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_soc(const bq7850_handle_t* handle, bq7850_soc_data_t* soc_data);

/* --- Status Functions --- */

/**
 * @brief Read all status flags
 *
 * @param[in]  handle Pointer to initialized BMS handle
 * @param[out] status Status structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_status(const bq7850_handle_t* handle, bq7850_status_t* status);

/**
 * @brief Read complete battery state
 *
 * Reads all battery parameters in a single call (cells, temps, current, SOC, status).
 * This is more efficient than calling individual functions.
 *
 * @param[in]  handle Pointer to initialized BMS handle
 * @param[out] state  Complete battery state structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_battery_state(const bq7850_handle_t*  handle,
                                             bq7850_battery_state_t* state);

/* --- Cell Balancing Functions --- */

/**
 * @brief Enable cell balancing
 *
 * Enables cell balancing for specified cells. The BQ7850 will automatically
 * balance cells based on configured thresholds.
 *
 * @param[in] handle    Pointer to initialized BMS handle
 * @param[in] cell_mask Bitmask of cells to balance (bit 0 = cell 1, etc.)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_enable_cell_balancing(const bq7850_handle_t* handle, uint16_t cell_mask);

/**
 * @brief Disable cell balancing
 *
 * @param[in] handle Pointer to initialized BMS handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_disable_cell_balancing(const bq7850_handle_t* handle);

/**
 * @brief Get cell balancing status
 *
 * @param[in]  handle      Pointer to initialized BMS handle
 * @param[out] active_mask Bitmask of cells currently balancing
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_get_balancing_status(const bq7850_handle_t* handle,
                                               uint16_t*              active_mask);

/* --- Protection Functions --- */

/**
 * @brief Read protection thresholds
 *
 * @param[in]  handle     Pointer to initialized BMS handle
 * @param[out] protection Protection thresholds structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_read_protection(const bq7850_handle_t* handle,
                                          bq7850_protection_t*   protection);

/**
 * @brief Write protection thresholds
 *
 * Note: Device must be unsealed to modify protection settings.
 *
 * @param[in] handle     Pointer to initialized BMS handle
 * @param[in] protection Protection thresholds to write
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bms_bq7850_write_protection(const bq7850_handle_t*     handle,
                                           const bq7850_protection_t* protection);

/* --- FET Control Functions --- */

/**
 * @brief Control charge/discharge FETs
 *
 * @param[in] handle        Pointer to initialized BMS handle
 * @param[in] charge_fet    true to enable charge FET
 * @param[in] discharge_fet true to enable discharge FET
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bms_bq7850_control_fets(const bq7850_handle_t* handle, bool charge_fet, bool discharge_fet);

/* --- Helper Functions --- */

/**
 * @brief Convert temperature from 0.1K to C
 *
 * @param[in] temp_01k Temperature in 0.1K units
 *
 * @return Temperature in C (float)
 */
float star_bms_bq7850_convert_temp_to_celsius(int16_t temp_01k);

/**
 * @brief Check if any protection fault is active
 *
 * @param[in] status Status structure
 *
 * @return true if any fault is active
 */
bool star_bms_bq7850_is_fault_active(const bq7850_status_t* status);

/**
 * @brief Get human-readable status string
 *
 * @param[in]  status Status structure
 * @param[out] buffer String buffer
 * @param[in]  size   Buffer size
 *
 * @return Number of characters written (excluding null terminator)
 */
size_t star_bms_bq7850_status_to_string(const bq7850_status_t* status, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* STAR_BMS_BQ7850_H */
