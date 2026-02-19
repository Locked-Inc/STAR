/* libs/rx_hal/src/sci_iic.c */

/**
 * @file sci_iic.c
 * @brief SCI Simple IIC (SIIC) Bus Driver Implementation
 *
 * @details
 * Implements I2C controller functionality using the RX72N SCI peripheral in
 * Simple IIC (SIIC) mode. The SIIC mode is enabled by setting SIMR1.IICM=1,
 * which redirects SCI pin control to the SIMR2/SIMR3 registers for I2C
 * START/STOP/RESTART generation and ACK/NACK handling.
 *
 * This driver is the hardware analogue of sci_spi.c: same SCI peripheral,
 * different mode. Instead of SPMR for SPI clock polarity, SIMR3 is used to
 * generate I2C bus conditions.
 *
 * @par Protocol State Machine
 * @startuml
 * [*] --> Idle
 * Idle --> Start : sci_iic_write/read/write_read()
 * Start --> AddrWrite : internal_send_start()
 * AddrWrite --> DataWrite : ACK received
 * AddrWrite --> Stop : NACK received
 * DataWrite --> DataWrite : more bytes, ACK
 * DataWrite --> Stop : all bytes sent (write) or NACK
 * DataWrite --> Restart : write_read() write phase done
 * Restart --> AddrRead : internal_send_restart()
 * AddrRead --> DataRead : ACK received
 * AddrRead --> Stop : NACK received
 * DataRead --> DataRead : ACK, more bytes
 * DataRead --> Stop : NACK on last byte
 * Stop --> Idle : internal_send_stop()
 * @enduml
 *
 * @par Register Sequence (SIIC init)
 * 1. SCR=0x00: disable TE/RE
 * 2. SMR=0x00: async base (SIIC overrides protocol handling)
 * 3. SIMR1=0x01: IICM=1 enables SIIC mode
 * 4. SIMR2=0x02: IICCSC=1 (clock sync), IICINTM=0 (polling)
 * 5. BRR: PCLKB/(4*freq_hz) - 1 (e.g., 36 for 400kHz @ 60MHz PCLKB)
 * 6. SSR=0x84: clear error flags
 * 7. SIMR3=0xF0: idle (IICSCLS=11, IICSDAS=11, both lines released)
 * 8. SCR=0x30: enable TE=1, RE=1
 *
 * @see rx_sci_iic.h Public API header
 * @see rx72n_sci_regs.h SCI register definitions
 * @see sci_spi.c SCI SPI driver (analogous implementation)
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "hardware.h"
#include "rx72n_sci_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_sci_iic.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Log tag for this module */
static const char* const s_tag = "SCI_IIC";

