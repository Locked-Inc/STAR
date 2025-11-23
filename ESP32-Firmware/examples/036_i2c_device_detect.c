/**
 * @file 036_i2c_device_detect.c
 * @brief Device detection and identification patterns
 *
 * This example demonstrates:
 * - Detecting I2C device presence
 * - Reading device ID registers
 * - Identifying device type by signature
 * - Auto-detection of common sensors
 * - WHO_AM_I register patterns
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "I2C_DETECT";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/**
 * @brief Known device signature
 */
typedef struct {
  const char *name;
  uint8_t     addr;
  uint8_t     id_reg;
  uint8_t     id_value;
  const char *description;
} device_signature_t;

/**
 * @brief Database of known I2C devices
 */
static const device_signature_t s_known_devices[] = {
  /* Inertial Measurement Units (IMUs) */
  { "MPU6050",    0x68, 0x75, 0x68, "6-axis IMU (Accel + Gyro)" },
  { "MPU9250",    0x68, 0x75, 0x71, "9-axis IMU (Accel + Gyro + Mag)" },
  { "LSM6DS3",    0x6A, 0x0F, 0x69, "6-axis IMU" },

  /* Magnetometers */
  { "HMC5883L",   0x1E, 0x0A, 0x48, "3-axis Magnetometer" },
  { "QMC5883L",   0x0D, 0x0D, 0xFF, "3-axis Magnetometer" },

  /* Environmental Sensors */
  { "BME280",     0x76, 0xD0, 0x60, "Temp + Humidity + Pressure" },
  { "BMP280",     0x76, 0xD0, 0x58, "Temp + Pressure" },
  { "BME680",     0x76, 0xD0, 0x61, "Temp + Humidity + Pressure + Gas" },

  /* OLED Displays */
  { "SSD1306",    0x3C, 0x00, 0x00, "128x64 OLED Display" },

  /* ADCs */
  { "ADS1115",    0x48, 0x00, 0x00, "16-bit 4-channel ADC" },

  /* Real-Time Clocks */
  { "DS1307",     0x68, 0x00, 0x00, "Real-Time Clock" },
  { "DS3231",     0x68, 0x00, 0x00, "High-Precision RTC" },

  /* Accelerometers */
  { "ADXL345",    0x53, 0x00, 0xE5, "3-axis Accelerometer" },

  /* Temperature Sensors */
  { "TMP102",     0x48, 0x00, 0x00, "Digital Temperature Sensor" },
  { "LM75",       0x48, 0x00, 0x00, "Digital Temperature Sensor" },
};

#define NUM_KNOWN_DEVICES (sizeof(s_known_devices) / sizeof(device_signature_t))

/**
 * @brief Check if device is present at address
 */
