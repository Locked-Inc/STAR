/* esp32-firmware/components/star_bus/include/star_bus_devices.h */

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
 */

/* --- BMP280 Pressure/Temperature Sensor --- */

#define BMP280_DEFAULT_ADDR (0x76)
#define BMP280_ALT_ADDR (0x77)

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

#define MPU6050_DEFAULT_ADDR (0x68)
#define MPU6050_ALT_ADDR (0x69)

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

#define SSD1306_DEFAULT_ADDR (0x3C)
#define SSD1306_ALT_ADDR (0x3D)

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

#define ADS1115_DEFAULT_ADDR (0x48)

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

#define PCF8574_DEFAULT_ADDR (0x20)

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

#define AT24C256_DEFAULT_ADDR (0x50)

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

#define DS3231_DEFAULT_ADDR (0x68)

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
