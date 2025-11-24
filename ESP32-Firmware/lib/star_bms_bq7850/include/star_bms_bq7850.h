/* esp32-firmware/components/star_bms_bq7850/include/star_bms_bq7850.h */

#ifndef STAR_BMS_BQ7850_H
#define STAR_BMS_BQ7850_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_error_interface.h"

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
 */

/* --- Constants --- */

/** BQ7850 default SMBus address (can be configured) */
#define BQ7850_DEFAULT_ADDR (0x08)

/** Maximum number of cells supported by BQ7850 */
#define BQ7850_MAX_CELLS (16)

/** Maximum number of temperature sensors */
#define BQ7850_MAX_TEMP_SENSORS (3)

/* --- Register Map --- */

/* Standard Commands (SMBus) */
#define BQ7850_CMD_MANUFACTURER_ACCESS (0x00)   /**< Manufacturer access */
#define BQ7850_CMD_REMAINING_CAPACITY (0x0F)    /**< Remaining capacity (mAh) */
#define BQ7850_CMD_FULL_CHARGE_CAPACITY (0x10)  /**< Full charge capacity (mAh) */
#define BQ7850_CMD_TEMPERATURE (0x08)           /**< Temperature (0.1K) */
#define BQ7850_CMD_VOLTAGE (0x09)               /**< Pack voltage (mV) */
#define BQ7850_CMD_CURRENT (0x0A)               /**< Pack current (mA) */
#define BQ7850_CMD_AVERAGE_CURRENT (0x0B)       /**< Average current (mA) */
#define BQ7850_CMD_RELATIVE_STATE_CHARGE (0x0D) /**< Relative SOC (%) */
#define BQ7850_CMD_ABSOLUTE_STATE_CHARGE (0x0E) /**< Absolute SOC (%) */
#define BQ7850_CMD_CYCLE_COUNT (0x17)           /**< Cycle count */
#define BQ7850_CMD_DESIGN_CAPACITY (0x18)       /**< Design capacity (mAh) */
#define BQ7850_CMD_DESIGN_VOLTAGE (0x19)        /**< Design voltage (mV) */
#define BQ7850_CMD_SERIAL_NUMBER (0x1C)         /**< Serial number */
#define BQ7850_CMD_MANUFACTURE_NAME (0x20)      /**< Manufacturer name (block) */
#define BQ7850_CMD_DEVICE_NAME (0x21)           /**< Device name (block) */
#define BQ7850_CMD_DEVICE_CHEMISTRY (0x22)      /**< Device chemistry (block) */
#define BQ7850_CMD_MANUFACTURE_DATA (0x23)      /**< Manufacturer data (block) */

/* Extended Commands (via Manufacturer Access 0x00) */
#define BQ7850_SUBCMD_DEVICE_TYPE (0x0001)  /**< Device type */
#define BQ7850_SUBCMD_FW_VERSION (0x0002)   /**< Firmware version */
#define BQ7850_SUBCMD_HW_VERSION (0x0003)   /**< Hardware version */
#define BQ7850_SUBCMD_RESET (0x0041)        /**< Software reset */
#define BQ7850_SUBCMD_SEAL (0x0030)         /**< Seal device */
#define BQ7850_SUBCMD_IT_ENABLE (0x0021)    /**< Enable impedance track */
#define BQ7850_SUBCMD_CAL_MODE (0x0040)     /**< Enter calibration mode */
#define BQ7850_SUBCMD_DEVICE_RESET (0x0041) /**< Full device reset */

/* Cell Voltage Registers (via subcommands) */
#define BQ7850_SUBCMD_CELL_VOLTAGE_BASE                                                            \
  (0x0070) /**< Cell 1 voltage = 0x0070, Cell 2 = 0x0071, etc. */

/* Temperature Registers */
#define BQ7850_SUBCMD_TEMP_SENSOR_BASE (0x0078) /**< Temp sensor 1 = 0x0078, etc. */

/* Status and Control Registers */
#define BQ7850_CMD_BATTERY_STATUS (0x16)   /**< Battery status flags */
#define BQ7850_CMD_CHARGING_CURRENT (0x14) /**< Charging current (mA) */
#define BQ7850_CMD_CHARGING_VOLTAGE (0x15) /**< Charging voltage (mV) */
#define BQ7850_CMD_ALARM_WARNING (0x50)    /**< Alarm/Warning flags */
#define BQ7850_CMD_SAFETY_STATUS (0x51)    /**< Safety status */
#define BQ7850_CMD_PF_STATUS (0x53)        /**< Permanent fail status */
#define BQ7850_CMD_OPERATION_STATUS (0x54) /**< Operation status */
#define BQ7850_CMD_FET_CONTROL (0x46)      /**< FET control */

/* Cell Balancing Control */
#define BQ7850_CMD_CELL_BALANCE_CTRL (0x40) /**< Cell balancing control */

/* Protection Thresholds */
#define BQ7850_SUBCMD_PROTECTION_OV_THRESH (0x0010) /**< Overvoltage threshold */
#define BQ7850_SUBCMD_PROTECTION_UV_THRESH (0x0011) /**< Undervoltage threshold */
#define BQ7850_SUBCMD_PROTECTION_OC_THRESH (0x0012) /**< Overcurrent threshold */
#define BQ7850_SUBCMD_PROTECTION_OT_THRESH (0x0013) /**< Over-temperature threshold */
#define BQ7850_SUBCMD_PROTECTION_UT_THRESH (0x0014) /**< Under-temperature threshold */

