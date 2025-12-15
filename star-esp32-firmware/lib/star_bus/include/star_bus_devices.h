/* lib/star_bus/include/star_bus_devices.h */

#ifndef STAR_COMPONENT_BUS_DEVICES_H
#define STAR_COMPONENT_BUS_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_manager.h"

/**
 * @file star_bus_devices.h
 * @brief Device profiles and helper functions for common I2C devices
 *
 * This module provides high-level APIs for interacting with common I2C sensors
 * and peripherals. All functions use the star_bus_manager to communicate with
 * devices by their configured bus name.
 *
 * Example Usage:
 * @code
 * // === BMP280 Pressure/Temperature Sensor ===
 *
 * #include "star_bus_devices.h"
 * #include "star_bus_manager.h"
 *
 * // Create I2C bus for BMP280
 * star_bus_config_t* bmp_bus = star_bus_config_create_i2c(
 *     "bmp280", I2C_NUM_0, s_bmp280_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, bmp_bus);
 *
 * // Initialize BMP280 (reads calibration data)
 * bmp280_config_t bmp_config = {0};
 * esp_err_t ret = star_bus_bmp280_init(&bus_manager, "bmp280", &bmp_config);
 *
 * // Configure oversampling and mode
 * bmp_config.oversampling_temp = 2;  // x2 oversampling
 * bmp_config.oversampling_pres = 4;  // x8 oversampling
 * bmp_config.mode = 3;               // Normal mode
 * star_bus_bmp280_configure(&bus_manager, "bmp280", &bmp_config);
 *
 * // Read temperature and pressure
 * float temperature, pressure;
 * star_bus_bmp280_read_temperature(&bus_manager, "bmp280", &bmp_config, &temperature);
 * star_bus_bmp280_read_pressure(&bus_manager, "bmp280", &bmp_config, &pressure);
 * ESP_LOGI(TAG, "Temp: %.2f C, Pressure: %.2f Pa", temperature, pressure);
 *
 *
 * // === MPU6050 6-Axis IMU ===
 *
 * // Create I2C bus for MPU6050
 * star_bus_config_t* imu_bus = star_bus_config_create_i2c(
 *     "mpu6050", I2C_NUM_0, s_mpu6050_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, imu_bus);
 *
 * // Initialize MPU6050
 * ret = star_bus_mpu6050_init(&bus_manager, "mpu6050");
 *
 * // Configure ranges
 * mpu6050_config_t mpu_config = {
 *     .accel_range = 1,      // +/- 4g
 *     .gyro_range = 1,       // +/- 500 deg/s
 *     .dlpf_mode = 3,        // Low-pass filter
 *     .sample_rate_div = 9   // 100Hz sample rate
 * };
 * star_bus_mpu6050_configure(&bus_manager, "mpu6050", &mpu_config);
 *
 * // Read all sensor data
 * mpu6050_data_t mpu_data;
 * star_bus_mpu6050_read_all(&bus_manager, "mpu6050", &mpu_data);
 * ESP_LOGI(TAG, "Accel: X=%d Y=%d Z=%d", mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z);
 * ESP_LOGI(TAG, "Gyro: X=%d Y=%d Z=%d", mpu_data.gyro_x, mpu_data.gyro_y, mpu_data.gyro_z);
 *
 *
 * // === SSD1306 OLED Display ===
 *
 * // Create I2C bus for OLED
 * star_bus_config_t* oled_bus = star_bus_config_create_i2c(
 *     "oled", I2C_NUM_0, s_ssd1306_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, oled_bus);
 *
 * // Initialize 128x64 OLED
 * star_bus_ssd1306_init(&bus_manager, "oled", 128, 64);
 *
 * // Clear display
 * star_bus_ssd1306_clear(&bus_manager, "oled");
 *
 * // Update display with framebuffer
 * uint8_t framebuffer[128 * 64 / 8];  // 1 bit per pixel
 * memset(framebuffer, 0xFF, sizeof(framebuffer));  // All pixels on
 * star_bus_ssd1306_update(&bus_manager, "oled", framebuffer, sizeof(framebuffer));
 *
 *
 * // === ADS1115 16-bit ADC ===
 *
 * star_bus_config_t* adc_bus = star_bus_config_create_i2c(
 *     "adc", I2C_NUM_0, s_ads1115_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, adc_bus);
 *
 * star_bus_ads1115_init(&bus_manager, "adc");
 *
 * // Configure for single-ended input on AIN0, +/- 4.096V range
 * ads1115_config_t adc_config = {
 *     .mux = 4,        // AIN0 vs GND
 *     .gain = 1,       // +/- 4.096V
 *     .mode = 1,       // Single-shot
 *     .data_rate = 4   // 128 SPS
 * };
 * star_bus_ads1115_configure(&bus_manager, "adc", &adc_config);
 *
 * // Read conversion
 * int16_t adc_value;
 * star_bus_ads1115_read(&bus_manager, "adc", &adc_value);
 * float voltage = (adc_value / 32768.0f) * 4.096f;
 * ESP_LOGI(TAG, "ADC: %d (%.3f V)", adc_value, voltage);
 *
 *
 * // === PCF8574 I/O Expander ===
 *
 * star_bus_config_t* io_bus = star_bus_config_create_i2c(
 *     "io_expander", I2C_NUM_0, s_pcf8574_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 100000);
 * star_bus_manager_add_bus(&bus_manager, io_bus);
 *
 * // Write outputs (0 = low, 1 = high/input)
 * star_bus_pcf8574_write(&bus_manager, "io_expander", 0xF0);  // Lower 4 low, upper 4 high
 *
 * // Read inputs
 * uint8_t inputs;
 * star_bus_pcf8574_read(&bus_manager, "io_expander", &inputs);
 * ESP_LOGI(TAG, "IO state: 0x%02X", inputs);
 *
 *
 * // === AT24C256 EEPROM ===
 *
 * star_bus_config_t* eeprom_bus = star_bus_config_create_i2c(
 *     "eeprom", I2C_NUM_0, s_at24c256_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 100000);
 * star_bus_manager_add_bus(&bus_manager, eeprom_bus);
 *
 * // Write data to EEPROM
 * uint8_t write_data[] = {0x12, 0x34, 0x56, 0x78};
 * star_bus_at24c256_write(&bus_manager, "eeprom", 0x0000, write_data, sizeof(write_data));
 * vTaskDelay(pdMS_TO_TICKS(10));  // Write cycle time
 *
 * // Read data from EEPROM
 * uint8_t read_data[4];
 * star_bus_at24c256_read(&bus_manager, "eeprom", 0x0000, read_data, sizeof(read_data));
 *
 *
 * // === DS3231 RTC ===
 *
 * star_bus_config_t* rtc_bus = star_bus_config_create_i2c(
 *     "rtc", I2C_NUM_0, s_ds3231_default_addr,
 *     GPIO_NUM_21, GPIO_NUM_22, 100000);
 * star_bus_manager_add_bus(&bus_manager, rtc_bus);
 *
 * star_bus_ds3231_init(&bus_manager, "rtc");
 *
 * // Set time
 * ds3231_time_t set_time = {
 *     .year = 24, .month = 6, .date = 15,
 *     .day = 6,   // Saturday
 *     .hour = 14, .minute = 30, .second = 0
 * };
 * star_bus_ds3231_set_time(&bus_manager, "rtc", &set_time);
 *
 * // Read time
 * ds3231_time_t read_time;
 * star_bus_ds3231_get_time(&bus_manager, "rtc", &read_time);
 * ESP_LOGI(TAG, "Time: 20%02d-%02d-%02d %02d:%02d:%02d",
 *          read_time.year, read_time.month, read_time.date,
 *          read_time.hour, read_time.minute, read_time.second);
 *
 * // Read temperature from RTC's internal sensor
 * float rtc_temp;
 * star_bus_ds3231_get_temperature(&bus_manager, "rtc", &rtc_temp);
 * ESP_LOGI(TAG, "RTC temperature: %.2f C", rtc_temp);
 * @endcode
 */

