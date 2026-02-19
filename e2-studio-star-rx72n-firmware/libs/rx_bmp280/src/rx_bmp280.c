/* libs/rx_bmp280/src/rx_bmp280.c */

/**
 * @file rx_bmp280.c
 * @brief Bosch BMP280 Barometric Pressure Sensor Driver Implementation
 *
 * @details
 * Implements initialization and data reading for the BMP280 barometric
 * pressure and temperature sensor. Register access is via the SCI2 SIIC HAL
 * (sci_iic_write_read / sci_iic_write).
 *
 * Compensation formulas are the Bosch BMP280 32-bit integer algorithms from
 * datasheet BST-BMP280-DS001-19 Section 8.2. Temperature must always be
 * compensated before pressure because the compensation shares the intermediate
 * value t_fine stored in s_t_fine.
 *
 * @see rx_bmp280.h Public API header
 * @see rx_bmp280_constants.h Register addresses and constants
 * @see rx_sci_iic.h SCI SIIC bus driver
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdbool.h>
#include <stdint.h>

#include "rx_bmp280.h"
#include "rx_bmp280_constants.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_sci_iic.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Log tag for this module */
static const char* const s_tag = "BMP280";

/**
 * @enum bmp280_comp_temp_t
 * @brief Bit shift and scaling constants for Bosch temperature compensation formula
 *
 * @details
 * Named constants for all numeric literals in the BMP280 32-bit integer
 * temperature compensation formula (BMP280 datasheet Section 8.2).
 * Formula: T = (t_fine × 5 + 128) >> 8, where t_fine is in internal units.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_ct_adc_rshift1   = 3,   /**< adc_T >> 3 for var1 numerator */
  k_bmp280_ct_t1_lshift     = 1,   /**< dig_T1 << 1 for var1 subtraction */
  k_bmp280_ct_var1_rshift   = 11,  /**< var1 final right-shift */
  k_bmp280_ct_adc_rshift2   = 4,   /**< adc_T >> 4 for var2 term */
  k_bmp280_ct_inner_rshift  = 12,  /**< Inner squared-term right-shift for var2 */
  k_bmp280_ct_var2_rshift   = 14,  /**< var2 final right-shift */
  k_bmp280_ct_fine_mult     = 5,   /**< Multiplier: T = t_fine × 5 / 256 → 0.01°C */
  k_bmp280_ct_round         = 128, /**< Rounding constant (2^7) before output shift */
  k_bmp280_ct_out_rshift    = 8,   /**< Output right-shift yielding 0.01°C units */
} bmp280_comp_temp_t;

/**
 * @enum bmp280_comp_press_shift_t
 * @brief Bit shift constants for Bosch pressure compensation formula (8-bit values)
 *
 * @details
 * Named bit-shift counts for the BMP280 32-bit integer pressure compensation
 * formula (BMP280 datasheet Section 8.2). These fit in uint8_t.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_cp_tfine_rshift  = 1,   /**< t_fine >> 1 for var1 */
  k_bmp280_cp_v1_rshift1    = 2,   /**< var1 >> 2 in squared inner term */
  k_bmp280_cp_inner_rshift  = 11,  /**< Inner squared term >> 11 */
  k_bmp280_cp_p5_lshift     = 1,   /**< P5 term left-shift */
  k_bmp280_cp_v2_rshift1    = 2,   /**< var2 >> 2 for final P4 summation */
  k_bmp280_cp_p4_lshift     = 16,  /**< P4 << 16 */
  k_bmp280_cp_p3_rshift1    = 2,   /**< Inner var1 >> 2 for P3 */
  k_bmp280_cp_p3_rshift2    = 13,  /**< P3 inner term >> 13 */
  k_bmp280_cp_p3_rshift3    = 3,   /**< P3 term >> 3 */
  k_bmp280_cp_p2_rshift     = 1,   /**< P2 combined term >> 1 */
  k_bmp280_cp_v1_rshift2    = 18,  /**< var1 final >> 18 */
  k_bmp280_cp_p1_rshift     = 15,  /**< P1 final >> 15 */
  k_bmp280_cp_p9_rshift1    = 3,   /**< p >> 3 in P9 term */
  k_bmp280_cp_p9_rshift2    = 13,  /**< Squared P9 term >> 13 */
  k_bmp280_cp_p9_rshift3    = 12,  /**< P9 outer term >> 12 */
  k_bmp280_cp_p8_rshift1    = 2,   /**< p >> 2 in P8 term */
  k_bmp280_cp_p8_rshift2    = 13,  /**< P8 term >> 13 */
  k_bmp280_cp_out_rshift    = 4,   /**< Final output right-shift */
} bmp280_comp_press_shift_t;

