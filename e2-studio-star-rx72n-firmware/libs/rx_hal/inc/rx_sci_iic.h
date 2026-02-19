/* libs/rx_hal/inc/rx_sci_iic.h */

/**
 * @file rx_sci_iic.h
 * @brief SCI Simple IIC (SIIC) Bus Driver for RX72N
 *
 * @details
 * Provides I2C controller functionality using the RX72N SCI peripheral in
 * Simple IIC (SIIC) mode. Any SCI channel with SSCL/SSDA pin functions can
 * be used. STAR Project uses SCI2 on P52/P50 for the BNO055 + BMP280 IMU bus.
 *
 * **SIIC vs RIIC:**
 * The RX72N has two I2C peripheral types. This driver targets SIIC (SCI-based),
 * which is the SCI peripheral with SIMR1.IICM=1. It is a different hardware
 * block from RIIC (dedicated I2C controller used on RIIC0/RIIC1 for host and
 * BMS). SIIC is the hardware analogue of SCI SSPI — same SCI peripheral,
 * different mode register.
 *
 * **Pin requirements:**
 * - SCL and SDA pins must be configured for open-drain output via the PORT ODR
 *   register before calling sci_iic_init(). Hardware_init.c handles this.
 * - MPC must map the pins to SCI SSCL/SSDA functions (PSEL=0x0A) before init.
 *
 * **Thread safety:**
 * This driver is NOT thread-safe. If multiple threads access the same SIIC
 * bus, the caller must provide external synchronization (e.g., TX_MUTEX).
 * In the STAR design, the IMU task is the sole user of SCI2 SIIC, so no
 * external synchronization is required.
 *
 * @par Hardware Pin Mapping (STAR Project — SCI2 SIIC)
 *
 * | Signal   | RX72N Pin | Pkg Pin | SCI2 Function |
 * |----------|-----------|---------|---------------|
 * | IMU_SCL  | P52       | 54      | SSCL2         |
 * | IMU_SDA  | P50       | 56      | SSDA2         |
 *
 * @par Baud Rate Calculation (SIIC mode)
 * bit_rate = PCLKB / (4 * (BRR + 1)) for SMR.CKS=0
 * With PCLKB=60 MHz: BRR=36 → ~405 kHz (I2C fast mode, within ±5% tolerance)
 *
 * @see rx72n_sci_regs.h SCI register definitions (SIMR1/SIMR2/SIMR3/SISR)
 * @see rx_sci_spi.h SCI SPI driver (analogous implementation pattern)
 * @see rx_bno055.h BNO055 IMU driver (consumer of this HAL)
 * @see rx_bmp280.h BMP280 barometer driver (consumer of this HAL)
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @struct sci_iic_config_t
 * @brief Configuration for SCI SIIC controller mode
 *
 * @details
 * Passed to sci_iic_init() to configure the SCI peripheral for I2C operation.
 * Hardware pin configuration (MPC, ODR) must be done separately in hardware_init.c
 * before calling sci_iic_init().
 *
 * @invariant freq_hz must be > 0 and ≤ 400000 for reliable operation
 *
 * @code
 * sci_iic_config_t imu_iic_config = {
 *     .freq_hz = 400000,  // 400 kHz fast mode
 * };
 * rx_err_t err = sci_iic_init(k_imu_iic_channel, &imu_iic_config);
 * @endcode
 *
 * @see sci_iic_init() Uses this configuration
 *
 * @since 1.0.0
 */
