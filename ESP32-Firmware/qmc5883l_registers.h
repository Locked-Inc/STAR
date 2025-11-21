/**
 * QMC5883L 3-Axis Magnetometer Register Definitions
 *
 * Complete register map and bit field definitions for safe driver development
 * Suitable for ESP32-IDF C development
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DEVICE CONFIGURATION
 * ============================================================================= */

#define QMC5883L_I2C_ADDRESS        0x0D      /**< 7-bit I2C address */
#define QMC5883L_I2C_WRITE_ADDR     0x1A      /**< 8-bit write address (0x0D << 1) */
#define QMC5883L_I2C_READ_ADDR      0x1B      /**< 8-bit read address (0x0D << 1 | 1) */
#define QMC5883L_EXPECTED_CHIP_ID   0xFF      /**< Chip ID register value */

/* =============================================================================
 * REGISTER ADDRESS MAP
 * ============================================================================= */

/**
 * Output Data Registers (Read-Only)
 * All values are 16-bit signed integers in little-endian (LSB first) format
 */
#define QMC5883L_REG_X_OUT_LSB      0x00      /**< X-axis data, low byte */
#define QMC5883L_REG_X_OUT_MSB      0x01      /**< X-axis data, high byte (includes sign) */
#define QMC5883L_REG_Y_OUT_LSB      0x02      /**< Y-axis data, low byte */
#define QMC5883L_REG_Y_OUT_MSB      0x03      /**< Y-axis data, high byte (includes sign) */
#define QMC5883L_REG_Z_OUT_LSB      0x04      /**< Z-axis data, low byte */
#define QMC5883L_REG_Z_OUT_MSB      0x05      /**< Z-axis data, high byte (includes sign) */

/**
 * Status Register (0x06)
 * Bit[0]: DRDY - Data Ready flag
 * Bit[1]: OVL  - Overflow flag
 * Bit[2]: DRDY_EV - Data Ready Event
 * Bit[7:3]: Reserved (always 0)
 */
#define QMC5883L_REG_STATUS         0x06      /**< Status register */
#define QMC5883L_STATUS_DRDY        0x01      /**< Data Ready bit mask */
#define QMC5883L_STATUS_OVL         0x02      /**< Overflow bit mask */
#define QMC5883L_STATUS_DRDY_EV     0x04      /**< Data Ready Event bit mask */

/**
 * Temperature Registers (Optional, Read-Only)
 * Some QMC5883L variants include temperature sensing
 */
#define QMC5883L_REG_TEMP_LSB       0x07      /**< Temperature data, low byte */
#define QMC5883L_REG_TEMP_MSB       0x08      /**< Temperature data, high byte */

/**
 * Control Register 1 (0x09) - Mode, ODR, Range, and Oversampling
 * Bit[1:0]: MODE[1:0]   - Operation mode
 * Bit[3:2]: ODR[1:0]    - Output Data Rate
 * Bit[5:4]: RNG[1:0]    - Field range / sensitivity
 * Bit[7:6]: OSR[1:0]    - Over-Sample Rate
 */
#define QMC5883L_REG_CONTROL1       0x09      /**< Control register 1 */
#define QMC5883L_CTRL1_DEFAULT      0x1D      /**< Default value after reset */

/* Control Register 1 - Mode bits [1:0] */
#define QMC5883L_MODE_STANDBY       0x00      /**< Standby mode (power off, retains data) */
#define QMC5883L_MODE_CONTINUOUS    0x01      /**< Continuous measurement mode */
#define QMC5883L_MODE_SINGLE        0x02      /**< Single measurement mode */
#define QMC5883L_MODE_RESERVED      0x03      /**< Reserved - do not use */
#define QMC5883L_MODE_MASK          0x03      /**< Mode field mask */
#define QMC5883L_MODE_SHIFT         0         /**< Mode field bit shift */

/* Control Register 1 - Output Data Rate bits [3:2] */
#define QMC5883L_ODR_10HZ           0x00      /**< 10 Hz output rate */
#define QMC5883L_ODR_50HZ           0x01      /**< 50 Hz output rate */
#define QMC5883L_ODR_100HZ          0x02      /**< 100 Hz output rate */
#define QMC5883L_ODR_200HZ          0x03      /**< 200 Hz output rate */
#define QMC5883L_ODR_MASK           0x0C      /**< ODR field mask */
#define QMC5883L_ODR_SHIFT          2         /**< ODR field bit shift */