/**
 * @enum bmp280_comp_press_const_t
 * @brief Numeric constants for Bosch pressure compensation formula (32-bit values)
 *
 * @details
 * Numeric constants that appear in the BMP280 32-bit integer pressure
 * compensation formula and do not fit in uint8_t.
 *
 * @since 1.0.0
 */
typedef enum : uint32_t {
  k_bmp280_cp_var1_offset = 64000,     /**< var1 offset: t_fine/2 minus 64000 */
  k_bmp280_cp_p1_base     = 32768,     /**< P1 base: (32768 + var1) before multiply */
  k_bmp280_cp_adc_range   = 1048576,   /**< 2^20: numerator = adc_range - adc_P */
  k_bmp280_cp_scale       = 3125,      /**< Pressure scaling factor in Bosch formula */
  k_bmp280_cp_half_uint32 = 0x80000000, /**< 2^31: branch point for division order */
} bmp280_comp_press_const_t;

/**
 * @enum bmp280_calib_shifts_t
 * @brief Bit shifts for assembling 16-bit trim words from little-endian bytes
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_calib_byte_shift = 8, /**< Left-shift high byte by 8 to form 16-bit word */
} bmp280_calib_shifts_t;

/* =============================================================================
 * Internal Types
 * =============================================================================
 */

/**
 * @struct bmp280_trim_t
 * @brief BMP280 NVM calibration trim parameters
 *
 * @details
 * Stores the 12 trim words read from registers 0x88–0x9F during initialization.
 * T1 and P1 are unsigned; all others are signed. Used exclusively by the
 * internal compensation functions.
 *
 * @invariant All fields valid only after rx_bmp280_init() returns k_rx_ok
 *
 * @see internal_load_trim() Populates this struct from raw register bytes
 *
 * @since 1.0.0
 */
typedef struct {
  uint16_t dig_T1; /**< Unsigned temperature trim word 1 */
  int16_t  dig_T2; /**< Signed temperature trim word 2 */
  int16_t  dig_T3; /**< Signed temperature trim word 3 */
  uint16_t dig_P1; /**< Unsigned pressure trim word 1 */
  int16_t  dig_P2; /**< Signed pressure trim word 2 */
  int16_t  dig_P3; /**< Signed pressure trim word 3 */
  int16_t  dig_P4; /**< Signed pressure trim word 4 */
  int16_t  dig_P5; /**< Signed pressure trim word 5 */
  int16_t  dig_P6; /**< Signed pressure trim word 6 */
  int16_t  dig_P7; /**< Signed pressure trim word 7 */
  int16_t  dig_P8; /**< Signed pressure trim word 8 */
  int16_t  dig_P9; /**< Signed pressure trim word 9 */
} bmp280_trim_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @var s_config
 * @brief Saved configuration from last successful rx_bmp280_init() call
 *
 * @note Updated only on successful init.
 * @warning Do not access before rx_bmp280_init() returns k_rx_ok.
 *
 * @since 1.0.0
 */
static rx_bmp280_config_t s_config;

/**
 * @var s_initialized
 * @brief True if rx_bmp280_init() has completed successfully
 *
 * @note Guards rx_bmp280_read() against use before init.
 *
 * @since 1.0.0
 */