typedef struct {
  uint32_t freq_hz; /**< Desired I2C bus frequency in Hz. Typically 100000 (standard)
                         or 400000 (fast mode). Formula: BRR = PCLKB/(4*freq_hz) - 1. */
} sci_iic_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize SCI channel in Simple IIC (SIIC) controller mode
 *
 * @details
 * Configures the SCI peripheral for I2C operation using SIMR1.IICM=1:
 * 1. Disables TE/RE (SCR=0x00)
 * 2. Clears SMR to 8-bit async base (SIIC mode overrides protocol)
 * 3. Sets SIMR1.IICM=1 to enable SIIC mode
 * 4. Sets SIMR2 for clock synchronization and polling mode
 * 5. Calculates and programs BRR for the requested frequency
 * 6. Clears SSR error flags
 * 7. Sets SIMR3=0xF0 (both lines released/idle)
 * 8. Enables TE+RE (SCR=0x30)
 *
 * Prerequisite: Before calling this function, hardware_init.c must have:
 * - Set MPC to PSEL=0x0A for the SSCL/SSDA pins
 * - Enabled open-drain mode on SCL/SDA via PORT ODR register
 * - Cleared the module stop bit for this SCI channel
 *
 * @param[in] channel SCI channel number (0-12, all channels supported)
 * @param[in] config  SIIC configuration; freq_hz must be > 0
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok              Initialization successful
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_invalid_arg channel > 12 or freq_hz == 0
 * @retval k_rx_err_invalid_state Channel already initialized
 * @retval k_rx_err_hw_init_failed SCI register base returned NULL
 *
 * @pre config != NULL
 * @pre config->freq_hz > 0
 * @pre MPC pins configured for SCI SSCL/SSDA functions (PSEL=0x0A)
 * @pre Open-drain enabled on SCL/SDA pins via PORT ODR register
 * @post SCI in SIIC mode with TE=RE=1, SIMR3=0xF0 (bus idle)
 * @post s_channels[channel].initialized == true on success
 *
 * @note Not thread-safe during initialization. Call once during hardware init.
 * @warning Call only after MPC and ODR pin configuration, before any I2C transfers.
 *
 * @see sci_iic_write() Write bytes to I2C peripheral after init
 * @see sci_iic_read() Read bytes from I2C peripheral after init
 *
 * @since 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 4 preconditions, 2 postconditions
 */
[[nodiscard]] rx_err_t sci_iic_init(uint8_t channel, const sci_iic_config_t* config);

/**
 * @brief Write bytes to an I2C peripheral device
 *
 * @details
 * Performs a complete I2C write transaction:
 * START → [dev_addr|W] → ACK → data[0] → ACK → ... → data[len-1] → ACK → STOP
 *
 * Returns k_rx_err_nack if the peripheral NAKs the address or any data byte.
 * The STOP condition is always generated before returning, even on error.
 *
 * @param[in] channel  SCI channel number (must be initialized)
 * @param[in] dev_addr 7-bit I2C device address (not pre-shifted; 0x28 for BNO055)
 * @param[in] data     Pointer to bytes to transmit
 * @param[in] len      Number of bytes to transmit (must be > 0)
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok              All bytes transmitted and ACKed
 * @retval k_rx_err_null_ptr    data is NULL
 * @retval k_rx_err_invalid_arg channel > 12 or len == 0
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_nack          NACK received from peripheral (address or data)
 * @retval k_rx_err_timeout     Polling flag timeout (hardware unresponsive)
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre data != NULL, len > 0
 * @post STOP condition generated on bus (regardless of ACK/NACK result)
 * @post Bus in idle state (SIMR3=0xF0) on return
 *
 * @note Not thread-safe. Caller must serialize concurrent access.
 * @warning Returns k_rx_err_nack if peripheral NAKs; caller must handle.
 *
 * @see sci_iic_write_read() Combined write+read for register access
 *
 * @since 1.0.0
 */
[[nodiscard]] rx_err_t
sci_iic_write(uint8_t channel, uint8_t dev_addr, const uint8_t* data, uint8_t len);

