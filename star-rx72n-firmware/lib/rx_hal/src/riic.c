/* lib/rx_hal/src/riic.c */

/**
 * @file riic.c
 * @brief RIIC (I2C) Driver for RX72N
 *
 * Provides I2C controller mode communication using RIIC peripheral.
 * Supports standard (100kHz), fast (400kHz), and fast-plus (1MHz) modes.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief RIIC channel and timeout constants */
typedef enum {
  k_riic_max_channels = 3,     /**< RIIC0, RIIC1, RIIC2 */
  k_riic_timeout_us   = 10000, /**< 10ms timeout for operations */
  k_riic_timeout_zero = 0,     /**< Timeout expired value */
} riic_constants_t;

/** @brief RIIC channel numbers for switch statements */
typedef enum {
  k_riic_channel_0 = 0, /**< RIIC0 */
  k_riic_channel_1 = 1, /**< RIIC1 */
  k_riic_channel_2 = 2, /**< RIIC2 */
} riic_channel_num_t;

/** @brief System protection register values */
typedef enum {
  k_riic_prcr_unlock = 0xA50B, /**< Enable writes to MSTPCR */
  k_riic_prcr_lock   = 0xA500, /**< Disable writes to MSTPCR */
} riic_prcr_values_t;

/** @brief RIIC module stop bit positions in MSTPCRB */
typedef enum {
  k_riic_mstpb_riic0 = 21, /**< RIIC0 module stop bit */
  k_riic_mstpb_riic1 = 20, /**< RIIC1 module stop bit */
  k_riic_mstpb_riic2 = 19, /**< RIIC2 module stop bit */
} riic_module_stop_bits_t;

/** @brief I2C frequency constants in Hz */
typedef enum {
  k_riic_freq_100khz = 100000,  /**< Standard mode: 100 kHz */
  k_riic_freq_400khz = 400000,  /**< Fast mode: 400 kHz */
  k_riic_freq_1mhz   = 1000000, /**< Fast mode plus: 1 MHz */
} riic_frequency_t;

/** @brief RIIC bit rate calculation constants */
typedef enum {
  k_riic_brr_divisor = 3,   /**< Bit rate divisor for 50% duty cycle */
  k_riic_brr_min     = 1,   /**< Minimum valid bit rate divisor */
  k_riic_brr_max     = 255, /**< Maximum valid bit rate divisor (8-bit register) */
} riic_bit_rate_t;

/** @brief ICMR1 register configuration values */
typedef enum {
  k_riic_icmr1_controller_7bit = 0x08, /**< Controller mode, 7-bit addressing */
} riic_icmr1_values_t;

/** @brief ICMR2 register configuration values */
typedef enum {
  k_riic_icmr2_default = 0x00, /**< No timeout, no clock sync */
} riic_icmr2_values_t;

/** @brief ICMR3 register ACK/NACK bit */
typedef enum {
  k_riic_icmr3_ackbt_pos = 3, /**< ACKBT bit position in ICMR3 */
} riic_icmr3_bits_t;

/** @brief I2C address constants */
typedef enum {
  k_riic_addr_shift     = 1, /**< Shift for 7-bit address */
  k_riic_addr_write_bit = 0, /**< Write direction bit (0) */
  k_riic_addr_read_bit  = 1, /**< Read direction bit (1) */
} riic_address_bits_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* s_tag = "RIIC";

/* Track initialized RIIC channels */
static bool s_riic_channel_initialized[k_riic_max_channels] = {false, false, false};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get RIIC base address from channel number
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Pointer to RIIC register base, or NULL if invalid channel
 */
static volatile rx_riic_regs_t* internal_get_riic_base(uint8_t channel)
{
  switch (channel) {
    case k_riic_channel_0: {
      return &RIIC0;
    }
    case k_riic_channel_1: {
      return &RIIC1;
    }
    case k_riic_channel_2: {
      return &RIIC2;
    }
    default: {
      return NULL;
    }
  }
}

/**
 * @brief Calculate RIIC bit rate registers for given frequency
 *
 * @param[in] frequency_hz Desired I2C clock frequency
 * @param[out] icbrl Pointer to store ICBRL value
 * @param[out] icbrh Pointer to store ICBRH value
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t internal_calculate_bit_rate(uint32_t frequency_hz, uint8_t* icbrl, uint8_t* icbrh)
{
  /* PCLKB is used for RIIC (60 MHz on RX72N) */
  const uint32_t pclk = k_pclkb_hz;

  /* Calculate bit rate for standard formula:
   * I2C_CLK = PCLK / (2 * (ICBRL + 1) + (ICBRH + 1))
   * Simplified: ICBRL = ICBRH for 50% duty cycle
   */
  uint32_t divisor = (pclk / frequency_hz) / k_riic_brr_divisor;

  if (divisor < k_riic_brr_min || divisor > k_riic_brr_max) {
    rx_log_error(s_tag, "Invalid frequency for PCLKB");
    return k_rx_err_invalid_arg;
  }

  *icbrl = (uint8_t)divisor;
  *icbrh = (uint8_t)divisor;

  return k_rx_ok;
}

