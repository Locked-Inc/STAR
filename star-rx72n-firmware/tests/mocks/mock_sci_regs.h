// SPDX-License-Identifier: MIT
/* tests/mocks/mock_sci_regs.h */

/**
 * @file mock_sci_regs.h
 * @brief Mock SCI (UART/Serial) register structures for unit testing
 *
 * @details
 * Provides test double for SCI (Serial Communication Interface) hardware
 * registers to enable unit testing of UART drivers without RX72N hardware.
 * Mirrors real hardware register layout for drop-in replacement.
 *
 * Enables testing of:
 * - UART initialization sequences
 * - Register configuration (baud rate, mode, interrupts)
 * - TX/RX data register access
 * - Status flag manipulation (TDRE, RDRF, ORER, FER, PER)
 * - Interrupt enable/disable logic
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_sci {
 *   rankdir=LR;
 *   test [label="UART Tests"];
 *   mock [label="Mock SCI Regs\ng_mock_sci[]"];
 *   real [label="Real SCI Regs\n0x0008A000", style=dashed];
 *   test -> mock;
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature               | Supported | Description |
 * |-----------------------|-----------|-------------|
 * | Multi-channel         | Yes       | 12 SCI channels (SCI0-11) |
 * | Register mirroring    | Yes       | Matches real register layout |
 * | State persistence     | Yes       | Registers retain written values |
 * | Flag simulation       | Yes       | TX/RX status flags |
 *
 * @par Mock vs Real:
 * | Aspect | Mock | Real |
 * |--------|------|------|
 * | Location | RAM (g_mock_sci) | Hardware (0x0008A000+) |
 * | Behavior | No side effects | Triggers TX/RX, interrupts |
 *
 * @par Usage: tests/test_uart.c, tests/test_sci_*.c
 *
 * @see rx72n_sci_regs.h Real SCI register definitions
 * @see uart_hal.c UART HAL implementation
 *
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - Dependency Inversion
 *
 * @author STAR Team
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Number of SCI channels to mock */
typedef enum : uint8_t {
  k_mock_sci_channel_count = 13, /**< SCI channels 0-12 */
} mock_sci_constants_t;

/* =============================================================================
 * Mock SCI Register Structure
 * Mirrors rx_sci_regs_t from rx72n_sci_regs.h
 * =============================================================================
 */

typedef struct {
  uint8_t smr;   /**< Serial Mode Register (0x00) */
  uint8_t brr;   /**< Bit Rate Register (0x01) */
  uint8_t scr;   /**< Serial Control Register (0x02) */
  uint8_t tdr;   /**< Transmit Data Register (0x03) */
  uint8_t ssr;   /**< Serial Status Register (0x04) */
  uint8_t rdr;   /**< Receive Data Register (0x05) */
  uint8_t scmr;  /**< Smart Card Mode Register (0x06) */
  uint8_t semr;  /**< Serial Extended Mode Register (0x07) */
  uint8_t snfr;  /**< Noise Filter Setting Register (0x08) */
  uint8_t simr1; /**< I2C Mode Register 1 (0x09) */
  uint8_t simr2; /**< I2C Mode Register 2 (0x0A) */
  uint8_t simr3; /**< I2C Mode Register 3 (0x0B) */
  uint8_t sisr;  /**< I2C Status Register (0x0C) */
  uint8_t spmr;  /**< SPI Mode Register (0x0D) */
} mock_sci_regs_t;

/* =============================================================================
 * SCI Register Bit Definitions
 * =============================================================================
 */

/** @brief SCR (Serial Control Register) bits */
typedef enum : uint8_t {
  k_mock_sci_scr_cke_mask = 0x03, /**< Clock enable mask */
  k_mock_sci_scr_teie     = 0x04, /**< Transmit end interrupt enable */
  k_mock_sci_scr_mpie     = 0x08, /**< Multi-processor interrupt enable */
  k_mock_sci_scr_re       = 0x10, /**< Receive enable */
  k_mock_sci_scr_te       = 0x20, /**< Transmit enable */
  k_mock_sci_scr_rie      = 0x40, /**< Receive interrupt enable */
  k_mock_sci_scr_tie      = 0x80, /**< Transmit interrupt enable */
} mock_sci_scr_bits_t;

/** @brief SSR (Serial Status Register) bits */
typedef enum : uint8_t {
  k_mock_sci_ssr_mpbt = 0x01, /**< Multi-processor bit transfer */
  k_mock_sci_ssr_mpb  = 0x02, /**< Multi-processor bit */
  k_mock_sci_ssr_tend = 0x04, /**< Transmit end flag */
  k_mock_sci_ssr_per  = 0x08, /**< Parity error flag */
  k_mock_sci_ssr_fer  = 0x10, /**< Framing error flag */
  k_mock_sci_ssr_orer = 0x20, /**< Overrun error flag */
  k_mock_sci_ssr_rdrf = 0x40, /**< Receive data register full */
  k_mock_sci_ssr_tdre = 0x80, /**< Transmit data register empty */
} mock_sci_ssr_bits_t;

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

/** @brief Mock SCI registers for all channels (0-12) */
extern mock_sci_regs_t g_mock_sci[k_mock_sci_channel_count];

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Initialize all mock SCI registers to default values
 *
 * Sets TDRE flag to indicate transmit buffer is empty (ready to send).
 */
void mock_sci_regs_init(void);

/**
 * @brief Clear all mock SCI registers to zero
 */
void mock_sci_regs_clear(void);

/**
 * @brief Set SSR register value for a channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] value SSR register value
 */
void mock_sci_set_ssr(uint8_t channel, uint8_t value);

/**
 * @brief Set TDRE (Transmit Data Register Empty) flag
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] empty True to set TDRE, false to clear
 */
void mock_sci_set_tdre(uint8_t channel, bool empty);

/**
 * @brief Set RDRF (Receive Data Register Full) flag
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] full True to set RDRF, false to clear
 */
void mock_sci_set_rdrf(uint8_t channel, bool full);

/**
 * @brief Set received data in RDR register
 *
 * Also sets RDRF flag to indicate data is available.
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Received data byte
 */
void mock_sci_set_rx_data(uint8_t channel, uint8_t data);

/**
 * @brief Get last transmitted data from TDR register
 *
 * @param[in] channel SCI channel (0-12)
 * @return Last byte written to TDR
 */
uint8_t mock_sci_get_tx_data(uint8_t channel);

/**
 * @brief Set error flags in SSR register
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] orer Overrun error flag
 * @param[in] fer Framing error flag
 * @param[in] per Parity error flag
 */
void mock_sci_set_errors(uint8_t channel, bool orer, bool fer, bool per);

/**
 * @brief Clear all error flags in SSR register
 *
 * @param[in] channel SCI channel (0-12)
 */
void mock_sci_clear_errors(uint8_t channel);

/**
 * @brief Check if channel is configured for TX
 *
 * @param[in] channel SCI channel (0-12)
 * @return True if TE bit is set in SCR
 */
bool mock_sci_is_tx_enabled(uint8_t channel);

/**
 * @brief Check if channel is configured for RX
 *
 * @param[in] channel SCI channel (0-12)
 * @return True if RE bit is set in SCR
 */
bool mock_sci_is_rx_enabled(uint8_t channel);

/**
 * @brief Get BRR register value (baud rate divisor)
 *
 * @param[in] channel SCI channel (0-12)
 * @return BRR value
 */
uint8_t mock_sci_get_brr(uint8_t channel);

#ifdef __cplusplus
}
#endif