/**
 * @brief Read bytes from an I2C peripheral device
 *
 * @details
 * Performs a complete I2C read transaction:
 * START → [dev_addr|R] → ACK → data[0] (ACK) → ... → data[len-2] (ACK)
 *       → data[len-1] (NACK) → STOP
 *
 * The master sends ACK after each byte except the last, where NACK is sent to
 * signal the end of the transfer before STOP. This follows the I2C specification.
 *
 * @param[in]  channel  SCI channel number (must be initialized)
 * @param[in]  dev_addr 7-bit I2C device address (not pre-shifted; 0x28 for BNO055)
 * @param[out] data     Buffer to store received bytes
 * @param[in]  len      Number of bytes to receive (must be > 0)
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok              All bytes received successfully
 * @retval k_rx_err_null_ptr    data is NULL
 * @retval k_rx_err_invalid_arg channel > 12 or len == 0
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_nack          NACK received from peripheral at address phase
 * @retval k_rx_err_timeout     Polling flag timeout (hardware unresponsive)
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre data != NULL, len > 0
 * @post STOP condition generated on bus
 * @post data[0..len-1] filled with received bytes on k_rx_ok
 *
 * @note Not thread-safe. Caller must serialize concurrent access.
 *
 * @see sci_iic_write_read() Preferred for register read (write reg addr, then read)
 *
 * @since 1.0.0
 */
[[nodiscard]] rx_err_t
sci_iic_read(uint8_t channel, uint8_t dev_addr, uint8_t* data, uint8_t len);

/**
 * @brief Write register address then read response (combined write+read)
 *
 * @details
 * Performs the standard I2C register-read sequence using a repeated START:
 * START → [dev_addr|W] → ACK → write_data[0..wlen-1] → (each ACKed)
 *       → RESTART → [dev_addr|R] → ACK → read_data[0..rlen-2] (ACK each)
 *       → read_data[rlen-1] (NACK) → STOP
 *
 * This is the most common I2C access pattern: write a register address
 * (write phase), then read back the register value (read phase), all in
 * one atomic bus transaction with no STOP between phases.
 *
 * @param[in]  channel    SCI channel number (must be initialized)
 * @param[in]  dev_addr   7-bit I2C device address (not pre-shifted)
 * @param[in]  write_data Bytes to write in the write phase (register address)
 * @param[in]  write_len  Number of bytes in write phase (must be > 0)
 * @param[out] read_data  Buffer to store bytes from read phase
 * @param[in]  read_len   Number of bytes to read (must be > 0)
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok              Transaction completed successfully
 * @retval k_rx_err_null_ptr    write_data or read_data is NULL
 * @retval k_rx_err_invalid_arg channel > 12, write_len == 0, or read_len == 0
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_nack          NACK received at any point in the transaction
 * @retval k_rx_err_timeout     Polling flag timeout (hardware unresponsive)
 *
 * @pre Channel initialized via sci_iic_init()
 * @pre write_data != NULL, write_len > 0
 * @pre read_data != NULL, read_len > 0
 * @post STOP condition generated on bus
 * @post read_data[0..read_len-1] filled with received bytes on k_rx_ok
 *
 * @note Not thread-safe. Caller must serialize concurrent access.
 *
 * @par Example (BNO055 register read):
 * @code
 * uint8_t reg_addr = BNO055_CHIP_ID_ADDR;
 * uint8_t chip_id  = 0;
 * rx_err_t err = sci_iic_write_read(k_imu_iic_channel, 0x28,
 *                                   &reg_addr, 1, &chip_id, 1);
 * // chip_id should be 0xA0 for BNO055
 * @endcode
 *
 * @see sci_iic_write() Write-only transaction
 * @see sci_iic_read() Read-only transaction
 *
 * @since 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 5 preconditions, 2 postconditions
 */
[[nodiscard]] rx_err_t sci_iic_write_read(uint8_t        channel,
                                          uint8_t        dev_addr,
                                          const uint8_t* write_data,
                                          uint8_t        write_len,
                                          uint8_t*       read_data,
                                          uint8_t        read_len);

#ifdef __cplusplus
}
#endif
