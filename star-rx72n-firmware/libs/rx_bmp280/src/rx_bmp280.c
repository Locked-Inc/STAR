/* libs/rx_bmp280/src/rx_bmp280.c */

/**
 * @file rx_bmp280.c
 * @brief BMP280 Digital Barometric Pressure and Temperature Sensor Driver Implementation
 *
 * @details
 * # Overview
 *
 * Complete driver implementation for the Bosch BMP280 barometric pressure
 * sensor connected via I2C on RIIC1. Implements the Bosch integer-only
 * compensation algorithm (datasheet section 4.2.3) for accurate temperature
 * and pressure calculation without floating-point operations.
 *
 * # Integer Compensation Algorithm
 *
 * The Bosch compensation formulas use 64-bit integer arithmetic for pressure
 * to maintain precision across the full operating range. The intermediate
 * variable t_fine is computed from the temperature ADC value and reused
 * in the pressure formula for temperature compensation.
 *
 * Formula source: BMP280 datasheet v1.19, section 4.2.3
 * "Compensation formulas in double precision floating point"
 * (Integer version provided in appendix A)
 *
 * # NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. No goto | [PASS] | Structured if/while only |
 * | 2. Bounded loops | [PASS] | Status poll bounded by k_bmp280_poll_max |
 * | 3. No dynamic memory | [PASS] | All buffers on stack |
 * | 4. Short functions | [PASS] | Max function ~40 lines |
 * | 5. Assertions | [PASS] | 2+ checks per function |
 * | 6. Data scope | [PASS] | Locals at point of use |
 * | 7. Check returns | [PASS] | All I2C returns validated |
 * | 8. Limit preprocessor | [PASS] | C23 typed enums only |
 * | 9. Pointer restrictions | [WARN] | bus_manager function pointers (DIP) |
 * | 10. Compile warnings | [PASS] | -Wall -Wextra -Werror |
 *
 * @see rx_bmp280.h Public API
 * @see rx_bmp280_regs.h Register and constant definitions
 *
 * @author STAR Team
 * @date 2026-03-04
 * @since Version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "rx_bmp280.h"

#include <stdint.h>

#include "rx_bmp280_regs.h"
#include "rx_bus_i2c.h"
#include "rx_check.h"
#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * Module-level Constants
 * =============================================================================
 */

/**
 * @enum bmp280_le16_idx_t
 * @brief Byte indices for little-endian 16-bit pair (LSB first)
 *
 * @details
 * Index constants for accessing LSB and MSB within a 2-byte little-endian pair.
 * Used by internal_parse_u16_le() and internal_parse_s16_le().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_le16_lsb_idx = 0, /**< Index of least-significant byte in a 2-byte LE pair */
  k_bmp280_le16_msb_idx = 1, /**< Index of most-significant byte in a 2-byte LE pair */
} bmp280_le16_idx_t;

/**
 * @enum bmp280_adc20_idx_t
 * @brief Byte indices for 3-byte 20-bit ADC buffer (MSB first)
 *
 * @details
 * Index constants for accessing MSB, LSB, and XLSB within a 3-byte ADC buffer.
 * Used by internal_assemble_adc20().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_adc20_msb_idx  = 0, /**< Index of MSB byte in 3-byte ADC buffer */
  k_bmp280_adc20_lsb_idx  = 1, /**< Index of LSB byte in 3-byte ADC buffer */
  k_bmp280_adc20_xlsb_idx = 2, /**< Index of XLSB byte in 3-byte ADC buffer */
} bmp280_adc20_idx_t;

/**
 * @enum bmp280_adc20_range_t
 * @brief Valid range for 20-bit ADC values assembled by internal_assemble_adc20()
 *
 * @details
 * The BMP280 ADC output is a 20-bit non-negative integer.
 * Maximum value is 2^20 - 1 = 0xFFFFF = 1048575.
 * Used as a postcondition bound in internal_assemble_adc20().
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_bmp280_adc_20bit_max = 0xFFFFFU, /**< Maximum valid 20-bit ADC value (2^20 - 1 = 1048575) */
} bmp280_adc20_range_t;

/**
 * @enum bmp280_press_range_pa_t
 * @brief BMP280 valid pressure range in base Pascal units
 *
 * @details
 * Valid pressure limits per the BMP280 datasheet operating specifications:
 * - 300 hPa  = 30000 Pa  (minimum, e.g. high altitude)
 * - 1100 hPa = 110000 Pa (maximum, e.g. sea level)
 *
 * These base-Pa values are multiplied by 256 for the fixed-point Pa*256 format
 * used by the Bosch integer compensation formula output.
 * See bmp280_press_range_t for the Pa*256 scaled limits.
 *
 * @see bmp280_press_range_t Pa*256 scaled limits used in postcondition check
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_bmp280_press_min_pa = 30000U,  /**< Minimum valid pressure: 300 hPa in Pa */
  k_bmp280_press_max_pa = 110000U, /**< Maximum valid pressure: 1100 hPa in Pa */
} bmp280_press_range_pa_t;