/**
 * @enum sci_iic_reg_t
 * @brief SCI register values for SIIC mode operation
 *
 * @details
 * Constants for configuring the SCI peripheral in Simple IIC mode and
 * controlling bus conditions via SIMR3. All values derived from RX72N
 * Hardware Manual Chapter 34 (SCI) and Section 34.12 (IIC Mode).
 *
 * @see rx72n_sci_regs.h Register structure definitions
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_sci_iic_scr_disable  = 0x00, /**< SCR: TE=0, RE=0 — all disabled for init */
  k_sci_iic_scr_enable   = 0x30, /**< SCR: TE=1, RE=1, CKE=00 (internal clock) */
  k_sci_iic_smr_async    = 0x00, /**< SMR: 8-bit async base; SIIC overrides serial protocol */
  k_sci_iic_simr1_iicm   = 0x01, /**< SIMR1: IICM=1 enables Simple IIC mode */
  k_sci_iic_simr2_normal = 0x02, /**< SIMR2: IICCSC=1 (clock sync), IICINTM=0 (polling), IICACKT=0 (ACK) */
  k_sci_iic_simr2_nack   = 0x22, /**< SIMR2: IICCSC=1, IICACKT=1 — master sends NACK after next received byte */
  k_sci_iic_simr3_idle   = 0xF0, /**< SIMR3: IICSCLS=11, IICSDAS=11 — both lines released (Hi-Z via open-drain) */
  k_sci_iic_simr3_start  = 0x51, /**< SIMR3: IICSCLS=01, IICSDAS=01, IICSTAREQ=1 — generate START */
  k_sci_iic_simr3_stop   = 0x54, /**< SIMR3: IICSCLS=01, IICSDAS=01, IICSTPREQ=1 — generate STOP */
  k_sci_iic_simr3_rstart = 0x52, /**< SIMR3: IICSCLS=01, IICSDAS=01, IICRSTAREQ=1 — generate RESTART */
  k_sci_iic_simr3_stif   = 0x08, /**< SIMR3.IICSTIF bit mask — set when START/STOP/RESTART is complete */
  k_sci_iic_sisr_nack    = 0x01, /**< SISR.IICACKR bit mask — 0=ACK received, 1=NACK received */
  k_sci_iic_ssr_tdre     = 0x80, /**< SSR.TDRE: transmit data register empty (ready to accept next byte) */
  k_sci_iic_ssr_rdrf     = 0x40, /**< SSR.RDRF: receive data register full (byte ready to read) */
  k_sci_iic_ssr_clear    = 0x84, /**< SSR: clear ORER/FER/PER error flags, preserve TDRE/TEND */
  k_sci_iic_rx_dummy     = 0xFF, /**< Dummy TX byte written to TDR to clock in received data from peripheral */
} sci_iic_reg_t;

/**
 * @enum sci_iic_addr_t
 * @brief I2C address frame bit constants
 *
 * @details
 * The I2C address byte is constructed as: (dev_addr << k_sci_iic_addr_shift) | rw_bit
 * where rw_bit is 0 for write and 1 for read.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_sci_iic_write_bit  = 0x00, /**< I2C R/W bit value for write transaction */
  k_sci_iic_read_bit   = 0x01, /**< I2C R/W bit value for read transaction */
  k_sci_iic_addr_shift = 1,    /**< Left-shift count to position 7-bit address in address byte */
} sci_iic_addr_t;

/**
 * @enum sci_iic_constants_t
 * @brief Numeric constants for SIIC driver
 *
 * @details
 * Timing and limit constants. BRR formula matches SCI sync/SSPI mode:
 * bit_rate = PCLKB / (4 * (BRR + 1)) with SMR.CKS=0.
 *
 * @since 1.0.0
 */
typedef enum : uint32_t {
  k_sci_iic_pclkb_hz     = 60000000, /**< PCLKB frequency: 60 MHz (from clock config) */
  k_sci_iic_brr_divisor  = 4,        /**< BRR formula divisor: rate = PCLKB/(4*(BRR+1)) */
  k_sci_iic_timeout      = 100000,   /**< Polling loop iteration limit (NASA Rule 2 bounded loop) */
  k_sci_iic_max_channels = 13,       /**< SCI0–SCI12 supported */
} sci_iic_constants_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @struct sci_iic_state_t
 * @brief Per-channel runtime state for the SIIC driver
 *
 * @details
 * Tracks initialization status for each SCI channel. Prevents double-init
 * and allows valid-channel checks in API functions.
 *
 * @since 1.0.0
 */
typedef struct {
  bool initialized; /**< True if this channel has been successfully initialized */
} sci_iic_state_t;

/**
 * @var s_channels
 * @brief Array of per-channel state, indexed by SCI channel number
 *
 * @note Zero-initialized at startup (BSS). Not thread-safe to access.
 * @warning Do not modify directly; use sci_iic_init() to set initialized flag.
 *
 * @since 1.0.0
 */
static sci_iic_state_t s_channels[k_sci_iic_max_channels];

/* =============================================================================
 * Internal Helpers — Forward Declarations
 * =============================================================================
 */