static bool s_initialized = false;

/**
 * @var s_trim
 * @brief NVM calibration trim parameters read during init
 *
 * @note Valid only after s_initialized == true.
 * @warning Must not be modified after init.
 *
 * @since 1.0.0
 */
static bmp280_trim_t s_trim;

/**
 * @var s_t_fine
 * @brief Fine temperature intermediate value shared between compensation functions
 *
 * @details
 * Set by internal_compensate_temperature() and consumed by
 * internal_compensate_pressure(). Temperature compensation MUST run before
 * pressure compensation each read cycle to keep s_t_fine current.
 *
 * @warning Access only within a single rx_bmp280_read() call (not thread-safe).
 *
 * @since 1.0.0
 */
static int32_t s_t_fine;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BMP280 register
 *
 * @param[in] reg   Register address to write
 * @param[in] value Byte value to write
 *
 * @return rx_err_t Status
 * @retval k_rx_ok          Write successful
 * @retval k_rx_err_nack      NACK received
 * @retval k_rx_err_timeout Polling timeout
 *
 * @pre sci_iic_init() called for s_config.channel
 * @post Register reg contains value on k_rx_ok
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t buf[2];

  buf[0] = reg;
  buf[1] = value;
  return sci_iic_write(s_config.channel, s_config.i2c_addr, buf, 2);
}

/**
 * @brief Read one or more bytes from a BMP280 register address
 *
 * @param[in]  reg Starting register address
 * @param[out] data Buffer to store read bytes
 * @param[in]  len  Number of bytes to read
 *
 * @return rx_err_t Status
 * @retval k_rx_ok          Read successful
 * @retval k_rx_err_nack      NACK received
 * @retval k_rx_err_timeout Polling timeout
 *
 * @pre data != NULL, len > 0
 * @pre sci_iic_init() called for s_config.channel
 * @post data[0..len-1] filled on k_rx_ok
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_read_reg(uint8_t reg, uint8_t* data, uint8_t len)
{
  return sci_iic_write_read(s_config.channel, s_config.i2c_addr, &reg, 1, data, len);
}

/**
 * @brief Assemble a little-endian uint16_t trim word from two bytes
 *
 * @param[in] lsb Low byte (from lower register address)
 * @param[in] msb High byte (from higher register address)
 *
 * @return Unsigned 16-bit trim word
 *
 * @pre No preconditions (pure function)
 * @post Return value = (uint16_t)((msb << 8) | lsb)
 *
 * @note Thread-safe (pure function).
 *
 * @since 1.0.0
 */
static uint16_t internal_u16_le(uint8_t lsb, uint8_t msb)
{
  return (uint16_t)((uint16_t)msb << k_bmp280_calib_byte_shift | (uint16_t)lsb);
}

/**
 * @brief Assemble a little-endian int16_t trim word from two bytes
 *
 * @param[in] lsb Low byte (from lower register address)
 * @param[in] msb High byte (from higher register address)
 *
 * @return Signed 16-bit trim word
 *
 * @pre No preconditions (pure function)
 * @post Return value = (int16_t)((msb << 8) | lsb)
 *
 * @note Thread-safe (pure function).
 *
 * @since 1.0.0
 */
static int16_t internal_s16_le(uint8_t lsb, uint8_t msb)
{
  return (int16_t)((uint16_t)msb << k_bmp280_calib_byte_shift | (uint16_t)lsb);
}