/* --- BMP280 Pressure/Temperature Sensor --- */

/* Type-safe I2C addresses for BMP280 */
static const uint8_t s_bmp280_default_addr = 0x76;
static const uint8_t s_bmp280_alt_addr     = 0x77;

/** BMP280 configuration structure */
typedef struct {
  uint8_t  oversampling_temp; /**< Temperature oversampling (0=skip, 1-5=2^(n-1)) */
  uint8_t  oversampling_pres; /**< Pressure oversampling (0=skip, 1-5=2^(n-1)) */
  uint8_t  mode;              /**< Power mode (0=sleep, 1/2=forced, 3=normal) */
  uint8_t  standby_time;      /**< Standby time in normal mode */
  uint8_t  filter;            /**< IIR filter coefficient */
  uint16_t dig_t1;            /**< Calibration coefficient T1 */
  int16_t  dig_t2;            /**< Calibration coefficient T2 */
  int16_t  dig_t3;            /**< Calibration coefficient T3 */
  uint16_t dig_p1;            /**< Calibration coefficient P1 */
  int16_t  dig_p2;            /**< Calibration coefficient P2 */
  int16_t  dig_p3;            /**< Calibration coefficient P3 */
  int16_t  dig_p4;            /**< Calibration coefficient P4 */
  int16_t  dig_p5;            /**< Calibration coefficient P5 */
  int16_t  dig_p6;            /**< Calibration coefficient P6 */
  int16_t  dig_p7;            /**< Calibration coefficient P7 */
  int16_t  dig_p8;            /**< Calibration coefficient P8 */
  int16_t  dig_p9;            /**< Calibration coefficient P9 */
  int32_t  t_fine;            /**< Fine temperature value for pressure compensation */
} bmp280_config_t;