static rx_err_t internal_channel_to_index(uint8_t channel, uint8_t* idx);
static rx_err_t internal_send_start(volatile rx_sci_regs_t* sci);
static rx_err_t internal_send_restart(volatile rx_sci_regs_t* sci);
static rx_err_t internal_send_stop(volatile rx_sci_regs_t* sci);
static rx_err_t internal_wait_stif(volatile rx_sci_regs_t* sci);
static rx_err_t internal_transmit_byte(volatile rx_sci_regs_t* sci, uint8_t byte);
static rx_err_t internal_receive_byte(volatile rx_sci_regs_t* sci, uint8_t* byte, bool send_nack);

/* =============================================================================
 * Internal Helpers — Implementation
 * =============================================================================
 */

/**
 * @brief Map SCI channel number to internal state array index
 *
 * @details
 * Validates that the channel number is within range and maps it directly to
 * the s_channels[] array index (1:1 mapping, channel N → index N).
 *
 * @param[in]  channel SCI channel number (0-12)
 * @param[out] idx     Pointer to store the validated index
 *
 * @return rx_err_t Status
 * @retval k_rx_ok              Channel is valid; *idx written
 * @retval k_rx_err_invalid_arg channel >= k_sci_iic_max_channels or idx is NULL
 *
 * @pre idx != NULL
 * @pre channel < k_sci_iic_max_channels
 * @post *idx == channel on k_rx_ok
 * @post *idx unchanged on error
 *
 * @note Thread-safe (no shared state access).
 *
 * @since 1.0.0
 */
static rx_err_t internal_channel_to_index(uint8_t channel, uint8_t* idx)
{
  if (idx == NULL) {
    return k_rx_err_invalid_arg;
  }
  if (channel < k_sci_iic_max_channels) {
    *idx = channel;
    return k_rx_ok;
  }
  return k_rx_err_invalid_arg;
}

/**
 * @brief Wait for SIMR3.IICSTIF to be set by hardware
 *
 * @details
 * Polls SIMR3.IICSTIF (bit 3) until the flag is set, indicating that a
 * START, STOP, or RESTART condition has been fully generated on the bus.
 * Clears the flag by writing 0 to bit 3 before returning.
 *
 * @param[in] sci Pointer to SCI register block
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         IICSTIF set and cleared successfully
 * @retval k_rx_err_timeout Flag did not set within k_sci_iic_timeout iterations
 *
 * @pre sci != NULL
 * @pre A START/STOP/RESTART request has been written to SIMR3
 * @post SIMR3.IICSTIF cleared (bit 3 = 0) on k_rx_ok
 * @post SIMR3 unchanged on timeout
 *
 * @note Not thread-safe (accesses shared hardware register).
 *
 * @since 1.0.0
 */
