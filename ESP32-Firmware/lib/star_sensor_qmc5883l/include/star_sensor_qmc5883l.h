/* lib/star_sensor_qmc5883l/include/star_sensor_qmc5883l.h */

#ifndef STAR_SENSOR_QMC5883L_H
#define STAR_SENSOR_QMC5883L_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_qmc5883l.h
 * @brief QMC5883L 3-axis magnetometer driver
 *
 * This driver provides interface to the QMC5883L digital compass.
 * It supports:
 * - 3-axis magnetic field measurement
 * - Configurable output data rate (10-200 Hz)
 * - Configurable range (±2G, ±8G)
 * - Oversampling ratio configuration
 * - Continuous and single measurement modes
 */

/* --- Constants --- */

#define QMC5883L_I2C_ADDR (0x0D)    /**< I2C address */
#define QMC5883L_CHIP_ID  (0xFF)    /**< Chip ID value */

/* --- Register Map --- */

#define QMC5883L_REG_DATA_OUT_X_LSB (0x00) /**< X-axis data LSB */
#define QMC5883L_REG_DATA_OUT_X_MSB (0x01) /**< X-axis data MSB */
#define QMC5883L_REG_DATA_OUT_Y_LSB (0x02) /**< Y-axis data LSB */
#define QMC5883L_REG_DATA_OUT_Y_MSB (0x03) /**< Y-axis data MSB */
#define QMC5883L_REG_DATA_OUT_Z_LSB (0x04) /**< Z-axis data LSB */
#define QMC5883L_REG_DATA_OUT_Z_MSB (0x05) /**< Z-axis data MSB */
#define QMC5883L_REG_STATUS         (0x06) /**< Status register */
#define QMC5883L_REG_TEMP_LSB       (0x07) /**< Temperature LSB */
#define QMC5883L_REG_TEMP_MSB       (0x08) /**< Temperature MSB */
#define QMC5883L_REG_CONTROL1       (0x09) /**< Control register 1 */
#define QMC5883L_REG_CONTROL2       (0x0A) /**< Control register 2 */
#define QMC5883L_REG_SET_RESET      (0x0B) /**< Set/Reset period */
#define QMC5883L_REG_CHIP_ID        (0x0D) /**< Chip ID register */

/* --- Control Register 1 Bits --- */

#define QMC5883L_MODE_STANDBY    (0x00)
#define QMC5883L_MODE_CONTINUOUS (0x01)

#define QMC5883L_ODR_10HZ  (0x00 << 2)
#define QMC5883L_ODR_50HZ  (0x01 << 2)
#define QMC5883L_ODR_100HZ (0x02 << 2)
#define QMC5883L_ODR_200HZ (0x03 << 2)

#define QMC5883L_RNG_2G (0x00 << 4)
#define QMC5883L_RNG_8G (0x01 << 4)

#define QMC5883L_OSR_512 (0x00 << 6)
#define QMC5883L_OSR_256 (0x01 << 6)
#define QMC5883L_OSR_128 (0x02 << 6)
#define QMC5883L_OSR_64  (0x03 << 6)

/* --- Control Register 2 Bits --- */

#define QMC5883L_SOFT_RST     (1 << 7)
#define QMC5883L_ROL_PNT      (1 << 6)
#define QMC5883L_INT_ENABLE   (1 << 0)

/* --- Status Register Bits --- */

#define QMC5883L_STATUS_DRDY (1 << 0) /**< Data ready */
#define QMC5883L_STATUS_OVL  (1 << 1) /**< Overflow */
#define QMC5883L_STATUS_DOR  (1 << 2) /**< Data skipped */

/* --- Types --- */

typedef enum {
  QMC5883L_ODR_10HZ_VAL  = 0,
  QMC5883L_ODR_50HZ_VAL  = 1,
  QMC5883L_ODR_100HZ_VAL = 2,
  QMC5883L_ODR_200HZ_VAL = 3
} qmc5883l_odr_t;

typedef enum {
  QMC5883L_RNG_2G_VAL = 0,
  QMC5883L_RNG_8G_VAL = 1
} qmc5883l_range_t;

typedef enum {
  QMC5883L_OSR_512_VAL = 0,
  QMC5883L_OSR_256_VAL = 1,
  QMC5883L_OSR_128_VAL = 2,
  QMC5883L_OSR_64_VAL  = 3
} qmc5883l_osr_t;

typedef enum {
  QMC5883L_MODE_STANDBY_VAL    = 0,
  QMC5883L_MODE_CONTINUOUS_VAL = 1
} qmc5883l_mode_t;

typedef struct {
  int16_t x; /**< X-axis magnetic field */
  int16_t y; /**< Y-axis magnetic field */
  int16_t z; /**< Z-axis magnetic field */
} qmc5883l_raw_data_t;

typedef struct {
  float x_gauss; /**< X-axis in Gauss */
  float y_gauss; /**< Y-axis in Gauss */
  float z_gauss; /**< Z-axis in Gauss */
} qmc5883l_mag_data_t;

typedef struct {
  qmc5883l_mode_t  mode;
  qmc5883l_odr_t   odr;
  qmc5883l_range_t range;
  qmc5883l_osr_t   osr;
} qmc5883l_config_t;

typedef struct qmc5883l_handle {
  star_bus_manager_t* manager;
  const char*         bus_name;
  uint8_t             i2c_addr;
  qmc5883l_config_t   config;
  error_handler_t     error_handler;
  bool                initialized;
  float               sensitivity;
} qmc5883l_handle_t;

/* --- Core Functions --- */

esp_err_t star_sensor_qmc5883l_init(qmc5883l_handle_t*       handle,
                                    star_bus_manager_t*      manager,
                                    const char*              bus_name,
                                    const qmc5883l_config_t* config);

esp_err_t star_sensor_qmc5883l_deinit(qmc5883l_handle_t* handle);

esp_err_t star_sensor_qmc5883l_reset(const qmc5883l_handle_t* handle);

/* --- Measurement Functions --- */

esp_err_t star_sensor_qmc5883l_read_raw(const qmc5883l_handle_t* handle,
                                        qmc5883l_raw_data_t*     data);

esp_err_t star_sensor_qmc5883l_read_mag(const qmc5883l_handle_t* handle,
                                        qmc5883l_mag_data_t*     data);

esp_err_t star_sensor_qmc5883l_read_heading(const qmc5883l_handle_t* handle, float* heading);

esp_err_t star_sensor_qmc5883l_read_temperature(const qmc5883l_handle_t* handle, float* temp_c);

/* --- Configuration Functions --- */

esp_err_t star_sensor_qmc5883l_set_mode(qmc5883l_handle_t* handle, qmc5883l_mode_t mode);

esp_err_t star_sensor_qmc5883l_set_odr(qmc5883l_handle_t* handle, qmc5883l_odr_t odr);

esp_err_t star_sensor_qmc5883l_set_range(qmc5883l_handle_t* handle, qmc5883l_range_t range);

/* --- Status Functions --- */

esp_err_t star_sensor_qmc5883l_is_data_ready(const qmc5883l_handle_t* handle, bool* ready);

esp_err_t star_sensor_qmc5883l_check_overflow(const qmc5883l_handle_t* handle, bool* overflow);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_QMC5883L_H */
