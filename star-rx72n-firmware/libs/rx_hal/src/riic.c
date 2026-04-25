/**
 * @file riic.c
 * @brief RIIC (I2C) Driver Implementation for RX72N
 *
 * @details
 * **Production-grade implementation** of the I2C Bus Interface (RIIC) peripheral
 * driver for the RX72N microcontroller. This driver provides controller-mode
 * I2C communication with support for standard (100 kHz), fast (400 kHz), and
 * fast-plus (1 MHz) modes.
 *
 * ## Module Architecture
 *
 * ```
 * +-------------------------------------------------------------------------+
 * |                         RIIC Driver Architecture                        |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   Application Layer                                                     |
 * |   +-----------------------------------------------------------------+  |
 * |   |  riic_init()  riic_write()  riic_read()  riic_write_read()     |  |
 * |   +--------------------------+--------------------------------------+  |
 * |                              |                                         |
 * |   Internal Layer            |                                         |
 * |   +--------------------------+--------------------------------------+  |
 * |   |  internal_get_riic_base()     - Channel to address mapping      |  |
 * |   |  internal_calculate_bit_rate() - Frequency calculation          |  |
 * |   |  internal_wait_bus_ready()     - Bus arbitration                |  |
 * |   |  internal_send_start()         - START condition generation     |  |
 * |   |  internal_send_stop()          - STOP condition generation      |  |
 * |   |  internal_write_byte()         - Single byte transmission       |  |
 * |   |  internal_read_byte()          - Single byte reception          |  |
 * |   |  internal_riic_write_phase()   - Combined write sequence        |  |
 * |   |  internal_riic_read_phase()    - Combined read sequence         |  |
 * |   +--------------------------+--------------------------------------+  |
 * |                              |                                         |
 * |   Hardware Layer            |                                         |
 * |   +--------------------------+--------------------------------------+  |
 * |   |  RIIC0: 0x00088300  (SCL0/P16, SDA0/P17)                        |  |
 * |   |  RIIC1: 0x00088320  (SCL1/P21, SDA1/P20)                        |  |
 * |   |  RIIC2: 0x00088340  (SCL2/P66, SDA2/P67)                        |  |
 * |   +-----------------------------------------------------------------+  |
 * |                                                                         |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * ## I2C Protocol Implementation
 *
 * This driver implements the I2C protocol in controller mode:
 *
 * ### Write Transaction
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + W (0) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- Data Byte 0 ------------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- Data Byte N ------------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ### Read Transaction
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + R (1) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |<--- Data Byte 0 -------------------------<-|
 *     |---- ACK --------------------------------->|
 *     |<--- Data Byte N -------------------------<-|
 *     |---- NACK (last byte) ------------------->|
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ### Write-Read Transaction (Register Read)
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + W (0) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- Register Address -------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- REPEATED START ---------------------->|
 *     |---- Address + R (1) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |<--- Data Byte 0 -------------------------<-|
 *     |---- NACK -------------------------------->|
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ## Bit Rate Calculation
 *
 * The I2C clock frequency is calculated from PCLKB (60 MHz) as:
 *
 * @f[
 *   f_{SCL} = \frac{f_{PCLKB}}{(ICBRL + ICBRH + 2) \times 2^{CKS}}
 * @f]
 *
 * For simplified calculation with 50% duty cycle (ICBRL = ICBRH):
 *
 * @f[
 *   \text{divisor} = \frac{f_{PCLKB}}{f_{SCL} \times 3}
 * @f]
 *
 * | Mode       | Speed   | Divisor | Actual Rate |
 * |------------|---------|---------|-------------|
 * | Standard   | 100 kHz | 200     | 100.0 kHz   |
 * | Fast       | 400 kHz | 50      | 400.0 kHz   |
 * | Fast Plus  | 1 MHz   | 20      | 1.00 MHz    |
 *
 * ## Memory Map (Chapter 42 - RIIC)
 *
 * | Channel | Base Address | Pin SCL | Pin SDA | Module Stop Bit |
 * |---------|--------------|---------|---------|-----------------|
 * | RIIC0   | 0x00088300   | P16     | P17     | MSTPB21 (MSTPCRB b21) |
 * | RIIC1   | 0x00088320   | P21     | P20     | MSTPB20 (MSTPCRB b20) |
 * | RIIC2   | 0x00088340   | P66     | P67     | MSTPC17 (MSTPCRC b17) |
 *
 * ## Register Offsets (per channel)
 *
 * | Offset | Register | Description              |
 * |--------|----------|--------------------------|
 * | 0x00   | ICCR1    | Control Register 1       |
 * | 0x01   | ICCR2    | Control Register 2       |
 * | 0x02   | ICMR1    | Mode Register 1          |
 * | 0x03   | ICMR2    | Mode Register 2          |
 * | 0x04   | ICMR3    | Mode Register 3          |
 * | 0x10   | ICBRL    | Bit Rate Low period      |
 * | 0x11   | ICBRH    | Bit Rate High period     |
 * | 0x12   | ICDRT    | Transmit Data Register   |
 * | 0x13   | ICDRR    | Receive Data Register    |
 *
 * ## Error Handling
 *
 * The driver provides comprehensive error detection:
 * - **k_rx_err_null_ptr**: nullptr passed to function
 * - **k_rx_err_invalid_arg**: Invalid channel, address, or frequency
 * - **k_rx_err_invalid_state**: Channel not initialized
 * - **k_rx_err_timeout**: Bus busy or operation timeout
 * - **k_rx_err_nack**: Peripheral did not acknowledge
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Implementation |
 * |------|----------------|
 * | Rule 1 | No goto, setjmp, or recursion - sequential error handling |
 * | Rule 2 | Bounded loops - all loops use k_riic_timeout_us timeout |
 * | Rule 3 | No dynamic memory - static channel state array |
 * | Rule 4 | Short functions - each < 60 lines |
 * | Rule 5 | Validation - RX_CHECK_NULL_PTR, RX_CHECK_RANGE_TAG |
 * | Rule 6 | Smallest scope - local variables declared at point of use |
 * | Rule 7 | All return values checked - RX_RETURN_ON_ERROR |
 * | Rule 8 | Limited preprocessor - only RX_CHECK macros |
 * | Rule 9 | Function pointers via interface pattern (not used here) |
 * | Rule 10 | Compiles with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles Compliance
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **S** Single Responsibility | Driver handles only RIIC I2C operations |
 * | **O** Open/Closed | Supports multiple frequencies via parameter |
 * | **L** Liskov Substitution | Consistent error codes across all functions |
 * | **I** Interface Segregation | Separate read/write/write-read APIs |
 * | **D** Dependency Inversion | Uses hardware.h abstraction for registers |
 *
 * @par Thread Safety
 * This module is **NOT thread-safe**. Each RIIC channel should be accessed
 * by only one thread at a time. If multiple threads need I2C access, provide
 * external synchronization (mutex/semaphore) at the application level.
 *
 * @par Example: Read Temperature Sensor
 * @code
 * // Initialize RIIC0 at 400 kHz (Fast mode)
 * riic_channel_t channel = {.value = 0};
 * rx_err_t err = riic_init(channel, 400000);
 * if (err != k_rx_ok) {
 *     handle_error(err);
 *     return;
 * }
 *
 * // Read 2 bytes from temperature sensor at address 0x48
 * i2c_device_addr_t sensor_addr = {.value = 0x48};
 * uint8_t temp_reg = 0x00;  // Temperature register
 * uint8_t temp_data[2];
 *
 * err = riic_write_read(channel, sensor_addr, &temp_reg, 1, temp_data, 2);
 * if (err == k_rx_ok) {
 *     int16_t raw_temp = (temp_data[0] << 8) | temp_data[1];
 *     float celsius = raw_temp * 0.0625f;  // LM75 resolution
 * }
 * @endcode
 *
 * @see rx72n_riic_regs.h RIIC register definitions
 * @see hardware.h Hardware abstraction layer
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware, Chapter 42 (I2C-bus Interface)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#ifdef __RX__

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 * All constants use C23 typed enums per STAR coding standards.
 * No magic numbers - every value has a named constant with documentation.
 * =============================================================================
 */

/**
 * @enum riic_constants_t
 * @brief RIIC channel count, timeout, and transfer limit constants
 *
 * @details
 * General-purpose constants for RIIC driver operation including channel
 * limits, timeout values, and transfer size constraints.
 *
 * @par Timeout Selection
 * The 10ms timeout provides sufficient margin for:
 * - Standard mode: 100 kHz = 10us/bit, 90us/byte -> 10ms = ~111 bytes
 * - Fast mode: 400 kHz = 2.5us/bit, 22.5us/byte -> 10ms = ~444 bytes
 * - Fast Plus: 1 MHz = 1us/bit, 9us/byte -> 10ms = ~1111 bytes
 *
 * @invariant k_riic_max_channels == 3 (hardware limitation)
 * @invariant k_riic_max_transfer_length == 256 (single-byte length limit)
 */
typedef enum : uint32_t {
  k_riic_max_channels        = 3,     /**< Total RIIC channels: RIIC0, RIIC1, RIIC2 */
  k_riic_timeout_us          = 25000, /**< Busy-loop iteration budget for I2C flag waits.
                                       * Each iteration reads ICSR2/ICCR2 + decrements a
                                       * volatile counter (~10 cycles @ 240 MHz ~= 40 ns),
                                       * so 25k iters ~= 1 ms wall time -- enough to
                                       * cover a byte at 100 kHz (90 us) with 10x slack
                                       * for clock stretching. Kept below the 900 ms IWDT
                                       * window even if a 256-byte transfer stacks up
                                       * 256 per-byte timeouts back-to-back. */
  k_riic_timeout_zero        = 0,     /**< Timeout counter expired sentinel value */
  k_riic_length_zero         = 0,     /**< Zero-length transfer (invalid) sentinel */
  k_riic_register_clear      = 0,     /**< Zero value used to clear hardware registers */
  k_riic_last_index_offset   = 1,     /**< Offset to calculate last byte index from length */
  k_riic_max_transfer_length = 256,   /**< Maximum bytes per transfer operation */
} riic_constants_t;

/**
 * @enum riic_channel_num_t
 * @brief RIIC channel identifiers for switch statement dispatch
 *
 * @details
 * Provides named constants for channel selection in internal_get_riic_base().
 * Using enums instead of raw integers ensures type safety and improves
 * code readability in switch statements.
 *
 * @par Channel to Pin Mapping
 * | Channel | SCL Pin | SDA Pin | Base Address |
 * |---------|---------|---------|--------------|
 * | RIIC0   | P16     | P17     | 0x00088300   |
 * | RIIC1   | P21     | P20     | 0x00088320   |
 * | RIIC2   | P66     | P67     | 0x00088340   |
 *
 * @invariant Values are contiguous starting from 0 and match hardware channel indices
 *
 * @code
 * // Select channel 0 base address
 * volatile rx_riic_regs_t* regs = internal_get_riic_base(k_riic_channel_0);
 * @endcode
 *
 * @see internal_get_riic_base() Maps channel number to hardware register pointer
 * @see riic_constants_t For k_riic_max_channels bound used with these values
 */
typedef enum : uint8_t {
  k_riic_channel_0 = 0, /**< RIIC channel 0 (P16/SCL0, P17/SDA0) */
  k_riic_channel_1 = 1, /**< RIIC channel 1 (P21/SCL1, P20/SDA1) */
  k_riic_channel_2 = 2, /**< RIIC channel 2 (P66/SCL2, P67/SDA2) */
} riic_channel_num_t;

/**
 * @enum riic_module_stop_bits_t
 * @brief RIIC module stop bit positions in MSTPCRB (RIIC0/1) and MSTPCRC (RIIC2)
 *
 * @details
 * The RX72N uses a module stop mechanism to reduce power consumption.
 * RIIC0 and RIIC1 module stop bits reside in MSTPCRB; RIIC2 resides in
 * MSTPCRC (manual Ch11, p408-410). RIIC channels must be enabled (bit
 * cleared) before use.
 *
 * @par Module Enable Procedure
 * 1. Unlock register protection: PRCR = k_rx_prcr_unlock_prc1_prc3
 * 2. RIIC0: MSTPCRB &= ~(1 << k_riic_mstpb_riic0)
 *    RIIC1: MSTPCRB &= ~(1 << k_riic_mstpb_riic1)
 *    RIIC2: MSTPCRC &= ~(1 << k_riic_mstpc_riic2)
 * 3. Lock register protection: PRCR = k_rx_prcr_lock
 *
 * @see rx_register_protection.h for PRCR register access
 */
typedef enum : uint8_t {
  k_riic_mstpb_riic0     = 21, /**< MSTPCRB bit 21: RIIC0 module stop control (manual p408) */
  k_riic_mstpb_riic1     = 20, /**< MSTPCRB bit 20: RIIC1 module stop control (manual p408) */
  k_riic_mstpc_riic2     = 17, /**< MSTPCRC bit 17: RIIC2 module stop control (manual p410) */
  k_riic_mstpb_bit_value = 1,  /**< Single bit value for bit manipulation */
} riic_module_stop_bits_t;

/**
 * @enum riic_frequency_t
 * @brief Supported I2C bus frequencies (Hz)
 *
 * @details
 * I2C standard defines three speed grades. This driver supports all three
 * modes with appropriate bit rate register configuration.
 *
 * @par Timing Characteristics (at PCLKB = 60 MHz)
 * | Mode       | Frequency | Bit Time | Byte Time | Max Cable |
 * |------------|-----------|----------|-----------|-----------|
 * | Standard   | 100 kHz   | 10 us    | 90 us     | ~1 meter  |
 * | Fast       | 400 kHz   | 2.5 us   | 22.5 us   | ~0.5 m    |
 * | Fast Plus  | 1 MHz     | 1 us     | 9 us      | ~0.2 m    |
 *
 * @invariant Only these three discrete values are accepted by riic_init()
 *
 * @code
 * // Initialize channel 0 at standard (100 kHz) mode
 * riic_channel_t ch = {.value = 0};
 * rx_err_t err = riic_init(ch, k_riic_freq_100khz);
 * @endcode
 *
 * @see riic_init() Validates frequency_hz against these enum values
 * @see internal_calculate_bit_rate() Uses frequency to compute ICBRL/ICBRH
 *
 * @note Fast Plus mode requires pull-up resistors < 1kOhm for proper rise time.
 */
typedef enum : uint32_t {
  k_riic_freq_100khz = 100000,  /**< Standard mode: 100 kHz (Sm) */
  k_riic_freq_400khz = 400000,  /**< Fast mode: 400 kHz (Fm) */
  k_riic_freq_1mhz   = 1000000, /**< Fast mode plus: 1 MHz (Fm+) */
} riic_frequency_t;