/**
 * @enum bmp280_temp_range_t
 * @brief BMP280 temperature output range limits for postcondition validation
 *
 * @details
 * Valid temperature output range per the BMP280 datasheet operating specifications:
 * - Temperature: -40.00 degC to +85.00 degC (in centi-degC units)
 *
 * @see bmp280_press_range_t Pressure output range limits
 * @see rx_bmp280_read() Applies these range checks after compensation
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_bmp280_temp_min_cdegc_impl = -4000, /**< Minimum valid temperature: -40.00 degC in centi-degC */
  k_bmp280_temp_max_cdegc_impl = 8500,  /**< Maximum valid temperature: +85.00 degC in centi-degC */
} bmp280_temp_range_t;

/**
 * @enum bmp280_press_range_t
 * @brief BMP280 pressure output range limits for postcondition validation
 *
 * @details
 * Valid pressure output range per the BMP280 datasheet operating specifications:
 * - Pressure: 300 hPa to 1100 hPa (in Pa*256 fixed-point units)
 *   Derivation: k_bmp280_press_min_pa * 256 = 30000 * 256 = 7680000
 *               k_bmp280_press_max_pa * 256 = 110000 * 256 = 28160000
 *
 * @see bmp280_temp_range_t Temperature output range limits
 * @see bmp280_press_range_pa_t Base Pa constants from which Pa*256 values are derived
 * @see rx_bmp280_read() Applies these range checks after compensation
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_bmp280_press_min_pa_256 =
    7680000U, /**< Minimum valid pressure: 300 hPa * 256 (k_bmp280_press_min_pa * 256) */
  k_bmp280_press_max_pa_256 =
    28160000U, /**< Maximum valid pressure: 1100 hPa * 256 (k_bmp280_press_max_pa * 256) */
} bmp280_press_range_t;

/* =============================================================================
 * Module-Static State
 * =============================================================================
 */

/** @brief Log tag for this module
 *  @note Constant; never modified after initialization
 *  @since Version 1.0.0
 */
static const char* const s_tag = "BMP280";

/** @brief Bus manager pointer stored during rx_bmp280_init()
 *  @note Set once in rx_bmp280_init(); NULL before init and on error
 *  @see rx_bmp280_init() Sets this pointer
 *  @since Version 1.0.0
 */
static rx_bus_manager_t* s_manager = NULL;

/** @brief Factory calibration coefficients read from BMP280 OTP during init
 *  @note Read-only after rx_bmp280_init(); access outside init is read-only
 *  @invariant s_initialized == true => s_calib contains valid OTP factory calibration coefficients
 *  @see rx_bmp280_init() Reads 24-byte OTP block into this struct
 *  @see BMP280 datasheet section 4.2.2 for coefficient descriptions
 *  @since Version 1.0.0
 */
static bmp280_calib_t s_calib;

/** @brief Guard flag: true after successful rx_bmp280_init()
 *  @note Only set to true in rx_bmp280_init() after all init steps succeed
 *  @invariant Once true, s_calib and s_manager are valid and non-NULL
 *  @since Version 1.0.0
 */
static bool s_initialized = false;

/** @brief I2C bus name for BMP280 (RIIC1, device addr 0x76, registered as "i2c1_baro" in main.c)
 *  @note Must match the bus name registered via rx_bus_manager_add_bus() in main.c
 *  @see main.c Bus registration with name "i2c1_baro"
 *  @since Version 1.0.0
 */
static const char* const s_bus_name = "i2c1_baro";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

/** @brief Write a single byte to a BMP280 register via I2C */
static rx_err_t internal_write_reg(uint8_t reg, uint8_t val);
/** @brief Burst-read consecutive registers from BMP280 via I2C write-read */
static rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len);
/** @brief Apply Bosch integer temperature compensation formula (returns centi-degC, sets t_fine) */
static int32_t internal_compensate_temp(int32_t adc_T, int32_t* t_fine_out);
/** @brief Apply Bosch integer pressure compensation formula (returns Pa*256 in press_out) */
static rx_err_t internal_compensate_pressure(int32_t adc_P, int32_t t_fine, uint32_t* press_out);
/** @brief Assemble an unsigned 16-bit little-endian value from two consecutive bytes */
static inline uint16_t internal_parse_u16_le(const uint8_t* buf);
/** @brief Assemble a signed 16-bit little-endian value from two consecutive bytes */
static inline int16_t internal_parse_s16_le(const uint8_t* buf);
/** @brief Assemble a 20-bit signed ADC value from a 3-byte MSB/LSB/XLSB buffer */
static inline int32_t internal_assemble_adc20(const uint8_t* buf);
/** @brief Read ADC registers, assemble values, apply compensation, and validate range */
static rx_err_t internal_read_and_compensate_adc(bmp280_data_t* out);

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BMP280 register via I2C
 *
 * @details
 * Sends a 2-byte I2C write transaction [reg, val] to the BMP280 at
 * device address 0x76 (embedded in the "i2c1_baro" bus configuration).
 *
 * @param[in] reg Register address (from bmp280_reg_t)
 * @param[in] val Byte value to write to the register
 *
 * @return rx_err_t I2C transaction result
 * @retval k_rx_ok Register written successfully
 * @retval k_rx_err_nack Device did not acknowledge
 *
 * @pre s_manager non-NULL (set by rx_bmp280_init)
 * @pre "i2c1_baro" bus initialized
 * @post Register contains val on k_rx_ok
 * @post Bus transaction completes before function returns
 *
 * @note Not thread-safe
 * @see rx_bus_i2c_write()
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_write_reg(uint8_t reg, uint8_t val)
{
  RX_ASSERT(s_manager != NULL, "s_manager must be non-NULL before write");
  RX_ASSERT(s_bus_name != NULL, "s_bus_name must be non-NULL");
  uint8_t buf[k_bmp280_write_buf_size];
  buf[k_bmp280_write_idx_reg] = reg;
  buf[k_bmp280_write_idx_val] = val;
  return rx_bus_i2c_write(s_manager, s_bus_name, buf, k_bmp280_write_buf_size);
}