/**
 * @brief Unpack 24-byte calibration buffer into s_trim trim word struct
 *
 * @param[in] buf 24-byte buffer starting at register 0x88 (calib_base)
 *
 * @pre buf != NULL (caller checked)
 * @post s_trim.dig_T1..T3 and s_trim.dig_P1..P9 populated from buf
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static void internal_load_trim(const uint8_t* buf)
{
  s_trim.dig_T1 = internal_u16_le(buf[k_bmp280_calib_t1_lsb], buf[k_bmp280_calib_t1_msb]);
  s_trim.dig_T2 = internal_s16_le(buf[k_bmp280_calib_t2_lsb], buf[k_bmp280_calib_t2_msb]);
  s_trim.dig_T3 = internal_s16_le(buf[k_bmp280_calib_t3_lsb], buf[k_bmp280_calib_t3_msb]);
  s_trim.dig_P1 = internal_u16_le(buf[k_bmp280_calib_p1_lsb], buf[k_bmp280_calib_p1_msb]);
  s_trim.dig_P2 = internal_s16_le(buf[k_bmp280_calib_p2_lsb], buf[k_bmp280_calib_p2_msb]);
  s_trim.dig_P3 = internal_s16_le(buf[k_bmp280_calib_p3_lsb], buf[k_bmp280_calib_p3_msb]);
  s_trim.dig_P4 = internal_s16_le(buf[k_bmp280_calib_p4_lsb], buf[k_bmp280_calib_p4_msb]);
  s_trim.dig_P5 = internal_s16_le(buf[k_bmp280_calib_p5_lsb], buf[k_bmp280_calib_p5_msb]);
  s_trim.dig_P6 = internal_s16_le(buf[k_bmp280_calib_p6_lsb], buf[k_bmp280_calib_p6_msb]);
  s_trim.dig_P7 = internal_s16_le(buf[k_bmp280_calib_p7_lsb], buf[k_bmp280_calib_p7_msb]);
  s_trim.dig_P8 = internal_s16_le(buf[k_bmp280_calib_p8_lsb], buf[k_bmp280_calib_p8_msb]);
  s_trim.dig_P9 = internal_s16_le(buf[k_bmp280_calib_p9_lsb], buf[k_bmp280_calib_p9_msb]);
}

/**
 * @brief Apply Bosch 32-bit integer temperature compensation formula
 *
 * @details
 * Bosch BMP280 datasheet v1.19, Section 8.2 — 32-bit integer compensation.
 * Returns temperature in 0.01°C units. Example: return value 5123 = 51.23°C.
 * Side effect: sets s_t_fine for subsequent pressure compensation.
 *
 * @param[in] adc_t Raw 20-bit temperature ADC value (unsigned, left-aligned in int32_t)
 *
 * @return int32_t Compensated temperature in 0.01°C units
 *
 * @pre s_trim populated by internal_load_trim()
 * @post s_t_fine updated; used by subsequent internal_compensate_pressure() call
 *
 * @note Not thread-safe (updates s_t_fine).
 *
 * @since 1.0.0
 */
static int32_t internal_compensate_temperature(int32_t adc_t)
{
  int32_t var1;
  int32_t var2;

  var1 = ((((adc_t >> k_bmp280_ct_adc_rshift1) - ((int32_t)s_trim.dig_T1 << k_bmp280_ct_t1_lshift))) *
           ((int32_t)s_trim.dig_T2)) >> k_bmp280_ct_var1_rshift;

  var2 = (((((adc_t >> k_bmp280_ct_adc_rshift2) - ((int32_t)s_trim.dig_T1)) *
             ((adc_t >> k_bmp280_ct_adc_rshift2) - ((int32_t)s_trim.dig_T1))) >>
            k_bmp280_ct_inner_rshift) *
           ((int32_t)s_trim.dig_T3)) >> k_bmp280_ct_var2_rshift;

  s_t_fine = var1 + var2;

  return (s_t_fine * k_bmp280_ct_fine_mult + k_bmp280_ct_round) >> k_bmp280_ct_out_rshift;
}