static rx_err_t internal_wait_stif(volatile rx_sci_regs_t* sci)
{
  uint32_t timeout = k_sci_iic_timeout;

  while ((sci->simr3 & k_sci_iic_simr3_stif) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Clear IICSTIF by writing 0 to bit 3, preserving IICSCLS/IICSDAS bits */
  sci->simr3 = (uint8_t)(sci->simr3 & (uint8_t)(~k_sci_iic_simr3_stif));
  return k_rx_ok;
}

/**
 * @brief Generate I2C START condition on the bus
 *
 * @details
 * Writes SIMR3=0x51 (IICSCLS=01, IICSDAS=01, IICSTAREQ=1) to request
 * a START condition. Waits for SIMR3.IICSTIF to confirm completion.
 * After return, the bus is in the address phase.
 *
 * @param[in] sci Pointer to SCI register block
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         START condition generated successfully
 * @retval k_rx_err_timeout Hardware did not generate START within timeout
 *
 * @pre sci != NULL
 * @pre Bus is in idle state (SIMR3=0xF0)
 * @post START condition present on bus
 * @post SIMR3.IICSTIF cleared
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_send_start(volatile rx_sci_regs_t* sci)
{
  sci->simr3 = k_sci_iic_simr3_start;
  return internal_wait_stif(sci);
}

/**
 * @brief Generate I2C RESTART condition on the bus
 *
 * @details
 * Writes SIMR3=0x52 (IICSCLS=01, IICSDAS=01, IICRSTAREQ=1) to request
 * a repeated START (RESTART). Used between write and read phases in a
 * combined write+read transaction.
 *
 * @param[in] sci Pointer to SCI register block
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         RESTART condition generated successfully
 * @retval k_rx_err_timeout Hardware did not generate RESTART within timeout
 *
 * @pre sci != NULL
 * @pre Write phase of a transaction has just completed
 * @post RESTART condition present on bus
 * @post SIMR3.IICSTIF cleared
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_send_restart(volatile rx_sci_regs_t* sci)
{
  sci->simr3 = k_sci_iic_simr3_rstart;
  return internal_wait_stif(sci);
}

/**
 * @brief Generate I2C STOP condition on the bus
 *
 * @details
 * Writes SIMR3=0x54 (IICSCLS=01, IICSDAS=01, IICSTPREQ=1) to request
 * a STOP condition. Waits for IICSTIF, then releases both lines to idle
 * by setting SIMR3=0xF0. Always called at the end of any transaction,
 * even on error, to free the bus.
 *
 * @param[in] sci Pointer to SCI register block
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         STOP condition generated, bus returned to idle
 * @retval k_rx_err_timeout Hardware did not generate STOP within timeout
 *
 * @pre sci != NULL
 * @pre In a valid I2C transaction (after START)
 * @post STOP condition present on bus
 * @post SIMR3=0xF0 (both lines released/idle)
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_send_stop(volatile rx_sci_regs_t* sci)
{
  rx_err_t err;

  sci->simr3 = k_sci_iic_simr3_stop;
  err        = internal_wait_stif(sci);

  /* Always release lines to idle regardless of timeout result */
  sci->simr3 = k_sci_iic_simr3_idle;
  return err;
}