/**
 * @brief Burst-read consecutive registers from BMP280 via I2C write-read
 *
 * @details
 * Issues a combined I2C write-read transaction: write starting register
 * address, then read len bytes. BMP280 auto-increments register address.
 *
 * @param[in]  reg Starting register address
 * @param[out] buf Buffer with capacity >= len bytes
 * @param[in]  len Number of bytes to read
 *
 * @return rx_err_t I2C transaction result
 * @retval k_rx_ok len bytes read into buf
 * @retval k_rx_err_nack Device not responding
 *
 * @pre s_manager non-NULL
 * @pre "i2c1_baro" bus initialized in s_manager
 * @pre buf capacity >= len
 * @post buf[0..len-1] contain register data on k_rx_ok
 * @post Bus transaction completes before function returns
 *
 * @note Not thread-safe
 * @see rx_bus_i2c_write_read()
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len)
{
  RX_ASSERT(s_manager != NULL, "s_manager must be non-NULL before read");
  RX_ASSERT(buf != NULL, "buf must be non-NULL for read operation");
  RX_ASSERT(len > 0U, "len must be positive for read operation");
  return rx_bus_i2c_write_read(s_manager, s_bus_name, &reg, k_bmp280_read_cmd_size, buf, len);
}

/**
 * @brief Assemble an unsigned 16-bit little-endian value from two consecutive bytes
 *
 * @details
 * Reads buf[0] (LSB) and buf[1] (MSB) and combines them into a uint16_t value
 * using little-endian byte order (LSB first), matching BMP280 calibration register layout.
 *
 * @param[in] buf Pointer to at least 2 bytes; buf[0] is LSB, buf[1] is MSB
 *
 * @return uint16_t Assembled unsigned 16-bit value
 *
 * @pre buf non-NULL, capacity >= 2
 * @post Return value == (uint16_t)buf[0] | ((uint16_t)buf[1] << 8)
 * @post No side effects; buf and global state are not modified
 *
 * @note Inline; zero overhead after optimization
 * @see internal_parse_s16_le() Signed variant
 *
 * @since Version 1.0.0
 */
static inline uint16_t internal_parse_u16_le(const uint8_t* buf)
{
  RX_ASSERT(buf != NULL, "buf must not be NULL");
  RX_ASSERT(k_bmp280_byte_shift == 8U, "byte shift must be 8 for LE 16-bit assembly");
  static_assert(k_bmp280_byte_shift == 8U, "byte shift must be 8 for LE 16-bit assembly");
  return (uint16_t)((uint16_t)buf[k_bmp280_le16_lsb_idx] |
                    ((uint16_t)buf[k_bmp280_le16_msb_idx] << k_bmp280_byte_shift));
}

/**
 * @brief Assemble a signed 16-bit little-endian value from two consecutive bytes
 *
 * @details
 * Reads buf[0] (LSB) and buf[1] (MSB) and reinterprets the assembled uint16_t
 * as a two's complement int16_t value. Matches BMP280 signed calibration coefficient layout.
 *
 * @param[in] buf Pointer to at least 2 bytes; buf[0] is LSB, buf[1] is MSB
 *
 * @return int16_t Assembled signed 16-bit value
 *
 * @pre buf non-NULL, capacity >= 2
 * @post Return value correctly represents two's complement signed value
 * @post No side effects; buf and global state are not modified
 *
 * @note Inline; zero overhead after optimization
 * @see internal_parse_u16_le() Unsigned variant
 *
 * @since Version 1.0.0
 */
static inline int16_t internal_parse_s16_le(const uint8_t* buf)
{
  RX_ASSERT(buf != NULL, "buf must not be NULL");
  RX_ASSERT(sizeof(int16_t) == sizeof(uint16_t),
            "int16_t and uint16_t must be same size for safe reinterpret cast");
  static_assert(sizeof(int16_t) == sizeof(uint16_t),
                "int16_t and uint16_t must be same size for safe reinterpret cast");
  return (int16_t)internal_parse_u16_le(buf);
}

