/**
 * @file 081_smbus_battery.c
 * @brief Smart Battery Data Specification (SBS) interface example
 *
 * Demonstrates:
 * - Smart Battery System interface using correct star_bus APIs
 * - Standard SBS commands
 * - Battery status monitoring
 * - Capacity and voltage readings
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char *s_tag = "SMBUS_BATTERY";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define BATTERY_ADDR   (0x0B)  // Standard Smart Battery address

// Smart Battery System Commands (SBS 1.1)
#define SBS_TEMPERATURE (0x08)
#define SBS_VOLs_tagE (0x09)
#define SBS_CURRENT (0x0A)
#define SBS_AVERAGE_CURRENT (0x0B)
#define SBS_RELATIVE_STATE_OF_CHARGE (0x0D)
#define SBS_ABSOLUTE_STATE_OF_CHARGE (0x0E)
#define SBS_REMAINING_CAPACITY (0x0F)
#define SBS_FULL_CHARGE_CAPACITY (0x10)
#define SBS_BATTERY_STATUS (0x16)
#define SBS_CYCLE_COUNT (0x17)
#define SBS_DESIGN_CAPACITY (0x18)
#define SBS_DESIGN_VOLs_tagE (0x19)
#define SBS_SERIAL_NUMBER (0x1C)
#define SBS_MANUFACTURER_NAME (0x20)
#define SBS_DEVICE_NAME (0x21)
#define SBS_DEVICE_CHEMISTRY (0x22)

static star_bus_manager_t s_manager;

/**
 * @brief Demonstrate Smart Battery System commands
 */
static void priv_demonstrate_sbs_commands(void)
{
  esp_err_t ret;
  uint16_t value;

  ESP_LOGI(s_tag, "\n=== Smart Battery System Commands ===");

  // Read voltage
  ESP_LOGI(s_tag, "\nReading Voltage:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_VOLs_tagE, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Voltage: %u mV (%.3f V)", value, value / 1000.0f);
  } else {
    ESP_LOGW(s_tag, "  Failed to read voltage: %s", esp_err_to_name(ret));
  }

  // Read current
  ESP_LOGI(s_tag, "\nReading Current:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_CURRENT, &value);
  if (ret == ESP_OK) {
    int16_t current = (int16_t)value;
    ESP_LOGI(s_tag, "  Current: %d mA (%.3f A)", current, current / 1000.0f);
    if (current > 0) {
      ESP_LOGI(s_tag, "  Battery is charging");
    } else if (current < 0) {
      ESP_LOGI(s_tag, "  Battery is discharging");
    }
  } else {
    ESP_LOGW(s_tag, "  Failed to read current: %s", esp_err_to_name(ret));
  }

  // Read temperature
  ESP_LOGI(s_tag, "\nReading Temperature:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_TEMPERATURE, &value);
  if (ret == ESP_OK) {
    float temp_k = value / 10.0f;
    float temp_c = temp_k - 273.15f;
    ESP_LOGI(s_tag, "  Temperature: %.1f C (%.1f K)", temp_c, temp_k);
  } else {
    ESP_LOGW(s_tag, "  Failed to read temperature: %s", esp_err_to_name(ret));
  }

  // Read state of charge
  ESP_LOGI(s_tag, "\nReading State of Charge:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_RELATIVE_STATE_OF_CHARGE, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Relative SOC: %u%%", value);
  } else {
    ESP_LOGW(s_tag, "  Failed to read SOC: %s", esp_err_to_name(ret));
  }

  // Read remaining capacity
  ESP_LOGI(s_tag, "\nReading Remaining Capacity:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_REMAINING_CAPACITY, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Remaining Capacity: %u mAh", value);
  } else {
    ESP_LOGW(s_tag, "  Failed to read remaining capacity: %s", esp_err_to_name(ret));
  }

  // Read full charge capacity
  ESP_LOGI(s_tag, "\nReading Full Charge Capacity:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_FULL_CHARGE_CAPACITY, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Full Charge Capacity: %u mAh", value);
  } else {
    ESP_LOGW(s_tag, "  Failed to read full charge capacity: %s", esp_err_to_name(ret));
  }

  // Read design capacity
  ESP_LOGI(s_tag, "\nReading Design Capacity:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_DESIGN_CAPACITY, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Design Capacity: %u mAh", value);
  } else {
    ESP_LOGW(s_tag, "  Failed to read design capacity: %s", esp_err_to_name(ret));
  }

  // Read cycle count
  ESP_LOGI(s_tag, "\nReading Cycle Count:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_CYCLE_COUNT, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Cycle Count: %u cycles", value);
  } else {
    ESP_LOGW(s_tag, "  Failed to read cycle count: %s", esp_err_to_name(ret));
  }

  // Read battery status
  ESP_LOGI(s_tag, "\nReading Battery Status:");
  ret = star_smbus_read_word(&s_manager, "battery", BATTERY_ADDR,
                              SBS_BATTERY_STATUS, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Status: 0x%04X", value);
    if (value & (1 << 5)) ESP_LOGI(s_tag, "  - Fully Charged");
    if (value & (1 << 6)) ESP_LOGI(s_tag, "  - Discharging");
    if (value & (1 << 7)) ESP_LOGI(s_tag, "  - Initialized");
    if (value & (1 << 12)) ESP_LOGI(s_tag, "  - Over Temperature Alarm");
  } else {
    ESP_LOGW(s_tag, "  Failed to read status: %s", esp_err_to_name(ret));
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Smart Battery System (SBS) Example ===\n");
  ESP_LOGI(s_tag, "Smart Battery address: 0x%02X\n", BATTERY_ADDR);

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "battery_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t *battery_config = star_bus_config_create_i2c(
    "battery",
    I2C_NUM_0,
    BATTERY_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (battery_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, battery_config);
  ret = star_bus_config_init(battery_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init battery interface");
    ESP_LOGW(s_tag, "Make sure a Smart Battery is connected at address 0x%02X", BATTERY_ADDR);
    return;
  }

  ESP_LOGI(s_tag, "Smart Battery interface initialized\n");

  /* Demonstrate SBS commands */
  priv_demonstrate_sbs_commands();

  /* Information */
  ESP_LOGI(s_tag, "\n=== Smart Battery Specification ===");
  ESP_LOGI(s_tag, "Standard address: 0x0B (0b0001011)");
  ESP_LOGI(s_tag, "Uses SMBus Read/Write Word commands");
  ESP_LOGI(s_tag, "All multi-byte values are little-endian");
  ESP_LOGI(s_tag, "Temperature in 0.1K units (subtract 273.15 for °C)");
  ESP_LOGI(s_tag, "Optional PEC support for critical commands");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSmart Battery example complete");
}