/**
 * @brief Transmit one byte and check for ACK from peripheral
 *
 * @details
 * Writes one byte to TDR and waits for the SCI to clock out 8 bits + the
 * ACK/NACK clock. After RDRF is set:
 *   - Read and discard the dummy RDR byte (peripheral's SDA during our TX)
 *   - Check SISR.IICACKR: 0=ACK (success), 1=NACK (error)
 *
 * Used for both address bytes and data bytes in the write phase.
 *
 * @param[in] sci  Pointer to SCI register block
 * @param[in] byte Byte to transmit (address frame or data)
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         Byte transmitted and ACKed by peripheral
 * @retval k_rx_err_nack   NACK received from peripheral
 * @retval k_rx_err_timeout TDRE or RDRF flag did not set within timeout
 *
 * @pre sci != NULL
 * @pre SCI in SIIC mode with TE/RE enabled
 * @post TDR empty (TDRE=1) after return
 * @post RDRF cleared after reading dummy byte
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_transmit_byte(volatile rx_sci_regs_t* sci, uint8_t byte)
{
  uint32_t timeout;
  uint8_t  dummy;

  /* Wait for transmit buffer empty */
  timeout = k_sci_iic_timeout;
  while ((sci->ssr & k_sci_iic_ssr_tdre) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Write byte to transmit */
  sci->tdr = byte;

  /* Wait for receive complete (signals end of 8-bit transfer + ACK clock) */
  timeout = k_sci_iic_timeout;
  while ((sci->ssr & k_sci_iic_ssr_rdrf) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Read and discard dummy received byte (peripheral drove SDA during our TX) */
  dummy = sci->rdr;
  (void)dummy;

  /* Check ACK/NACK: SISR.IICACKR=0 means ACK received */
  if ((sci->sisr & k_sci_iic_sisr_nack) != 0) {
    return k_rx_err_nack;
  }

  return k_rx_ok;
}

/**
 * @brief Receive one byte and send ACK or NACK to peripheral
 *
 * @details
 * To receive a byte on SIIC, the master must write 0xFF (all ones) to TDR
 * to generate 8 SCL clocks. The peripheral drives SDA during this time.
 * After RDRF is set, the received byte is read from RDR.
 *
 * ACK/NACK control:
 *   - send_nack=false: SIMR2.IICACKT=0 → master sends ACK (continue reading)
 *   - send_nack=true:  SIMR2.IICACKT=1 → master sends NACK (last byte before STOP)
 * The IICACKT bit must be set BEFORE writing 0xFF to TDR.
 *
 * @param[in]  sci       Pointer to SCI register block
 * @param[out] byte      Pointer to store the received byte
 * @param[in]  send_nack True to send NACK (last byte), false to send ACK
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         Byte received; byte written
 * @retval k_rx_err_timeout TDRE or RDRF flag did not set within timeout
 *
 * @pre sci != NULL
 * @pre byte != NULL
 * @pre SCI in SIIC receive phase (after address+R ACKed by peripheral)
 * @post *byte contains received data on k_rx_ok
 * @post SIMR2.IICACKT=0 restored after last byte (send_nack=true)
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t
internal_receive_byte(volatile rx_sci_regs_t* sci, uint8_t* byte, bool send_nack)
{
  uint32_t timeout;

  /* Configure ACK/NACK before clocking the byte */
  if (send_nack) {
    sci->simr2 = k_sci_iic_simr2_nack; /* IICACKT=1: master sends NACK after this byte */
  } else {
    sci->simr2 = k_sci_iic_simr2_normal; /* IICACKT=0: master sends ACK after this byte */
  }

  /* Wait for transmit buffer empty */
  timeout = k_sci_iic_timeout;
  while ((sci->ssr & k_sci_iic_ssr_tdre) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Write 0xFF to TDR to generate SCL clocks; peripheral drives SDA */
  sci->tdr = k_sci_iic_rx_dummy;

  /* Wait for receive buffer full */
  timeout = k_sci_iic_timeout;
  while ((sci->ssr & k_sci_iic_ssr_rdrf) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Read received byte */
  *byte = sci->rdr;

  /* Restore SIMR2 to ACK mode after last byte */
  if (send_nack) {
    sci->simr2 = k_sci_iic_simr2_normal;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize SCI channel in Simple IIC (SIIC) controller mode
 *
 * @details
 * Full implementation. See rx_sci_iic.h for API contract.
 *
 * @par Register sequence:
 * 1. SCR=0x00 — disable TE/RE before reconfiguring
 * 2. SMR=0x00 — 8-bit async base (SIIC overrides serial protocol)
 * 3. SIMR1=0x01 — IICM=1: engage SIIC mode
 * 4. SIMR2=0x02 — IICCSC=1 (clock sync), polling mode, ACK mode
 * 5. BRR = PCLKB/(4*freq_hz) - 1 (e.g., 36 for 400kHz)
 * 6. SSR=0x84 — clear error flags (ORER/FER/PER)
 * 7. SIMR3=0xF0 — idle: both lines released (Hi-Z via open-drain)
 * 8. SCR=0x30 — enable TE=1, RE=1 (internal clock)
 *
 * @param[in] channel SCI channel number
 * @param[in] config  SIIC configuration
 *
 * @return rx_err_t Status
 *
 * @pre config != NULL
 * @pre config->freq_hz > 0
 * @post Channel initialized and ready for I2C transfers on success
 *
 * @since 1.0.0
 */
rx_err_t sci_iic_init(uint8_t channel, const sci_iic_config_t* config)
{
  uint8_t idx;
  uint8_t brr_value;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SCI channel out of range");
    return k_rx_err_invalid_arg;
  }

  if (s_channels[idx].initialized) {
    rx_log_error(s_tag, "SCI IIC channel already initialized");
    return k_rx_err_invalid_state;
  }

  if (config->freq_hz == 0) {
    rx_log_error(s_tag, "freq_hz must be > 0");
    return k_rx_err_invalid_arg;
  }

  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == NULL) {
    rx_log_error(s_tag, "SCI register base is NULL");
    return k_rx_err_hw_init_failed;
  }

  /* 1. Disable TE/RE before reconfiguring */
  sci->scr = k_sci_iic_scr_disable;

  /* 2. Set 8-bit async base mode (SIIC mode overrides protocol handling) */
  sci->smr = k_sci_iic_smr_async;

  /* 3. Enable SIIC mode: SIMR1.IICM=1 */
  sci->simr1 = k_sci_iic_simr1_iicm;

  /* 4. Clock synchronization enabled, polling mode, ACK mode */
  sci->simr2 = k_sci_iic_simr2_normal;

  /* 5. Calculate and set baud rate: bit_rate = PCLKB / (4 * (BRR + 1)) */
  brr_value = (uint8_t)((k_sci_iic_pclkb_hz / (k_sci_iic_brr_divisor * config->freq_hz)) - 1u);
  sci->brr  = brr_value;

  /* 6. Clear error flags in SSR */
  sci->ssr = k_sci_iic_ssr_clear;

  /* 7. Set bus to idle: both lines released (Hi-Z via open-drain pullups) */
  sci->simr3 = k_sci_iic_simr3_idle;

  /* 8. Enable transmit and receive with internal clock */
  sci->scr = k_sci_iic_scr_enable;

  s_channels[idx].initialized = true;

  rx_log_info(s_tag, "SCI IIC controller initialized");
  return k_rx_ok;
}

/**
 * @brief Write bytes to an I2C peripheral device
 *
 * @details
 * Full implementation. See rx_sci_iic.h for API contract.
 * On any error (NACK or timeout), generates STOP before returning.
 *
 * @param[in] channel  SCI channel number
 * @param[in] dev_addr 7-bit I2C address
 * @param[in] data     Bytes to write
 * @param[in] len      Count of bytes to write
 *
 * @return rx_err_t Status
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre data != NULL, len > 0
 * @post STOP condition generated; bus idle on return
 *
 * @since 1.0.0
 */
rx_err_t sci_iic_write(uint8_t channel, uint8_t dev_addr, const uint8_t* data, uint8_t len)
{
  uint8_t idx;
  uint8_t addr_byte;
  uint8_t i;

  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return k_rx_err_invalid_arg;
  }
  if (!s_channels[idx].initialized) {
    return k_rx_err_invalid_state;
  }
  if (len == 0) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == NULL) {
    return k_rx_err_hw_init_failed;
  }

  /* Generate START condition */
  err = internal_send_start(sci);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  /* Transmit address byte: [dev_addr | WRITE] */
  addr_byte = (uint8_t)((dev_addr << k_sci_iic_addr_shift) | k_sci_iic_write_bit);
  err       = internal_transmit_byte(sci, addr_byte);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  /* Transmit data bytes */
  for (i = 0; i < len; i++) {
    err = internal_transmit_byte(sci, data[i]);
    if (err != k_rx_ok) {
      (void)internal_send_stop(sci);
      return err;
    }
  }

  /* Generate STOP condition */
  return internal_send_stop(sci);
}