/* Control Register 1 - Range/Sensitivity bits [5:4] */
#define QMC5883L_RNG_2G             0x00      /**< ±2 Gauss range (~1 mG/LSB) - max sensitivity */
#define QMC5883L_RNG_8G             0x01      /**< ±8 Gauss range (~4 mG/LSB) */
#define QMC5883L_RNG_MASK           0x30      /**< Range field mask */
#define QMC5883L_RNG_SHIFT          4         /**< Range field bit shift */

/* Control Register 1 - Over-Sample Rate bits [7:6] */
#define QMC5883L_OSR_512            0x00      /**< OSR=512 (lowest noise, longest measurement) */
#define QMC5883L_OSR_256            0x01      /**< OSR=256 */
#define QMC5883L_OSR_128            0x02      /**< OSR=128 */
#define QMC5883L_OSR_64             0x03      /**< OSR=64 (highest speed, highest noise) */
#define QMC5883L_OSR_MASK           0xC0      /**< OSR field mask */
#define QMC5883L_OSR_SHIFT          6         /**< OSR field bit shift */

/**
 * Control Register 2 (0x0A) - Advanced Settings
 * Bit[0]: INT_ENB - Interrupt enable (DRDY pin output)
 * Bit[6]: ROL_PNT - Register pointer auto-roll-over
 * Bit[7]: SOFT_RST - Software reset (self-clearing)
 */
#define QMC5883L_REG_CONTROL2       0x0A      /**< Control register 2 */

#define QMC5883L_CTRL2_INT_ENB      0x01      /**< Interrupt enable bit mask */
#define QMC5883L_CTRL2_INT_ENB_SHIFT 0        /**< Interrupt enable bit shift */
#define QMC5883L_CTRL2_INT_ENABLED  0x01      /**< Enable DRDY output */
#define QMC5883L_CTRL2_INT_DISABLED 0x00      /**< Disable DRDY output */

#define QMC5883L_CTRL2_ROL_PNT      0x40      /**< Register pointer roll-over bit mask */
#define QMC5883L_CTRL2_ROL_PNT_SHIFT 6        /**< Pointer roll-over bit shift */
#define QMC5883L_CTRL2_ROLL_ENABLED 0x40      /**< Enable pointer auto-increment (RECOMMENDED) */
#define QMC5883L_CTRL2_ROLL_DISABLED 0x00     /**< Disable pointer auto-increment */

#define QMC5883L_CTRL2_SOFT_RST     0x80      /**< Software reset bit mask (self-clearing) */
#define QMC5883L_CTRL2_SOFT_RST_SHIFT 7       /**< Software reset bit shift */

/**
 * SET/RESET Period Register (0x0B)
 * Controls the frequency of magnetic field SET/RESET calibration
 * Bit[7:0]: FBR[7:0] - SET/RESET frequency (0x01 recommended)
 */
#define QMC5883L_REG_SET_RESET      0x0B      /**< SET/RESET period register */
#define QMC5883L_SET_RESET_DEFAULT  0x01      /**< Recommended value */
#define QMC5883L_SET_RESET_DISABLED 0x00      /**< Not recommended (no calibration) */

/**
 * Chip ID Register (0x0D) - Read Only
 * Always returns 0xFF for QMC5883L devices
 */
#define QMC5883L_REG_CHIP_ID        0x0D      /**< Chip identification register */

/* =============================================================================
 * MEASUREMENT DATA STRUCTURE
 * ============================================================================= */

/**
 * Raw measurement data from sensor
 * All values are 16-bit signed integers (-32768 to +32767)
 */
typedef struct {
    int16_t x;              /**< X-axis magnetic field (raw ADC units) */
    int16_t y;              /**< Y-axis magnetic field (raw ADC units) */
    int16_t z;              /**< Z-axis magnetic field (raw ADC units) */
    uint8_t status;         /**< Status register value (DRDY, OVL flags) */
    bool drdy;              /**< Data Ready flag extracted from status */
    bool overflow;          /**< Overflow flag extracted from status */
} qmc5883l_raw_measurement_t;

/**
 * Calibrated measurement data in physical units
 */
typedef struct {
    float x_gauss;          /**< X-axis field in Gauss */
    float y_gauss;          /**< Y-axis field in Gauss */
    float z_gauss;          /**< Z-axis field in Gauss */
    float magnitude;        /**< Total field magnitude in Gauss */
    float heading_degrees;  /**< Compass heading in degrees (0-360) */
} qmc5883l_calibrated_measurement_t;

