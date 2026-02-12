/* libs/rx_hal/inc/rx_sci_spi.h */

/**
 * @file rx_sci_spi.h
 * @brief SCI SPI Controller Mode Driver for RX72N Clock-Synchronous Serial
 *
 * @details
 * Provides SPI controller functionality using the RX72N SCI peripheral in
 * clock-synchronous mode. Designed for communicating with DRV8243S motor
 * driver ICs over the SCI12 bus (PE0-PE2 pins).
 *
 * SCI "Simple SPI" performs 8-bit transfers via TDR/RDR registers. 16-bit
 * frames (required by DRV8243S) are constructed from two consecutive 8-bit
 * transfers with CS held asserted throughout. There is a clock gap between
 * bytes (SCK returns to idle) which is tolerated by DRV8243S.
 *
 * @par Hardware Pin Mapping (STAR Project)
 * | SCI12 Pin | RX72N Pin | Signal     | Direction |
 * |-----------|-----------|------------|-----------|
 * | SCK12     | PE0       | SPI Clock  | Output    |
 * | SMOSI12   | PE1       | COPI       | Output    |
 * | SMISO12   | PE2       | CIPO       | Input     |
 *
 * @par Baud Rate Calculation (Sync Mode)
 * bit_rate = PCLKB / (4 * (BRR + 1)) for CKS=0
 * With PCLKB=60 MHz: BRR=1 -> 7.5 MHz, BRR=3 -> 3.75 MHz
 *
 * @see rx72n_sci_regs.h SCI register definitions
 * @see rx_drv8243.h DRV8243 motor driver (primary consumer)
 *
 * @author STAR Team
 * @date 2026-02-11
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum sci_spi_mode_t
 * @brief SPI clock polarity and phase modes
 */
typedef enum : uint8_t {
  k_sci_spi_mode_0 = 0, /**< CPOL=0, CPHA=0 */
  k_sci_spi_mode_1 = 1, /**< CPOL=0, CPHA=1 (DRV8243S) */
  k_sci_spi_mode_2 = 2, /**< CPOL=1, CPHA=0 */
  k_sci_spi_mode_3 = 3, /**< CPOL=1, CPHA=1 */
} sci_spi_mode_t;

/**
 * @struct sci_spi_controller_config_t
 * @brief Configuration for SCI SPI controller mode
 */
typedef struct {
  uint8_t       spi_mode; /**< SPI mode (0-3, use sci_spi_mode_t values) */
  uint32_t      freq_hz;  /**< Desired SPI clock frequency in Hz */
  rx_port_pin_t cs;       /**< GPIO chip select pin (type-safe port+pin) */
} sci_spi_controller_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize SCI channel in SPI controller mode
 *
 * @param[in] channel SCI channel number (currently only 12 supported)
 * @param[in] config  SPI controller configuration
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if config is nullptr
 * @retval k_rx_err_invalid_arg if channel unsupported or freq_hz is 0
 * @retval k_rx_err_invalid_state if channel already initialized
 *
 * @pre GPIO pins for SCI channel configured via MPC in gpio_init()
 * @pre MSTPCRB bit for SCI channel cleared (module powered on)
 * @post SCI configured for clock-synchronous mode with MSB-first transfers
 * @post CS GPIO configured as output, driven HIGH (deselected)
 */
[[nodiscard]] rx_err_t sci_spi_init_controller(uint8_t                            channel,
                                               const sci_spi_controller_config_t* config);

/**
 * @brief Perform a 16-bit full-duplex SPI transfer
 *
 * @details
 * Executes two consecutive 8-bit SPI transfers (MSB first, then LSB) with
 * CS held asserted throughout. There is a brief clock gap between bytes
 * which DRV8243S tolerates.
 *
 * @param[in]  channel SCI channel number
 * @param[in]  tx_data 16-bit data to transmit
 * @param[out] rx_data Pointer to store 16-bit received data (may be NULL)
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_invalid_arg if channel unsupported
 * @retval k_rx_err_invalid_state if channel not initialized
 * @retval k_rx_err_timeout if TDRE or RDRF flag not set within timeout
 *
 * @pre Channel initialized via sci_spi_init_controller()
 * @post tx_data transmitted, rx_data contains response if non-NULL
 */
[[nodiscard]] rx_err_t sci_spi_controller_transfer_16bit(uint8_t  channel,
                                                         uint16_t tx_data,
                                                         uint16_t* rx_data);

/**
 * @brief Deinitialize SCI SPI controller
 *
 * @param[in] channel SCI channel number
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_invalid_arg if channel unsupported
 * @retval k_rx_err_invalid_state if channel not initialized
 *
 * @post SCI TE/RE disabled, channel marked as uninitialized
 */
[[nodiscard]] rx_err_t sci_spi_controller_deinit(uint8_t channel);

#ifdef __cplusplus
}
#endif