/**
 * @brief Read bytes from an I2C peripheral device
 *
 * @details
 * Full implementation. See rx_sci_iic.h for API contract.
 * Sends ACK after each byte except the last (NACK on last byte before STOP).
 *
 * @param[in]  channel  SCI channel number
 * @param[in]  dev_addr 7-bit I2C address
 * @param[out] data     Buffer for received bytes
 * @param[in]  len      Count of bytes to receive
 *
 * @return rx_err_t Status
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre data != NULL, len > 0
 * @post STOP condition generated; bus idle on return
 *
 * @since 1.0.0
 */
rx_err_t sci_iic_read(uint8_t channel, uint8_t dev_addr, uint8_t* data, uint8_t len)
{
  uint8_t idx;
  uint8_t addr_byte;
  uint8_t i;
  bool    is_last;

  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return k_rx_err_invalid_arg;
  }
  if (!s_channels[idx].initialized) {
    return k_rx_err_invalid_state;
  }
  if (len == 0) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == NULL) {
    return k_rx_err_hw_init_failed;
  }

  /* Generate START condition */
  err = internal_send_start(sci);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  /* Transmit address byte: [dev_addr | READ] */
  addr_byte = (uint8_t)((dev_addr << k_sci_iic_addr_shift) | k_sci_iic_read_bit);
  err       = internal_transmit_byte(sci, addr_byte);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  /* Receive data bytes; send NACK on the last byte */
  for (i = 0; i < len; i++) {
    is_last = (i == (uint8_t)(len - 1u));
    err     = internal_receive_byte(sci, &data[i], is_last);
    if (err != k_rx_ok) {
      (void)internal_send_stop(sci);
      return err;
    }
  }

  /* Generate STOP condition */
  return internal_send_stop(sci);
}