/**
 * @brief Assemble a 20-bit signed ADC value from a 3-byte MSB/LSB/XLSB buffer
 *
 * @details
 * The BMP280 outputs pressure and temperature as 20-bit raw values stored in
 * three consecutive bytes with the following layout:
 * - buf[0] (MSB):  bits[19:12] of the ADC value
 * - buf[1] (LSB):  bits[11:4] of the ADC value
 * - buf[2] (XLSB): bits[3:0] of the ADC value in the upper nibble (bits [7:4])
 *
 * Assembly formula: (MSB << 12) | (LSB << 4) | (XLSB >> 4)
 *
 * @param[in] buf Pointer to 3-byte buffer: buf[0]=MSB, buf[1]=LSB, buf[2]=XLSB
 *
 * @return int32_t Assembled 20-bit ADC value (range [0, 0xFFFFF])
 *
 * @pre buf non-NULL, capacity >= 3
 * @post Return value is a valid 20-bit non-negative integer
 *
 * @note Inline; zero overhead after optimization
 * @see internal_compensate_temp() Consumes the assembled temperature ADC value
 * @see internal_compensate_pressure() Consumes the assembled pressure ADC value
 *
 * @since Version 1.0.0
 */
static inline int32_t internal_assemble_adc20(const uint8_t* buf)
{
  RX_ASSERT(buf != NULL, "buf must not be NULL");
  /* Assembly: (MSB << 12) | (LSB << 4) | (XLSB >> 4) produces 20-bit value */
  const int32_t result =
    (int32_t)(((uint32_t)buf[k_bmp280_adc20_msb_idx] << k_bmp280_shift_msb) |
              ((uint32_t)buf[k_bmp280_adc20_lsb_idx] << k_bmp280_shift_lsb_left) |
              ((uint32_t)buf[k_bmp280_adc20_xlsb_idx] >> k_bmp280_shift_xlsb_right));
  RX_ASSERT(result >= 0 && result <= (int32_t)k_bmp280_adc_20bit_max,
            "ADC20 result out of 20-bit range");
  return result;
}

/**
 * @enum bmp280_temp_comp_t
 * @brief Integer shift and scale constants for BMP280 temperature compensation formula
 *
 * @details
 * Constants used exclusively by internal_compensate_temp(). Placed at module scope
 * to avoid -Werror=unused-local-typedefs when the GNURX cross-compiler sees the
 * typedef name unused inside the function body.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_temp_shift_adc_3 = 3,   /**< adc_T right shift for T1 subtraction step */
  k_temp_shift_t1_1  = 1,   /**< dig_T1 left shift in first var1 step */
  k_temp_shift_11    = 11,  /**< Right shift for var1 final step */
  k_temp_shift_adc_4 = 4,   /**< adc_T right shift for T1 comparison steps */
  k_temp_shift_12    = 12,  /**< Right shift for squared difference */
  k_temp_shift_14    = 14,  /**< Right shift for var2 final step */
  k_temp_fine_scale  = 5,   /**< Scale factor in fine-to-output conversion */
  k_temp_round_add   = 128, /**< Rounding constant in fine-to-output conversion */
  k_temp_shift_out   = 8,   /**< Right shift to produce 0.01 degC output */
} bmp280_temp_comp_t;

/**
 * @brief Apply Bosch integer temperature compensation formula
 *
 * @details
 * Implements the Bosch BMP280 integer temperature compensation algorithm
 * verbatim from datasheet v1.19 appendix A. Uses trimming parameters
 * from s_calib and computes the fine temperature value t_fine which
 * is required for pressure compensation.
 *
 * Algorithm:
 * @code
 * var1 = (((adc_T >> 3) - (dig_T1 << 1)) * dig_T2) >> 11
 * var2 = ((((adc_T >> 4) - dig_T1) * ((adc_T >> 4) - dig_T1)) >> 12) * dig_T3) >> 14
 * t_fine = var1 + var2
 * T = (t_fine * 5 + 128) >> 8    // Unit: centi-degrees (0.01 degC)
 * @endcode
 *
 * @param[in]  adc_T     Raw 20-bit temperature ADC value
 * @param[out] t_fine_out Intermediate fine temperature value (for pressure comp)
 *
 * @return int32_t Compensated temperature in 0.01 degC units (divide by 100 for degC)
 *
 * @pre adc_T is valid 20-bit ADC output from BMP280
 * @pre t_fine_out non-NULL
 * @post *t_fine_out set to intermediate temperature value
 * @post Return value in range [-4000, 8500] for valid operating range
 *
 * @note Pure integer arithmetic, no floating-point
 * @see BMP280 datasheet v1.19 appendix A, compensate_T_int32
 *
 * @since Version 1.0.0
 */
static int32_t internal_compensate_temp(int32_t adc_T, int32_t* t_fine_out)
{
  RX_ASSERT(t_fine_out != NULL, "t_fine_out must be non-NULL");
  RX_ASSERT(adc_T >= 0 && adc_T <= (int32_t)k_bmp280_adc_20bit_max,
            "adc_T must be a 20-bit ADC value");

  const int32_t var1 =
    ((((adc_T >> k_temp_shift_adc_3) - ((int32_t)s_calib.dig_T1 << k_temp_shift_t1_1))) *
     ((int32_t)s_calib.dig_T2)) >>
    k_temp_shift_11;
  const int32_t var2 = (((((adc_T >> k_temp_shift_adc_4) - ((int32_t)s_calib.dig_T1)) *
                          ((adc_T >> k_temp_shift_adc_4) - ((int32_t)s_calib.dig_T1))) >>
                         k_temp_shift_12) *
                        ((int32_t)s_calib.dig_T3)) >>
                       k_temp_shift_14;

  *t_fine_out = var1 + var2;

  return (*t_fine_out * k_temp_fine_scale + k_temp_round_add) >> k_temp_shift_out;
}