/**
 * @brief Apply Bosch 32-bit integer pressure compensation formula
 *
 * @details
 * Bosch BMP280 datasheet v1.19, Section 8.2 — 32-bit integer compensation.
 * Returns pressure in Pa. Example: return value 96386 = 96386 Pa = 963.86 hPa.
 * MUST be called after internal_compensate_temperature() in the same read cycle
 * because it relies on s_t_fine.
 *
 * @param[in] adc_p Raw 20-bit pressure ADC value (unsigned, left-aligned in int32_t)
 *
 * @return uint32_t Compensated pressure in Pa. Returns 0 on divide-by-zero guard.
 *
 * @pre s_trim populated by internal_load_trim()
 * @pre s_t_fine set by internal_compensate_temperature() this cycle
 * @post Return value is pressure in Pa (0 only on degenerate trim parameters)
 *
 * @note Not thread-safe (reads s_t_fine).
 *
 * @since 1.0.0
 */
static uint32_t internal_compensate_pressure(int32_t adc_p)
{
  int32_t  var1;
  int32_t  var2;
  uint32_t p;

  var1 = (((int32_t)s_t_fine) >> k_bmp280_cp_tfine_rshift) - (int32_t)k_bmp280_cp_var1_offset;

  var2 = (((var1 >> k_bmp280_cp_v1_rshift1) * (var1 >> k_bmp280_cp_v1_rshift1)) >>
          k_bmp280_cp_inner_rshift) *
         ((int32_t)s_trim.dig_P6);
  var2 = var2 + ((var1 * ((int32_t)s_trim.dig_P5)) << k_bmp280_cp_p5_lshift);
  var2 = (var2 >> k_bmp280_cp_v2_rshift1) + (((int32_t)s_trim.dig_P4) << k_bmp280_cp_p4_lshift);

  var1 = (((s_trim.dig_P3 *
            (((var1 >> k_bmp280_cp_p3_rshift1) * (var1 >> k_bmp280_cp_p3_rshift1)) >>
             k_bmp280_cp_p3_rshift2)) >> k_bmp280_cp_p3_rshift3) +
           ((((int32_t)s_trim.dig_P2) * var1) >> k_bmp280_cp_p2_rshift)) >>
          k_bmp280_cp_v1_rshift2;

  var1 = (((int32_t)k_bmp280_cp_p1_base + var1) * ((int32_t)s_trim.dig_P1)) >>
         k_bmp280_cp_p1_rshift;

  /* Guard against divide by zero (degenerate trim parameters) */
  if (var1 == 0) {
    return 0;
  }

  p = (uint32_t)(((int32_t)k_bmp280_cp_adc_range - adc_p) - (var2 >> k_bmp280_cp_v1_rshift1)) *
      k_bmp280_cp_scale;

  if (p < k_bmp280_cp_half_uint32) {
    p = (p << k_bmp280_cp_p5_lshift) / (uint32_t)var1;
  } else {
    p = (p / (uint32_t)var1) << k_bmp280_cp_p5_lshift;
  }

  var1 = (((int32_t)s_trim.dig_P9) *
          ((int32_t)(((p >> k_bmp280_cp_p9_rshift1) * (p >> k_bmp280_cp_p9_rshift1)) >>
                     k_bmp280_cp_p9_rshift2))) >>
         k_bmp280_cp_p9_rshift3;

  var2 = (((int32_t)(p >> k_bmp280_cp_p8_rshift1)) * ((int32_t)s_trim.dig_P8)) >>
         k_bmp280_cp_p8_rshift2;

  p = (uint32_t)((int32_t)p + ((var1 + var2 + s_trim.dig_P7) >> k_bmp280_cp_out_rshift));

  return p;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BMP280 and configure for continuous pressure measurement
 *
 * @details Full implementation. See rx_bmp280.h for API contract.
 *
 * @param[in] config BMP280 configuration
 *
 * @return rx_err_t Status
 *
 * @pre config != NULL
 * @pre sci_iic_init(config->channel) called
 * @post s_initialized = true on success; sensor in normal measurement mode
 *
 * @since 1.0.0
 */
rx_err_t rx_bmp280_init(const rx_bmp280_config_t* config)
{
  uint8_t  chip_id = 0;
  uint8_t  calib_buf[k_bmp280_calib_len];
  rx_err_t err;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  s_config = *config;

  /* 1. Verify chip ID — confirms device is present and powered */
  err = internal_read_reg(k_bmp280_id_reg, &chip_id, 1);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Chip ID read failed — check power and I2C wiring");
    return err;
  }
  if (chip_id != k_bmp280_id_value) {
    rx_log_error(s_tag, "Chip ID mismatch — not a BMP280");
    return k_rx_err_not_found;
  }

  /* 2. Soft reset — restores all registers to defaults */
  err = internal_write_reg(k_bmp280_reset_reg, k_bmp280_reset_value);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Soft reset write failed");
    return err;
  }
  (void)tx_thread_sleep(k_bmp280_reset_delay_ms);

  /* 3. Read 24-byte NVM calibration trim parameters (0x88–0x9F) */
  err = internal_read_reg(k_bmp280_calib_base, calib_buf, k_bmp280_calib_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Calibration read failed");
    return err;
  }
  internal_load_trim(calib_buf);

  /* 4. Configure measurement: osrs_t=×1, osrs_p=×4, normal mode */
  err = internal_write_reg(k_bmp280_ctrl_meas, k_bmp280_ctrl_meas_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "ctrl_meas write failed");
    return err;
  }

  /* 5. Configure standby time, IIR filter, disable SPI 3-wire */
  err = internal_write_reg(k_bmp280_config_reg, k_bmp280_config_val);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "config write failed");
    return err;
  }

  s_initialized = true;
  rx_log_info(s_tag, "BMP280 initialized, normal mode active");
  return k_rx_ok;
}