/**
 * @brief Wait for I2C bus to be ready
 *
 * @param[in] riic RIIC peripheral base
 *
 * @return k_rx_ok on success, k_rx_err_timeout on timeout
 */
static rx_err_t internal_wait_bus_ready(volatile rx_riic_regs_t* riic)
{
  uint32_t timeout = k_riic_timeout_us;

  while ((riic->iccr2 & k_riic_iccr2_bbsy) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "I2C bus busy timeout");
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Send start condition
 *
 * @param[in] riic RIIC peripheral base
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t internal_send_start(volatile rx_riic_regs_t* riic)
{
  /* Issue start condition */
  riic->iccr2 |= k_riic_iccr2_st;

  /* Wait for start condition to be issued */
  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_start) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "Start condition timeout");
    return k_rx_err_timeout;
  }

  /* Clear start flag */
  riic->icsr2 &= ~k_riic_icsr2_start;

  return k_rx_ok;
}

/**
 * @brief Send stop condition
 *
 * @param[in] riic RIIC peripheral base
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t internal_send_stop(volatile rx_riic_regs_t* riic)
{
  /* Issue stop condition */
  riic->iccr2 |= k_riic_iccr2_sp;

  /* Wait for stop condition to be issued */
  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_stop) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "Stop condition timeout");
    return k_rx_err_timeout;
  }

  /* Clear stop flag */
  riic->icsr2 &= ~k_riic_icsr2_stop;

  return k_rx_ok;
}

/**
 * @brief Write single byte to I2C
 *
 * @param[in] riic RIIC peripheral base
 * @param[in] data Byte to write
 *
 * @return k_rx_ok on success, k_rx_err_nack if NACK received
 */
static rx_err_t internal_write_byte(volatile rx_riic_regs_t* riic, uint8_t data)
{
  /* Wait for transmit data empty */
  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_tdre) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "Write timeout");
    return k_rx_err_timeout;
  }

  /* Write data */
  riic->icdrt = data;

  /* Wait for ACK/NACK */
  timeout = k_riic_timeout_us;
  while ((riic->icsr2 & k_riic_icsr2_tdre) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  /* Check for NACK */
  if (riic->icsr2 & k_riic_icsr2_nackf) {
    riic->icsr2 &= ~k_riic_icsr2_nackf;
    rx_log_error(s_tag, "NACK received");
    return k_rx_err_nack;
  }

  return k_rx_ok;
}