/**
 * @enum press_comp_constants_t
 * @brief Bosch BMP280 pressure compensation formula constants (datasheet appendix A)
 *
 * @details
 * Integer shift and scale constants used exclusively by internal_compensate_pressure()
 * and internal_finalize_pressure_q8(). Placed at module scope so both functions can
 * reference the same named constants without redundancy.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_press_t_offset = 128000,  /**< t_fine offset in pressure formula */
  k_press_p_scale  = 1048576, /**< ADC scaling constant */
  k_press_shift_31 = 31,      /**< Shift for 31-bit alignment */
  k_press_shift_47 = 47,      /**< Shift for 47-bit alignment */
  k_press_shift_8  = 8,       /**< Output shift for Q8 fixed-point Pa*256 */
  k_press_mul_3125 = 3125,    /**< Bosch formula scaling factor */
  k_press_shift_35 = 35,      /**< Shift for 35-bit intermediate */
  k_press_shift_25 = 25,      /**< Shift for 25-bit intermediate */
  k_press_shift_19 = 19,      /**< Shift for 19-bit intermediate */
  k_press_shift_13 = 13,      /**< Shift for P9 coefficient shift */
  k_press_shift_17 = 17,      /**< Shift for P5 coefficient step */
  k_press_shift_12 = 12,      /**< Right shift for dig_P3 step */
  k_press_shift_33 = 33,      /**< Divisor shift for var1 */
  k_press_shift_4  = 4,       /**< Shift for P7 addition */
} press_comp_constants_t;

/**
 * @brief Apply final Q8 fixed-point scaling and dig_P7 offset to pressure intermediate
 *
 * @details
 * Implements the last step of the Bosch BMP280 pressure compensation formula
 * (datasheet Section 4.2.3): right-shifts the accumulated intermediate value to
 * produce Pa*256 (Q8 fixed-point), then adds the dig_P7 offset term.
 *
 * Extracted from internal_compensate_pressure() to keep that function within
 * the 60-line NASA Power of 10 Rule 4 guideline.
 *
 * @param[in] p    Intermediate pressure before final scaling
 * @param[in] var1 Bosch P9 coefficient adjustment (already computed)
 * @param[in] var2 Bosch P8 coefficient adjustment (already computed)
 *
 * @return uint32_t Compensated pressure in Pa * 256 (Q8 fixed-point)
 *
 * @pre s_calib populated by internal_parse_calibration() (dig_P1 != 0)
 * @post Return value is pressure * 256 in Pascal units (Q8 format)
 *
 * @note Accesses s_calib.dig_P7 directly (module-level static)
 * @see internal_compensate_pressure() Sole caller
 *
 * @since Version 1.0.0
 */
static uint32_t internal_finalize_pressure_q8(int64_t p, int64_t var1, int64_t var2)
{
  RX_ASSERT(s_calib.dig_P1 != 0U, "s_calib must be initialized before finalizing pressure");
  RX_ASSERT(p >= 0, "p must be non-negative before final Q8 scaling");
  static_assert(sizeof(int64_t) == 8U, "int64_t must be 64-bit for overflow-safe pressure math");
  /* Bosch BMP280 datasheet formula (Section 4.2.3):
   * Step 1: shift (p + fine adjustments) right by 8 to get Q8 fixed-point Pa*256
   * Step 2: add dig_P7 offset (left-shifted 4) AFTER the right shift
   * Note: shifting the dig_P7 term together with the rest would incorrectly
   *       scale it by an additional factor of 256. */
  const int64_t p_scaled = (p + var1 + var2) >> k_press_shift_8;
  const int64_t dig_p7   = ((int64_t)s_calib.dig_P7) << k_press_shift_4;
  return (uint32_t)(p_scaled + dig_p7);
}

