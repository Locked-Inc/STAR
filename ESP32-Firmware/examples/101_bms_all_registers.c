/**
 * @file 101_bms_all_registers.c
 * @brief Read all BQ7850 registers and display register map
 *
 * Demonstrates:
 * - Reading all voltage registers
 * - Reading all current registers
 * - Reading all temperature registers
 * - Reading all status registers
 * - Displaying formatted register map with descriptions
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char* TAG = "BMS_ALL_REGISTERS";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/**
 * @brief Print a register value with description
 */
static void print_register(const char* name, uint16_t address, uint16_t value, const char* description)
{
  ESP_LOGI(TAG, "  [0x%04X] %-30s = 0x%04X (%5u) - %s",
           address, name, value, value, description);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Complete Register Map ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "register_demo", NULL, NULL);
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

  /* ===== DEVICE IDENTIFICATION REGISTERS ===== */
  ESP_LOGI(TAG, "===== DEVICE IDENTIFICATION =====");

  bq7850_device_info_t info;
  ret = star_bms_bq7850_get_device_info(&g_bms, &info);
  if (ret == ESP_OK) {
    print_register("DEVICE_TYPE", 0x0001, info.device_type, "Device type ID");
    print_register("FW_VERSION", 0x0002, info.fw_version, "Firmware version");
    print_register("HW_VERSION", 0x0003, info.hw_version, "Hardware version");
    print_register("SERIAL_NUMBER", 0x001C, info.serial_number & 0xFFFF, "Serial number (low)");
    print_register("SERIAL_NUMBER_HI", 0x001C, (info.serial_number >> 16) & 0xFFFF, "Serial number (high)");
    ESP_LOGI(TAG, "  Manufacturer: %s", info.manufacturer);
    ESP_LOGI(TAG, "  Device Name: %s", info.device_name);
    ESP_LOGI(TAG, "  Chemistry: %s", info.chemistry);
  } else {
    ESP_LOGW(TAG, "Failed to read device info");
  }

  /* ===== VOLTAGE REGISTERS ===== */
  ESP_LOGI(TAG, "\n===== VOLTAGE REGISTERS =====");

  uint16_t pack_voltage;
  ret = star_bms_bq7850_read_pack_voltage(&g_bms, &pack_voltage);
  if (ret == ESP_OK) {
    print_register("PACK_VOLTAGE", 0x0009, pack_voltage, "Total pack voltage (mV)");
  }

  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&g_bms, &cells);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "  Cell Voltages (base addr 0x0070):");
    for (int i = 0; i < cells.valid_cells; i++) {
      char desc[32];
      snprintf(desc, sizeof(desc), "Cell %d voltage (mV)", i + 1);
      char name[32];
      snprintf(name, sizeof(name), "CELL_%d_VOLTAGE", i + 1);
      print_register(name, 0x0070 + i, cells.cell_mv[i], desc);
    }

    /* Calculate min/max */
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
    }
    ESP_LOGI(TAG, "  Min voltage: %d mV (cell %d)", min_v, min_idx + 1);
    ESP_LOGI(TAG, "  Max voltage: %d mV (cell %d)", max_v, max_idx + 1);
    ESP_LOGI(TAG, "  Delta: %d mV", max_v - min_v);
  }

  /* ===== CURRENT REGISTERS ===== */
  ESP_LOGI(TAG, "\n===== CURRENT REGISTERS =====");

  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret == ESP_OK) {
    print_register("CURRENT", 0x000A, (uint16_t)current.current_ma, "Instantaneous current (mA)");
    print_register("AVG_CURRENT", 0x000B, (uint16_t)current.avg_current_ma, "Average current (mA)");
    print_register("POWER", 0x0000, (uint16_t)(current.power_mw & 0xFFFF), "Power (mW)");
    ESP_LOGI(TAG, "  Current direction: %s",
             current.current_ma > 0 ? "Charging" : current.current_ma < 0 ? "Discharging" : "Idle");
  }

  /* ===== TEMPERATURE REGISTERS ===== */
  ESP_LOGI(TAG, "\n===== TEMPERATURE REGISTERS =====");

  int16_t pack_temp;
  ret = star_bms_bq7850_read_temperature(&g_bms, &pack_temp);
  if (ret == ESP_OK) {
    float temp_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);
    ESP_LOGI(TAG, "  [0x0008] PACK_TEMPERATURE = %d (raw) = %.1f C", pack_temp, temp_c);
  }

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "  Temperature Sensors (base addr 0x0078):");
    for (int i = 0; i < temps.valid_sensors; i++) {
      float temp_c = star_bms_bq7850_convert_temp_to_celsius(temps.temp_c[i]);
      char desc[32];
      snprintf(desc, sizeof(desc), "Temp sensor %d (raw/C)", i + 1);
      ESP_LOGI(TAG, "  [0x%04X] TEMP_SENSOR_%d = %d (raw) = %.1f C",
               0x0078 + i, i + 1, temps.temp_c[i], temp_c);
    }

    /* Calculate min/max */
    int16_t min_t = 32767, max_t = -32768;
    for (int i = 0; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] < min_t) min_t = temps.temp_c[i];
      if (temps.temp_c[i] > max_t) max_t = temps.temp_c[i];
    }
    ESP_LOGI(TAG, "  Min temp: %.1f C",
             star_bms_bq7850_convert_temp_to_celsius(min_t));
    ESP_LOGI(TAG, "  Max temp: %.1f C",
             star_bms_bq7850_convert_temp_to_celsius(max_t));
  }

  /* ===== STATE OF CHARGE REGISTERS ===== */
  ESP_LOGI(TAG, "\n===== STATE OF CHARGE REGISTERS =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    print_register("REMAINING_CAPACITY", 0x000F, soc.remaining_capacity_mah, "Remaining capacity (mAh)");
    print_register("FULL_CAPACITY", 0x0010, soc.full_capacity_mah, "Full charge capacity (mAh)");
    print_register("RELATIVE_SOC", 0x000D, soc.relative_soc, "Relative SOC (%)");
    print_register("ABSOLUTE_SOC", 0x000E, soc.absolute_soc, "Absolute SOC (%)");
    print_register("CYCLE_COUNT", 0x0017, soc.cycle_count, "Charge/discharge cycles");
    /* Note: design_capacity and design_voltage are in config, not device_info */
  }

  /* ===== STATUS REGISTERS ===== */
  ESP_LOGI(TAG, "\n===== STATUS REGISTERS =====");

  bq7850_status_t status;
  ret = star_bms_bq7850_read_status(&g_bms, &status);
  if (ret == ESP_OK) {
    print_register("BATTERY_STATUS", 0x0016, status.battery_status, "Battery status flags");
    print_register("SAFETY_STATUS", 0x0051, status.safety_status, "Safety status flags");
    print_register("OPERATION_STATUS", 0x0054, status.operation_status, "Operation status");

    /* Decode battery status bits */
    ESP_LOGI(TAG, "  Battery Status Bits:");
    if (status.battery_status & BQ7850_BATTERY_STATUS_DSG)
      ESP_LOGI(TAG, "    [DSG] Discharging");
    if (status.battery_status & BQ7850_BATTERY_STATUS_FC)
      ESP_LOGI(TAG, "    [FC] Fully charged");
    if (status.battery_status & BQ7850_BATTERY_STATUS_FD)
      ESP_LOGI(TAG, "    [FD] Fully discharged");
    if (status.battery_status & BQ7850_BATTERY_STATUS_INIT)
      ESP_LOGI(TAG, "    [INIT] Initialized");

    /* Decode safety status bits */
    ESP_LOGI(TAG, "  Safety Status Bits:");
    if (status.safety_status & BQ7850_SAFETY_STATUS_OCC)
      ESP_LOGW(TAG, "    [OCC] Overcurrent in charge");
    if (status.safety_status & BQ7850_SAFETY_STATUS_OCD)
      ESP_LOGW(TAG, "    [OCD] Overcurrent in discharge");
    if (status.safety_status & BQ7850_SAFETY_STATUS_COV)
      ESP_LOGW(TAG, "    [COV] Cell overvoltage");
    if (status.safety_status & BQ7850_SAFETY_STATUS_CUV)
      ESP_LOGW(TAG, "    [CUV] Cell undervoltage");
    if (status.safety_status & BQ7850_SAFETY_STATUS_OTC)
      ESP_LOGW(TAG, "    [OTC] Over-temperature charge");
    if (status.safety_status & BQ7850_SAFETY_STATUS_OTD)
      ESP_LOGW(TAG, "    [OTD] Over-temperature discharge");
    if (status.safety_status == 0)
      ESP_LOGI(TAG, "    No safety faults");
  }

  /* ===== REGISTER MAP SUMMARY ===== */
  ESP_LOGI(TAG, "\n===== REGISTER MAP SUMMARY =====");
  ESP_LOGI(TAG, "Standard Commands (SMBus):");
  ESP_LOGI(TAG, "  0x00 - Manufacturer Access");
  ESP_LOGI(TAG, "  0x08 - Temperature");
  ESP_LOGI(TAG, "  0x09 - Voltage");
  ESP_LOGI(TAG, "  0x0A - Current");
  ESP_LOGI(TAG, "  0x0B - Average Current");
  ESP_LOGI(TAG, "  0x0D - Relative SOC");
  ESP_LOGI(TAG, "  0x0E - Absolute SOC");
  ESP_LOGI(TAG, "  0x0F - Remaining Capacity");
  ESP_LOGI(TAG, "  0x10 - Full Charge Capacity");
  ESP_LOGI(TAG, "  0x16 - Battery Status");
  ESP_LOGI(TAG, "  0x17 - Cycle Count");
  ESP_LOGI(TAG, "  0x1C - Serial Number");
  ESP_LOGI(TAG, "  0x51 - Safety Status");
  ESP_LOGI(TAG, "  0x54 - Operation Status");
  ESP_LOGI(TAG, "\nExtended Registers (via Manufacturer Access):");
  ESP_LOGI(TAG, "  0x0070-0x007F - Cell Voltages (16 cells)");
  ESP_LOGI(TAG, "  0x0078-0x007A - Temperature Sensors (3 sensors)");
  ESP_LOGI(TAG, "  0x0001 - Device Type");
  ESP_LOGI(TAG, "  0x0002 - Firmware Version");
  ESP_LOGI(TAG, "  0x0003 - Hardware Version");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nComplete register map example finished");
}
