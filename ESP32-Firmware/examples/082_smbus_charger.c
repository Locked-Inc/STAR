/**
 * @file 082_smbus_charger.c
 * @brief Smart Battery Charger interface example
 *
 * Demonstrates:
 * - Smart Battery Charger specification using correct star_bus APIs
 * - Charger control and status
 * - Charge current/voltage configuration
 * - Charging monitoring
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char *s_tag = "SMBUS_CHARGER";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define CHARGER_ADDR   (0x09)  // Standard Smart Battery Charger address

// Smart Battery Charger Commands
#define CHARGER_MODE (0x12)
#define CHARGER_STATUS (0x13)
#define CHARGING_CURRENT (0x14)
#define CHARGING_VOLs_tagE (0x15)

static star_bus_manager_t s_manager;

/**
 * @brief Demonstrate charger operations
 */
static void priv_demonstrate_charger_operations(void)
{
  esp_err_t ret;
  uint16_t value;

  ESP_LOGI(s_tag, "\n=== Smart Battery Charger Operations ===");

  // Read charger status
  ESP_LOGI(s_tag, "\nReading Charger Status:");
  ret = star_smbus_read_word(&s_manager, "charger", CHARGER_ADDR,
                              CHARGER_STATUS, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Status: 0x%04X", value);
    if (value & (1 << 5)) ESP_LOGI(s_tag, "  - AC Present");
    if (value & (1 << 6)) ESP_LOGI(s_tag, "  - Battery Present");
    if (value & (1 << 2)) ESP_LOGI(s_tag, "  - Fully Charged");
    if (value & (1 << 1)) ESP_LOGI(s_tag, "  - Discharging");
  } else {
    ESP_LOGW(s_tag, "  Failed to read status: %s", esp_err_to_name(ret));
  }

  // Read charging current
  ESP_LOGI(s_tag, "\nReading Charging Current:");
  ret = star_smbus_read_word(&s_manager, "charger", CHARGER_ADDR,
                              CHARGING_CURRENT, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Charging Current: %u mA (%.3f A)", value, value / 1000.0f);
  } else {
    ESP_LOGW(s_tag, "  Failed to read charging current: %s", esp_err_to_name(ret));
  }

  // Read charging voltage
  ESP_LOGI(s_tag, "\nReading Charging Voltage:");
  ret = star_smbus_read_word(&s_manager, "charger", CHARGER_ADDR,
                              CHARGING_VOLs_tagE, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Charging Voltage: %u mV (%.3f V)", value, value / 1000.0f);
  } else {
    ESP_LOGW(s_tag, "  Failed to read charging voltage: %s", esp_err_to_name(ret));
  }

  // Read charger mode
  ESP_LOGI(s_tag, "\nReading Charger Mode:");
  ret = star_smbus_read_word(&s_manager, "charger", CHARGER_ADDR,
                              CHARGER_MODE, &value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Mode: 0x%04X", value);
    if (value & (1 << 0)) {
      ESP_LOGI(s_tag, "  - Charging Inhibited");
    } else {
      ESP_LOGI(s_tag, "  - Charging Enabled");
    }
  } else {
    ESP_LOGW(s_tag, "  Failed to read mode: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate charger configuration
 */
static void priv_demonstrate_charger_config(void)
{
  ESP_LOGI(s_tag, "\n=== Charger Configuration Example ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "To configure charging parameters:");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "1. Set Maximum Charging Current:");
  ESP_LOGI(s_tag, "   star_smbus_write_word(manager, bus, 0x09, 0x14, 1500)");
  ESP_LOGI(s_tag, "   // Sets 1500 mA (1.5A) max charging current");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "2. Set Maximum Charging Voltage:");
  ESP_LOGI(s_tag, "   star_smbus_write_word(manager, bus, 0x09, 0x15, 4200)");
  ESP_LOGI(s_tag, "   // Sets 4200 mV (4.2V) max charging voltage");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "3. Enable/Disable Charging:");
  ESP_LOGI(s_tag, "   star_smbus_write_word(manager, bus, 0x09, 0x12, mode)");
  ESP_LOGI(s_tag, "   // Bit 0: 1=inhibit, 0=enable");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Smart Battery Charger Example ===\n");
  ESP_LOGI(s_tag, "Smart Battery Charger address: 0x%02X\n", CHARGER_ADDR);

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "charger_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t *charger_config = star_bus_config_create_i2c(
    "charger",
    I2C_NUM_0,
    CHARGER_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (charger_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, charger_config);
  ret = star_bus_config_init(charger_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init charger interface");
    ESP_LOGW(s_tag, "Make sure a Smart Battery Charger is connected at 0x%02X", CHARGER_ADDR);
    return;
  }

  ESP_LOGI(s_tag, "Smart Battery Charger interface initialized\n");

  /* Demonstrate charger operations */
  priv_demonstrate_charger_operations();

  /* Demonstrate configuration */
  priv_demonstrate_charger_config();

  /* Information */
  ESP_LOGI(s_tag, "\n=== Smart Battery Charger Specification ===");
  ESP_LOGI(s_tag, "Standard address: 0x09 (0b0001001)");
  ESP_LOGI(s_tag, "Uses SMBus Read/Write Word commands");
  ESP_LOGI(s_tag, "Charging algorithms: CC/CV (Constant Current/Constant Voltage)");
  ESP_LOGI(s_tag, "Safety features: Over-current, over-voltage, over-temperature protection");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSmart Battery Charger example complete");
}