/**
 * @brief Apply Bosch integer pressure compensation formula
 *
 * @details
 * Implements the Bosch BMP280 integer pressure compensation algorithm
 * verbatim from datasheet v1.19 appendix A. Uses t_fine from temperature
 * compensation and trimming parameters from s_calib.
 *
 * Returns k_rx_err_invalid_state if the internal var1 divisor would be zero
 * (prevents division by zero as required by NASA Power of 10 Rule 7).
 *
 * Output unit: Pa * 256 (fixed-point Q8.0 format). Divide by 256.0 for Pa.
 *
 * @param[in]  adc_P     Raw 20-bit pressure ADC value
 * @param[in]  t_fine    Intermediate temperature from internal_compensate_temp()
 * @param[out] press_out Compensated pressure in Pa * 256 on success
 *
 * @return rx_err_t Operation result
 * @retval k_rx_ok Pressure computed, press_out written
 * @retval k_rx_err_invalid_state var1 == 0 (dig_P1 == 0, hardware fault)
 *
 * @pre adc_P is valid 20-bit ADC output from BMP280
 * @pre t_fine was computed by internal_compensate_temp() for same measurement
 * @pre press_out non-NULL
 * @post *press_out in range [77312*256, 281472*256] for 300-1100 hPa on success
 * @post Returns k_rx_err_invalid_state without modifying *press_out when var1 == 0
 *
 * @note Uses 64-bit arithmetic to prevent overflow at intermediate values
 * @see BMP280 datasheet v1.19 appendix A, compensate_P_int64
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_compensate_pressure(int32_t adc_P, int32_t t_fine, uint32_t* press_out)
{
  RX_ASSERT(s_calib.dig_P1 != 0U, "dig_P1 must be non-zero (factory calibration)");
  RX_ASSERT(adc_P >= 0, "adc_P must be non-negative (20-bit unsigned ADC value)");
  RX_ASSERT(press_out != NULL, "press_out must not be NULL");

  /* k_press_base_one must be int64_t (not enum) because the expression
   * (k_press_base_one << k_press_shift_47) requires a 48-bit value.
   * A 32-bit integer cannot hold (1 << 47); int64_t is required to
   * prevent undefined behavior from shifting beyond the type width.
   * C23 typed enums cannot have int64_t as underlying type on RX72N. */
  static const int64_t k_press_base_one = 1;

  int64_t var1 = ((int64_t)t_fine) - k_press_t_offset;
  int64_t var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
  var2 += (var1 * (int64_t)s_calib.dig_P5) << k_press_shift_17;
  var2 += ((int64_t)s_calib.dig_P4) << k_press_shift_35;
  var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> k_press_shift_8) +
         ((var1 * (int64_t)s_calib.dig_P2) << k_press_shift_12);
  var1 = (((k_press_base_one << k_press_shift_47) + var1)) * ((int64_t)s_calib.dig_P1) >>
         k_press_shift_33;

  /* Division by zero guard (NASA Power of 10 Rule 7) */
  if (var1 == 0) {
    rx_log_error(s_tag, "Pressure compensation: var1==0 (dig_P1 fault)");
    return k_rx_err_invalid_state;
  }

  int64_t p = (int64_t)k_press_p_scale - adc_P;
  p         = (((p << k_press_shift_31) - var2) * k_press_mul_3125) / var1;

  var1 = (((int64_t)s_calib.dig_P9) * (p >> k_press_shift_13) * (p >> k_press_shift_13)) >>
         k_press_shift_25;
  var2 = (((int64_t)s_calib.dig_P8) * p) >> k_press_shift_19;

  *press_out = internal_finalize_pressure_q8(p, var1, var2);
  return k_rx_ok;
}

/* =============================================================================
 * Calibration Parsing
 * =============================================================================
 */

/**
 * @brief Parse 24-byte OTP calibration block into s_calib coefficients
 *
 * @details
 * Parses the 24-byte factory-calibrated trimming parameters read from OTP
 * registers k_bmp280_reg_calib_start..k_bmp280_reg_calib_end (0x88-0x9F).
 * All coefficients are stored little-endian (LSB first) in the OTP block.
 *
 * @param[in] buf Pointer to k_bmp280_calib_byte_count bytes of OTP register data
 *
 * @pre buf non-NULL
 * @pre buf contains exactly k_bmp280_calib_byte_count bytes from OTP registers
 * @post s_calib.dig_T1..T3 and dig_P1..P9 populated from buf
 * @post Caller must verify dig_T1 and dig_P1 are non-zero before use
 *
 * @note dig_T1 and dig_P1 may still be zero if OTP is blank; caller validates
 * @see rx_bmp280_init() Sole caller; validates non-zero after this returns
 *
 * @since Version 1.0.0
 */