/**
 * @brief Read and compensate pressure, temperature, and altitude from BMP280
 *
 * @details Full implementation. See rx_bmp280.h for API contract.
 *
 * @param[out] data Destination for compensated sensor readings
 *
 * @return rx_err_t Status
 *
 * @pre rx_bmp280_init() returned k_rx_ok
 * @pre data != NULL
 * @post data filled with compensated readings on k_rx_ok
 *
 * @since 1.0.0
 */
rx_err_t rx_bmp280_read(rx_bmp280_data_t* data)
{
  uint8_t  raw[k_bmp280_data_len];
  int32_t  adc_t;
  int32_t  adc_p;
  rx_err_t err;

  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Burst-read 6 bytes: press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb */
  err = internal_read_reg(k_bmp280_data_base, raw, k_bmp280_data_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Assemble 20-bit pressure ADC: msb[7:0] || lsb[7:0] || xlsb[7:4] */
  adc_p = ((int32_t)raw[k_bmp280_idx_press_msb]  << k_bmp280_shift_msb) |
          ((int32_t)raw[k_bmp280_idx_press_lsb]  << k_bmp280_shift_lsb) |
          ((int32_t)raw[k_bmp280_idx_press_xlsb] >> k_bmp280_shift_xlsb);

  /* Assemble 20-bit temperature ADC: msb[7:0] || lsb[7:0] || xlsb[7:4] */
  adc_t = ((int32_t)raw[k_bmp280_idx_temp_msb]  << k_bmp280_shift_msb) |
          ((int32_t)raw[k_bmp280_idx_temp_lsb]  << k_bmp280_shift_lsb) |
          ((int32_t)raw[k_bmp280_idx_temp_xlsb] >> k_bmp280_shift_xlsb);

  /* Temperature MUST be compensated first — sets s_t_fine for pressure formula */
  data->temperature_cdegc = internal_compensate_temperature(adc_t);

  /* Pressure compensation uses s_t_fine set above */
  data->pressure_pa = internal_compensate_pressure(adc_p);

  /* Linear altitude approximation: Δh ≈ ΔP × 0.084 m/Pa */
  data->altitude_m = ((int32_t)k_bmp280_sea_level_pa - (int32_t)data->pressure_pa) *
                     (int32_t)k_bmp280_alt_numerator / (int32_t)k_bmp280_alt_denominator;

  return k_rx_ok;
}