/* =============================================================================
 * CONFIGURATION ENUMERATIONS
 * ============================================================================= */

/**
 * Operation modes
 */
typedef enum {
    QMC5883L_MODE_STANDBY = QMC5883L_MODE_STANDBY,        /**< Standby (low power) */
    QMC5883L_MODE_CONTINUOUS = QMC5883L_MODE_CONTINUOUS,  /**< Continuous measurement */
    QMC5883L_MODE_SINGLE = QMC5883L_MODE_SINGLE,          /**< Single measurement */
} qmc5883l_mode_t;

/**
 * Output Data Rates
 */
typedef enum {
    QMC5883L_ODR_10HZ = QMC5883L_ODR_10HZ,      /**< 10 Hz (power-efficient) */
    QMC5883L_ODR_50HZ = QMC5883L_ODR_50HZ,      /**< 50 Hz */
    QMC5883L_ODR_100HZ = QMC5883L_ODR_100HZ,    /**< 100 Hz */
    QMC5883L_ODR_200HZ = QMC5883L_ODR_200HZ,    /**< 200 Hz (highest rate) */
} qmc5883l_odr_t;

/**
 * Magnetic field ranges and sensitivities
 */
typedef enum {
    QMC5883L_RNG_2G = QMC5883L_RNG_2G,      /**< ±2 Gauss (highest sensitivity: ~1 mG/LSB) */
    QMC5883L_RNG_8G = QMC5883L_RNG_8G,      /**< ±8 Gauss (lower sensitivity: ~4 mG/LSB) */
} qmc5883l_range_t;

/**
 * Over-sample rates
 */
typedef enum {
    QMC5883L_OSR_512 = QMC5883L_OSR_512,    /**< OSR=512 (lowest noise) */
    QMC5883L_OSR_256 = QMC5883L_OSR_256,    /**< OSR=256 */
    QMC5883L_OSR_128 = QMC5883L_OSR_128,    /**< OSR=128 */
    QMC5883L_OSR_64 = QMC5883L_OSR_64,      /**< OSR=64 (fastest response) */
} qmc5883l_osr_t;

/* =============================================================================
 * CONFIGURATION STRUCTURE
 * ============================================================================= */

/**
 * Complete device configuration
 * Use this structure to initialize the sensor with desired parameters
 */
typedef struct {
    qmc5883l_mode_t mode;           /**< Operation mode (standby/continuous/single) */
    qmc5883l_odr_t odr;             /**< Output data rate (10/50/100/200 Hz) */
    qmc5883l_range_t range;         /**< Magnetic field range (±2G or ±8G) */
    qmc5883l_osr_t osr;             /**< Over-sample rate (512/256/128/64) */
    bool enable_interrupt;          /**< Enable DRDY interrupt pin */
    bool enable_pointer_rollover;   /**< Enable register pointer auto-increment */
} qmc5883l_config_t;

/* =============================================================================
 * SENSITIVITY/CALIBRATION CONSTANTS
 * ============================================================================= */

/**
 * LSB sensitivity for each range (in milligauss per LSB)
 */
#define QMC5883L_SENS_2G_MG_PER_LSB  1.0f     /**< ±2G range: 1 mG/LSB */
#define QMC5883L_SENS_8G_MG_PER_LSB  4.0f     /**< ±8G range: 4 mG/LSB */

/**
 * Get sensitivity in mG/LSB for given range
 */
static inline float qmc5883l_get_sensitivity_mg_lsb(qmc5883l_range_t range)
{
    return (range == QMC5883L_RNG_2G) ?
           QMC5883L_SENS_2G_MG_PER_LSB :
           QMC5883L_SENS_8G_MG_PER_LSB;
}

/**
 * Get sensitivity in Gauss/LSB for given range
 */
static inline float qmc5883l_get_sensitivity_g_lsb(qmc5883l_range_t range)
{
    return qmc5883l_get_sensitivity_mg_lsb(range) / 1000.0f;
}

/**
 * Calibration offset and scale structure
 */
typedef struct {
    int16_t x_offset;       /**< X-axis offset (units: raw ADC counts) */
    int16_t y_offset;       /**< Y-axis offset (units: raw ADC counts) */
    int16_t z_offset;       /**< Z-axis offset (units: raw ADC counts) */
    float x_scale;          /**< X-axis scale factor (dimensionless) */
    float y_scale;          /**< Y-axis scale factor (dimensionless) */
    float z_scale;          /**< Z-axis scale factor (dimensionless) */
} qmc5883l_calibration_t;