bool is_device_present(star_bus_manager_t *manager, uint8_t addr)
{
  /* Create temporary config */
  char bus_name[32];
  snprintf(bus_name, sizeof(bus_name), "detect_0x%02X", addr);

  star_bus_config_t *config = star_bus_config_create_i2c(
    bus_name,
    I2C_NUM_0,
    addr,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (!config) {
    return false;
  }

  star_bus_manager_add_bus(manager, config);
  esp_err_t ret = star_bus_config_init(config, manager);

  if (ret != ESP_OK) {
    star_bus_manager_remove_bus(manager, bus_name);
    star_bus_config_destroy(config);
    return false;
  }

  /* Try to read one byte */
  uint8_t dummy;
  size_t  bytes_read;
  ret = star_bus_i2c_read_raw(manager, bus_name, &dummy, 1, &bytes_read);

  /* Cleanup */
  star_bus_manager_remove_bus(manager, bus_name);
  star_bus_config_destroy(config);

  return (ret == ESP_OK);
}

/**
 * @brief Read device ID register
 */
bool read_device_id(star_bus_manager_t *manager, uint8_t addr, uint8_t id_reg,
                    uint8_t *id_value)
{
  char bus_name[32];
  snprintf(bus_name, sizeof(bus_name), "id_0x%02X", addr);

  star_bus_config_t *config = star_bus_config_create_i2c(
    bus_name,
    I2C_NUM_0,
    addr,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (!config) {
    return false;
  }

  star_bus_manager_add_bus(manager, config);
  esp_err_t init_ret = star_bus_config_init(config, manager);

  if (init_ret != ESP_OK) {
    star_bus_manager_remove_bus(manager, bus_name);
    star_bus_config_destroy(config);
    return false;
  }

  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, bus_name, id_value, 1,
                                    id_reg, &bytes_read);

  star_bus_manager_remove_bus(manager, bus_name);
  star_bus_config_destroy(config);

  return (ret == ESP_OK && bytes_read == 1);
}

/**
 * @brief Try to identify device
 */
static const device_signature_t *priv_identify_device(star_bus_manager_t *manager,
                                                       uint8_t addr)
{
  /* Check each known device at this address */
  for (int i = 0; i < NUM_KNOWN_DEVICES; i++) {
    if (s_known_devices[i].addr == addr) {
      uint8_t id_value;

      if (read_device_id(manager, addr, s_known_devices[i].id_reg, &id_value)) {
        if (id_value == s_known_devices[i].id_value) {
          return &s_known_devices[i];
        }
      }
    }
  }

  return NULL;
}

/**
 * @brief Scan bus and identify all devices
 */
void scan_and_identify(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Scanning I2C Bus ===");
  ESP_LOGI(s_tag, "Scanning addresses 0x08 - 0x77...\n");

  int found_count      = 0;
  int identified_count = 0;

  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (is_device_present(manager, addr)) {
      found_count++;
      ESP_LOGI(s_tag, "Device found at 0x%02X", addr);

      /* Try to identify */
      const device_signature_t *device = priv_identify_device(manager, addr);

      if (device) {
        ESP_LOGI(s_tag, "  Identified: %s", device->name);
        ESP_LOGI(s_tag, "  Description: %s", device->description);
        identified_count++;
      } else {
        ESP_LOGI(s_tag, "  Type: Unknown");
      }

      ESP_LOGI(s_tag, "");  /* Blank line */
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }

  ESP_LOGI(s_tag, "\n=== Scan Summary ===");
  ESP_LOGI(s_tag, "Total devices found: %d", found_count);
  ESP_LOGI(s_tag, "Identified devices: %d", identified_count);
  ESP_LOGI(s_tag, "Unknown devices: %d", found_count - identified_count);
}

/**
 * @brief Detect specific device by known patterns
 */
void detect_mpu6050(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Detecting MPU6050 ===");

  uint8_t addr = 0x68;

  if (!is_device_present(manager, addr)) {
    ESP_LOGI(s_tag, "No device at 0x68");
    return;
  }

  ESP_LOGI(s_tag, "Device present at 0x68");

  /* Read WHO_AM_I register (0x75) */
  uint8_t who_am_i;
  if (read_device_id(manager, addr, 0x75, &who_am_i)) {
    ESP_LOGI(s_tag, "WHO_AM_I register: 0x%02X", who_am_i);

    if (who_am_i == 0x68) {
      ESP_LOGI(s_tag, "Device identified as MPU6050!");

      /* Read additional registers for verification */
      char bus_name[] = "mpu6050";
      star_bus_config_t *config = star_bus_config_create_i2c(
        bus_name, I2C_NUM_0, addr, I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLK_SPEED
      );

      if (config) {
        star_bus_manager_add_bus(manager, config);
        star_bus_config_init(config, manager);

        /* Read PWR_MGMT_1 register (0x6B) */
        uint8_t pwr_mgmt;
        size_t  bytes_read;
        if (star_bus_i2c_read(manager, bus_name, &pwr_mgmt, 1,
                              0x6B, &bytes_read) == ESP_OK) {
          ESP_LOGI(s_tag, "PWR_MGMT_1: 0x%02X", pwr_mgmt);

          if (pwr_mgmt & 0x40) {
            ESP_LOGI(s_tag, "Device is in SLEEP mode");
          } else {
            ESP_LOGI(s_tag, "Device is ACTIVE");
          }
        }

        star_bus_manager_remove_bus(manager, bus_name);
        star_bus_config_destroy(config);
      }
    } else {
      ESP_LOGI(s_tag, "Not an MPU6050 (WHO_AM_I mismatch)");
    }
  }
}

/**
 * @brief Detect BME280 environmental sensor
 */
void detect_bme280(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Detecting BME280 ===");

  uint8_t addr = 0x76;  /* or 0x77 */

  if (!is_device_present(manager, addr)) {
    ESP_LOGI(s_tag, "No device at 0x76");
    return;
  }

  ESP_LOGI(s_tag, "Device present at 0x76");

  /* Read chip ID register (0xD0) */
  uint8_t chip_id;
  if (read_device_id(manager, addr, 0xD0, &chip_id)) {
    ESP_LOGI(s_tag, "Chip ID: 0x%02X", chip_id);

    if (chip_id == 0x60) {
      ESP_LOGI(s_tag, "Device identified as BME280!");
      ESP_LOGI(s_tag, "Capabilities: Temperature, Humidity, Pressure");
    } else if (chip_id == 0x58) {
      ESP_LOGI(s_tag, "Device identified as BMP280!");
      ESP_LOGI(s_tag, "Capabilities: Temperature, Pressure");
    } else {
      ESP_LOGI(s_tag, "Unknown Bosch sensor (ID: 0x%02X)", chip_id);
    }
  }
}

void i2c_device_detect_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Detect", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Device detection system initialized");

  /* Perform full bus scan and identification */
  scan_and_identify(&manager);

  vTaskDelay(pdMS_TO_TICKS(500));

  /* Specific device detection examples */
  detect_mpu6050(&manager);

  vTaskDelay(pdMS_TO_TICKS(500));

  detect_bme280(&manager);

  /* Print known device database */
  ESP_LOGI(s_tag, "\n=== Known Device Database ===");
  ESP_LOGI(s_tag, "Total known devices: %d\n", NUM_KNOWN_DEVICES);

  for (int i = 0; i < NUM_KNOWN_DEVICES; i++) {
    ESP_LOGI(s_tag, "%s (0x%02X)", s_known_devices[i].name,
             s_known_devices[i].addr);
    ESP_LOGI(s_tag, "  ID Reg: 0x%02X, Value: 0x%02X",
             s_known_devices[i].id_reg, s_known_devices[i].id_value);
    ESP_LOGI(s_tag, "  %s\n", s_known_devices[i].description);
  }

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Device Detection Best Practices ===");
  ESP_LOGI(s_tag, "1. Always check WHO_AM_I or ID registers");
  ESP_LOGI(s_tag, "2. Verify with multiple register reads");
  ESP_LOGI(s_tag, "3. Check device-specific reset values");
  ESP_LOGI(s_tag, "4. Handle devices with same address differently");
  ESP_LOGI(s_tag, "5. Document all device signatures");
  ESP_LOGI(s_tag, "6. Test with real hardware when possible");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nDevice detection example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Device Detection Example ===\n");
  i2c_device_detect_example();
}