/**
 * @enum riic_bit_rate_t
 * @brief Bit rate calculation constants for ICBRL/ICBRH registers
 *
 * @details
 * These constants are used in internal_calculate_bit_rate() to compute
 * the correct ICBRL and ICBRH register values for a target frequency.
 *
 * @par Calculation Formula
 * For 50% duty cycle (ICBRL = ICBRH = divisor):
 * @f[
 *   \text{divisor} = \frac{f_{PCLKB}}{f_{target} \times 3}
 * @f]
 *
 * Where the factor of 3 accounts for:
 * - SCL high period (ICBRH + 1) cycles
 * - SCL low period (ICBRL + 1) cycles
 * - Internal synchronization overhead
 */
/* ICMR1 CKS field and ICBRH/ICBRL values for the three supported transfer
 * rates at PCLKB = 60 MHz. Values straight out of RX72N HW manual Table
 * 42.5 "Examples of ICBRH/ICBRL Settings for Transfer Rate", PCLK=60 MHz
 * column. Low 5 bits of ICBRH/ICBRL are the actual counts; high 3 bits are
 * reserved (read-as-1, write-ignored). Using CKS=0 with a computed divisor
 * silently truncated >31 divisors to 5 bits and produced MHz-scale SCL on
 * a 100 kHz bus, which most peripherals mostly tolerate but which BNO055
 * reads do not (the addr|R ACK succeeds but the first SCL of data
 * reception glitches the RIIC FSM and MST drops mid-read). */
typedef enum : uint8_t {
  k_riic_cks_100khz = 4U,  /**< ICMR1.CKS[2:0] = 100b for 100 kbps @ 60 MHz PCLK */
  k_riic_icbrh_100k = 14U, /**< ICBRH count, HUM Table 42.5 */
  k_riic_icbrl_100k = 17U, /**< ICBRL count, HUM Table 42.5 */
  k_riic_cks_400khz = 2U,  /**< ICMR1.CKS[2:0] = 010b for 400 kbps @ 60 MHz PCLK */
  k_riic_icbrh_400k = 8U,  /**< ICBRH count, HUM Table 42.5 */
  k_riic_icbrl_400k = 19U, /**< ICBRL count, HUM Table 42.5 */
  k_riic_cks_1mhz   = 0U,  /**< ICMR1.CKS[2:0] = 000b for 1 Mbps @ 60 MHz PCLK */
  k_riic_icbrh_1m   = 15U, /**< ICBRH count, HUM Table 42.5 */
  k_riic_icbrl_1m   = 29U, /**< ICBRL count, HUM Table 42.5 */
  k_riic_cks_shift  = 4U,  /**< ICMR1 bit position of CKS[0] (CKS is bits [6:4]) */
} riic_bit_rate_t;

/**
 * @enum riic_icmr1_values_t
 * @brief ICMR1 (Mode Register 1) configuration values
 *
 * @details
 * ICMR1 configures the controller/peripheral mode and addressing type.
 * For STAR project, we use controller mode with 7-bit addressing.
 *
 * @par ICMR1 Register Layout (Bits 7-4)
 * | Bit | Name | Description |
 * |-----|------|-------------|
 * | 6-4 | CKS  | Clock divider (0-7) |
 * | 3   | BCWP | BC write protect |
 * | 2-0 | BC   | Bit counter |
 */
typedef enum : uint8_t {
  k_riic_icmr1_controller_7bit =
    0x08, /**< CKS=0, BCWP=1, 7-bit addressing; MTWP=0 keeps MST/TRS read-only so the hardware START/STOP FSM drives them, per HUM section 42.2.3. MTWP=1 was tried and caused "Start condition failed" on every transaction because iccr2=MST|TRS pre-writes the mode bits before START fires and the FSM gets desynced. */
} riic_icmr1_values_t;

/**
 * @enum riic_icmr2_values_t
 * @brief ICMR2 (Mode Register 2) configuration values
 *
 * @details
 * ICMR2 configures hardware timeout detection and SDA output hold-time delay.
 * The STAR project disables hardware timeout (TMOE = 0) because bus-busy
 * detection is handled in software by internal_wait_bus_ready() using the
 * k_riic_timeout_us countdown. The SDA output delay is also left at zero
 * (no additional hold time) since the pull-up resistors on the target PCB
 * provide adequate signal integrity at all supported speeds.
 *
 * ## ICMR2 Register Bit Layout
 *
 * | Bit | Name  | Function                          | Value Used |
 * |-----|-------|-----------------------------------|------------|
 * |  7  | TMOH  | Timeout detection (high period)   | 0 (off)    |
 * |  6  | TMOL  | Timeout detection (low period)    | 0 (off)    |
 * |  5  | TMOE  | Timeout enable                    | 0 (off)    |
 * | 4:3 | SDDL  | SDA output hold-time delay        | 0 (none)   |
 * |  2  | DLCS  | SDA delay clock source            | 0          |
 * | 1:0 | -     | Reserved                          | 0          |
 *
 * @invariant k_riic_icmr2_default == 0; writing 0x00 leaves all ICMR2 control
 *            bits in their hardware-reset state (timeouts off, no SDA delay)
 *
 * @code
 * // Apply default ICMR2 configuration during RIIC channel initialization
 * volatile rx_riic_regs_t* riic = internal_get_riic_base(k_riic_channel_0);
 * riic->icmr2 = k_riic_icmr2_default;
 * @endcode
 *
 * @see riic_init() Applies this value during channel initialization
 * @see k_riic_timeout_us Software timeout used instead of hardware TMOE
 */
typedef enum : uint8_t {
  k_riic_icmr2_default = 0, /**< No hardware timeout, no SDA delay */
} riic_icmr2_values_t;

/**
 * @enum riic_icmr3_bits_t
 * @brief ICMR3 (Mode Register 3) ACK/NACK control bit definitions
 *
 * @details
 * ICMR3 controls ACK/NACK transmission during receive operations. The key
 * field is ACKBT (Acknowledge Bit), which determines whether the controller
 * sends an ACK or NACK after each received byte.
 *
 * ## ACK/NACK Signalling Rules (I2C Specification)
 *
 * - Set ACKBT=0 (clear bit) to send ACK: instructs the peripheral to continue
 *   transmitting; used for all bytes except the last in a read transfer.
 * - Set ACKBT=1 (set bit) to send NACK: instructs the peripheral to stop
 *   transmitting; must be applied before clocking in the final byte so that
 *   the NACK is sent automatically at the correct clock edge.
 *
 * ## ICMR3 Register Bit Layout (relevant bits)
 *
 * | Bit | Name  | Function                         |
 * |-----|-------|----------------------------------|
 * |  4  | ACKWP | ACKBT write protect (1=writable) |
 * |  3  | ACKBT | ACK/NACK bit to transmit         |
 *
 * @invariant k_riic_icmr3_ackbt_pos == 3 matches the RX72N hardware bit
 *            position for ACKBT in ICMR3; changing this breaks ACK/NACK control
 * @invariant k_riic_icmr3_ackbt_mask == (1U << 3) == 0x08; derived from pos
 * @invariant k_riic_icmr3_ackwp_pos == 4 matches the RX72N hardware bit
 *            position for ACKWP; changing this silently breaks ACKBT writes
 * @invariant k_riic_icmr3_ackwp_mask == (1U << 4) == 0x10; derived from pos
 *
 * @note NACK must be sent before reading the last byte to signal to the
 *       peripheral that the transfer is complete per I2C specification.
 *
 * @note ACKWP=1 is REQUIRED before any ACKBT write. Without it, writes to
 *       ACKBT are silently ignored -- the controller keeps ACKing every byte,
 *       the peripheral never sees NACK, and STOP times out with the bus
 *       locked. ACKWP=1 is set in riic_init() as part of k_riic_icmr3_init.
 *
 * @code
 * // Prepare to NACK the last byte in a read transfer (ACKWP must already be 1)
 * volatile rx_riic_regs_t* riic = internal_get_riic_base(k_riic_channel_0);
 * riic->icmr3 |= k_riic_icmr3_ackbt_mask;   // Set ACKBT=1 -> send NACK
 *
 * // Re-enable ACK for subsequent transfers
 * riic->icmr3 &= (uint8_t)~k_riic_icmr3_ackbt_mask;  // Set ACKBT=0 -> send ACK
 * @endcode
 *
 * @see internal_read_byte() Uses these constants to control ACK/NACK output
 * @see riic_read() Orchestrates NACK signalling for multi-byte reads
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_riic_icmr3_ackbt_pos  = 3,                              /**< ACKBT bit position (bit 3) */
  k_riic_icmr3_ackbt_mask = (1U << k_riic_icmr3_ackbt_pos), /**< ACKBT bit mask (0x08) */
  k_riic_icmr3_ackwp_pos  = 4,                              /**< ACKWP bit position (bit 4) */
  k_riic_icmr3_ackwp_mask = (1U << k_riic_icmr3_ackwp_pos), /**< ACKWP bit mask (0x10) */
  k_riic_icmr3_wait_pos   = 6,                              /**< WAIT bit position (bit 6) */
  k_riic_icmr3_wait_mask  = (1U << k_riic_icmr3_wait_pos),  /**< WAIT bit mask (0x40) */
  k_riic_icmr3_init       = k_riic_icmr3_ackwp_mask,        /**< ICMR3 init: ACKWP=1, ACKBT=0 */
} riic_icmr3_bits_t;

/**
 * @enum riic_address_bits_t
 * @brief I2C 7-bit address formatting constants
 *
 * @details
 * I2C uses a 7-bit address plus a read/write direction bit in the first
 * byte after START. The address is left-shifted by 1 and OR'd with the
 * direction bit.
 *
 * @par Address Byte Format
 * ```
 *   Bit:  7  6  5  4  3  2  1  0
 *         +--------------+  +- R/W (0=Write, 1=Read)
 *         +- 7-bit Address
 * ```
 *
 * @par Address Calculation
 * - Write to 0x50: (0x50 << 1) | 0 = 0xA0
 * - Read from 0x50: (0x50 << 1) | 1 = 0xA1
 *
 * @invariant k_riic_addr_max_7bit == 127 (I2C spec: addresses 0x00-0x7F)
 *
 * @code
 * // Format a write address byte for device at 0x50
 * riic_device_addr_t dev = {.value = 0x50};
 * uint8_t addr_byte = (uint8_t)((dev.value << k_riic_addr_shift) | k_riic_addr_write_bit);
 * @endcode
 *
 * @see internal_write_byte() Uses these constants to format address bytes
 * @see riic_write() Passes formatted address bytes during write transactions
 */
typedef enum : uint8_t {
  k_riic_addr_shift     = 1,   /**< Left shift amount for 7-bit address */
  k_riic_addr_write_bit = 0,   /**< R/W bit value for write operation */
  k_riic_addr_read_bit  = 1,   /**< R/W bit value for read operation */
  k_riic_addr_max_7bit  = 127, /**< Maximum valid 7-bit address (0x7F) */
} riic_address_bits_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 * Module-level state variables. These track initialization status and
 * provide logging context. Protected by the s_ prefix per STAR coding standards.
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Logging tag for RIIC driver messages
 *
 * @details
 * Used with rx_log_error() and rx_log_debug() for consistent log output
 * formatting. All RIIC driver log messages are prefixed with "RIIC".
 *
 * @note Not thread-safe, but read-only after initialization.
 */
static const char* const s_tag = "RIIC";

/**
 * @enum riic_recovery_constants_t
 * @brief Bit-bang bus-recovery timing and ODR register addresses
 *
 * @details
 * When a peripheral on the bus holds SDA/SCL low, IICRST alone cannot clear a
 * stuck BBSY because the peripheral reset does not toggle external SCL
 * edges. internal_riic_bit_bang_recover() temporarily drops PMR, drives
 * SCL low-high 9 times to walk any stuck peripheral through a byte + ACK,
 * generates a manual STOP, then hands the pads back to the RIIC peripheral.
 *
 * ODR0 register addresses per port (RX72N HW manual Table 22.2):
 *   Port 2 ODR0 @ 0x0008C084 -- pin pair bits (P20 -> bit1, P21 -> bit3)
 */
typedef enum : uint32_t {
  k_riic_recov_cycles    = 9U,          /**< 9 SCL edges: flush one byte + ACK */
  k_riic_recov_half_us   = 5U,          /**< 5 us half-period -> ~100 kHz SCL */
  k_riic_recov_cpu_mhz   = 240U,        /**< ICLK frequency for busy-wait math */
  k_riic_recov_stop_us   = 5U,          /**< STOP condition SDA-low-then-high delay */
  k_riic_port2_odr0_addr = 0x0008C084U, /**< Port 2 ODR0 (P20/P21 N-ch open-drain) */
} riic_recovery_constants_t;

/**
 * @struct riic_recovery_pins_t
 * @brief Channel-specific port + pin info for bit-bang bus recovery
 *
 * @details
 * Indexed by riic channel number. A zero port_base entry means no recovery
 * table entry has been provided for that channel (skipped at runtime).
 * Only RIIC1 is currently populated because only the IMU uses RIIC; add
 * RIIC0/RIIC2 entries when those channels are activated.
 */
typedef struct {
  uintptr_t port_base; /**< rx_port_regs_t* base address for the port holding SCL+SDA */
  uintptr_t odr0_addr; /**< ODR register address for N-ch open-drain selection */
  uint8_t   scl_bit;   /**< SCL pin bit position within the port (0-7) */
  uint8_t   sda_bit;   /**< SDA pin bit position within the port (0-7) */
  uint8_t   odr_mask;  /**< ODR bit mask for N-ch open-drain on SCL + SDA pin pair */
} riic_recovery_pins_t;

/**
 * @brief Per-channel recovery pin lookup
 *
 * @details
 * Mirrors hardware_init.c::internal_riic1_bus_recover for the RIIC1
 * channel; zero-filled entries are skipped. See STAR PCB schematic +
 * docs/sections/03_hardware_pinout.tex for the canonical pin assignment.
 */
static const riic_recovery_pins_t k_riic_recovery_pins[k_riic_max_channels] = {
  [k_riic_channel_0] = {0U, 0U, 0U, 0U, 0U},
  [k_riic_channel_1] =
    {0x0008C002U, k_riic_port2_odr0_addr, 1U, 0U, (uint8_t)((1U << 1) | (1U << 3))},
  [k_riic_channel_2] = {0U, 0U, 0U, 0U, 0U},
};

/**
 * @brief Resolve a RIIC register pointer to its channel number
 *
 * @details
 * Compares the pointer against the three RIIC base addresses. Returns
 * k_riic_max_channels (sentinel) if the pointer does not match any channel,
 * which callers treat as "skip bit-bang recovery".
 *
 */