/**
 * @brief Initialize BMP280 sensor and read calibration data
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration structure to populate with calibration data
 * @return ESP_OK on success
 */
esp_err_t star_bus_bmp280_init(const star_bus_manager_t* manager,
                               const char*               bus_name,
                               bmp280_config_t*          config);

/**
 * @brief Configure BMP280 operating mode and oversampling
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration to apply
 * @return ESP_OK on success
 */
esp_err_t star_bus_bmp280_configure(const star_bus_manager_t* manager,
                                    const char*               bus_name,
                                    const bmp280_config_t*    config);

/**
 * @brief Read temperature from BMP280
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration with calibration data
 * @param temp_celsius Output temperature in degrees Celsius
 * @return ESP_OK on success
 */
esp_err_t star_bus_bmp280_read_temperature(const star_bus_manager_t* manager,
                                           const char*               bus_name,
                                           bmp280_config_t*          config,
                                           float*                    temp_celsius);

/**
 * @brief Read pressure from BMP280
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration with calibration data (must have valid t_fine)
 * @param pressure_pa Output pressure in Pascals
 * @return ESP_OK on success
 */
esp_err_t star_bus_bmp280_read_pressure(const star_bus_manager_t* manager,
                                        const char*               bus_name,
                                        bmp280_config_t*          config,
                                        float*                    pressure_pa);

/* --- MPU6050 6-Axis IMU --- */

/* Type-safe I2C addresses for MPU6050 */
static const uint8_t s_mpu6050_default_addr = 0x68;
static const uint8_t s_mpu6050_alt_addr     = 0x69;

/** MPU6050 configuration structure */
typedef struct {
  uint8_t accel_range; /**< Accelerometer range (0=+/-2g, 1=+/-4g, 2=+/-8g, 3=+/-16g) */
  uint8_t
    gyro_range; /**< Gyroscope range (0=+/-250deg/s, 1=+/-500deg/s, 2=+/-1000deg/s, 3=+/-2000deg/s) */
  uint8_t dlpf_mode;       /**< Digital low-pass filter mode */
  uint8_t sample_rate_div; /**< Sample rate divider */
} mpu6050_config_t;

/** MPU6050 sensor data structure */
typedef struct {
  int16_t accel_x; /**< Accelerometer X-axis raw value */
  int16_t accel_y; /**< Accelerometer Y-axis raw value */
  int16_t accel_z; /**< Accelerometer Z-axis raw value */
  int16_t temp;    /**< Temperature raw value */
  int16_t gyro_x;  /**< Gyroscope X-axis raw value */
  int16_t gyro_y;  /**< Gyroscope Y-axis raw value */
  int16_t gyro_z;  /**< Gyroscope Z-axis raw value */
} mpu6050_data_t;

/**
 * @brief Initialize MPU6050 IMU
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @return ESP_OK on success
 */
