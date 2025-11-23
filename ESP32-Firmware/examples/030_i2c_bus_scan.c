/**
 * @file 030_i2c_bus_scan.c
 * @brief Scan I2C bus for all devices (0x00-0x7F)
 *
 * This example demonstrates:
 * - Scanning entire I2C address range
 * - Detecting all connected devices
 * - Identifying reserved addresses
 * - Displaying results in a formatted table
 * - Quick vs thorough scan modes
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_SCAN";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Clock speed */
#define I2C_CLK_SPEED    (100000)  /* 100 kHz for compatibility */

/* I2C address range (7-bit) */
#define I2C_ADDR_MIN    (0x08)  /* First valid address */
#define I2C_ADDR_MAX    (0x77)  /* Last valid address */

/**
 * @brief Check if address is reserved
 */
static bool priv_is_reserved_address(uint8_t addr)
{
  /* Reserved addresses in 7-bit mode */
  if (addr < 0x08) return true;  /* 0x00-0x07 reserved */
  if (addr > 0x77) return true;  /* 0x78-0x7F reserved */

  /* 10-bit addressing indicator */
  if (addr >= 0x78 && addr <= 0x7B) return true;

  return false;
}

/**
 * @brief Get description for common I2C addresses
 */
static const char *priv_get_device_description(uint8_t addr)
{
  switch (addr) {
    case 0x00: return "General Call";
    case 0x01: return "CBUS Address";
    case 0x02: return "Reserved";
    case 0x03: return "Reserved";
    case 0x1E: return "Magnetometer (LSM303, HMC5883)";
    case 0x20: return "PCF8574, MCP23017 (I/O Expander)";
    case 0x21: return "PCF8574, MCP23017 (I/O Expander)";
    case 0x27: return "LCD Display (PCF8574)";
    case 0x3C: return "OLED Display (SSD1306)";
    case 0x3D: return "OLED Display (SSD1306)";
    case 0x40: return "HTU21D, SI7021 (Humidity)";
    case 0x48: return "ADS1115, PCF8591 (ADC), TMP102";
    case 0x50: return "EEPROM (24C series)";
    case 0x51: return "EEPROM (24C series)";
    case 0x52: return "EEPROM (24C series)";
    case 0x53: return "ADXL345 (Accelerometer)";
    case 0x57: return "EEPROM (24C series)";
    case 0x68: return "DS1307, MPU6050 (RTC/IMU)";
    case 0x69: return "MPU6050 (IMU)";
    case 0x76: return "BMP280, BME280 (Pressure/Humidity)";
    case 0x77: return "BMP180, BMP280, BME280";
    default: return NULL;
  }
}

/**
 * @brief Perform quick scan (single read attempt)
 */