static uint8_t internal_riic_channel_from_base(const volatile rx_riic_regs_t* riic)
{
  const uintptr_t base = (uintptr_t)riic;
  if (base == (uintptr_t)k_riic0_base_addr) {
    return (uint8_t)k_riic_channel_0;
  }
  if (base == (uintptr_t)k_riic1_base_addr) {
    return (uint8_t)k_riic_channel_1;
  }
  if (base == (uintptr_t)k_riic2_base_addr) {
    return (uint8_t)k_riic_channel_2;
  }
  return (uint8_t)k_riic_max_channels;
}

/**
 * @brief Cycle-counted busy-wait for bit-bang timing
 *
 * @details
 * Volatile counter loop calibrated for the current ICLK frequency. Mirrors
 * hardware_init.c::internal_busy_wait_us -- duplicated here so riic.c has
 * no cross-module dependency on src/ helpers.
 *
 */
static inline void internal_riic_busy_wait_us(uint16_t us, uint16_t cpu_mhz)
{
  volatile uint32_t cycles = (uint32_t)us * (uint32_t)cpu_mhz;
  while (cycles > 0U) {
    cycles--;
  }
}

/**
 * @brief Bit-bang 9 SCL edges + manual STOP to recover an externally-stuck bus
 *
 * @details
 * Called from internal_wait_bus_ready when BBSY stays latched even after
 * an IICRST pulse. That combination means an I2C peripheral on the bus
 * (BNO055 or BMP280 in the STAR system) got interrupted mid-byte -- e.g.
 * the RX72N reset during a previous transaction -- and is still holding
 * SDA low waiting to finish sending the remaining bits of its byte + ACK.
 *
 * IICRST only resets the RIIC controller side; it does not toggle SCL on
 * the wire, so it cannot unstick an external peripheral. The only
 * software path that clears this is to physically wiggle SCL from the
 * MCU pad -- which requires handing the pad from RIIC back to GPIO long
 * enough to clock out a full byte + ACK + STOP.
 *
 * Sequence:
 *   1. Drop PMR -> pads are GPIO again, RIIC loses them.
 *   2. Enable N-ch open-drain so we cannot drive-fight a peripheral still
 *      holding the line low.
 *   3. Drive both lines high, set as output. External pull-ups dominate.
 *   4. 9x { SCL low; wait; SCL high; wait } -- one byte + ACK worth of
 *      clocks, enough to clock out any peripheral stuck mid-byte.
 *   5. Manual STOP: SDA low -> high with SCL high -- tells any peripheral
 *      still listening "transaction over, idle the bus".
 *   6. Tristate pads, clear open-drain. Leave PMR=0. The caller raises
 *      PMR back to 1 AFTER it has reset the RIIC peripheral (IICRST +
 *      SOWP dance), so the peripheral hands the pads back already in a
 *      "driving high" state. Raising PMR while the peripheral still has
 *      leftover SCLO=SDAO=0 from the previous transaction would glitch
 *      the bus low again immediately.
 *
 * Only RIIC channels with a populated entry in k_riic_recovery_pins[]
 * are handled; other channels return silently.
 *
 *
 * @pre  MPC PFS values for SCL/SDA were set during channel init and have
 *       not been disturbed (we only toggle PMR, not PFS).
 * @post PMR for SCL/SDA is CLEARED (pads stay in GPIO mode, tristated).
 *       Caller must reset the RIIC peripheral and then raise PMR.
 * @post PDR bits for SCL/SDA are cleared (tristated).
 *
 * @par NASA Power of 10 Compliance
 * - Rule 2: bounded loop (k_riic_recov_cycles = 9)
 * - Rule 5: channel bounds check + port_base != 0 validation
 *
 * @see internal_riic_bit_bang_handback() Companion that raises PMR once
 *      the peripheral has been reset to a clean SCLO=SDAO=1 state.
 */
static void internal_riic_bit_bang_recover(uint8_t channel)
{
  if (channel >= (uint8_t)k_riic_max_channels) {
    return;
  }
  const riic_recovery_pins_t* const pins = &k_riic_recovery_pins[channel];
  if (pins->port_base == 0U) {
    return; /* No recovery table entry for this channel. */
  }

  volatile rx_port_regs_t* const port = (volatile rx_port_regs_t*)pins->port_base;
  volatile uint8_t* const        odr0 = (volatile uint8_t*)pins->odr0_addr;

  const uint8_t scl_mask = (uint8_t)(1U << pins->scl_bit);
  const uint8_t sda_mask = (uint8_t)(1U << pins->sda_bit);
  const uint8_t both     = (uint8_t)(scl_mask | sda_mask);

  /* Hand pads back to GPIO so we can bit-bang. PFS values are preserved
   * (we only toggle PMR, not PFS/PWPR). */
  port->pmr &= (uint8_t)~both;

  /* N-ch open-drain: avoid push-pull fight with a peripheral still holding low. */
  *odr0 |= pins->odr_mask;

  /* Drive both high, then set as output. Pull-ups bring the bus up. */
  port->podr |= both;
  port->pdr |= both;

  for (uint16_t i = 0; i < (uint16_t)k_riic_recov_cycles; i++) {
    port->podr &= (uint8_t)~scl_mask;
    internal_riic_busy_wait_us((uint16_t)k_riic_recov_half_us, (uint16_t)k_riic_recov_cpu_mhz);
    port->podr |= scl_mask;
    internal_riic_busy_wait_us((uint16_t)k_riic_recov_half_us, (uint16_t)k_riic_recov_cpu_mhz);
  }

  /* Manual STOP: SDA low (SCL already high), then SDA high. */
  port->podr &= (uint8_t)~sda_mask;
  internal_riic_busy_wait_us((uint16_t)k_riic_recov_stop_us, (uint16_t)k_riic_recov_cpu_mhz);
  port->podr |= sda_mask;
  internal_riic_busy_wait_us((uint16_t)k_riic_recov_stop_us, (uint16_t)k_riic_recov_cpu_mhz);

  /* Tristate pads + clear open-drain. DO NOT raise PMR here -- see
   * internal_riic_bit_bang_handback() for the reason. */
  port->pdr &= (uint8_t)~both;
  *odr0 &= (uint8_t)~pins->odr_mask;
}

/**
 * @brief Raise PMR so the RIIC peripheral takes the pads back
 *
 * @details
 * Companion to internal_riic_bit_bang_recover(). Call this AFTER the
 * RIIC peripheral has been IICRST-pulsed and put through the SOWP / SCLO
 * / SDAO release dance so its internal output latches are high. Raising
 * PMR while those latches are still 0 would glitch the bus low again
 * immediately and could re-confuse the peripheral we just unstuck.
 *
 *
 * @pre  internal_riic_bit_bang_recover() was just called on the same channel.
 * @pre  The RIIC peripheral has been reset (IICRST cycle + SOWP release).
 * @post PMR bits for SCL/SDA are set, pads routed to RIIC peripheral.
 */
/**
 * @brief Pulse IICRST on a controller-initialized RIIC channel and restore config.
 *
 * @details
 * Per RX72N HW manual section 42.2.1, writing 1 to ICCR1.IICRST while
 * ICCR1.ICE=0 performs an internal reset of the RIIC peripheral's FSM
 * without touching the pin mux. That drops any half-completed transaction
 * state -- BBSY, MST, TRS, pending SP/RS/ST request bits, NACKF, latched
 * AL, the "dummy read kicked reception" flag -- back to idle, which is
 * what we need before a new transaction when an earlier transaction on
 * the same peripheral may have left the FSM partially advanced. The
 * config registers (ICBRH/L, ICMR1..3) are wiped by IICRST, so we
 * restore them from the live values (which were set in riic_init() and
 * must already be valid since ICE was on before the pulse).
 *
 * Do NOT substitute this for riic_init(): this helper assumes MSTPCR and
 * MPC/PFS are already set up and that ICE=1 was previously toggled to
 * latch the peripheral out of module-stop. It is a fast pre-transaction
 * FSM scrub, not a cold boot.
 *
 * Currently unused in the main transaction path -- kept as a [[maybe_unused]]
 * helper because it is the right escape hatch when the FSM has been
 * observed to get stuck mid-transaction and the bit-bang recovery in
 * internal_wait_bus_ready() is too heavyweight (it tears down and rebuilds
 * pad direction via PMR, which adds ~100 us versus ~2 us here).
 *
 *
 * @pre  ICE was set to 1 earlier (channel previously passed riic_init).
 * @post ICCR1.ICE = 1, ICCR1.IICRST = 0, ICMR1..3 / ICBRH/L restored,
 *       ICSR1 / ICSR2 cleared.
 */
[[maybe_unused]]
static void internal_riic_force_fsm_reset(volatile rx_riic_regs_t* riic)
{
  const uint8_t saved_icbrh = riic->icbrh;
  const uint8_t saved_icbrl = riic->icbrl;
  const uint8_t saved_icmr1 = riic->icmr1;
  const uint8_t saved_icmr2 = riic->icmr2;

  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_register_clear;

  riic->icbrh = saved_icbrh;
  riic->icbrl = saved_icbrl;
  riic->icmr1 = saved_icmr1;
  riic->icmr2 = saved_icmr2;
  riic->icmr3 = k_riic_icmr3_init;

  riic->icsr1 = k_riic_register_clear;
  riic->icsr2 = k_riic_register_clear;

  riic->iccr1 = k_riic_iccr1_ice;

  uint8_t iccr1 = riic->iccr1;
  iccr1 &= (uint8_t) ~(uint8_t)k_riic_iccr1_sowp;
  iccr1 |= (uint8_t)(k_riic_iccr1_sclo | k_riic_iccr1_sdao);
  riic->iccr1 = iccr1;
  iccr1 |= (uint8_t)k_riic_iccr1_sowp;
  riic->iccr1 = iccr1;
}

static void internal_riic_bit_bang_handback(uint8_t channel)
{
  if (channel >= (uint8_t)k_riic_max_channels) {
    return;
  }
  const riic_recovery_pins_t* const pins = &k_riic_recovery_pins[channel];
  if (pins->port_base == 0U) {
    return;
  }

  volatile rx_port_regs_t* const port     = (volatile rx_port_regs_t*)pins->port_base;
  const uint8_t                  scl_mask = (uint8_t)(1U << pins->scl_bit);
  const uint8_t                  sda_mask = (uint8_t)(1U << pins->sda_bit);
  port->pmr |= (uint8_t)(scl_mask | sda_mask);
}

/**
 * @enum riic_channel_mode_t
 * @brief Per-channel RIIC initialization mode
 *
 * @details
 * Tracks whether each RIIC channel has been initialized, and if so, whether
 * it was initialized for controller or peripheral operation. This prevents
 * peripheral-mode APIs from running on a controller-initialized channel and
 * vice-versa, satisfying the Liskov Substitution Principle for RIIC API
 * implementations.
 *
 * @invariant Set only by riic_init() (controller) and riic_init_peripheral() (peripheral)
 * @invariant Reset to k_riic_mode_uninitialized by riic_deinit_peripheral() on success
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_riic_mode_uninitialized = 0x00u, /**< Channel not yet initialized */
  k_riic_mode_controller    = 0x01u, /**< Initialized by riic_init() -- controller mode */
  k_riic_mode_peripheral = 0x02u, /**< Initialized by riic_init_peripheral() -- peripheral mode */
} riic_channel_mode_t;

/**
 * @var s_riic_channel_mode
 * @brief Per-channel RIIC mode tracking array
 *
 * @details
 * Tracks the operational mode of each RIIC channel: uninitialized, controller,
 * or peripheral. Used to enforce that controller APIs are only called on
 * controller-initialized channels and vice-versa.
 *
 * | Index | Channel | Initial State            |
 * |-------|---------|--------------------------|
 * | 0     | RIIC0   | k_riic_mode_uninitialized |
 * | 1     | RIIC1   | k_riic_mode_uninitialized |
 * | 2     | RIIC2   | k_riic_mode_uninitialized |
 *
 * @invariant Array size == k_riic_max_channels (3)
 * @warning Do not modify directly -- only riic_init(), riic_init_peripheral(),
 *          and riic_deinit_peripheral() may write to this array.
 * @note Not thread-safe; provide external synchronization if needed.
 *
 * @since Version 1.0.0
 */
static riic_channel_mode_t s_riic_channel_mode[k_riic_max_channels] = {k_riic_mode_uninitialized,
                                                                       k_riic_mode_uninitialized,
                                                                       k_riic_mode_uninitialized};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 * These functions provide low-level I2C operations used by the public API.
 * All internal functions follow NASA Power of 10 rules for safety-critical code.
 * =============================================================================
 */

/**
 * @brief Get RIIC channel register base address from channel number
 *
 * @details
 * Maps an RIIC channel number (0-2) to its corresponding hardware register
 * base address using a compile-time-dispatched switch statement. This function
 * is the single point of truth for the channel-to-address mapping and provides
 * a clean abstraction between the logical channel index used throughout the
 * driver and the physical memory-mapped peripheral addresses defined by the
 * RX72N hardware.
 *
 * ## Channel-to-Address Mapping
 *
 * The three RIIC channels are located at fixed, evenly spaced addresses in the
 * RX72N peripheral address space (0x00088300, 0x00088320, 0x00088340). Each
 * block is 32 bytes in size and contains all control, status, bit-rate, and
 * data registers for one channel.
 *
 * ## Memory Map
 *
 * | Channel | Accessor | Base Address | Register Block Size |
 * |---------|----------|--------------|---------------------|
 * | 0       | riic0()  | 0x00088300   | 32 bytes            |
 * | 1       | riic1()  | 0x00088320   | 32 bytes            |
 * | 2       | riic2()  | 0x00088340   | 32 bytes            |
 *
 * ## Why Return NULL for Invalid Channel
 *
 * Returning nullptr for an out-of-range channel allows every caller to perform
 * a single null-pointer check with RX_CHECK_NULL_PTR rather than duplicating
 * range validation logic. This pattern is consistent with rx_port_get_base()
 * in the GPIO driver.
 *
 *
 *
 * @pre channel must be less than k_riic_max_channels (3); values >= 3 yield nullptr
 * @pre caller must have verified that the corresponding RIIC module stop bit in
 *      MSTPCRB has been cleared (module enabled) before using the returned pointer
 * @post returned pointer is non-NULL if and only if channel is 0, 1, or 2
 * @post returned pointer, when non-NULL, is aligned to the RIIC peripheral register
 *       block boundary as required by the RX72N hardware specification
 *
 * @note This function does NOT validate the channel - the caller is responsible
 *       for ensuring the value is in range before calling
 * @note The return type is volatile to prevent the compiler from optimizing away
 *       hardware register reads or writes through the returned pointer
 *
 * @par Performance:
 * O(1) - simple switch statement with a direct return per case
 *
 * @par Example:
 * @code
 * // Look up the register base for RIIC channel 1 and send a START condition
 * volatile rx_riic_regs_t* riic = internal_get_riic_base(k_riic_channel_1);
 * if (riic == nullptr) {
 *     return k_rx_err_invalid_arg;
 * }
 * riic->iccr2 |= k_riic_iccr2_st;  // Issue START condition
 * @endcode
 *
 * @see riic0() Inline accessor for RIIC channel 0 base address
 * @see riic1() Inline accessor for RIIC channel 1 base address
 * @see riic2() Inline accessor for RIIC channel 2 base address
 * @see k_riic_max_channels Upper bound on valid channel numbers
 *
 * @since Version 1.0.0
 */