/* =============================================================================
 * INLINE UTILITY FUNCTIONS
 * ============================================================================= */

/**
 * Build Control Register 1 value from configuration
 * @param config: Configuration structure
 * @return Control Register 1 byte value
 */
static inline uint8_t qmc5883l_build_ctrl1(const qmc5883l_config_t *config)
{
    return ((uint8_t)config->osr << QMC5883L_OSR_SHIFT) |
           ((uint8_t)config->range << QMC5883L_RNG_SHIFT) |
           ((uint8_t)config->odr << QMC5883L_ODR_SHIFT) |
           ((uint8_t)config->mode << QMC5883L_MODE_SHIFT);
}

/**
 * Build Control Register 2 value from configuration
 * @param config: Configuration structure
 * @return Control Register 2 byte value
 */
static inline uint8_t qmc5883l_build_ctrl2(const qmc5883l_config_t *config)
{
    uint8_t value = 0;

    if (config->enable_interrupt) {
        value |= QMC5883L_CTRL2_INT_ENB;
    }

    if (config->enable_pointer_rollover) {
        value |= QMC5883L_CTRL2_ROL_PNT;
    }

    return value;
}

/**
 * Extract DRDY flag from status register
 * @param status: Status register value
 * @return true if data is ready
 */
static inline bool qmc5883l_status_get_drdy(uint8_t status)
{
    return (status & QMC5883L_STATUS_DRDY) != 0;
}

/**
 * Extract overflow flag from status register
 * @param status: Status register value
 * @return true if overflow occurred
 */
static inline bool qmc5883l_status_get_overflow(uint8_t status)
{
    return (status & QMC5883L_STATUS_OVL) != 0;
}

/**
 * Reconstruct signed 16-bit value from little-endian bytes
 * @param lsb: Low byte (register LSB)
 * @param msb: High byte (register MSB)
 * @return Signed 16-bit value
 */
static inline int16_t qmc5883l_combine_bytes(uint8_t lsb, uint8_t msb)
{
    /* Important: QMC5883L uses LITTLE-ENDIAN byte order
     * LSB first (bits 0-7), then MSB (bits 8-15)
     * C automatically handles two's complement sign extension */
    return (int16_t)(((uint16_t)msb << 8) | (uint16_t)lsb);
}

/**
 * Convert raw measurement to Gauss using range sensitivity
 * @param raw: Raw ADC value
 * @param range: Current range setting
 * @return Field value in Gauss
 */
static inline float qmc5883l_raw_to_gauss(int16_t raw, qmc5883l_range_t range)
{
    float sensitivity = qmc5883l_get_sensitivity_g_lsb(range);
    return (float)raw * sensitivity;
}

/**
 * Calculate magnetic field magnitude in Gauss
 * @param x: X-component in Gauss
 * @param y: Y-component in Gauss
 * @param z: Z-component in Gauss
 * @return Magnitude in Gauss
 */
static inline float qmc5883l_magnitude(float x, float y, float z)
{
    return __builtin_sqrtf(x*x + y*y + z*z);
}

/**
 * Calculate compass heading from X and Y components
 * @param x: X-component in Gauss
 * @param y: Y-component in Gauss
 * @return Heading in degrees (0-360)
 */
static inline float qmc5883l_heading(float x, float y)
{
    float heading = atan2f(y, x) * 180.0f / M_PI;
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    return heading;
}

/* =============================================================================
 * VALIDATION FUNCTIONS
 * ============================================================================= */

/**
 * Validate if a register address is safe to read
 * @param reg: Register address
 * @return true if register is valid for reading
 */
static inline bool qmc5883l_is_valid_read_register(uint8_t reg)
{
    return (reg <= 0x08) ||                    /* Data registers and temperature */
           ((reg >= 0x09) && (reg <= 0x0B)) || /* Control registers */
           (reg == 0x0D);                      /* Chip ID */
}

/**
 * Validate if a register address is safe to write
 * @param reg: Register address
 * @return true if register is writable
 */
static inline bool qmc5883l_is_valid_write_register(uint8_t reg)
{
    return (reg >= 0x09 && reg <= 0x0B);  /* Only control/calibration registers */
}

#ifdef __cplusplus
}
#endif

#endif /* QMC5883L_REGISTERS_H */