esp_err_t star_bus_mpu6050_init(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Configure MPU6050 ranges and filters
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration to apply
 * @return ESP_OK on success
 */
esp_err_t star_bus_mpu6050_configure(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     const mpu6050_config_t*   config);

/**
 * @brief Read all sensor data from MPU6050
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param data Output sensor data
 * @return ESP_OK on success
 */
esp_err_t star_bus_mpu6050_read_all(const star_bus_manager_t* manager,
                                    const char*               bus_name,
                                    mpu6050_data_t*           data);

/* --- SSD1306 OLED Display --- */

/* Type-safe I2C addresses for SSD1306 */
static const uint8_t s_ssd1306_default_addr = 0x3C;
static const uint8_t s_ssd1306_alt_addr     = 0x3D;

/**
 * @brief Initialize SSD1306 OLED display
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param width Display width in pixels (typically 128)
 * @param height Display height in pixels (typically 32 or 64)
 * @return ESP_OK on success
 */
esp_err_t star_bus_ssd1306_init(const star_bus_manager_t* manager,
                                const char*               bus_name,
                                uint8_t                   width,
                                uint8_t                   height);

/**
 * @brief Clear SSD1306 display
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @return ESP_OK on success
 */
esp_err_t star_bus_ssd1306_clear(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write framebuffer to SSD1306 display
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param buffer Framebuffer data (1 bit per pixel, column-major)
 * @param size Size of buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t star_bus_ssd1306_update(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  const uint8_t*            buffer,
                                  size_t                    size);

/* --- ADS1115 16-bit ADC --- */

/* Type-safe I2C address for ADS1115 */
static const uint8_t s_ads1115_default_addr = 0x48;

/** ADS1115 configuration structure */
typedef struct {
  uint8_t mux;       /**< Input multiplexer config (0-7) */
  uint8_t gain;      /**< Programmable gain amplifier (0-5) */
  uint8_t mode;      /**< Operating mode (0=continuous, 1=single-shot) */
  uint8_t data_rate; /**< Data rate (0-7) */
} ads1115_config_t;

/**
 * @brief Initialize ADS1115 ADC
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @return ESP_OK on success
 */
esp_err_t star_bus_ads1115_init(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Configure ADS1115 ADC
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param config Configuration to apply
 * @return ESP_OK on success
 */
esp_err_t star_bus_ads1115_configure(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     const ads1115_config_t*   config);

/**
 * @brief Read conversion result from ADS1115
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param value Output ADC value (signed 16-bit)
 * @return ESP_OK on success
 */
esp_err_t
star_bus_ads1115_read(const star_bus_manager_t* manager, const char* bus_name, int16_t* value);

/* --- PCF8574 8-bit I/O Expander --- */

/* Type-safe I2C address for PCF8574 */
static const uint8_t s_pcf8574_default_addr = 0x20;

/**
 * @brief Write byte to PCF8574 I/O expander
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param value Byte value to write (bit=1 for high/input, bit=0 for low)
 * @return ESP_OK on success
 */
esp_err_t
star_bus_pcf8574_write(const star_bus_manager_t* manager, const char* bus_name, uint8_t value);

/**
 * @brief Read byte from PCF8574 I/O expander
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param value Output byte value
 * @return ESP_OK on success
 */
esp_err_t
star_bus_pcf8574_read(const star_bus_manager_t* manager, const char* bus_name, uint8_t* value);

/* --- AT24C256 EEPROM --- */

/* Type-safe I2C address for AT24C256 */
static const uint8_t s_at24c256_default_addr = 0x50;

/**
 * @brief Write data to AT24C256 EEPROM
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param address Memory address (0-32767)
 * @param data Data to write
 * @param len Number of bytes to write
 * @return ESP_OK on success
 */
esp_err_t star_bus_at24c256_write(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  uint16_t                  address,
                                  const uint8_t*            data,
                                  size_t                    len);

/**
 * @brief Read data from AT24C256 EEPROM
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param address Memory address (0-32767)
 * @param data Buffer to read into
 * @param len Number of bytes to read
 * @return ESP_OK on success
 */
esp_err_t star_bus_at24c256_read(const star_bus_manager_t* manager,
                                 const char*               bus_name,
                                 uint16_t                  address,
                                 uint8_t*                  data,
                                 size_t                    len);

/* --- DS3231 RTC --- */

/* Type-safe I2C address for DS3231 */
static const uint8_t s_ds3231_default_addr = 0x68;

/** DS3231 time structure */
typedef struct {
  uint8_t second; /**< Seconds (0-59) */
  uint8_t minute; /**< Minutes (0-59) */
  uint8_t hour;   /**< Hours (0-23) */
  uint8_t day;    /**< Day of week (1-7) */
  uint8_t date;   /**< Date (1-31) */
  uint8_t month;  /**< Month (1-12) */
  uint8_t year;   /**< Year (0-99, represents 2000-2099) */
} ds3231_time_t;

/**
 * @brief Initialize DS3231 RTC
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @return ESP_OK on success
 */
esp_err_t star_bus_ds3231_init(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Set DS3231 time
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param time Time structure to set
 * @return ESP_OK on success
 */
esp_err_t star_bus_ds3231_set_time(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   const ds3231_time_t*      time);

/**
 * @brief Read DS3231 time
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param time Output time structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_ds3231_get_time(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   ds3231_time_t*            time);

/**
 * @brief Read DS3231 temperature
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus configured for this device
 * @param temp_celsius Output temperature in degrees Celsius
 * @return ESP_OK on success
 */
esp_err_t star_bus_ds3231_get_temperature(const star_bus_manager_t* manager,
                                          const char*               bus_name,
                                          float*                    temp_celsius);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_DEVICES_H */