static volatile rx_riic_regs_t* internal_get_riic_base(const uint8_t channel)
{
  switch (channel) {
    case k_riic_channel_0: {
      return riic0();
    }
    case k_riic_channel_1: {
      return riic1();
    }
    case k_riic_channel_2: {
      return riic2();
    }
    default: {
      return nullptr;
    }
  }
}

/**
 * @brief Calculate RIIC bit rate register values for target frequency
 *
 * @details
 * Computes the ICBRL and ICBRH register values to achieve the desired I2C
 * bus frequency. Uses a simplified calculation assuming 50% duty cycle
 * (equal high and low periods).
 *
 * ## Calculation Algorithm
 *
 * The RX72N RIIC bit rate formula (from Chapter 42):
 * @f[
 *   f_{SCL} = \frac{f_{PCLKB}}{(ICBRL + 1) + (ICBRH + 1) + \text{sync}}
 * @f]
 *
 * Simplified for 50% duty cycle (ICBRL = ICBRH):
 * @f[
 *   \text{divisor} = \frac{f_{PCLKB}}{f_{target} \times 3}
 * @f]
 *
 * ## Example Calculations (PCLKB = 60 MHz)
 *
 * | Target    | Calculation         | Divisor | Actual Rate |
 * |-----------|---------------------|---------|-------------|
 * | 100 kHz   | 60M / (100k x 3)    | 200     | 100.0 kHz   |
 * | 400 kHz   | 60M / (400k x 3)    | 50      | 400.0 kHz   |
 * | 1 MHz     | 60M / (1M x 3)      | 20      | 1.00 MHz    |
 *
 *
 *
 * @pre icbrl != nullptr
 * @pre icbrh != nullptr
 * @pre frequency_hz is a supported I2C frequency
 * @post *icbrl contains the low-period divisor (1-255)
 * @post *icbrh contains the high-period divisor (1-255)
 * @post *icbrl == *icbrh (50% duty cycle)
 *
 * @note Uses PCLKB (60 MHz) as the source clock per RX72N specification.
 * @note For asymmetric duty cycle, manually set ICBRL != ICBRH after init.
 *
 * @warning Frequencies outside 100 kHz - 1 MHz may produce out-of-range divisors.
 *
 * @see riic_init() Uses this function during channel initialization
 * @see k_pclkb_hz PCLKB frequency constant from hardware.h
 */
static rx_err_t internal_calculate_bit_rate(const uint32_t frequency_hz,
                                            uint8_t*       icbrl,
                                            uint8_t*       icbrh,
                                            uint8_t*       cks_field)
{
  if (icbrl == nullptr || icbrh == nullptr || cks_field == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Look up the HUM Table 42.5 row for the requested rate. Only the three
   * discrete rates the riic_channel_freq_t enum accepts are supported; we
   * do not compute CKS/ICBRH/ICBRL from a formula because the formula in
   * the HUM depends on SCL rise/fall times and the table values already
   * bake in the standard-mode / fast-mode / fast-mode-plus tr/tf budgets. */
  if (frequency_hz == (uint32_t)k_riic_freq_100khz) {
    *cks_field = (uint8_t)k_riic_cks_100khz;
    *icbrh     = (uint8_t)k_riic_icbrh_100k;
    *icbrl     = (uint8_t)k_riic_icbrl_100k;
    return k_rx_ok;
  }
  if (frequency_hz == (uint32_t)k_riic_freq_400khz) {
    *cks_field = (uint8_t)k_riic_cks_400khz;
    *icbrh     = (uint8_t)k_riic_icbrh_400k;
    *icbrl     = (uint8_t)k_riic_icbrl_400k;
    return k_rx_ok;
  }
  if (frequency_hz == (uint32_t)k_riic_freq_1mhz) {
    *cks_field = (uint8_t)k_riic_cks_1mhz;
    *icbrh     = (uint8_t)k_riic_icbrh_1m;
    *icbrl     = (uint8_t)k_riic_icbrl_1m;
    return k_rx_ok;
  }

  rx_log_error(s_tag, "Invalid frequency (expected 100 kHz, 400 kHz, or 1 MHz)");
  return k_rx_err_invalid_arg;
}

/**
 * @brief Wait for I2C bus to become idle (not busy)
 *
 * @details
 * Polls the BBSY (Bus Busy) bit in ICCR2 until the bus is free or timeout
 * occurs. This function must be called before starting a new I2C transaction
 * to ensure no other controller is using the bus (multi-controller arbitration).
 *
 * ## Bus Busy Detection
 *
 * The BBSY bit is set when:
 * - A START condition is detected on the bus
 * - The controller has issued a START condition
 *
 * The BBSY bit is cleared when:
 * - A STOP condition is detected on the bus
 * - Bus arbitration is lost
 *
 * ## Timeout Calculation
 *
 * At 100 kHz (slowest mode), maximum byte transfer time:
 * - 9 bits per byte (8 data + 1 ACK) x 10 us = 90 us
 * - For 256 bytes (max transfer): 256 x 90 us = 23 ms
 *
 * The 10 ms timeout (k_riic_timeout_us) provides margin for typical
 * operations while catching true bus-stuck conditions.
 *
 *
 *
 * @pre riic != nullptr
 * @pre RIIC channel must be initialized and enabled
 * @post Bus is idle if k_rx_ok returned
 *
 * @note This is a blocking function - polls in tight loop.
 * @note Timeout is ~10ms at k_riic_timeout_us iterations.
 *
 * @warning In multi-controller systems, bus may become busy again immediately
 *          after this function returns - caller should proceed quickly.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 2: [OK] Bounded loop with k_riic_timeout_us maximum iterations
 * - Rule 5: [OK] nullptr check via RX_CHECK_NULL_PTR
 */
static rx_err_t internal_wait_bus_ready(volatile rx_riic_regs_t* riic)
{
  uint32_t timeout = k_riic_timeout_us;

  RX_CHECK_NULL_PTR(riic, s_tag, "RIIC peripheral base is nullptr");

  while ((riic->iccr2 & k_riic_iccr2_bbsy) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout > k_riic_timeout_zero) {
    return k_rx_ok;
  }

  /* BBSY can latch stuck when the peripheral comes up observing lines that
   * are mid-transition (e.g. a peripheral holding SDA low from a prior aborted
   * byte). Per RX72N HW manual section 42.2.1 the only software-side clear
   * is an IICRST pulse -- which wipes the config registers back to reset,
   * so we must restore them after. */
  rx_log_warn(s_tag, "BBSY stuck - pulsing IICRST to recover");

  /* Debug-level detail (compiled out when LOG_LEVEL < k_log_debug): SDA/SCL
   * pad state + ICCR2/ICSR2 before recovery. PIDR tells us whether the bus
   * is held physically low (pad bit=0 -> peripheral still driving) or only
   * the RIIC state machine latched stuck (pad bit=1). RIIC1 = port 2
   * (0x0008C002). Struct access lets rx72n_port_regs.h static_asserts catch
   * layout drift at compile time. */
  rx_log_debug_val(s_tag, "  PIDR =0x", ((volatile const rx_port_regs_t*)0x0008C002U)->pidr);
  rx_log_debug_val(s_tag, "  ICCR2=0x", riic->iccr2);
  rx_log_debug_val(s_tag, "  ICSR2=0x", riic->icsr2);
  const uint8_t saved_icbrh = riic->icbrh;
  const uint8_t saved_icbrl = riic->icbrl;
  const uint8_t saved_icmr1 = riic->icmr1;
  const uint8_t saved_icmr2 = riic->icmr2;

  riic->iccr1 = k_riic_iccr1_iicrst;   /* assert reset (ICEEN=0, IICRST=1) */
  riic->iccr1 = k_riic_register_clear; /* deassert -> configuration state */

  riic->icbrh = saved_icbrh;
  riic->icbrl = saved_icbrl;
  riic->icmr1 = saved_icmr1;
  riic->icmr2 = saved_icmr2;
  /* IICRST wipes ICMR3 too. Restore ACKWP=1 so subsequent read transactions
   * can still NACK the final byte; without this, the first read after a
   * recovery would lock the bus all over again. */
  riic->icmr3 = k_riic_icmr3_init;

  /* Clear any stale status flags left by the aborted transaction. IICRST
   * resets most of ICSR1/ICSR2, but writing explicit zeros closes any
   * silicon window where a flag might survive the reset window. */
  riic->icsr1 = k_riic_register_clear;
  riic->icsr2 = k_riic_register_clear;

  riic->iccr1 = k_riic_iccr1_ice; /* re-enable */

  /* SOWP / SCLO / SDAO release dance (same as the post-init path). */
  uint8_t iccr1 = riic->iccr1;
  iccr1 &= (uint8_t) ~(uint8_t)k_riic_iccr1_sowp;
  iccr1 |= (uint8_t)(k_riic_iccr1_sclo | k_riic_iccr1_sdao);
  riic->iccr1 = iccr1;
  iccr1 |= (uint8_t)k_riic_iccr1_sowp;
  riic->iccr1 = iccr1;

  /* Second wait. If BBSY still set after a full IICRST cycle, the bus is
   * genuinely stuck -- probably a peripheral holding the line low -- and
   * IICRST alone cannot clear it because the peripheral reset does not
   * toggle external SCL edges. */
  timeout = k_riic_timeout_us;
  while ((riic->iccr2 & k_riic_iccr2_bbsy) && timeout > k_riic_timeout_zero) {
    timeout--;
  }
  if (timeout > k_riic_timeout_zero) {
    return k_rx_ok;
  }

  /* Last-resort: bit-bang 9 SCL edges + manual STOP to walk any stuck
   * peripheral through a byte + ACK. This is the only software path
   * that can release a peripheral that is clock-stretching or holding
   * SDA low from an aborted transaction across an MCU reboot (verified
   * on BNO055 against the production STAR PCB).
   *
   * Critical ordering: bit-bang leaves PMR=0 (pads still GPIO), THEN we
   * reset the RIIC peripheral while it has no pads, THEN we hand the
   * pads back. If we raised PMR before resetting the peripheral, its
   * leftover SCLO=SDAO=0 latches would drive both lines LOW again the
   * moment the peripheral takes the pads, glitching the bus. */
  const uint8_t channel = internal_riic_channel_from_base(riic);
  rx_log_warn(s_tag, "IICRST insufficient - bit-bang bus recovery");
  internal_riic_bit_bang_recover(channel); /* leaves PMR=0 */

  /* Reset the peripheral fully while pads are still GPIO. IICRST clears
   * the internal state machine; config restore reprograms what IICRST
   * wiped; ICE=1 re-enables; SOWP dance forces SCLO=SDAO=1 so the
   * peripheral's open-drain drivers release (high-Z) on handback. */
  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_register_clear;
  riic->icbrh = saved_icbrh;
  riic->icbrl = saved_icbrl;
  riic->icmr1 = saved_icmr1;
  riic->icmr2 = saved_icmr2;
  riic->icmr3 = k_riic_icmr3_init;     /* Restore ACKWP=1 (IICRST wiped it). */
  riic->icsr1 = k_riic_register_clear; /* Drop any stale flag that survived. */
  riic->icsr2 = k_riic_register_clear;
  riic->iccr1 = k_riic_iccr1_ice;

  iccr1 = riic->iccr1;
  iccr1 &= (uint8_t) ~(uint8_t)k_riic_iccr1_sowp;
  iccr1 |= (uint8_t)(k_riic_iccr1_sclo | k_riic_iccr1_sdao);
  riic->iccr1 = iccr1;
  iccr1 |= (uint8_t)k_riic_iccr1_sowp;
  riic->iccr1 = iccr1;

  /* Now hand the pads back to the freshly-reset peripheral (PMR=1). */
  internal_riic_bit_bang_handback(channel);

  /* Debug-only post-recovery pad state. PIDR with both SCL and SDA bits
   * set means the bus is idle (pull-ups in control); any cleared bit means
   * a peripheral is still physically holding the line low and no amount
   * of software will fix it. */
  if (channel == (uint8_t)k_riic_channel_1) {
    rx_log_debug_val(s_tag,
                     "  post-recover PIDR=0x",
                     ((volatile const rx_port_regs_t*)0x0008C002U)->pidr);
  }

  timeout = k_riic_timeout_us;
  while ((riic->iccr2 & k_riic_iccr2_bbsy) && timeout > k_riic_timeout_zero) {
    timeout--;
  }
  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "I2C bus busy timeout (after bit-bang recovery)");
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Generate I2C START condition on the bus
 *
 * @details
 * Issues a START condition by setting the ST bit in ICCR2, then waits for
 * hardware confirmation via the START flag in ICSR2. The START condition
 * signals all peripherals that a transaction is beginning.
 *
 * ## START Condition Timing
 *
 * ```
 *     SDA  -----+
 *               +----------
 *     SCL  -----------+
 *                     +----
 *              |      |
 *              |      +- SCL goes low (start of first bit)
 *              +- SDA goes low while SCL high (START)
 * ```
 *
 * ## Hardware Sequence
 *
 * 1. Set ST bit in ICCR2 (request START)
 * 2. Hardware pulls SDA low while SCL is high
 * 3. Hardware sets START flag in ICSR2
 * 4. Software clears START flag to acknowledge
 *
 *
 *
 * @pre riic != nullptr
 * @pre Bus must be idle (BBSY = 0) - use internal_wait_bus_ready() first
 * @pre RIIC channel must be enabled (ICE = 1)
 * @post START flag is cleared in ICSR2
 * @post Bus is now busy (BBSY = 1)
 *
 * @note Clears the START detection flag after successful START.
 * @note After START, caller must send address byte within timeout.
 *
 * @warning If START fails, caller must issue STOP to release bus.
 *
 * @see internal_send_stop() Companion function for ending transactions
 * @see internal_wait_bus_ready() Should be called before START
 */
static rx_err_t internal_send_start(volatile rx_riic_regs_t* riic)
{
  RX_CHECK_NULL_PTR(riic, s_tag, "RIIC peripheral base is nullptr");

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
  riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_start;

  return k_rx_ok;
}

/**
 * @brief Generate I2C STOP condition on the bus
 *
 * @details
 * Issues a STOP condition by setting the SP bit in ICCR2, then waits for
 * hardware confirmation via the STOP flag in ICSR2. The STOP condition
 * releases the bus for other controllers.
 *
 * ## STOP Condition Timing
 *
 * ```
 *     SDA  ----------+-----
 *                    |
 *     SCL  ----+-----+-----
 *              |     |
 *              |     +- SDA goes high while SCL high (STOP)
 *              +- SCL goes high (end of last bit/ACK)
 * ```
 *
 * ## Hardware Sequence
 *
 * 1. Set SP bit in ICCR2 (request STOP)
 * 2. Hardware drives SCL high
 * 3. Hardware drives SDA high while SCL is high
 * 4. Hardware sets STOP flag in ICSR2
 * 5. Software clears STOP flag to acknowledge
 *
 * ## Error Recovery
 *
 * STOP is also used for error recovery - if a transaction fails at any
 * point, issuing STOP releases the bus and resets the peripheral state
 * machine.
 *
 *
 *
 * @pre riic != nullptr
 * @pre A transaction is in progress (bus is busy)
 * @post STOP flag is cleared in ICSR2
 * @post Bus is idle (BBSY = 0)
 * @post Peripheral state machine returns to idle
 *
 * @note Always call STOP at the end of a transaction or to recover from errors.
 * @note STOP after NACK in read transactions signals end of transfer to peripheral.
 *
 * @par Best-Effort Cleanup
 * In error paths, STOP is called to release the bus even if the original
 * error should be preserved. The calling function should:
 * @code
 * if (err != k_rx_ok) {
 *     rx_err_t stop_err = internal_send_stop(riic);
 *     (void)stop_err;  // Preserve original error
 *     return err;
 * }
 * @endcode
 *
 * @see internal_send_start() Companion function for starting transactions
 */
static rx_err_t internal_send_stop(volatile rx_riic_regs_t* riic)
{
  RX_CHECK_NULL_PTR(riic, s_tag, "RIIC peripheral base is nullptr");

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
  riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_stop;

  return k_rx_ok;
}

/**
 * @brief Transmit single byte over I2C bus
 *
 * @details
 * Writes one byte to the I2C bus and waits for acknowledgment from the
 * peripheral. This function handles both address bytes and data bytes.
 *
 * ## Transmission Sequence
 *
 * 1. Wait for TDRE (Transmit Data Register Empty) flag
 * 2. Write byte to ICDRT (Transmit Data Register)
 * 3. Hardware shifts out 8 bits MSB-first
 * 4. Hardware generates 9th clock for ACK
 * 5. Peripheral drives SDA low (ACK) or leaves high (NACK)
 * 6. Check NACKF flag to determine response
 *
 * ## I2C Byte Timing
 *
 * ```
 *     SCL  ++ ++ ++ ++ ++ ++ ++ ++ ++
 *          || || || || || || || || ||
 *          ++ ++ ++ ++ ++ ++ ++ ++ ++
 *          D7 D6 D5 D4 D3 D2 D1 D0 ACK
 *     SDA  -----------------------+
 *          Data bits (8)         +- ACK (low)
 * ```
 *
 * ## NACK Handling
 *
 * NACK is received when:
 * - Address byte: No peripheral at that address
 * - Data byte: Peripheral buffer full or error condition
 * - Last byte: Normal end-of-read signaling (expected)
 *
 *
 *
 * @pre riic != nullptr (not checked - caller must ensure)
 * @pre START condition has been issued
 * @pre Previous byte (if any) has been acknowledged
 * @post On success, byte has been transmitted and ACK received
 * @post On NACK, NACKF flag is cleared for next operation
 *
 * @note Does NOT send START or STOP - caller manages transaction framing.
 * @note First byte after START should be address with R/W bit.
 *
 * @warning On NACK, caller should issue STOP to release bus.
 *
 * @par Address Byte Format
 * For address bytes, caller must format as: (addr << 1) | direction
 * - Write: (0x50 << 1) | 0 = 0xA0
 * - Read: (0x50 << 1) | 1 = 0xA1
 *
 * @see internal_read_byte() Companion function for receiving bytes
 */
static rx_err_t internal_write_byte(volatile rx_riic_regs_t* riic, const uint8_t data)
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

  /* Wait for transmit END (byte + ACK clock complete). Per RX72N HW manual
   * section 42.2.7, TEND asserts on the rising edge of the 9th clock after
   * the ACK/NACK response has been sampled, so NACKF is valid only once
   * TEND=1. Waiting for TDRE to clear is wrong: TDRE clears the instant
   * ICDRT is written, so the wait would exit in zero iterations and NACKF
   * would be checked before the byte has actually been transmitted. */
  timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_tend) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "TEND timeout (byte never finished transmitting)");
    return k_rx_err_timeout;
  }

  /* Check for NACK -- valid only now that TEND=1. */
  if (riic->icsr2 & k_riic_icsr2_nackf) {
    riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_nackf;
    rx_log_error(s_tag, "NACK received");
    return k_rx_err_nack;
  }

  return k_rx_ok;
}