/**
 * @brief Write register address then read response (combined write+read)
 *
 * @details
 * Full implementation. See rx_sci_iic.h for API contract.
 * Uses RESTART (not STOP+START) between write and read phases.
 *
 * @param[in]  channel    SCI channel number
 * @param[in]  dev_addr   7-bit I2C address
 * @param[in]  write_data Bytes for write phase (register address)
 * @param[in]  write_len  Count of write-phase bytes
 * @param[out] read_data  Buffer for read-phase bytes
 * @param[in]  read_len   Count of bytes to read
 *
 * @return rx_err_t Status
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre write_data != NULL, write_len > 0, read_data != NULL, read_len > 0
 * @post STOP condition generated; bus idle on return
 *
 * @since 1.0.0
 */
rx_err_t sci_iic_write_read(uint8_t        channel,
                             uint8_t        dev_addr,
                             const uint8_t* write_data,
                             uint8_t        write_len,
                             uint8_t*       read_data,
                             uint8_t        read_len)
{
  uint8_t idx;
  uint8_t addr_byte;
  uint8_t i;
  bool    is_last;

  RX_CHECK_NULL_PTR(write_data, s_tag, "write_data pointer is NULL");
  RX_CHECK_NULL_PTR(read_data, s_tag, "read_data pointer is NULL");

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return k_rx_err_invalid_arg;
  }
  if (!s_channels[idx].initialized) {
    return k_rx_err_invalid_state;
  }
  if (write_len == 0 || read_len == 0) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == NULL) {
    return k_rx_err_hw_init_failed;
  }

  /* --- Write phase: START → addr+W → register address bytes --- */

  err = internal_send_start(sci);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  addr_byte = (uint8_t)((dev_addr << k_sci_iic_addr_shift) | k_sci_iic_write_bit);
  err       = internal_transmit_byte(sci, addr_byte);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  for (i = 0; i < write_len; i++) {
    err = internal_transmit_byte(sci, write_data[i]);
    if (err != k_rx_ok) {
      (void)internal_send_stop(sci);
      return err;
    }
  }

  /* --- Transition: RESTART → addr+R --- */

  err = internal_send_restart(sci);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  addr_byte = (uint8_t)((dev_addr << k_sci_iic_addr_shift) | k_sci_iic_read_bit);
  err       = internal_transmit_byte(sci, addr_byte);
  if (err != k_rx_ok) {
    (void)internal_send_stop(sci);
    return err;
  }

  /* --- Read phase: read bytes, NACK on last --- */

  for (i = 0; i < read_len; i++) {
    is_last = (i == (uint8_t)(read_len - 1u));
    err     = internal_receive_byte(sci, &read_data[i], is_last);
    if (err != k_rx_ok) {
      (void)internal_send_stop(sci);
      return err;
    }
  }

  /* Generate STOP condition */
  return internal_send_stop(sci);
}