/* --- Battery Status Flags (0x16) --- */
#define BQ7850_BATTERY_STATUS_OCA (1 << (0))  /**< Over-current in charge */
#define BQ7850_BATTERY_STATUS_TCA (1 << (1))  /**< Terminate charge alarm */
#define BQ7850_BATTERY_STATUS_OTA (1 << (15)) /**< Over-temperature alarm */
#define BQ7850_BATTERY_STATUS_TDA (1 << (14)) /**< Terminate discharge alarm */
#define BQ7850_BATTERY_STATUS_RCA (1 << (9))  /**< Remaining capacity alarm */
#define BQ7850_BATTERY_STATUS_RTA (1 << (10)) /**< Remaining time alarm */
#define BQ7850_BATTERY_STATUS_INIT (1 << (7)) /**< Initialized */
#define BQ7850_BATTERY_STATUS_DSG (1 << (6))  /**< Discharging */
#define BQ7850_BATTERY_STATUS_FC (1 << (5))   /**< Fully charged */
#define BQ7850_BATTERY_STATUS_FD (1 << (4))   /**< Fully discharged */

/* --- Safety Status Flags (0x51) --- */
#define BQ7850_SAFETY_STATUS_CUV (1 << (0)) /**< Cell undervoltage */
#define BQ7850_SAFETY_STATUS_COV (1 << (1)) /**< Cell overvoltage */
#define BQ7850_SAFETY_STATUS_OCC (1 << (2)) /**< Overcurrent in charge */
#define BQ7850_SAFETY_STATUS_OCD (1 << (3)) /**< Overcurrent in discharge */
#define BQ7850_SAFETY_STATUS_OTC (1 << (4)) /**< Over-temperature charge */
#define BQ7850_SAFETY_STATUS_OTD (1 << (5)) /**< Over-temperature discharge */
#define BQ7850_SAFETY_STATUS_UTC (1 << (6)) /**< Under-temperature charge */
#define BQ7850_SAFETY_STATUS_UTD (1 << (7)) /**< Under-temperature discharge */

/* --- FET Control Bits (0x46) --- */
#define BQ7850_FET_CONTROL_CHG_FET (1 << (0))  /**< Charge FET enable */
#define BQ7850_FET_CONTROL_DSG_FET (1 << (1))  /**< Discharge FET enable */
#define BQ7850_FET_CONTROL_PCHG_FET (1 << (2)) /**< Pre-charge FET enable */

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
  uint16_t cell_mv[BQ7850_MAX_CELLS]; /**< Individual cell voltages (mV) */
  uint8_t  valid_cells;               /**< Number of valid cell readings */
  uint16_t pack_mv;                   /**< Total pack voltage (mV) */
} bq7850_cell_data_t;

/**
 * @brief BQ7850 temperature data
 */
typedef struct {
  int16_t temp_c[BQ7850_MAX_TEMP_SENSORS]; /**< Temperature sensors (0.1C) */
  uint8_t valid_sensors;                   /**< Number of valid sensors */
  int16_t avg_temp_c;                      /**< Average temperature (0.1C) */
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
  uint16_t device_type;      /**< Device type ID */
  uint16_t fw_version;       /**< Firmware version */
  uint16_t hw_version;       /**< Hardware version */
  uint32_t serial_number;    /**< Serial number */
  char     manufacturer[32]; /**< Manufacturer name */
  char     device_name[32];  /**< Device name */
  char     chemistry[32];    /**< Battery chemistry */
} bq7850_device_info_t;

/**
 * @brief BQ7850 device handle
 *
 * Maintains state and error handling for BMS operations.
 * Thread-safe: All operations are protected by an internal mutex.
 */
typedef struct bq7850_handle {
  star_bus_manager_t*     manager;            /**< Pointer to bus manager */
  const char*             bus_name;           /**< Name of I2C/SMBus for this device */
  uint8_t                 smbus_addr;         /**< SMBus device address */
  bq7850_config_t         config;             /**< Device configuration */
  star_error_interface_t* error_iface;        /**< Injected error interface (NULL = default) */
  SemaphoreHandle_t       mutex;              /**< Mutex for thread-safe operations */
  bool                    initialized;        /**< Initialization state */
  bool                    owns_error_handler; /**< True if we created default error handler */
} bq7850_handle_t;

/* --- Core Functions --- */

/**
 * @brief Create and initialize BQ7850 BMS device handle
 *
 * Performs device detection, reads configuration, and verifies communication.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle      Pointer to handle structure (must be allocated by caller)
 * @param[in]  manager     Pointer to initialized bus manager
 * @param[in]  bus_name    Name of I2C/SMBus configured for this device
 * @param[in]  error_iface Error interface for error handling (NULL = create default internally)
 * @param[in]  config      Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_bms_bq7850_deinit() when done
 * @note If error_iface is NULL, a default error handler will be created internally
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_bms_bq7850_init(bq7850_handle_t*        handle,
                               star_bus_manager_t*     manager,
                               const char*             bus_name,
                               star_error_interface_t* error_iface,
                               const bq7850_config_t*  config);

/**
 * @brief Deinitialize BQ7850 BMS device handle
 *
 * Cleans up resources associated with the handle, including the error handler.
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