/**
 * @brief Send the address|R byte that opens a controller-receive transaction
 *
 * @details
 * After the controller switches to receive mode (ICCR2 = MST, TRS=0) and
 * has issued START or repeated-START, the address byte with R bit must be
 * sent. internal_write_byte() cannot be used here: per RX72N HW manual
 * section 42.2.1 the TRS bit auto-clears to 0 on the rising edge of the
 * 9th SCL of an addr|R byte, which moves the peripheral into receive mode
 * and sets RDRF automatically -- TEND and TDRE-reassertion no longer fire.
 * Waiting for transmit-side flags in that state hangs forever; this helper
 * waits for RDRF | NACKF instead.
 *
 *
 *
 * @pre Bus is active; ICCR2 = MST (TRS=0); start condition already issued
 * @post On success: RDRF=1 in ICSR2, ICDRR holds the first data byte
 *
 * @see internal_write_byte() Standard transmit-mode byte writer
 * @see internal_read_byte()  Reads byte after RDRF asserts
 */
static rx_err_t internal_write_address_for_read(volatile rx_riic_regs_t* riic,
                                                const uint8_t            addr_byte)
{
  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_tdre) && timeout > k_riic_timeout_zero) {
    timeout--;
  }
  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "TDRE timeout before addr|R");
    return k_rx_err_timeout;
  }

  riic->icdrt = addr_byte;

  /* In controller-receive mode, RDRF fires once the first data byte has been
   * clocked in after the address ACK. NACKF fires if the peripheral didn't
   * ACK the address. Wait for either. */
  const uint8_t wait_mask = (uint8_t)(k_riic_icsr2_rdrf | k_riic_icsr2_nackf);
  timeout                 = k_riic_timeout_us;
  while (!(riic->icsr2 & wait_mask) && timeout > k_riic_timeout_zero) {
    timeout--;
  }
  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "addr|R: neither RDRF nor NACKF asserted");
    return k_rx_err_timeout;
  }

  if (riic->icsr2 & k_riic_icsr2_nackf) {
    riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_nackf;
    rx_log_error(s_tag, "Peripheral NACKed address|R");
    return k_rx_err_nack;
  }

  /* Caller is responsible for the dummy ICDRR read that kicks off actual
   * data reception. Doing it here unconditionally would be wrong for the
   * single-byte read case where ACKBT=1 must be set BEFORE the dummy read
   * so byte 0 is NACKed (RX72N HW manual Figure 42.10, 1-byte path). The
   * caller sets ACKBT per the transfer length, then does the dummy read. */
  return k_rx_ok;
}

/**
 * @brief Receive single byte from I2C bus
 *
 * @details
 * Reads one byte from the I2C bus and sends ACK or NACK as specified.
 * The ACK/NACK response is configured before reading the byte from the
 * receive register.
 *
 * ## Reception Sequence
 *
 * 1. Wait for RDRF (Receive Data Register Full) flag
 * 2. Configure ACK/NACK response in ICMR3.ACKBT
 * 3. Read byte from ICDRR (Receive Data Register)
 * 4. Hardware generates ACK/NACK on 9th clock
 * 5. RDRF flag is cleared by read
 *
 * ## ACK vs NACK Decision
 *
 * | Situation         | Response | ACKBT | Purpose |
 * |-------------------|----------|-------|---------|
 * | More bytes to read | ACK      | 0     | Continue transfer |
 * | Last byte         | NACK     | 1     | Signal end of read |
 *
 * ## I2C Read Byte Timing
 *
 * ```
 *     SCL  ++ ++ ++ ++ ++ ++ ++ ++ ++
 *          || || || || || || || || ||
 *          ++ ++ ++ ++ ++ ++ ++ ++ ++
 *          D7 D6 D5 D4 D3 D2 D1 D0 ACK/NACK
 *     SDA  -----------------------+
 *          Peripheral drives      +- Controller drives ACK/NACK
 * ```
 *
 *
 *
 * @pre riic != nullptr
 * @pre data != nullptr
 * @pre Controller is in receive mode (address + read bit sent)
 * @post *data contains the received byte
 * @post ACK or NACK has been sent to peripheral
 * @post RDRF flag is cleared
 *
 * @note ACK/NACK is configured BEFORE reading ICDRR.
 * @note NACK on last byte is REQUIRED per I2C specification.
 *
 * @warning Failure to send NACK on last byte may cause peripheral
 *          to continue driving data, corrupting next transaction.
 *
 * @par Example: Reading Multiple Bytes
 * @code
 * for (uint16_t i = 0; i < length; i++) {
 *     bool send_ack = (i < length - 1);  // ACK all except last
 *     err = internal_read_byte(riic, &buffer[i], send_ack);
 *     if (err != k_rx_ok) break;
 * }
 * @endcode
 *
 * @see internal_write_byte() Companion function for transmitting bytes
 */
[[maybe_unused]]
static rx_err_t internal_read_byte(volatile rx_riic_regs_t* riic,
                                   uint8_t*                 data,
                                   const bool               send_ack,
                                   const bool               last_byte)
{
  /* Wait for receive data full */
  uint32_t timeout = k_riic_timeout_us;

  RX_CHECK_NULL_PTR(riic, s_tag, "RIIC peripheral base is nullptr");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");

  while (!(riic->icsr2 & k_riic_icsr2_rdrf) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "Read timeout");
    return k_rx_err_timeout;
  }

  /* Configure ACK/NACK for the byte we are about to release from ICDRR.
   * The peripheral clock-stretches SCL between the 8th clock and the ACK
   * clock while RDRF is set, so setting ACKBT now takes effect on the
   * upcoming ACK/NACK bit. */
  if (!send_ack) {
    riic->icmr3 |= k_riic_icmr3_ackbt_mask; /* ACKBT = 1 (NACK) */
  } else {
    riic->icmr3 &= (uint8_t) ~(uint8_t)k_riic_icmr3_ackbt_mask; /* ACKBT = 0 (ACK) */
  }

  /* For the final byte of a receive, queue SP before reading ICDRR so the
   * RIIC generates NACK + STOP atomically after the 9th clock. Per RX72N
   * HW manual Figure 42.10 the SP request must be set between RDRF and
   * the final ICDRR read so the STOP is chained to the 9th clock's NACK.
   *
   * Do NOT queue SP after the final ICDRR read: the 9th clock will have
   * already fired, SCL is released, and a fresh SP produces no STOP edge.
   * That was the bug behind "Stop condition timeout" on 1-byte reads. */
  if (last_byte) {
    riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_stop;
    riic->iccr2 |= k_riic_iccr2_sp;
  }

  /* Read data -- releases SCL for the ACK/NACK clock. For the last byte,
   * SP was queued above so hardware generates STOP immediately after the
   * NACK on the 9th clock.
   *
   * Do NOT instead issue SP AFTER this ICDRR read. Per RX72N HW manual
   * Figure 42.10, once the 9th clock has fired the peripheral drops the
   * auto clock-stretch and releases SCL; the STOP condition must be
   * chained into that release, not requested after the bus has already
   * returned to idle. Calling internal_send_stop() after the read will
   * time out waiting for ICSR2.STOP to assert because no STOP edge is
   * ever produced. */
  *data = riic->icdrr;

  if (last_byte) {
    timeout = k_riic_timeout_us;
    while (!(riic->icsr2 & k_riic_icsr2_stop) && timeout > k_riic_timeout_zero) {
      timeout--;
    }
    if (timeout == k_riic_timeout_zero) {
      rx_log_error(s_tag, "STOP after final ICDRR timeout");
      return k_rx_err_timeout;
    }
    /* Clear STOP + NACKF so the next transaction starts from a clean
     * status register. ICMR3.ACKBT left at 1 is harmless -- it gets
     * re-written by the next internal_read_byte. */
    riic->icsr2 &= (uint8_t) ~(uint8_t)(k_riic_icsr2_stop | k_riic_icsr2_nackf);
  }

  return k_rx_ok;
}

/**
 * @brief Execute I2C write phase for combined write-read transfer
 *
 * @details
 * Performs the write portion of a write-read (register read) transaction.
 * This typically writes a register address to the peripheral, which sets
 * an internal pointer for the subsequent read phase.
 *
 * ## Transaction Sequence
 *
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + W (0) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- Register Address -------------------->|  <- write_data[0]
 *     |<--- ACK ---------------------------------<-|
 *     |---- [Additional bytes...] --------------->|  <- write_data[1..N]
 *     |                                            |
 *     |    (No STOP - read phase follows)          |
 * ```
 *
 * ## Error Handling
 *
 * On any error (timeout or NACK), this function issues a STOP condition
 * to release the bus before returning the error code.
 *
 *
 *
 * @pre riic points to valid, initialized RIIC channel
 * @pre device_addr.value <= 127
 * @pre write_data != nullptr
 * @pre write_length > 0
 * @pre Bus is idle (not busy)
 * @post On success: Bus is active, internal address pointer set
 * @post On failure: STOP issued, bus released
 *
 * @note Does NOT issue STOP on success - caller must continue with read
 *       phase or issue STOP.
 * @note On error, STOP is always issued for bus cleanup.
 *
 * @see internal_riic_read_phase() Continues the transaction with read
 * @see riic_write_read() Public API that combines both phases
 */