static void internal_parse_calibration(const uint8_t* buf)
{
  RX_ASSERT(buf != NULL, "calibration buffer must not be NULL");
  static_assert(k_bmp280_calib_p9_lsb + 2U == k_bmp280_calib_byte_count,
                "P9 coefficient must occupy the final two bytes of the calibration block");
  s_calib.dig_T1 = internal_parse_u16_le(&buf[k_bmp280_calib_t1_lsb]);
  s_calib.dig_T2 = internal_parse_s16_le(&buf[k_bmp280_calib_t2_lsb]);
  s_calib.dig_T3 = internal_parse_s16_le(&buf[k_bmp280_calib_t3_lsb]);
  s_calib.dig_P1 = internal_parse_u16_le(&buf[k_bmp280_calib_p1_lsb]);
  s_calib.dig_P2 = internal_parse_s16_le(&buf[k_bmp280_calib_p2_lsb]);
  s_calib.dig_P3 = internal_parse_s16_le(&buf[k_bmp280_calib_p3_lsb]);
  s_calib.dig_P4 = internal_parse_s16_le(&buf[k_bmp280_calib_p4_lsb]);
  s_calib.dig_P5 = internal_parse_s16_le(&buf[k_bmp280_calib_p5_lsb]);
  s_calib.dig_P6 = internal_parse_s16_le(&buf[k_bmp280_calib_p6_lsb]);
  s_calib.dig_P7 = internal_parse_s16_le(&buf[k_bmp280_calib_p7_lsb]);
  s_calib.dig_P8 = internal_parse_s16_le(&buf[k_bmp280_calib_p8_lsb]);
  s_calib.dig_P9 = internal_parse_s16_le(&buf[k_bmp280_calib_p9_lsb]);
  /* Post-condition: verify calibration data was populated (catches blank OTP early) */
  RX_ASSERT(s_calib.dig_T1 != 0U,
            "dig_T1 must be non-zero after calibration parse (OTP data valid)");
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BMP280 sensor and read factory calibration coefficients
 *
 * @details
 * Reads 24 bytes of factory-calibrated trimming parameters from OTP
 * registers k_bmp280_reg_calib_start..k_bmp280_reg_calib_end (0x88-0x9F) and parses them into s_calib. Then configures
 * the IIR filter by writing to the config register 0xF5.
 *
 * @param[in] manager Initialized bus manager with "i2c1_baro" registered
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Calibration coefficients loaded, filter configured
 * @retval k_rx_err_null_ptr manager is NULL
 * @retval k_rx_err_nack I2C NACK (device not found)
 *
 * @pre manager non-NULL, "i2c1_baro" registered and initialized
 * @pre BMP280 powered on RIIC1 bus
 * @post s_calib contains valid OTP coefficients
 * @post s_initialized == true on success
 *
 * @note Not thread-safe
 * @see rx_bmp280_read() For triggered measurements
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bmp280_init(rx_bus_manager_t* manager)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "Bus manager is NULL");
  RX_ASSERT(s_bus_name != NULL, "Bus name must not be NULL");

  s_manager     = manager;
  s_initialized = false;

  /* Verify chip ID before reading calibration (detects wrong device on bus) */
  uint8_t  chip_id = 0U;
  rx_err_t err = internal_read_regs((uint8_t)k_bmp280_reg_chip_id, &chip_id, k_bmp280_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Chip ID read failed");
    s_manager = NULL;
    return err;
  }
  if (chip_id != (uint8_t)k_bmp280_chip_id_expected) {
    rx_log_error_val(s_tag, "Unexpected chip ID", (uint32_t)chip_id);
    s_manager = NULL;
    return k_rx_err_invalid_state;
  }

  /* Read 24-byte factory calibration block from OTP (k_bmp280_reg_calib_start..k_bmp280_reg_calib_end) */
  uint8_t calib_buf[k_bmp280_calib_byte_count];
  err = internal_read_regs((uint8_t)k_bmp280_reg_calib_start, calib_buf, k_bmp280_calib_byte_count);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Calibration read failed");
    s_manager = NULL;
    return err;
  }

  /* Parse calibration coefficients (all little-endian: LSB first) */
  internal_parse_calibration(calib_buf);

  /* Postcondition: verify critical calibration coefficients are non-zero */
  if (s_calib.dig_T1 == 0U || s_calib.dig_P1 == 0U) {
    rx_log_error(s_tag, "Invalid calibration: dig_T1 or dig_P1 is zero");
    s_manager = NULL;
    return k_rx_err_invalid_state;
  }

  /* Write IIR filter configuration (filter=2) */
  err = internal_write_reg((uint8_t)k_bmp280_reg_config, (uint8_t)k_bmp280_config_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Config register write failed");
    s_manager = NULL;
    return err;
  }

  s_initialized = true;
  rx_log_info(s_tag, "BMP280 initialized, calibration loaded");

  return k_rx_ok;
}

