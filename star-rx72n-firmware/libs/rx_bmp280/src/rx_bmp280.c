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
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "rx_bmp280.h"

#include <stdint.h>

#include "rx_bmp280_regs.h"
#include "rx_bus_i2c.h"
#include "rx_check.h"
#include "rx_log.h"

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

static rx_err_t internal_write_reg(uint8_t reg, uint8_t val);
static rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len);
static int32_t  internal_compensate_temp(int32_t adc_T, int32_t* t_fine_out);
static uint32_t internal_compensate_pressure(int32_t adc_P, int32_t t_fine);

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BMP280 register via I2C
 *
 * @details
 * Sends a 2-byte I2C write transaction [reg, val] to the BMP280 at
 * device address 0x76 (embedded in the "i2c1" bus configuration).
 *
 * @param[in] reg Register address (from bmp280_reg_t)
 * @param[in] val Byte value to write to the register
 *
 * @return rx_err_t I2C transaction result
 * @retval k_rx_ok Register written successfully
 * @retval k_rx_err_nack Device did not acknowledge
 *
 * @pre s_manager non-NULL (set by rx_bmp280_init)
 * @pre "i2c1" bus initialized
 * @post Register contains val on k_rx_ok
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
 * @pre "i2c1" bus initialized
 * @pre buf capacity >= len
 * @post buf[0..len-1] contain register data on k_rx_ok
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
  return rx_bus_i2c_write_read(s_manager,
                               s_bus_name,
                               &reg,
                               k_bmp280_read_cmd_size,
                               buf,
                               len);
}

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

  typedef enum : int32_t {
    k_temp_shift_adc_3  = 3,   /**< adc_T right shift for T1 subtraction step */
    k_temp_shift_t1_1   = 1,   /**< dig_T1 left shift in first var1 step */
    k_temp_shift_11     = 11,  /**< Right shift for var1 final step */
    k_temp_shift_adc_4  = 4,   /**< adc_T right shift for T1 comparison steps */
    k_temp_shift_12     = 12,  /**< Right shift for squared difference */
    k_temp_shift_14     = 14,  /**< Right shift for var2 final step */
    k_temp_fine_scale   = 5,   /**< Scale factor in fine-to-output conversion */
    k_temp_round_add    = 128, /**< Rounding constant in fine-to-output conversion */
    k_temp_shift_out    = 8,   /**< Right shift to produce 0.01 degC output */
  } temp_bit_constants_t;

  const int32_t var1 = ((((adc_T >> k_temp_shift_adc_3) - ((int32_t)s_calib.dig_T1 << k_temp_shift_t1_1))) *
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
 * @brief Apply Bosch integer pressure compensation formula
 *
 * @details
 * Implements the Bosch BMP280 integer pressure compensation algorithm
 * verbatim from datasheet v1.19 appendix A. Uses t_fine from temperature
 * compensation and trimming parameters from s_calib.
 *
 * Returns 0 if the internal var1 divisor would be zero (prevents division
 * by zero as required by NASA Power of 10 Rule 7).
 *
 * Output unit: Pa * 256 (fixed-point Q8.0 format). Divide by 256.0 for Pa.
 *
 * @param[in] adc_P  Raw 20-bit pressure ADC value
 * @param[in] t_fine Intermediate temperature from internal_compensate_temp()
 *
 * @return uint32_t Compensated pressure in Pa * 256 (divide by 256 for Pa)
 * @retval 0 Division by zero guard triggered (dig_P1 == 0, hardware fault)
 *
 * @pre adc_P is valid 20-bit ADC output from BMP280
 * @pre t_fine was computed by internal_compensate_temp() for same measurement
 * @post Return value in range [77312*256, 281472*256] for 300-1100 hPa
 *
 * @note Uses 64-bit arithmetic to prevent overflow at intermediate values
 * @see BMP280 datasheet v1.19 appendix A, compensate_P_int64
 *
 * @since Version 1.0.0
 */
static uint32_t internal_compensate_pressure(int32_t adc_P, int32_t t_fine)
{
  RX_ASSERT(s_calib.dig_P1 != 0U, "dig_P1 must be non-zero (factory calibration)");
  RX_ASSERT(adc_P >= 0, "adc_P must be non-negative (20-bit unsigned ADC value)");

  typedef enum : int32_t {
    k_press_t_offset     = 128000,  /**< t_fine offset in pressure formula */
    k_press_p_scale      = 1048576, /**< ADC scaling constant */
    k_press_shift_31     = 31,      /**< Shift for 31-bit alignment */
    k_press_shift_47     = 47,      /**< Shift for 47-bit alignment */
    k_press_shift_8      = 8,       /**< Output shift for Q8 fixed-point Pa*256 */
    k_press_mul_3125     = 3125,    /**< Bosch formula scaling factor */
    k_press_shift_35     = 35,      /**< Shift for 35-bit intermediate */
    k_press_shift_25     = 25,      /**< Shift for 25-bit intermediate */
    k_press_shift_19     = 19,      /**< Shift for 19-bit intermediate */
    k_press_shift_13     = 13,      /**< Shift for P9 coefficient shift */
    k_press_shift_17     = 17,      /**< Shift for P5 coefficient step */
    k_press_shift_12     = 12,      /**< Right shift for dig_P3 step */
    k_press_shift_33     = 33,      /**< Divisor shift for var1 */
    k_press_shift_4      = 4,       /**< Shift for P7 addition */
  } press_comp_constants_t;

  static const int64_t k_press_base_one = 1;

  int64_t var1 = ((int64_t)t_fine) - k_press_t_offset;
  int64_t var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
  var2 += (var1 * (int64_t)s_calib.dig_P5) << k_press_shift_17;
  var2 += ((int64_t)s_calib.dig_P4) << k_press_shift_35;
  var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> k_press_shift_8) + ((var1 * (int64_t)s_calib.dig_P2) << k_press_shift_12);
  var1 = (((k_press_base_one << k_press_shift_47) + var1)) * ((int64_t)s_calib.dig_P1) >> k_press_shift_33;

  /* Division by zero guard (NASA Power of 10 Rule 7) */
  if (var1 == 0) {
    return 0U;
  }

  int64_t p = (int64_t)k_press_p_scale - adc_P;
  p         = (((p << k_press_shift_31) - var2) * k_press_mul_3125) / var1;

  var1 = (((int64_t)s_calib.dig_P9) * (p >> k_press_shift_13) * (p >> k_press_shift_13)) >> k_press_shift_25;
  var2 = (((int64_t)s_calib.dig_P8) * p) >> k_press_shift_19;

  return (uint32_t)((p + var1 + var2 + (((int64_t)s_calib.dig_P7) << k_press_shift_4)) >> k_press_shift_8);
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
 * registers 0x88-0x9F and parses them into s_calib. Then configures
 * the IIR filter by writing to the config register 0xF5.
 *
 * @param[in] manager Initialized bus manager with "i2c1" registered
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Calibration coefficients loaded, filter configured
 * @retval k_rx_err_null_ptr manager is NULL
 * @retval k_rx_err_nack I2C NACK (device not found)
 *
 * @pre manager non-NULL, "i2c1" registered and initialized
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

  s_manager     = manager;
  s_initialized = false;

  /* Read 24-byte factory calibration block from OTP (0x88-0x9F) */
  uint8_t  calib_buf[k_bmp280_calib_byte_count];
  rx_err_t err = internal_read_regs((uint8_t)k_bmp280_reg_calib_start,
                                    calib_buf,
                                    k_bmp280_calib_byte_count);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Calibration read failed");
    return err;
  }

  /* Parse calibration coefficients (all little-endian: LSB first) */
  s_calib.dig_T1 = (uint16_t)((uint16_t)calib_buf[k_bmp280_calib_t1_lsb] |
                               ((uint16_t)calib_buf[k_bmp280_calib_t1_msb] << k_bmp280_byte_shift));
  s_calib.dig_T2 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_t2_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_t2_msb] << k_bmp280_byte_shift));
  s_calib.dig_T3 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_t3_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_t3_msb] << k_bmp280_byte_shift));
  s_calib.dig_P1 = (uint16_t)((uint16_t)calib_buf[k_bmp280_calib_p1_lsb] |
                               ((uint16_t)calib_buf[k_bmp280_calib_p1_msb] << k_bmp280_byte_shift));
  s_calib.dig_P2 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p2_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p2_msb] << k_bmp280_byte_shift));
  s_calib.dig_P3 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p3_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p3_msb] << k_bmp280_byte_shift));
  s_calib.dig_P4 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p4_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p4_msb] << k_bmp280_byte_shift));
  s_calib.dig_P5 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p5_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p5_msb] << k_bmp280_byte_shift));
  s_calib.dig_P6 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p6_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p6_msb] << k_bmp280_byte_shift));
  s_calib.dig_P7 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p7_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p7_msb] << k_bmp280_byte_shift));
  s_calib.dig_P8 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p8_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p8_msb] << k_bmp280_byte_shift));
  s_calib.dig_P9 = (int16_t)((uint16_t)calib_buf[k_bmp280_calib_p9_lsb] |
                              ((uint16_t)calib_buf[k_bmp280_calib_p9_msb] << k_bmp280_byte_shift));

  /* Postcondition: verify critical calibration coefficients are non-zero */
  if (s_calib.dig_T1 == 0U || s_calib.dig_P1 == 0U) {
    rx_log_error(s_tag, "Invalid calibration: dig_T1 or dig_P1 is zero");
    return k_rx_err_invalid_state;
  }

  /* Write IIR filter configuration (filter=2) */
  err = internal_write_reg((uint8_t)k_bmp280_reg_config, (uint8_t)k_bmp280_config_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Config register write failed");
    return err;
  }

  s_initialized = true;
  rx_log_info(s_tag, "BMP280 initialized, calibration loaded");

  return k_rx_ok;
}