static rx_err_t internal_riic_write_phase(volatile rx_riic_regs_t* riic,
                                          const i2c_device_addr_t  device_addr,
                                          const uint8_t*           write_data,
                                          const uint16_t           write_length)
{
  /* Set controller transmit mode */
  riic->iccr2 = k_riic_iccr2_mst | k_riic_iccr2_trs;

  /* Send start condition */
  rx_err_t err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (write) */
  err = internal_write_byte(riic, (device_addr.value << k_riic_addr_shift) | k_riic_addr_write_bit);
  if (err != k_rx_ok) {
    rx_err_t stop_err = internal_send_stop(riic);
    (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
    return err;
  }

  /* Send write data */
  for (uint16_t i = 0; i < write_length; i++) {
    err = internal_write_byte(riic, write_data[i]);
    if (err != k_rx_ok) {
      rx_err_t stop_err = internal_send_stop(riic);
      (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Execute I2C read phase for combined write-read transfer
 *
 * @details
 * Performs the read portion of a write-read (register read) transaction.
 * Issues a repeated START condition (without releasing the bus), then
 * reads data bytes from the peripheral.
 *
 * ## Transaction Sequence
 *
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |    (Write phase completed, no STOP)        |
 *     |                                            |
 *     |---- REPEATED START ---------------------->|
 *     |---- Address + R (1) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |<--- Data Byte 0 -------------------------<-|
 *     |---- ACK --------------------------------->|
 *     |<--- Data Byte N-1 -----------------------<-|
 *     |---- NACK -------------------------------->|  <- Signals end
 *     |                                            |
 *     |    (Caller issues STOP)                    |
 * ```
 *
 * ## Repeated START vs STOP+START
 *
 * The repeated START (Sr) is critical for atomic register reads:
 * - Prevents other controllers from arbitrating between address write and data read
 * - Ensures peripheral's internal address pointer is still valid
 * - Required by many I2C peripherals (EEPROMs, sensors, etc.)
 *
 * ## ACK/NACK Protocol
 *
 * Per I2C specification, the controller must NACK the last byte to signal
 * end of read. This allows the peripheral to release SDA for the STOP
 * condition.
 *
 *
 *
 * @pre riic points to valid, initialized RIIC channel
 * @pre Write phase has been completed (bus is active)
 * @pre device_addr.value <= 127
 * @pre read_data != nullptr
 * @pre read_length > 0
 * @post On success: read_data contains received bytes
 * @post On failure: STOP issued, bus released
 *
 * @note Does NOT issue STOP on success - caller must issue STOP.
 * @note On error, STOP is always issued for bus cleanup.
 * @note NACK is automatically sent on last byte.
 *
 * @see internal_riic_write_phase() Precedes this function in write-read
 * @see riic_write_read() Public API that combines both phases
 */
static rx_err_t internal_riic_read_phase(volatile rx_riic_regs_t* riic,
                                         const i2c_device_addr_t  device_addr,
                                         uint8_t*                 read_data,
                                         const uint16_t           read_length)
{
  /* Send repeated start condition */
  riic->iccr2 |= k_riic_iccr2_rs;

  uint32_t timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_start) && timeout > k_riic_timeout_zero) {
    timeout--;
  }

  if (timeout == k_riic_timeout_zero) {
    rx_err_t stop_err = internal_send_stop(riic);
    (void)stop_err; /* Best-effort cleanup on timeout */
    rx_log_error(s_tag, "Repeated start timeout");
    return k_rx_err_timeout;
  }

  riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_start;

  /* Explicit ICCR2 write to clear any pending SP/RS/ST request bits ahead of
   * the addr|R write. MST and TRS are read-only when ICMR1.MTWP=0, so this
   * is a no-op for them; TRS will auto-clear to 0 on the 9th clock of the
   * addr|R byte per HUM 42.2.1. */
  riic->iccr2 = k_riic_iccr2_mst;

  /* Send device address (read) -- use the receive-mode helper, NOT
   * internal_write_byte: TEND/TDRE-reassertion don't fire after addr|R in
   * controller-receive mode; RDRF does. This leaves the address-ACK RDRF
   * asserted but does NOT do the dummy read (that responsibility is the
   * caller's so it can pre-set ACKBT for 1-byte transfers). */
  rx_err_t err = internal_write_address_for_read(
    riic,
    (uint8_t)((device_addr.value << k_riic_addr_shift) | k_riic_addr_read_bit));
  if (err != k_rx_ok) {
    rx_err_t stop_err = internal_send_stop(riic);
    (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
    return err;
  }

  /* For a 1-byte read, set ACKBT=1 BEFORE the dummy read so the sole data
   * byte's 9th-clock ACK slot carries NACK (HUM Fig 42.10 "1- or 2-byte
   * receive" path). The dummy read below releases SCL and starts clocking
   * byte 0; the 9th clock of byte 0 samples ACKBT at that moment. For
   * multi-byte reads we leave ACKBT=0 here and flip it to 1 inside the
   * loop on iteration N-2 (HUM Fig 42.11 "3 bytes or more" path) -- the
   * read of byte N-2 is what releases SCL for byte N-1 to clock in, and
   * byte N-1's 9th clock then picks up ACKBT=1. Setting ACKBT=1 on
   * iteration N-1 is ALWAYS TOO LATE: by the time we enter that iteration
   * RDRF has already asserted, meaning byte N-1's 9th clock has already
   * fired and its ACK slot has already been transmitted with whatever
   * ACKBT was in the prior iteration (typically 0 = ACK). That leaves
   * the peripheral thinking "keep streaming" and holding SDA for another
   * byte, which is why the subsequent SP never chains a STOP edge and we
   * see "Stop condition timeout" at the end of every multi-byte read. */
  if (read_length == 1U) {
    riic->icmr3 |= k_riic_icmr3_ackbt_mask;
  }

  (void)riic->icdrr; /* dummy read kicks reception */

  /* Receive loop. For each iteration:
   *   - Wait for RDRF (byte i now in ICDRR, byte i's 9th clock fired).
   *   - If we are on the penultimate byte (i == N-2), flip ACKBT=1 so
   *     byte N-1's 9th clock carries NACK. Nothing to do on iteration
   *     N-1 except clear STOP / queue SP ahead of the final ICDRR read.
   *   - Issue the final STOP request between reading byte N-2 and the
   *     final ICDRR read.
   *   - Read ICDRR: copies byte i data and (on non-last iterations)
   *     releases SCL for byte i+1 to clock in. On the last iteration
   *     there is no byte i+1; with SP queued the 9th-clock NACK is
   *     followed by a STOP edge.
   *
   * Do NOT call internal_send_stop() after this loop on success: SP was
   * already queued in the final iteration and the hardware generates the
   * STOP edge chained to byte N-1's 9th clock. A second SP on an idle
   * bus either times out or spuriously re-raises BBSY. */
  for (uint16_t i = 0; i < read_length; i++) {
    uint32_t timeout = k_riic_timeout_us;
    while (!(riic->icsr2 & k_riic_icsr2_rdrf) && timeout > k_riic_timeout_zero) {
      timeout--;
    }
    if (timeout == k_riic_timeout_zero) {
      rx_log_error_val(s_tag, "Read byte RDRF timeout i=", (uint8_t)i);
      rx_err_t stop_err = internal_send_stop(riic);
      (void)stop_err;
      return k_rx_err_timeout;
    }

    if (read_length >= 2U && i == (uint16_t)(read_length - 2U)) {
      /* Penultimate byte: NACK will fall on byte N-1's 9th clock. */
      riic->icmr3 |= k_riic_icmr3_ackbt_mask;
    }

    if (i == (uint16_t)(read_length - k_riic_last_index_offset)) {
      /* Last iteration: queue STOP before the final read so the NACK on
       * byte N-1's 9th clock is chained to a STOP edge by hardware. */
      riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_stop;
      riic->iccr2 |= k_riic_iccr2_sp;
    }

    read_data[i] = riic->icdrr;
  }

  /* Wait for the hardware-generated STOP to complete. */
  uint32_t stop_timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_stop) && stop_timeout > k_riic_timeout_zero) {
    stop_timeout--;
  }
  if (stop_timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "STOP after final ICDRR timeout");
    return k_rx_err_timeout;
  }

  /* Clear STOP / NACKF for the next transaction. ACKBT stays latched at 1
   * from the penultimate-iteration write; riic_init / recovery / the
   * pre-transaction ICSR2 clear all restore ICMR3 as needed. */
  riic->icsr2 &= (uint8_t) ~(uint8_t)(k_riic_icsr2_stop | k_riic_icsr2_nackf);

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 * These functions provide the user-facing I2C interface. All functions
 * validate inputs, check initialization state, and provide comprehensive
 * error reporting.
 * =============================================================================
 */

/**
 * @brief Initialize RIIC (I2C) channel for controller mode operation
 *
 * @details
 * Configures an RIIC channel for I2C controller mode communication at the
 * specified bus frequency. This function must be called before any I2C
 * transfers on the channel.
 *
 * ## Initialization Sequence
 *
 * 1. Validate channel number and frequency
 * 2. Enable RIIC module clock (clear MSTPCRB bit)
 * 3. Reset RIIC internal state (IICRST)
 * 4. Calculate and configure bit rate registers
 * 5. Configure controller mode, 7-bit addressing
 * 6. Enable I2C bus interface (ICE)
 * 7. Mark channel as initialized
 *
 * ## Module Stop Control
 *
 * The RIIC peripheral is disabled by default to save power. This function
 * enables the module clock by clearing the appropriate MSTPCRB bit:
 *
 * | Channel | MSTPCRB Bit | Default State |
 * |---------|-------------|---------------|
 * | RIIC0   | Bit 21      | Stopped       |
 * | RIIC1   | Bit 20      | Stopped       |
 * | RIIC2   | Bit 19      | Stopped       |
 *
 * ## Supported Frequencies
 *
 * | Frequency | Mode         | Use Case                |
 * |-----------|--------------|-------------------------|
 * | 100 kHz   | Standard     | Legacy devices          |
 * | 400 kHz   | Fast         | Most I2C peripherals    |
 * | 1 MHz     | Fast Plus    | High-speed sensors      |
 *
 *
 *
 * @pre channel.value < 3
 * @pre frequency_hz is one of: 100000, 400000, 1000000
 * @post Channel is enabled and ready for I2C transfers
 * @post s_riic_channel_mode[channel.value] == k_riic_mode_controller
 *
 * @note This function modifies protected registers (MSTPCRB) using PRCR unlock.
 * @note Re-initializing an already initialized channel is allowed.
 * @note Pin configuration (MPC) must be done separately if needed.
 *
 * @warning Using unsupported frequencies may cause communication failures.
 *
 * @par Example
 * @code
 * // Initialize RIIC0 for Fast mode (400 kHz)
 * riic_channel_t channel = {.value = 0};
 * rx_err_t err = riic_init(channel, 400000);
 * if (err != k_rx_ok) {
 *     rx_log_error("I2C", "Failed to initialize RIIC0");
 * }
 * @endcode
 *
 * @see riic_write() Write data after initialization
 * @see riic_read() Read data after initialization
 * @see riic_write_read() Combined write-read after initialization
 *
 * @since Version 1.0.0
 *
 * @callgraph
 */
rx_err_t riic_init(const riic_channel_t channel, const uint32_t frequency_hz)
{
  /* Validate channel */
  if (channel.value >= k_riic_max_channels) {
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
  volatile rx_riic_regs_t* const riic = internal_get_riic_base(channel.value);
  if (riic == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RIIC module (clear module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;

  if (channel.value == k_riic_channel_0) {
    system_regs()->mstpcrb &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpb_riic0);
  } else if (channel.value == k_riic_channel_1) {
    system_regs()->mstpcrb &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpb_riic1);
  } else {
    system_regs()->mstpcrc &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpc_riic2);
  }

  *prcr_reg() = k_rx_prcr_lock;

  /* Unconditional bit-bang bus recovery BEFORE the peripheral observes the
   * bus. Per RX72N HW manual section 42.2.1, the RIIC comes out of
   * module-stop + IICRST with SCLO=SDAO=0 internally -- it *drives the bus
   * low* until the SOWP dance below writes SCLO=SDAO=1. hardware_init has
   * already raised PMR for SCL/SDA by the time riic_init runs, so without
   * this step the low-drive glitches the external bus and can latch
   * BBSY=1 as soon as ICE=1. The bit-bang helper drops PMR (pads back to
   * GPIO, tristated via pull-ups), wiggles SCL 9x, issues a manual STOP,
   * and leaves PMR=0. We hand the pads back AFTER the SOWP dance below.
   *
   * Do NOT skip this on the assumption that hardware_init's bus recovery is
   * enough: that one runs before PMR is raised, so it only cleans the pull-
   * ups, not the freshly-enabled RIIC's internal start-detect state. Do
   * NOT run bit-bang AFTER the SOWP dance either -- PMR would already be 1
   * and the RIIC would drive against our GPIO writes. Channels without an
   * entry in k_riic_recovery_pins[] (RIIC0, RIIC2) are no-ops. */
  internal_riic_bit_bang_recover(channel.value); /* leaves PMR=0 */

  /* Reset RIIC */
  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_register_clear;

  /* Bit-rate lookup. CKS is in ICMR1[6:4] and must be written alongside
   * MTWP / BCWP / BC in a single ICMR1 write. ICBRH/ICBRL hold the 5-bit
   * counts (upper 3 bits are reserved, read-as-1, write-ignored). */
  uint8_t        icbrl     = 0;
  uint8_t        icbrh     = 0;
  uint8_t        cks_field = 0;
  const rx_err_t err       = internal_calculate_bit_rate(frequency_hz, &icbrl, &icbrh, &cks_field);
  RX_RETURN_ON_ERROR(err, s_tag, "Bit rate calculation failed");

  /* Configure bit rate */
  riic->icbrl = icbrl;
  riic->icbrh = icbrh;

  /* Configure RIIC for controller mode + CKS per HUM Table 42.5 */
  riic->icmr1 = (uint8_t)(k_riic_icmr1_controller_7bit | (uint8_t)(cks_field << k_riic_cks_shift));
  riic->icmr2 = k_riic_icmr2_default; /* No timeout, no clock sync */
  /* ICMR3 = ACKWP | 0 -- ACKWP=1 is mandatory so internal_read_byte() can
   * toggle ACKBT to NACK the final received byte. Leaving ACKWP=0 here was
   * the silent root cause of "Stop condition timeout" after every read: the
   * peripheral never saw a NACK, kept driving SDA to ACK the imaginary next
   * byte, and the controller's STOP request could not complete. */
  riic->icmr3 = k_riic_icmr3_init;

  /* Enable I2C bus interface */
  riic->iccr1 = k_riic_iccr1_ice;

  /* Post-enable bus release: per RX72N HW manual section 42.2.1 (ICCR1.SCLO
   * and ICCR1.SDAO reset to 0), the RIIC leaves SCLO/SDAO=0 after ICE=1 --
   * which drives both lines LOW -- until it detects its first STOP on the
   * wire. Force an early release by clearing SOWP, writing SCLO=SDAO=1
   * (open-drain release -> external pull-ups bring lines to 3.3V), then
   * re-locking SOWP. Without this the very first transaction sees BBSY=1
   * with both lines stuck low and never reaches ACK detection. The SOWP
   * write-protect must be cleared in its own ICCR1 write before the SCLO/
   * SDAO change; combining them into one write leaves SCLO/SDAO locked. */
  uint8_t iccr1 = riic->iccr1;
  iccr1 &= (uint8_t) ~(uint8_t)k_riic_iccr1_sowp;
  iccr1 |= (uint8_t)(k_riic_iccr1_sclo | k_riic_iccr1_sdao);
  riic->iccr1 = iccr1;
  iccr1 |= (uint8_t)k_riic_iccr1_sowp;
  riic->iccr1 = iccr1;

  /* Pair to the unconditional bit-bang above: now that SCLO=SDAO=1 in the
   * peripheral (set by the SOWP dance), it is safe to route the pads back
   * to RIIC. Raising PMR before the dance would glitch the bus low. */
  internal_riic_bit_bang_handback(channel.value);

  /* Mark channel as initialized in controller mode */
  s_riic_channel_mode[channel.value] = k_riic_mode_controller;

  rx_log_debug(s_tag, "RIIC channel initialized");

  return k_rx_ok;
}

/**
 * @brief Write data to an I2C peripheral device
 *
 * @details
 * Performs a complete I2C write transaction: START, address+W, data bytes,
 * STOP. This function is suitable for writing configuration registers,
 * sending commands, or storing data in EEPROM.
 *
 * ## Transaction Sequence
 *
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + W (0) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- data[0] ----------------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- data[N-1] --------------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ## Common Use Cases
 *
 * | Use Case               | Data Format                    |
 * |------------------------|--------------------------------|
 * | Config register write  | [reg_addr, value]              |
 * | Multi-byte register    | [reg_addr, val0, val1, ...]    |
 * | EEPROM page write      | [addr_hi, addr_lo, data...]    |
 * | Sensor command         | [command_byte]                 |
 *
 * ## Error Handling
 *
 * On any error (timeout or NACK), the function issues a STOP condition
 * to release the bus before returning. The original error code is preserved.
 *
 *
 *
 * @pre riic_init() called for this channel
 * @pre data != nullptr
 * @pre length >= 1 && length <= 256
 * @pre device_addr.value <= 127
 * @post On success: All bytes transferred, bus released
 * @post On failure: Bus released via STOP
 *
 * @note This is a blocking function - returns only after transfer complete.
 * @note STOP is always issued (success or failure) to release the bus.
 *
 * @warning NACK on address usually means device not present or wrong address.
 * @warning NACK on data may indicate device buffer full or write-protected.
 *
 * @par Example: Write Configuration Register
 * @code
 * // Write value 0x42 to register 0x05 of device at address 0x48
 * uint8_t config_data[2] = {0x05, 0x42};  // [reg_addr, value]
 * riic_channel_t channel = {.value = 0};
 * i2c_device_addr_t addr = {.value = 0x48};
 *
 * rx_err_t err = riic_write(channel, addr, config_data, 2);
 * if (err != k_rx_ok) {
 *     handle_i2c_error(err);
 * }
 * @endcode
 *
 * @see riic_init() Initialize channel before use
 * @see riic_read() Read-only transaction
 * @see riic_write_read() Combined write-then-read transaction
 *
 * @callgraph
 */
rx_err_t riic_write(const riic_channel_t    channel,
                    const i2c_device_addr_t device_addr,
                    const uint8_t*          data,
                    const uint16_t          length)
{
  RX_CHECK_NULL_PTR(data, s_tag, "Data pointer is nullptr");
  RX_CHECK_RANGE_TAG(channel.value,
                     k_riic_channel_0,
                     (uint8_t)(k_riic_max_channels - k_riic_last_index_offset),
                     k_rx_err_invalid_arg,
                     s_tag);

  if (device_addr.value > k_riic_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid device address");
    return k_rx_err_invalid_arg;
  }

  if (length == k_riic_length_zero) {
    rx_log_error(s_tag, "Write length cannot be zero");
    return k_rx_err_invalid_arg;
  }

  if (length > k_riic_max_transfer_length) {
    rx_log_error(s_tag, "Write length exceeds maximum");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel mode (range already checked by RX_CHECK_RANGE_TAG above) */
  if (s_riic_channel_mode[channel.value] != k_riic_mode_controller) {
    rx_log_error(s_tag, "RIIC channel not initialized in controller mode");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel.value);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* Pre-clear any status flags left over from a previous transaction (e.g.
   * NACKF, STOP, AL) that normal bus activity does not clear. Per RX72N
   * HW manual section 42.2.7, these are sticky software-clear flags that
   * IICRST wipes but normal transactions do not. Without this pre-clear a
   * stale NACKF survives into the next write and internal_write_byte
   * returns k_rx_err_nack on a byte that was actually ACKed; a stale AL
   * silently clears MST on the next START. */
  riic->icsr2 = k_riic_register_clear;

  /* Set controller transmit mode */
  riic->iccr2 = k_riic_iccr2_mst | k_riic_iccr2_trs;

  /* Send start condition */
  err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (write) */
  err = internal_write_byte(riic, (device_addr.value << k_riic_addr_shift) | k_riic_addr_write_bit);
  if (err != k_rx_ok) {
    rx_err_t stop_err = internal_send_stop(riic);
    (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
    return err;
  }

  /* Send data bytes */
  for (uint16_t i = 0; i < length; i++) {
    err = internal_write_byte(riic, data[i]);
    if (err != k_rx_ok) {
      rx_err_t stop_err = internal_send_stop(riic);
      (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
      return err;
    }
  }

  /* Send stop condition */
  err = internal_send_stop(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Stop condition failed");

  return k_rx_ok;
}

/**
 * @brief Read data from an I2C peripheral device
 *
 * @details
 * Performs a complete I2C read transaction: START, address+R, receive data
 * bytes (ACK all except last), NACK last byte, STOP. This function is suitable
 * for reading device status, sensor data, or sequential memory reads where
 * the device auto-increments its internal pointer.
 *
 * ## Transaction Sequence
 *
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + R (1) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |<--- data[0] -----------------------------<-|
 *     |---- ACK --------------------------------->|
 *     |<--- data[N-2] ---------------------------<-|
 *     |---- ACK --------------------------------->|
 *     |<--- data[N-1] ---------------------------<-|
 *     |---- NACK -------------------------------->|  <- Last byte
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ## When to Use riic_read() vs riic_write_read()
 *
 * | Function | Use When |
 * |----------|----------|
 * | riic_read() | Device auto-increments address, or reading status |
 * | riic_write_read() | Need to specify register address first |
 *
 * ## Common Use Cases
 *
 * - Read device status register (if device supports direct read)
 * - Continue reading after previous write set address pointer
 * - Read from devices with auto-increment address mode
 *
 *
 *
 * @pre riic_init() called for this channel
 * @pre data != nullptr
 * @pre Buffer size >= length
 * @pre length >= 1 && length <= 256
 * @pre device_addr.value <= 127
 * @post On success: data contains received bytes, bus released
 * @post On failure: Bus released via STOP
 *
 * @note This is a blocking function - returns only after transfer complete.
 * @note NACK is automatically sent before reading last byte.
 *
 * @warning Most peripherals require riic_write_read() to specify which
 *          register to read from. Use this function only when the device
 *          supports direct reads or address auto-increment.
 *
 * @par Example: Read Temperature Sensor (Direct Read)
 * @code
 * // Read 2 bytes from temperature sensor at address 0x48
 * // (sensor auto-reports temperature on read)
 * uint8_t temp_data[2];
 * riic_channel_t channel = {.value = 0};
 * i2c_device_addr_t addr = {.value = 0x48};
 *
 * rx_err_t err = riic_read(channel, addr, temp_data, 2);
 * if (err == k_rx_ok) {
 *     int16_t raw = (temp_data[0] << 8) | temp_data[1];
 *     float celsius = raw * 0.0625f;
 * }
 * @endcode
 *
 * @see riic_init() Initialize channel before use
 * @see riic_write() Write-only transaction
 * @see riic_write_read() Combined write-then-read (register read)
 *
 * @callgraph
 */
rx_err_t riic_read(const riic_channel_t    channel,
                   const i2c_device_addr_t device_addr,
                   uint8_t*                data,
                   const uint16_t          length)
{
  RX_CHECK_NULL_PTR(data, s_tag, "Data pointer is nullptr");
  RX_CHECK_RANGE_TAG(channel.value,
                     k_riic_channel_0,
                     (uint8_t)(k_riic_max_channels - k_riic_last_index_offset),
                     k_rx_err_invalid_arg,
                     s_tag);

  if (device_addr.value > k_riic_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid device address");
    return k_rx_err_invalid_arg;
  }

  if (length == k_riic_length_zero) {
    rx_log_error(s_tag, "Read length cannot be zero");
    return k_rx_err_invalid_arg;
  }

  if (length > k_riic_max_transfer_length) {
    rx_log_error(s_tag, "Read length exceeds maximum");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel mode (range already checked by RX_CHECK_RANGE_TAG above) */
  if (s_riic_channel_mode[channel.value] != k_riic_mode_controller) {
    rx_log_error(s_tag, "RIIC channel not initialized in controller mode");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel.value);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* Pre-clear stale flags from any previous cleanup. See riic_write(). */
  riic->icsr2 = k_riic_register_clear;

  /* Set controller receive mode */
  riic->iccr2 = k_riic_iccr2_mst;

  /* Send start condition */
  err = internal_send_start(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Start condition failed");

  /* Send device address (read) -- use the receive-mode helper, NOT
   * internal_write_byte: TEND/TDRE-reassertion don't fire after addr|R in
   * controller-receive mode; RDRF does. */
  err = internal_write_address_for_read(
    riic,
    (uint8_t)((device_addr.value << k_riic_addr_shift) | k_riic_addr_read_bit));
  if (err != k_rx_ok) {
    rx_err_t stop_err = internal_send_stop(riic);
    (void)stop_err; /* Preserve original error, stop is best-effort cleanup */
    return err;
  }

  /* 1-byte read: pre-set ACKBT=1 before the dummy read so the sole byte's
   * 9th-clock ACK slot carries NACK (HUM Fig 42.10). */
  if (length == 1U) {
    riic->icmr3 |= k_riic_icmr3_ackbt_mask;
  }

  (void)riic->icdrr; /* dummy read kicks reception */

  /* Mirror of the read loop in internal_riic_read_phase -- see that
   * function for the why-ACKBT-on-N-2 rationale. */
  for (uint16_t i = 0; i < length; i++) {
    uint32_t timeout = k_riic_timeout_us;
    while (!(riic->icsr2 & k_riic_icsr2_rdrf) && timeout > k_riic_timeout_zero) {
      timeout--;
    }
    if (timeout == k_riic_timeout_zero) {
      rx_log_error_val(s_tag, "Read byte RDRF timeout i=", (uint8_t)i);
      rx_err_t stop_err = internal_send_stop(riic);
      (void)stop_err;
      return k_rx_err_timeout;
    }

    if (length >= 2U && i == (uint16_t)(length - 2U)) {
      riic->icmr3 |= k_riic_icmr3_ackbt_mask;
    }

    if (i == (uint16_t)(length - k_riic_last_index_offset)) {
      riic->icsr2 &= (uint8_t) ~(uint8_t)k_riic_icsr2_stop;
      riic->iccr2 |= k_riic_iccr2_sp;
    }

    data[i] = riic->icdrr;
  }

  uint32_t stop_timeout = k_riic_timeout_us;
  while (!(riic->icsr2 & k_riic_icsr2_stop) && stop_timeout > k_riic_timeout_zero) {
    stop_timeout--;
  }
  if (stop_timeout == k_riic_timeout_zero) {
    rx_log_error(s_tag, "STOP after final ICDRR timeout");
    return k_rx_err_timeout;
  }
  riic->icsr2 &= (uint8_t) ~(uint8_t)(k_riic_icsr2_stop | k_riic_icsr2_nackf);

  return k_rx_ok;
}

/**
 * @brief Combined write-then-read I2C transaction (register read)
 *
 * @details
 * Performs a complete I2C write-read transaction using a repeated START
 * condition. This is the most common pattern for reading registers from
 * I2C peripherals: write the register address, then read the data.
 *
 * ## Transaction Sequence (Register Read)
 *
 * ```
 * Controller                                   Peripheral
 *     |                                            |
 *     |---- START -------------------------------->|
 *     |---- Address + W (0) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |---- Register Address -------------------->|  <- write_data
 *     |<--- ACK ---------------------------------<-|
 *     |                                            |
 *     |---- REPEATED START ---------------------->|  <- No STOP!
 *     |                                            |
 *     |---- Address + R (1) ---------------------->|
 *     |<--- ACK ---------------------------------<-|
 *     |<--- Register Data -----------------------<-|  <- read_data
 *     |---- ACK/NACK ---------------------------->|
 *     |---- STOP -------------------------------->|
 * ```
 *
 * ## Why Repeated START?
 *
 * The repeated START (Sr) is critical for atomic register access:
 *
 * | Approach | Bus Release | Risk |
 * |----------|-------------|------|
 * | STOP + START | Yes | Another controller could interrupt |
 * | Repeated START | No | Transaction is atomic |
 *
 * Many I2C peripherals require repeated START and may not work correctly
 * with separate write-then-read transactions.
 *
 * ## Common Use Cases
 *
 * | Peripheral Type | write_data        | read_length | Purpose |
 * |-----------------|-------------------|-------------|---------|
 * | Temperature     | [reg_addr]        | 2           | Read temp register |
 * | EEPROM          | [addr_hi, addr_lo]| N           | Read N bytes |
 * | Accelerometer   | [reg_addr]        | 6           | Read XYZ axes |
 * | RTC             | [seconds_reg]     | 7           | Read time |
 *
 *
 *
 * @pre riic_init() called for this channel
 * @pre write_data != nullptr
 * @pre read_data != nullptr
 * @pre write_length >= 1 && write_length <= 256
 * @pre read_length >= 1 && read_length <= 256
 * @pre device_addr.value <= 127
 * @post On success: read_data contains register contents, bus released
 * @post On failure: Bus released via STOP
 *
 * @note This is a blocking function - returns only after transfer complete.
 * @note Uses repeated START (no bus release between write and read phases).
 * @note NACK is automatically sent before reading last byte.
 *
 * @par Example: Read WHO_AM_I Register from IMU
 * @code
 * // Read WHO_AM_I register (0x0F) from MPU6050 at address 0x68
 * riic_channel_t channel = {.value = 0};
 * i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 * uint8_t reg_addr = 0x0F;  // WHO_AM_I register
 * uint8_t who_am_i;
 *
 * rx_err_t err = riic_write_read(channel, imu_addr,
 *                                &reg_addr, 1,    // Write: register address
 *                                &who_am_i, 1);   // Read: 1 byte
 *
 * if (err == k_rx_ok && who_am_i == 0x68) {
 *     // IMU detected and responding correctly
 * }
 * @endcode
 *
 * @par Example: Read 6 Bytes of Accelerometer Data
 * @code
 * // Read ACCEL_XOUT_H through ACCEL_ZOUT_L (6 bytes starting at 0x3B)
 * uint8_t reg_addr = 0x3B;
 * uint8_t accel_data[6];
 *
 * rx_err_t err = riic_write_read(channel, imu_addr,
 *                                &reg_addr, 1,
 *                                accel_data, 6);
 *
 * if (err == k_rx_ok) {
 *     int16_t accel_x = (accel_data[0] << 8) | accel_data[1];
 *     int16_t accel_y = (accel_data[2] << 8) | accel_data[3];
 *     int16_t accel_z = (accel_data[4] << 8) | accel_data[5];
 * }
 * @endcode
 *
 * @see riic_init() Initialize channel before use
 * @see riic_write() Write-only transaction
 * @see riic_read() Read-only transaction (no address write)
 *
 * @callgraph
 */
rx_err_t riic_write_read(const riic_channel_t    channel,
                         const i2c_device_addr_t device_addr,
                         const uint8_t*          write_data,
                         const uint16_t          write_length,
                         uint8_t*                read_data,
                         const uint16_t          read_length)
{
  RX_CHECK_NULL_PTR(write_data, s_tag, "Write data pointer is nullptr");
  RX_CHECK_NULL_PTR(read_data, s_tag, "Read data pointer is nullptr");
  RX_CHECK_RANGE_TAG(channel.value,
                     k_riic_channel_0,
                     (uint8_t)(k_riic_max_channels - k_riic_last_index_offset),
                     k_rx_err_invalid_arg,
                     s_tag);

  if (device_addr.value > k_riic_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid device address");
    return k_rx_err_invalid_arg;
  }

  if (write_length == k_riic_length_zero || read_length == k_riic_length_zero) {
    rx_log_error(s_tag, "Read/write length cannot be zero");
    return k_rx_err_invalid_arg;
  }

  if (write_length > k_riic_max_transfer_length || read_length > k_riic_max_transfer_length) {
    rx_log_error(s_tag, "Read/write length exceeds maximum");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel mode (range already checked by RX_CHECK_RANGE_TAG above) */
  if (s_riic_channel_mode[channel.value] != k_riic_mode_controller) {
    rx_log_error(s_tag, "RIIC channel not initialized in controller mode");
    return k_rx_err_invalid_state;
  }

  /* Get RIIC base */
  volatile rx_riic_regs_t* riic = internal_get_riic_base(channel.value);

  /* Wait for bus ready */
  rx_err_t err = internal_wait_bus_ready(riic);
  RX_RETURN_ON_ERROR(err, s_tag, "Bus not ready");

  /* Pre-clear stale flags from any previous cleanup. See riic_write(). */
  riic->icsr2 = k_riic_register_clear;

  /* Perform write phase */
  err = internal_riic_write_phase(riic, device_addr, write_data, write_length);
  if (err != k_rx_ok) {
    return err;
  }

  /* Perform read phase. On success the final byte's NACK + STOP is
   * issued atomically inside internal_read_byte(), so no separate
   * internal_send_stop() call is needed here. */
  err = internal_riic_read_phase(riic, device_addr, read_data, read_length);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * RIIC Peripheral (Device) Mode Functions
 * =============================================================================
 */

/**
 * @enum riic_peripheral_reg_constants_t
 * @brief Register bit-field constants for peripheral mode configuration
 *
 * @details
 * Bit field values used when configuring RIIC for peripheral (device) mode.
 * In peripheral mode the RX72N responds to an external I2C controller (RPi5).
 * All values fit in uint8_t since they are 8-bit register constants.
 *
 * @see riic_init_peripheral()
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_riic_icmr1_peripheral_7bit = 0x00, /**< CKS=0, MS=0 (peripheral mode), 7-bit */
  k_riic_icser_sar0e           = 0x01, /**< ICSER bit 0: enable SAR0 address matching */
  k_riic_sarl_addr_shift       = 1,    /**< 7-bit addr stored in SARL[7:1] */
  k_riic_saru0_7bit            = 0x00, /**< SARU0 = 0 for 7-bit addressing */
} riic_peripheral_reg_constants_t;

/**
 * @enum riic_peripheral_size_constants_t
 * @brief Size and timeout constants for peripheral mode operations
 *
 * @details
 * Timeout values for peripheral mode read/write.
 * k_riic_peripheral_transfer_limit is declared in hardware.h (public API) so
 * callers can statically size their buffers; only the timeout lives here.
 *
 * @see riic_peripheral_read()
 * @see riic_peripheral_write()
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_riic_periph_timeout_us = 1000, /**< 1ms timeout for TDRE poll in write */
} riic_peripheral_size_constants_t;

/**
 * @brief Initialize RIIC channel in peripheral (device) mode
 *
 * @details
 * Configures the RIIC peripheral to act as an I2C peripheral device, responding
 * to a fixed 7-bit I2C address issued by a controller (RPi5). Sets up the
 * peripheral address register (SARL0/SARU0), enables address match detection (ICSER.SAR0E),
 * and leaves the module in peripheral mode (MS=0 in ICMR1).
 *
 * ## Initialization Sequence
 *
 * 1. Validate channel number and device address
 * 2. Enable RIIC module clock (clear MSTPCRB bit)
 * 3. Reset RIIC internal state (IICRST)
 * 4. Configure peripheral address in SARL0/SARU0
 * 5. Enable SAR0 address match detection (ICSER.SAR0E)
 * 6. Configure peripheral mode (MS=0), 7-bit addressing
 * 7. Enable I2C bus interface (ICE)
 * 8. Mark channel as initialized
 *
 *
 *
 * @pre channel.value must be in range [0, k_riic_max_channels - 1]
 * @pre device_addr.value must be <= k_riic_addr_max_7bit (0x7F)
 * @post RIIC peripheral responds to device_addr on the I2C bus
 * @post s_riic_channel_mode[channel.value] == k_riic_mode_peripheral
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe during initialization
 *
 * @see riic_peripheral_read() Read data written by I2C controller
 * @see riic_peripheral_write() Provide data to I2C controller read
 *
 * @since Version 1.0.0
 */
rx_err_t riic_init_peripheral(const riic_channel_t channel, const i2c_device_addr_t device_addr)
{
  if (channel.value >= k_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (device_addr.value > k_riic_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid peripheral device address");
    return k_rx_err_invalid_arg;
  }

  volatile rx_riic_regs_t* const riic = internal_get_riic_base(channel.value);
  if (riic == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RIIC module (clear module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;

  if (channel.value == k_riic_channel_0) {
    system_regs()->mstpcrb &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpb_riic0);
  } else if (channel.value == k_riic_channel_1) {
    system_regs()->mstpcrb &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpb_riic1);
  } else {
    system_regs()->mstpcrc &= ~((uint32_t)k_riic_mstpb_bit_value << k_riic_mstpc_riic2);
  }

  *prcr_reg() = k_rx_prcr_lock;

  /* Reset RIIC */
  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_register_clear;

  /* Set peripheral (device) address in SARL0/SARU0
   * SARL0[7:1] = device_addr.value (7-bit address)
   * SARU0 = 0 for 7-bit mode */
  riic->sarl0 = (uint8_t)(device_addr.value << k_riic_sarl_addr_shift);
  riic->saru0 = k_riic_saru0_7bit;

  /* Enable SAR0 address match detection */
  riic->icser = k_riic_icser_sar0e;

  /* Configure for peripheral mode (MS=0), 7-bit addressing, CKS=0 */
  riic->icmr1 = k_riic_icmr1_peripheral_7bit;
  riic->icmr2 = k_riic_icmr2_default;
  riic->icmr3 = k_riic_icmr2_default;

  /* Enable I2C bus interface */
  riic->iccr1 = k_riic_iccr1_ice;

  /* Mark channel as initialized in peripheral mode */
  s_riic_channel_mode[channel.value] = k_riic_mode_peripheral;

  rx_log_debug(s_tag, "RIIC peripheral mode initialized");

  return k_rx_ok;
}

/**
 * @brief Read bytes written by the I2C controller to this peripheral
 *
 * @details
 * Polls ICSR2.RDRF (Receive Data Register Full) to detect incoming bytes from
 * the I2C controller (RPi5). Reads up to max_length bytes into the caller-supplied
 * buffer. Returns immediately with bytes_read == 0 if no data is currently available
 * (non-blocking). Each byte read clears the RDRF flag automatically on the RX72N.
 *
 * ## Read Algorithm
 *
 * 1. Validate pointers, channel, and max_length
 * 2. Clear *bytes_read to 0
 * 3. Poll ICSR2.RDRF; if set, read ICDRR into data[*bytes_read], increment count
 * 4. Repeat until RDRF clear or max_length reached
 * 5. Return k_rx_ok with actual count in *bytes_read
 *
 *
 *
 * @pre channel must be initialized via riic_init_peripheral()
 * @pre data != nullptr with at least max_length bytes of capacity
 * @pre max_length >= 1 && max_length <= k_riic_peripheral_transfer_limit
 * @post *bytes_read contains the number of bytes placed in data (0 to max_length)
 * @post data[0..(*bytes_read)-1] contain valid received bytes on k_rx_ok
 *
 * @note Non-blocking: returns immediately with available data (bytes_read may be 0)
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe, caller must provide external synchronization
 *
 * @see riic_init_peripheral() Initialize channel first
 * @see riic_peripheral_write() Provide read data to controller
 *
 * @since Version 1.0.0
 */
rx_err_t riic_peripheral_read(const riic_channel_t channel,
                              uint8_t*             data,
                              const uint16_t       max_length,
                              uint16_t*            bytes_read)
{
  if (data == nullptr || bytes_read == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (channel.value >= k_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (max_length == 0) {
    return k_rx_err_invalid_arg;
  }

  if (max_length > k_riic_peripheral_transfer_limit) {
    rx_log_error(s_tag, "Peripheral read max_length exceeds limit");
    return k_rx_err_invalid_arg;
  }

  if (s_riic_channel_mode[channel.value] != k_riic_mode_peripheral) {
    return k_rx_err_invalid_state;
  }

  volatile rx_riic_regs_t* const riic = internal_get_riic_base(channel.value);
  if (riic == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *bytes_read = 0;

  /* Poll ICSR2.RDRF (bit 1) for each available byte - non-blocking */
  while (*bytes_read < max_length) {
    if (!(riic->icsr2 & k_riic_icsr2_rdrf)) {
      break; /* No more data available right now */
    }
    data[*bytes_read] = riic->icdrr;
    (*bytes_read)++;
  }

  return k_rx_ok;
}

/**
 * @brief Write bytes for the I2C controller to read from this peripheral
 *
 * @details
 * Loads data bytes into the RIIC transmit data register (ICDRT) one at a time.
 * For each byte, polls ICSR2.TDRE (Transmit Data Register Empty) until the
 * register is available or a 1 ms timeout expires, then writes the byte.
 * The controller clock-shifts bytes out automatically after each write.
 *
 * ## Write Algorithm
 *
 * 1. Validate pointer, channel, and length
 * 2. For each byte in data[0..length-1]:
 *    a. Poll ICSR2.TDRE with k_riic_periph_timeout_us countdown
 *    b. If timeout, return k_rx_err_timeout
 *    c. Write data[i] to ICDRT
 * 3. Return k_rx_ok
 *
 *
 *
 * @pre channel must be initialized via riic_init_peripheral()
 * @pre data != nullptr with at least length valid bytes
 * @pre length >= 1
 * @post On k_rx_ok: all length bytes have been written to ICDRT
 * @post On k_rx_err_timeout: partial write may have occurred (i bytes written)
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe, caller must provide external synchronization
 *
 * @see riic_init_peripheral() Initialize channel first
 * @see riic_peripheral_read() Receive data from controller
 *
 * @since Version 1.0.0
 */
rx_err_t
riic_peripheral_write(const riic_channel_t channel, const uint8_t* data, const uint16_t length)
{
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (channel.value >= k_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (length == 0 || length > k_riic_peripheral_transfer_limit) {
    rx_log_error(
      s_tag,
      "riic_peripheral_write: length out of range (1..k_riic_peripheral_transfer_limit)");
    return k_rx_err_invalid_arg;
  }

  if (s_riic_channel_mode[channel.value] != k_riic_mode_peripheral) {
    return k_rx_err_invalid_state;
  }

  volatile rx_riic_regs_t* const riic = internal_get_riic_base(channel.value);
  if (riic == nullptr) {
    return k_rx_err_invalid_arg;
  }

  for (uint16_t i = 0; i < length; i++) {
    /* Wait for ICSR2.TDRE (transmit data register empty) */
    uint32_t timeout = k_riic_periph_timeout_us;
    while (!(riic->icsr2 & k_riic_icsr2_tdre) && timeout > k_riic_timeout_zero) {
      timeout--;
    }

    if (timeout == k_riic_timeout_zero) {
      return k_rx_err_timeout;
    }

    riic->icdrt = data[i];
  }

  rx_log_debug(s_tag, "RIIC peripheral write complete");

  return k_rx_ok;
}

/**
 * @brief Deinitialize an RIIC channel that was initialized in peripheral mode
 *
 * @details
 * Disables the peripheral address match (ICSER.SAR0E), clears the SARL0/SARU0
 * address registers, resets the RIIC module, and marks the channel as
 * uninitialized. After this call, riic_init() or riic_init_peripheral() may
 * be called again on the same channel.
 *
 *
 *
 * @pre channel.value must be in range [0, k_riic_max_channels - 1]
 * @pre Channel must have been initialized via riic_init_peripheral()
 * @post s_riic_channel_mode[channel.value] == k_riic_mode_uninitialized on success
 * @post RIIC peripheral address match disabled; hardware in reset state
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe; provide external synchronization if needed
 *
 * @see riic_init_peripheral() Complementary initialization function
 *
 * @since Version 1.0.0
 */
rx_err_t riic_deinit_peripheral(const riic_channel_t channel)
{
  if (channel.value >= k_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (s_riic_channel_mode[channel.value] != k_riic_mode_peripheral) {
    return k_rx_err_invalid_state;
  }

  volatile rx_riic_regs_t* const riic = internal_get_riic_base(channel.value);
  if (riic == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Disable peripheral address match (clear SAR0E in ICSER) */
  riic->icser = (uint8_t)(riic->icser & ~((uint8_t)k_riic_icser_sar0e));

  /* Clear peripheral address registers */
  riic->sarl0 = k_riic_register_clear;
  riic->saru0 = k_riic_register_clear;

  /* Reset the RIIC module */
  riic->iccr1 = k_riic_iccr1_iicrst;
  riic->iccr1 = k_riic_register_clear;

  /* Mark channel as uninitialized */
  s_riic_channel_mode[channel.value] = k_riic_mode_uninitialized;

  rx_log_debug(s_tag, "RIIC peripheral mode deinitialized");

  return k_rx_ok;
}

#endif /* __RX__ */