/**
 * @brief Read ADC registers, assemble values, apply Bosch compensation, and validate range
 *
 * @details
 * Performs the ADC read + compensation + range-check portion of the measurement cycle,
 * after the forced-mode trigger and status poll have completed. Specifically:
 * 1. Read 6 bytes of ADC data from k_bmp280_reg_press_msb (0xF7)
 * 2. Assemble 20-bit pressure and temperature ADC values via internal_assemble_adc20()
 * 3. Apply Bosch integer temperature compensation (internal_compensate_temp())
 * 4. Apply Bosch integer pressure compensation (internal_compensate_pressure())
 * 5. Validate temperature in [-40.00, +85.00] degC (centi-degC units)
 * 6. Validate pressure in [300, 1100] hPa (Pa*256 units)
 *
 * @param[out] out Output structure populated with temp_centi_degc and press_pa_256
 *
 * @return rx_err_t Operation result
 * @retval k_rx_ok ADC data read and compensation applied; out populated with valid values
 * @retval k_rx_err_nack I2C communication failure during ADC register read
 * @retval k_rx_err_invalid_state var1 == 0 in pressure compensation, or output out of range
 *
 * @pre s_manager non-NULL (rx_bmp280_init succeeded)
 * @pre s_initialized == true
 * @pre out non-NULL
 * @pre Forced measurement complete (status poll cleared by caller)
 * @post out->temp_centi_degc in [-4000, 8500] on k_rx_ok
 * @post out->press_pa_256 in [7680000, 28160000] on k_rx_ok
 *
 * @note Not thread-safe
 * @see rx_bmp280_read() Caller that triggers measurement and polls status before calling this
 * @see internal_compensate_temp() Temperature compensation algorithm
 * @see internal_compensate_pressure() Pressure compensation algorithm
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_read_and_compensate_adc(bmp280_data_t* out)
{
  RX_ASSERT(out != NULL, "out must be non-NULL");
  RX_ASSERT(s_initialized, "BMP280 must be initialized before ADC read");

  /* Step 1: Read 6 bytes of ADC data: pressure[3] + temperature[3] */
  uint8_t        adc_buf[k_bmp280_adc_buf_size];
  const rx_err_t err =
    internal_read_regs((uint8_t)k_bmp280_reg_press_msb, adc_buf, k_bmp280_adc_buf_size);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "ADC data read failed");
    return err;
  }

  /* Step 2: Assemble 20-bit raw ADC values */
  const int32_t adc_P = internal_assemble_adc20(&adc_buf[k_bmp280_press_msb_idx]);
  const int32_t adc_T = internal_assemble_adc20(&adc_buf[k_bmp280_temp_msb_idx]);

  /* Step 3: Apply Bosch integer temperature compensation */
  int32_t t_fine       = 0;
  out->temp_centi_degc = internal_compensate_temp(adc_T, &t_fine);

  /* Step 4: Apply Bosch integer pressure compensation */
  const rx_err_t press_err = internal_compensate_pressure(adc_P, t_fine, &out->press_pa_256);
  if (press_err != k_rx_ok) {
    rx_log_error(s_tag, "Pressure compensation failed (var1==0)");
    return press_err;
  }

  /* Step 5: Postcondition range checks (BMP280 datasheet operating limits) */
  if (out->temp_centi_degc < (int32_t)k_bmp280_temp_min_cdegc_impl ||
      out->temp_centi_degc > (int32_t)k_bmp280_temp_max_cdegc_impl) {
    rx_log_error_val(s_tag, "Temperature out of range (centi-degC)", (int32_t)out->temp_centi_degc);
    return k_rx_err_invalid_state;
  }

  if (out->press_pa_256 < (uint32_t)k_bmp280_press_min_pa_256 ||
      out->press_pa_256 > (uint32_t)k_bmp280_press_max_pa_256) {
    rx_log_error_val(s_tag, "Pressure out of range (Pa*256)", out->press_pa_256);
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Trigger forced measurement and read compensated pressure and temperature
 *
 * @details
 * Executes the complete forced-mode measurement cycle:
 * 1. Trigger forced measurement (write ctrl_meas)
 * 2. Poll status register for completion (bounded loop)
 * 3. Read ADC data, apply compensation, validate range (internal_read_and_compensate_adc)
 *
 * @param[out] out Output structure for measurement results
 *
 * @return rx_err_t Measurement result
 * @retval k_rx_ok Measurement complete, out populated
 * @retval k_rx_err_null_ptr out is NULL
 * @retval k_rx_err_not_initialized init not called
 * @retval k_rx_err_timeout Status poll timeout
 * @retval k_rx_err_nack I2C communication failure
 * @retval k_rx_err_invalid_state Compensation or range check failed
 *
 * @pre rx_bmp280_init() succeeded
 * @pre out non-NULL
 * @post out populated with compensated measurement on k_rx_ok
 * @post Sensor in sleep mode (forced measurement complete)
 *
 * @note Not thread-safe
 * @see internal_read_and_compensate_adc() ADC read, compensation, and range validation
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bmp280_read(bmp280_data_t* out)
{
  RX_CHECK_NULL_PTR(out, s_tag, "Output data pointer is NULL");
  RX_ASSERT(s_initialized, "BMP280 not initialized");

  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Step 1: Write ctrl_meas to trigger forced measurement */
  rx_err_t err =
    internal_write_reg((uint8_t)k_bmp280_reg_ctrl_meas, (uint8_t)k_bmp280_ctrl_meas_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "ctrl_meas write failed");
    return err;
  }

  /* Step 2: Poll status register until measuring bit (bit 3) clears */
  uint32_t poll_count = 0;
  while (poll_count < (uint32_t)k_bmp280_poll_max) {
    uint8_t status_byte = 0;
    err = internal_read_regs((uint8_t)k_bmp280_reg_status, &status_byte, k_bmp280_single_byte);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Status register read failed");
      return err;
    }

    if ((status_byte & (uint8_t)k_bmp280_status_meas_mask) == 0U) {
      break; /* Measurement complete */
    }

    (void)tx_thread_sleep(k_bmp280_poll_sleep_ticks); /* Yield CPU while sensor is measuring */
    poll_count++;
  }

  if (poll_count >= (uint32_t)k_bmp280_poll_max) {
    rx_log_error(s_tag, "Status poll timeout");
    return k_rx_err_timeout;
  }

  /* Step 3: Read ADC, apply compensation, validate output range */
  return internal_read_and_compensate_adc(out);
}

#ifdef TESTING
/**
 * @brief Reset BMP280 driver static state for unit test isolation
 *
 * @details
 * Clears s_initialized to false and zeroes the calibration struct so
 * each test starts from the same known state regardless of execution
 * order. Available only in test builds (TESTING defined by CMakeLists.txt
 * for the test target).
 *
 * @pre Called from test setUp() before each test
 * @post s_initialized == false; s_calib zeroed
 *
 * @note Test-only helper; not compiled into production firmware
 * @see setUp() in test_rx_bmp280.c Calls this function
 *
 * @since Version 1.0.0
 */
void rx_bmp280_test_reset_state(void)
{
  RX_ASSERT(true, "rx_bmp280_test_reset_state: test-only function");
  s_initialized                   = false;
  const bmp280_calib_t zero_calib = {0};
  s_calib                         = zero_calib;
}
#endif /* TESTING */