static int priv_quick_scan(star_bus_manager_t *manager, uint8_t *found_devices)
{
  ESP_LOGI(s_tag, "\n=== Quick Scan (0x08 - 0x77) ===");
  ESP_LOGI(s_tag, "Scanning...");

  int found_count = 0;

  for (uint8_t addr = I2C_ADDR_MIN; addr <= I2C_ADDR_MAX; addr++) {
    /* Skip reserved addresses */
    if (priv_is_reserved_address(addr)) {
      continue;
    }

    /* Create temporary config for this address */
    star_bus_config_t* scan_config = star_bus_config_create_i2c(
      "scan_temp",
      I2C_NUM_0,
      addr,
      I2C_SDA_PIN,
      I2C_SCL_PIN,
      I2C_CLK_SPEED
    );

    if (scan_config) {
      star_bus_manager_add_bus(manager, scan_config);
      esp_err_t init_ret = star_bus_config_init(scan_config, manager);

      if (init_ret == ESP_OK) {
        /* Try to read one byte */
        uint8_t dummy;
        size_t bytes_read;
        esp_err_t ret = star_bus_i2c_read_raw(manager, "scan_temp", &dummy, 1, &bytes_read);

        if (ret == ESP_OK) {
          found_devices[found_count++] = addr;
          ESP_LOGI(s_tag, "  Found device at 0x%02X", addr);
        }
      }

      /* Remove temporary config */
      star_bus_manager_remove_bus(manager, "scan_temp");
      star_bus_config_destroy(scan_config);
    }

    /* Small delay between scans */
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  return found_count;
}

/**
 * @brief Display scan results in table format
 */
static void priv_display_scan_results(uint8_t *found_devices, int count)
{
  ESP_LOGI(s_tag, "\n=== Scan Results ===");
  ESP_LOGI(s_tag, "Found %d device%s", count, count == 1 ? "" : "s");

  if (count == 0) {
    ESP_LOGI(s_tag, "No I2C devices found. Check connections and pull-ups.");
    return;
  }

  ESP_LOGI(s_tag, "\n     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
  ESP_LOGI(s_tag, "00:          ");

  for (int row = 0; row < 8; row++) {
    printf("%02X: ", row * 16);

    for (int col = 0; col < 16; col++) {
      uint8_t addr = row * 16 + col;

      if (priv_is_reserved_address(addr)) {
        printf("   ");
      } else {
        bool found = false;
        for (int i = 0; i < count; i++) {
          if (found_devices[i] == addr) {
            found = true;
            break;
          }
        }

        if (found) {
          printf("%02X ", addr);
        } else {
          printf("-- ");
        }
      }
    }
    printf("\n");
  }

  /* Show details for found devices */
  ESP_LOGI(s_tag, "\n=== Device Details ===");
  for (int i = 0; i < count; i++) {
    uint8_t addr = found_devices[i];
    const char *desc = priv_get_device_description(addr);

    if (desc) {
      ESP_LOGI(s_tag, "0x%02X: %s", addr, desc);
    } else {
      ESP_LOGI(s_tag, "0x%02X: Unknown device", addr);
    }
  }
}

/**
 * @brief Thorough scan with multiple attempts
 */
static int priv_thorough_scan(star_bus_manager_t *manager, uint8_t *found_devices)
{
  ESP_LOGI(s_tag, "\n=== Thorough Scan (Multiple Attempts) ===");
  ESP_LOGI(s_tag, "This may take longer but is more reliable...");

  int found_count = 0;
  const int attempts_per_addr = 3;

  for (uint8_t addr = I2C_ADDR_MIN; addr <= I2C_ADDR_MAX; addr++) {
    if (priv_is_reserved_address(addr)) {
      continue;
    }

    int success_count = 0;

    /* Try multiple times */
    for (int attempt = 0; attempt < attempts_per_addr; attempt++) {
      star_bus_config_t* scan_config = star_bus_config_create_i2c(
        "scan_temp",
        I2C_NUM_0,
        addr,
        I2C_SDA_PIN,
        I2C_SCL_PIN,
        I2C_CLK_SPEED
      );

      if (scan_config) {
        star_bus_manager_add_bus(manager, scan_config);
        esp_err_t init_ret = star_bus_config_init(scan_config, manager);

        if (init_ret == ESP_OK) {
          uint8_t dummy;
          size_t bytes_read;
          esp_err_t ret = star_bus_i2c_read_raw(manager, "scan_temp", &dummy, 1, &bytes_read);

          if (ret == ESP_OK) {
            success_count++;
          }
        }

        star_bus_manager_remove_bus(manager, "scan_temp");
        star_bus_config_destroy(scan_config);
      }

      vTaskDelay(pdMS_TO_TICKS(2));
    }

    /* Consider device found if at least 2/3 attempts succeeded */
    if (success_count >= (attempts_per_addr * 2 / 3)) {
      found_devices[found_count++] = addr;
      ESP_LOGI(s_tag, "  Found device at 0x%02X (%d/%d attempts)",
               addr, success_count, attempts_per_addr);
    }
  }

  return found_count;
}

static void priv_i2c_bus_scan_example(void)
{
  esp_err_t ret;

  /* Array to store found device addresses */
  uint8_t found_devices[128];
  int found_count = 0;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Scanner", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "I2C Bus Scanner initialized");
  ESP_LOGI(s_tag, "SDA: GPIO%d, SCL: GPIO%d", I2C_SDA_PIN, I2C_SCL_PIN);
  ESP_LOGI(s_tag, "Clock: %d Hz", I2C_CLK_SPEED);

  /* Perform quick scan */
  found_count = priv_quick_scan(&manager, found_devices);
  priv_display_scan_results(found_devices, found_count);

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* Optionally perform thorough scan */
  ESP_LOGI(s_tag, "\n\n=== Performing Thorough Scan ===");
  found_count = priv_thorough_scan(&manager, found_devices);
  priv_display_scan_results(found_devices, found_count);

  /* Show reserved address information */
  ESP_LOGI(s_tag, "\n=== Reserved Addresses ===");
  ESP_LOGI(s_tag, "0x00-0x07: Reserved (General Call, etc.)");
  ESP_LOGI(s_tag, "0x78-0x7F: Reserved (10-bit addressing)");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nBus scan complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Bus Scanner ===\n");
  priv_i2c_bus_scan_example();
}