/**
 * @brief Read single byte from I2C
 *
 * @param[in] riic RIIC peripheral base
 * @param[out] data Pointer to store received byte
 * @param[in] send_ack True to send ACK, false for NACK
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t internal_read_byte(volatile rx_riic_regs_t* riic, uint8_t* data, bool send_ack)
{
  /* Wait for receive data full */
  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_rdrf) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "Read timeout");
    return k_rx_err_timeout;
  }

  /* Configure ACK/NACK for next byte */
  if (!send_ack) {
    riic->icmr3 |= (1 << k_riic_icmr3_ackbt_pos); /* ACKBT = 1 (NACK) */
  } else {
    riic->icmr3 &= ~(1 << k_riic_icmr3_ackbt_pos); /* ACKBT = 0 (ACK) */
  }

  /* Read data */
  *data = riic->icdrr;

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t riic_init(uint8_t channel, uint32_t frequency_hz)
{
  /* Validate channel */
  if (channel >= k_riic_max_channels) {
    rx_log_error(s_tag, "Invalid RIIC channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate frequency (100kHz, 400kHz, or 1MHz) */
  if (frequency_hz != k_riic_freq_100khz && frequency_hz != k_riic_freq_400khz &&
      frequency_hz != k_riic_freq_1mhz) {
    rx_log_error(s_tag, "Invalid I2C frequency (use 100000, 400000, or 1000000)");
    return k_rx_err_invalid_arg;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel);
  if (riic == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RIIC module (clear module stop bit) */
  SYSTEM.prcr = k_riic_prcr_unlock;

  if (channel == k_riic_channel_0) {
    SYSTEM.mstpcrb &= ~(1UL << k_riic_mstpb_riic0);
  } else if (channel == k_riic_channel_1) {
    SYSTEM.mstpcrb &= ~(1UL << k_riic_mstpb_riic1);
  } else {
    SYSTEM.mstpcrb &= ~(1UL << k_riic_mstpb_riic2);
  }

  SYSTEM.prcr = k_riic_prcr_lock;

  /* Reset RIIC */
  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_timeout_zero;

  /* Calculate bit rate */
  uint8_t  icbrl, icbrh;
  rx_err_t err = internal_calculate_bit_rate(frequency_hz, &icbrl, &icbrh);
  RX_RETURN_ON_ERROR(err, s_tag, "Bit rate calculation failed");

  /* Configure bit rate */
  riic->icbrl = icbrl;
  riic->icbrh = icbrh;

  /* Configure RIIC for controller mode */
  riic->icmr1 = k_riic_icmr1_controller_7bit; /* Controller mode, 7-bit addressing */
  riic->icmr2 = k_riic_icmr2_default;         /* No timeout, no clock sync */
  riic->icmr3 = k_riic_icmr2_default;         /* ACKBT = 0 (ACK) */

  /* Enable I2C bus interface */
  riic->iccr1 = k_riic_iccr1_ice;

  /* Mark channel as initialized */
  s_riic_channel_initialized[channel] = true;

  rx_log_debug(s_tag, "RIIC channel initialized");

  return k_rx_ok;
}

rx_err_t riic_write(uint8_t channel, uint8_t device_addr, const uint8_t* data, uint16_t length)
{
  RX_CHECK_NULL_PTR(data, s_tag, "Data pointer is NULL");

  /* Validate channel */
  if (channel >= k_riic_max_channels || !s_riic_channel_initialized[channel]) {
    rx_log_error(s_tag, "RIIC channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* Set controller transmit mode */
  riic->iccr2 = k_riic_iccr2_mst | k_riic_iccr2_trx;

  /* Send start condition */
  err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (write) */
  err = internal_write_byte(riic, (device_addr << k_riic_addr_shift) | k_riic_addr_write_bit);
  if (err != k_rx_ok) {
    internal_send_stop(riic);
    return err;
  }

  /* Send data bytes */
  for (uint16_t i = 0; i < length; i++) {
    err = internal_write_byte(riic, data[i]);
    if (err != k_rx_ok) {
      internal_send_stop(riic);
      return err;
    }
  }

  /* Send stop condition */
  err = internal_send_stop(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Stop condition failed");

  return k_rx_ok;
}

rx_err_t riic_read(uint8_t channel, uint8_t device_addr, uint8_t* data, uint16_t length)
{
  RX_CHECK_NULL_PTR(data, s_tag, "Data pointer is NULL");

  /* Validate channel */
  if (channel >= k_riic_max_channels || !s_riic_channel_initialized[channel]) {
    rx_log_error(s_tag, "RIIC channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* Set controller receive mode */
  riic->iccr2 = k_riic_iccr2_mst;

  /* Send start condition */
  err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (read) */
  err = internal_write_byte(riic, (device_addr << k_riic_addr_shift) | k_riic_addr_read_bit);
  if (err != k_rx_ok) {
    internal_send_stop(riic);
    return err;
  }

  /* Receive data bytes */
  for (uint16_t i = 0; i < length; i++) {
    bool send_ack = (i < length - 1); /* NACK on last byte */
    err           = internal_read_byte(riic, &data[i], send_ack);
    if (err != k_rx_ok) {
      internal_send_stop(riic);
      return err;
    }
  }

  /* Send stop condition */
  err = internal_send_stop(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Stop condition failed");

  return k_rx_ok;
}

rx_err_t riic_write_read(uint8_t        channel,
                         uint8_t        device_addr,
                         const uint8_t* write_data,
                         uint16_t       write_length,
                         uint8_t*       read_data,
                         uint16_t       read_length)
{
  RX_CHECK_NULL_PTR(write_data, s_tag, "Write data pointer is NULL");
  RX_CHECK_NULL_PTR(read_data, s_tag, "Read data pointer is NULL");

  /* Validate channel */
  if (channel >= k_riic_max_channels || !s_riic_channel_initialized[channel]) {
    rx_log_error(s_tag, "RIIC channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* === Write Phase === */

  /* Set controller transmit mode */
  riic->iccr2 = k_riic_iccr2_mst | k_riic_iccr2_trx;

  /* Send start condition */
  err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (write) */
  err = internal_write_byte(riic, (device_addr << k_riic_addr_shift) | k_riic_addr_write_bit);
  if (err != k_rx_ok) {
    internal_send_stop(riic);
    return err;
  }

  /* Send write data */
  for (uint16_t i = 0; i < write_length; i++) {
    err = internal_write_byte(riic, write_data[i]);
    if (err != k_rx_ok) {
      internal_send_stop(riic);
      return err;
    }
  }

  /* === Read Phase === */

  /* Send repeated start condition */
  riic->iccr2 |= k_riic_iccr2_rs;

  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_start) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    internal_send_stop(riic);
    rx_log_error(s_tag, "Repeated start timeout");
    return k_rx_err_timeout;
  }

  riic->icsr2 &= ~k_riic_icsr2_start;

  /* Set controller receive mode */
  riic->iccr2 = k_riic_iccr2_mst;

  /* Send device address (read) */
  err = internal_write_byte(riic, (device_addr << k_riic_addr_shift) | k_riic_addr_read_bit);
  if (err != k_rx_ok) {
    internal_send_stop(riic);
    return err;
  }

  /* Receive data bytes */
  for (uint16_t i = 0; i < read_length; i++) {
    bool send_ack = (i < read_length - 1); /* NACK on last byte */
    err           = internal_read_byte(riic, &read_data[i], send_ack);
    if (err != k_rx_ok) {
      internal_send_stop(riic);
      return err;
    }
  }

  /* Send stop condition */
  err = internal_send_stop(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Stop condition failed");

  return k_rx_ok;
}