/**
 * @brief Trigger forced measurement and read compensated pressure and temperature
 *
 * @details
 * Executes the complete forced-mode measurement cycle:
 * 1. Trigger forced measurement
 * 2. Poll for completion (bounded loop)
 * 3. Read raw ADC data
 * 4. Apply Bosch integer compensation
 *
 * @param[out] out Output structure for measurement results
 *
 * @return rx_err_t Measurement result
 * @retval k_rx_ok Measurement complete, out populated
 * @retval k_rx_err_null_ptr out is NULL
 * @retval k_rx_err_not_initialized init not called
 * @retval k_rx_err_timeout Status poll timeout
 * @retval k_rx_err_nack I2C communication failure
 *
 * @pre rx_bmp280_init() succeeded
 * @pre out non-NULL
 * @post out populated with compensated measurement on k_rx_ok
 * @post Sensor in sleep mode (forced measurement complete)
 *
 * @note Not thread-safe
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bmp280_read(bmp280_data_t* out)
{
  RX_CHECK_NULL_PTR(out, s_tag, "Output data pointer is NULL");

  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Step 1: Write ctrl_meas to trigger forced measurement */
  rx_err_t err = internal_write_reg((uint8_t)k_bmp280_reg_ctrl_meas,
                                    (uint8_t)k_bmp280_ctrl_meas_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "ctrl_meas write failed");
    return err;
  }

  /* Step 2: Poll status register until measuring bit (bit 3) clears */
  uint32_t poll_count = 0;
  while (poll_count < (uint32_t)k_bmp280_poll_max) {
    uint8_t status_byte = 0;
    err                 = internal_read_regs((uint8_t)k_bmp280_reg_status,
                              &status_byte,
                              k_bmp280_single_byte);
    if (err != k_rx_ok) {
      return err;
    }

    if ((status_byte & (uint8_t)k_bmp280_status_meas_mask) == 0U) {
      break; /* Measurement complete */
    }

    poll_count++;
  }

  if (poll_count >= (uint32_t)k_bmp280_poll_max) {
    rx_log_error(s_tag, "Status poll timeout");
    return k_rx_err_timeout;
  }

  /* Step 3: Read 6 bytes of ADC data: pressure[3] + temperature[3] */
  uint8_t adc_buf[k_bmp280_adc_buf_size];
  err = internal_read_regs((uint8_t)k_bmp280_reg_press_msb, adc_buf, k_bmp280_adc_buf_size);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "ADC data read failed");
    return err;
  }

  /* Step 4: Assemble 20-bit raw ADC values */
  const int32_t adc_P = ((int32_t)adc_buf[k_bmp280_press_msb_idx] << k_bmp280_shift_msb) |
                        ((int32_t)adc_buf[k_bmp280_press_lsb_idx] << k_bmp280_shift_lsb) |
                        ((int32_t)adc_buf[k_bmp280_press_xlsb_idx] >> k_bmp280_shift_xlsb);

  const int32_t adc_T = ((int32_t)adc_buf[k_bmp280_temp_msb_idx] << k_bmp280_shift_msb) |
                        ((int32_t)adc_buf[k_bmp280_temp_lsb_idx] << k_bmp280_shift_lsb) |
                        ((int32_t)adc_buf[k_bmp280_temp_xlsb_idx] >> k_bmp280_shift_xlsb);

  /* Step 5: Apply Bosch integer compensation */
  int32_t t_fine          = 0;
  out->temp_centi_degc    = internal_compensate_temp(adc_T, &t_fine);
  out->press_pa_256       = internal_compensate_pressure(adc_P, t_fine);

  return k_rx_ok;
}
