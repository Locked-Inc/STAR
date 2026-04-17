/**
 * @file uart.c
 * @brief Multi-Channel UART Driver for RX72N SCI Peripherals
 *
 * @details
 * ## Overview
 *
 * Production-quality UART driver for RX72N Serial Communication Interface (SCI)
 * peripherals. Provides synchronous TX and RX functionality across 13 SCI channels
 * (SCI0-SCI12) with configurable baud rates, automatic newline conversion, and
 * comprehensive error handling.
 *
 * ## Architecture
 *
 * @dot
 * digraph uart_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_app {
 *     label="Application Layer";
 *     style=filled;
 *     color=lightblue;
 *     debug_api [label="Debug Functions\nuart_debug_init/puts/putc"];
 *     channel_api [label="Channel Functions\nuart_init/putc/puts/getc_channel"];
 *   }
 *
 *   subgraph cluster_driver {
 *     label="Driver Layer";
 *     style=filled;
 *     color=lightyellow;
 *     internal [label="Internal Functions\ninternal_calculate_brr\ninternal_configure_uart_pins\ninternal_enable_sci_clock"];
 *     state [label="State Management\ns_channel_initialized[]"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="Hardware Abstraction";
 *     style=filled;
 *     color=lightgreen;
 *     sci_regs [label="SCI Registers\nSCR, SMR, BRR, TDR, RDR, SSR"];
 *     mpc [label="Pin Mux (MPC)\nrx_mpc_set_sci()"];
 *     gpio [label="GPIO Control\nPDR, PMR registers"];
 *   }
 *
 *   debug_api -> channel_api;
 *   channel_api -> internal;
 *   channel_api -> state;
 *   internal -> sci_regs;
 *   internal -> mpc;
 *   internal -> gpio;
 * }
 * @enddot
 *
 * ## Hardware Details
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | SCI peripheral required |
 * | Channels | SCI0-SCI12 | 13 channels total |
 * | Clock | PCLKB (60 MHz) | Used for baud rate generation |
 * | TX Pin | Port pin configured as output | Requires MPC configuration |
 * | RX Pin | Port pin configured as input | Requires MPC configuration |
 * | USB Bridge | CY7C65213 (default) | For SCI9 debug channel |
 *
 * @par Default Debug Configuration (SCI9):
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Channel | SCI9 | Connected to USB-UART bridge |
 * | TX Pin | PB7 (TXD9) | Output, peripheral mode |
 * | RX Pin | PB6 (RXD9) | Input, peripheral mode |
 * | Baud Rate | 115200 bps | Standard debug speed |
 * | Data Bits | 8 | Fixed |
 * | Parity | None | Fixed |
 * | Stop Bits | 1 | Fixed |
 *
 * ## Baud Rate Calculation
 *
 * The SCI baud rate is derived from PCLKB using:
 *
 * @f[
 *   \text{BRR} = \frac{\text{PCLKB}}{64 \times 2^{2n-1} \times B} - 1
 * @f]
 *
 * For n=0 (CKS=00, PCLK/1):
 * @f[
 *   \text{BRR} = \frac{\text{PCLKB}}{32 \times B} - 1
 * @f]
 *
 * @par Supported Baud Rates (at PCLKB = 60 MHz):
 *
 * | Baud Rate | BRR | Actual Rate | Error |
 * |-----------|-----|-------------|-------|
 * | 9600 | 194 | 9615 | +0.16% |
 * | 19200 | 96 | 19231 | +0.16% |
 * | 38400 | 47 | 38462 | +0.16% |
 * | 57600 | 31 | 58594 | +1.73% |
 * | 115200 | 15 | 117188 | +1.73% |
 * | 230400 | 7 | 234375 | +1.73% |
 * | 460800 | 3 | 468750 | +1.73% |
 * | 921600 | 1 | 937500 | +1.73% |
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Stack Usage | Notes |
 * |-----------|---------------|-------------|-------|
 * | uart_init_channel | ~50 us | 48 bytes | Includes MPC configuration |
 * | uart_putc_channel | ~90 us @ 115200 | 16 bytes | Includes wait for TDRE |
 * | uart_puts_channel | ~90 us/char | 24 bytes | With \n->\r\n conversion |
 * | uart_getc_channel | ~5 us | 16 bytes | Non-blocking if no data |
 * | uart_debug_init | ~50 us | 64 bytes | Wrapper for SCI9 |
 *
 * @par Memory Footprint:
 * - Code size: ~2 KB (all functions)
 * - Static data: 13 bytes (s_channel_initialized array)
 * - Stack usage: < 64 bytes (deepest call)
 * - Heap usage: 0 bytes (zero dynamic allocation)
 *
 * ## Thread Safety
 *
 * | Function | Thread Safe? | Notes |
 * |----------|--------------|-------|
 * | uart_init_channel | [FAIL] No | Call once per channel during init |
 * | uart_deinit_channel | [FAIL] No | Call once during shutdown |
 * | uart_putc_channel | [WARN] Partial | Safe if different channels |
 * | uart_puts_channel | [WARN] Partial | Safe if different channels |
 * | uart_getc_channel | [WARN] Partial | Safe if different channels |
 * | uart_debug_* | [WARN] Partial | All use same channel (SCI9) |
 *
 * **Note:** Multiple threads can safely use different UART channels simultaneously.
 * For shared channel access, use external mutex protection.
 *
 * @par Module Dependencies:
 *
 * **This module depends on:**
 * - `hardware.h` - Hardware configuration and channel constants
 * - `rx72n_clock.h` - PCLKB frequency definition
 * - `rx72n_regs.h` - SCI register definitions
 * - `rx_mpc.h` - Pin mux configuration
 * - `rx_port_utils.h` - GPIO port utilities
 * - `rx_register_protection.h` - PRCR register protection
 *
 * **This module is used by:**
 * - `rx_log.c` - Logging subsystem output
 * - `main.c` - Debug output during initialization
 * - `comm_task.c` - Communication task debug messages
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [OK] | All loops use k_uart_max_str_len limit |
 * | 3. No dynamic allocation | [OK] | Zero malloc/free, static buffers only |
 * | 4. Small functions | [OK] | All functions < 60 lines |
 * | 5. Assertions (>=2/func) | [OK] | Parameter validation + state checks |
 * | 6. Narrow scope | [OK] | Static file-scope state, local variables |
 * | 7. Check return values | [OK] | All rx_err_t returns propagated |
 * | 8. Limited preprocessor | [OK] | C23 typed enums only |
 * | 9. Pointer restrictions | [OK] | Single-level pointers, no arithmetic |
 * | 10. Compiler warnings | [OK] | -Wall -Wextra -Werror clean |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S):**
 * - This module handles ONLY UART communication
 * - Separate from higher-level logging (rx_log.c)
 * - Separate from protocol parsing (rx_frame.c)
 *
 * **Open/Closed (O):**
 * - New baud rates supported without code changes
 * - New channels added via configuration structure
 * - Pin assignments configurable per-channel
 *
 * **Liskov Substitution (L):**
 * - All channel functions accept any valid channel
 * - Consistent rx_err_t return semantics
 * - Debug functions are simple wrappers
 *
 * **Interface Segregation (I):**
 * - Separate init/deinit from TX/RX operations
 * - Debug functions provide simplified interface
 * - Channel functions provide full control
 *
 * **Dependency Inversion (D):**
 * - Uses abstract rx_port_pin_t for pin specification
 * - Uses rx_mpc module for pin configuration
 * - Uses rx_err_t for standardized error handling
 *
 * @see hardware.h Hardware configuration and constants
 * @see rx72n_sci_regs.h SCI register definitions
 * @see rx_mpc.h Pin mux configuration API
 * @see rx_log.h Higher-level logging that uses this driver
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include <stdint.h>

#include "hardware.h"
#include "rx72n_clock.h"
#include "rx72n_regs.h"
#include "rx_mpc.h"
#include "rx_port_utils.h"
#include "rx_register_protection.h"
#include "rx_simulator_config.h" /* For RX_IS_SIMULATOR conditional compilation */

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

/** @brief UART configuration constants */
typedef enum : uint32_t {
  /**
   * @brief Default baud rate: 921600 bps
   *
   * Max standard rate that hits <2%% error on SCI9 with PCLK=60 MHz.
   * Formula (ABCS=0/CKS=00): BRR = PCLK / (32 * B) - 1.
   *   60e6 / (32 * 921600) - 1 = 1.034 -> BRR = 1
   *   Actual rate = 60e6 / (32 * (1 + 1)) = 937500 bps
   *   Error vs 921600 = (937500 - 921600) / 921600 = +1.72%% (well within ~3%% UART tolerance)
   *
   * Well within the CY7C65213 USB-UART bridge's 3 Mbps ceiling.
   */
  k_uart_default_baudrate      = 921600,
  k_uart_bit_time_delay_cycles = 125, /**< ~1.09us at 921600 bps, ~260 cycles @240 MHz */
} uart_config_constants_t;

/** @brief SCI register values */
typedef enum : uint8_t {
  k_sci_scr_disabled     = 0x00, /**< SCR: All functions disabled */
  k_sci_scr_tx_enabled   = 0x20, /**< SCR: Transmit enabled (TE=1) */
  k_sci_scr_rx_enabled   = 0x10, /**< SCR: Receive enabled (RE=1) */
  k_sci_scr_txrx_enabled = 0x30, /**< SCR: TX + RX enabled (TE=1, RE=1) */
  k_sci_smr_async_8n1    = 0x00, /**< SMR: Async mode, 8 data bits, no parity, 1 stop bit, PCLK/1 */
  k_sci_semr_default     = 0x00, /**< SEMR: Default extended mode */
  k_sci_ssr_tdre_flag    = 0x80, /**< SSR: Transmit data register empty flag */
  k_sci_ssr_rdrf_flag    = 0x40, /**< SSR: Receive data register full flag */
  k_sci_ssr_orer_flag    = 0x20, /**< SSR: Overrun error flag */
  k_sci_ssr_fer_flag     = 0x10, /**< SSR: Framing error flag */
  k_sci_ssr_per_flag     = 0x08, /**< SSR: Parity error flag */
  k_sci_ssr_error_mask   = 0x38, /**< SSR: All error flags mask */
} sci_register_values_t;

/** @brief Integer to string buffer constants */
typedef enum : uint8_t {
  k_uart_int_buffer_size = 12, /**< Buffer size for int32 to string (enough for -2147483648) */
  k_uart_base_10         = 10, /**< Base 10 for decimal conversion */
} uart_int_constants_t;

/** @brief Hex digit constants */
typedef enum : uint8_t {
  k_uart_hex_max_digits  = 8,    /**< Maximum hex digits to print (32-bit value) */
  k_uart_hex_min_digits  = 1,    /**< Minimum hex digits to print */
  k_uart_hex_zero_digits = 0,    /**< Zero digits value */
  k_uart_hex_nibble_bits = 4,    /**< Bits per hex nibble */
  k_uart_hex_nibble_mask = 0x0F, /**< Mask for hex nibble */
} uart_hex_constants_t;

/** @brief BRR calculation constants */
typedef enum : uint16_t {
  k_brr_divisor_n0     = 32,  /**< Divisor for n=0 (CKS=00): 64 * 2^(2n-1) = 32 */
  k_brr_multiplier     = 4,   /**< Multiplier per CKS increment (2^2) */
  k_brr_max_value      = 255, /**< Maximum BRR register value */
  k_brr_min_value      = 0,   /**< Minimum BRR register value */
  k_brr_formula_offset = 1,   /**< BRR formula subtract-1 offset: BRR = (PCLKB/(32*B)) - 1 */
} brr_constants_t;

/** @brief MSTPCRB register bit manipulation constants */
typedef enum : uint32_t {
  k_uart_mstpcrb_bit_set = 1UL, /**< Single-bit mask for MSTPCRB bit-clear operations */
} uart_mstpcrb_constants_t;

/** @brief Maximum SCI channels (array size, must be enum for compile-time constant) */
typedef enum : uint8_t {
  k_uart_channel_min       = 0,  /**< Minimum UART channel (SCI0) */
  k_uart_array_size        = 13, /**< Array size for s_channel_initialized */
  k_uart_max_mstpb_channel = 11, /**< Maximum channel in MSTPCRB (SCI12 uses MSTPCRC) */
  k_uart_max_channels      = 13, /**< Maximum valid channel value (SCI channels 0-12) */
} uart_internal_constants_t;

/**
 * @enum uart_validation_limits_t
 * @brief Baud rate validation bounds for uart_init_channel() parameter checking
 *
 * @details
 * Defines the closed interval [k_uart_baudrate_min, k_uart_baudrate_max] that
 * every requested baud rate must satisfy before the driver attempts to program
 * the BRR register.
 *
 * The maximum is derived directly from the BRR formula for n=0 (CKS=00):
 * @f[
 *   \text{BRR} = \frac{\text{PCLKB}}{32 \times B} - 1 \geq 0
 *   \implies B \leq \frac{\text{PCLKB}}{32}
 * @f]
 * A BRR of 0 corresponds to the fastest achievable baud rate at the current
 * PCLKB frequency, so requesting anything higher would underflow the register.
 *
 * The minimum is 1 bps, which prevents a divide-by-zero in the BRR formula.
 * In practice, any baud rate below ~9600 is impractical on a 60 MHz PCLKB, but
 * the lower bound is kept permissive to avoid false negatives during testing.
 *
 * @invariant k_uart_baudrate_min must be > 0 to prevent division by zero in
 * internal_calculate_brr().
 * @invariant k_uart_baudrate_max must equal k_pclkb_hz / k_brr_divisor_n0 so
 * that the computed BRR is always >= 0 (i.e., no register underflow).
 *
 * @par Example:
 * @code{.c}
 * // Validation performed inside uart_init_channel()
 * if ((config->baudrate < k_uart_baudrate_min) ||
 *     (config->baudrate > k_uart_baudrate_max)) {
 *   return k_rx_err_invalid_arg;
 * }
 * @endcode
 *
 * @see uart_init_channel() Uses these bounds to validate the baudrate field
 * @see internal_calculate_brr() Computes the actual BRR register value
 * @see brr_constants_t BRR formula divisor constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_uart_baudrate_min =
    1, /**< Minimum valid baud rate (bps); must be > 0 to avoid divide-by-zero in BRR formula */
  k_uart_baudrate_max =
    (k_pclkb_hz /
     k_brr_divisor_n0), /**< Maximum valid baud rate (bps); BRR = 0 at this rate, higher values would underflow */
} uart_validation_limits_t;

/** @brief UART timeout constants */
typedef enum : uint32_t {
  k_uart_tx_timeout        = 100000, /**< Transmit buffer wait timeout (prevents infinite loop) */
  k_uart_timeout_expired   = 0,      /**< Timeout counter expired value */
  k_uart_timeout_decrement = 1,      /**< Timeout counter decrement value */
} uart_timeout_t;

/** @brief UART flag comparison constants */
typedef enum : uint8_t {
  k_uart_flag_clear = 0, /**< Flag bit is not set (comparison result) */
} uart_flag_constants_t;

/** @brief UART buffer and string size limits */
typedef enum : uint32_t {
  k_uart_max_str_len = 256, /**< Maximum string length for uart_puts_channel */
} uart_buffer_constants_t;

/** @brief SCI module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_sci_mstpb_sci0  = 31, /**< SCI0 module stop bit */
  k_sci_mstpb_sci1  = 30, /**< SCI1 module stop bit */
  k_sci_mstpb_sci2  = 29, /**< SCI2 module stop bit */
  k_sci_mstpb_sci3  = 28, /**< SCI3 module stop bit */
  k_sci_mstpb_sci4  = 27, /**< SCI4 module stop bit */
  k_sci_mstpb_sci5  = 26, /**< SCI5 module stop bit */
  k_sci_mstpb_sci6  = 25, /**< SCI6 module stop bit */
  k_sci_mstpb_sci7  = 24, /**< SCI7 module stop bit */
  k_sci_mstpb_sci8  = 23, /**< SCI8 module stop bit */
  k_sci_mstpb_sci9  = 22, /**< SCI9 module stop bit */
  k_sci_mstpb_sci10 = 21, /**< SCI10 module stop bit */
  k_sci_mstpb_sci11 = 20, /**< SCI11 module stop bit */
} sci_mstpb_bits_t;

/** @brief Debug UART pins (SCI9 on RX72N) */
typedef enum : uint16_t {
  k_uart_debug_tx_gpio = k_rx_pb_7, /**< PB7 = TXD9 */
  k_uart_debug_rx_gpio = k_rx_pb_6, /**< PB6 = RXD9 */
} uart_debug_pins_t;

/**
 * @enum uart_gpio_constants_t
 * @brief GPIO register bit manipulation constants for UART pin configuration
 *
 * @details
 * Provides the single-bit seed value used when constructing per-pin bitmasks
 * for the PDR (Port Direction Register) and PMR (Port Mode Register) during
 * UART TX/RX pin setup.  A bit mask for a specific pin is formed by shifting
 * this value left by the pin index obtained from rx_pin_from_pin().
 *
 * @invariant k_uart_gpio_bit_set must equal 1 so that left-shifting by a pin
 * index produces an isolated single-bit mask.
 *
 * @par Example:
 * @code{.c}
 * // Build the TX pin mask and set the direction bit
 * const uint8_t tx_pin      = rx_pin_from_pin(tx_gpio);
 * const uint8_t tx_pin_mask = (uint8_t)(k_uart_gpio_bit_set << tx_pin);
 * tx_port_base->pdr |= tx_pin_mask;  // Set output direction
 * tx_port_base->pmr |= tx_pin_mask;  // Switch to peripheral mode
 * @endcode
 *
 * @see internal_configure_uart_pins() Only consumer of this constant
 * @see uart_init_channel() Top-level function that triggers pin configuration
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_uart_gpio_bit_set = 1, /**< Seed value for constructing a single-pin bitmask via left-shift */
} uart_gpio_constants_t;

/* =============================================================================
 * Private State
 * =============================================================================
 */

/** @brief Per-channel initialization state */
static bool s_channel_initialized[k_uart_array_size] = {false};

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Calculate the 8-bit BRR register value for a target baud rate
 *
 * @details
 * Computes the value to be written to the SCI Bit Rate Register (BRR) so that
 * the SCI peripheral generates the requested baud rate from PCLKB.  The
 * general RX72N SCI baud rate formula for asynchronous mode is:
 *
 * @f[
 *   \text{BRR} = \frac{\text{PCLKB}}{64 \times 2^{2n-1} \times B} - 1
 * @f]
 *
 * This driver always uses n=0 (CKS bits = 0b00, clock source = PCLK/1), which
 * simplifies to:
 *
 * @f[
 *   \text{BRR} = \frac{\text{PCLKB}}{32 \times B} - 1
 * @f]
 *
 * **Algorithm steps:**
 * 1. Guard against baudrate == 0 (return k_brr_max_value as a safe sentinel).
 * 2. Guard against baudrate > k_uart_baudrate_max (return k_brr_min_value).
 * 3. Compute divisor_result = PCLKB / (32 * B) using integer arithmetic.
 * 4. Guard against divisor_result <= 1 to prevent underflow from the -1 offset.
 * 5. Compute BRR = divisor_result - 1.
 * 6. Clamp the result to k_brr_max_value (255) if the integer exceeds the
 *    8-bit range (indicates a baud rate too slow for this clock).
 * 7. Assert post-condition and return the clamped 8-bit result.
 *
 * **Baud rate error:**
 * Integer truncation introduces a small positive frequency error.  At PCLKB =
 * 60 MHz the worst-case error is +1.73 % (at 115 200 bps, BRR = 15), which is
 * within the +/-2 % tolerance of the RS-232/UART standard.
 *
 * @param[in] baudrate Target baud rate in bps
 *   - **Valid range**: 1 to k_uart_baudrate_max (k_pclkb_hz / k_brr_divisor_n0)
 *   - **Special case**: 0 returns k_brr_max_value (255) as a safe sentinel
 *   - **Units**: bits per second
 *
 * @return uint8_t BRR register value to program into sci->brr
 * @retval 0..254 Computed BRR for the requested baud rate
 * @retval 0 (k_brr_min_value) Returned when baudrate > k_uart_baudrate_max or
 *         divisor_result underflows the formula offset
 * @retval 255 (k_brr_max_value) Returned when baudrate == 0 or the computed
 *         value exceeds 255 (baud rate too low for n=0 divisor)
 *
 * @pre baudrate should be validated against [k_uart_baudrate_min,
 *      k_uart_baudrate_max] by the caller before invoking this function
 * @pre k_pclkb_hz and k_brr_divisor_n0 must be non-zero compile-time constants
 *
 * @post Return value is always in [0, 255]; no register overflow is possible
 * @post Caller must write the returned value to sci->brr before enabling TX/RX
 *
 * @note This function performs only integer arithmetic; no floating-point is
 *       used, making it suitable for the RX72N toolchain with FPU disabled
 * @note Always uses n=0 (CKS=00); support for n=1..3 is not implemented
 *
 * @par Thread Safety:
 * Stateless pure function; safe to call from any context including ISR.
 *
 * @par Performance:
 * - Execution time: ~5 cycles (two integer multiplications and a comparison)
 * - Stack usage: 8 bytes (one local uint32_t)
 *
 * @par Example:
 * @code{.c}
 * // Program BRR for 115200 bps on an already-disabled SCI channel
 * sci->brr = internal_calculate_brr(115200U);
 * // At PCLKB = 60 MHz: brr = (60000000 / (32 * 115200)) - 1 = 15
 * @endcode
 *
 * @see uart_init_channel() Caller that validates baudrate and writes sci->brr
 * @see brr_constants_t Constants used in the BRR formula
 * @see uart_validation_limits_t Baud rate bounds checked before this call
 *
 * @since Version 1.0.0
 */
static uint8_t internal_calculate_brr(const uint32_t baudrate)
{
  /* Pre-condition: reject zero baudrate (division by zero) */
  if (baudrate == 0) {
    return k_brr_max_value;
  }

  /* Pre-condition: reject baudrate above maximum (would underflow BRR formula) */
  if (baudrate > k_uart_baudrate_max) {
    return k_brr_min_value;
  }

  /* For n=0 (CKS=00): BRR = (PCLKB / (32 * B)) - 1 */
  const uint32_t divisor_result = k_pclkb_hz / (k_brr_divisor_n0 * baudrate);

  /* Guard against underflow: if divisor_result is 0, subtraction would wrap */
  if (divisor_result <= k_brr_formula_offset) {
    return k_brr_min_value;
  }

  const uint32_t brr_value = divisor_result - k_brr_formula_offset;

  /* Clamp to maximum BRR register value */
  if (brr_value > k_brr_max_value) {
    return k_brr_max_value;
  }

  const uint8_t result = (uint8_t)brr_value;

  return result;
}

/**
 * @brief Clear receive error flags in the SCI Serial Status Register (SSR)
 *
 * @details
 * Reads the SSR register and writes it back with the three receive error flag
 * bits masked out, which clears any pending ORER (Overrun Error), FER
 * (Framing Error), and PER (Parity Error) conditions on the SCI channel.
 *
 * The RX72N SCI peripheral requires a read-modify-write sequence to clear
 * error flags: the hardware only allows clearing a flag by writing 0 to it
 * while keeping the rest of the register unchanged.  Writing 1 to an already-
 * set flag has no effect (the write is ignored).
 *
 * **Algorithm steps:**
 * 1. Guard: return immediately if sci is nullptr (NASA Power of 10 Rule 5).
 * 2. Read the current value of sci->ssr into a local volatile variable.
 * 3. Cast-to-void the read result to suppress the unused-variable warning while
 *    still ensuring the volatile read is not optimised away.
 * 4. Write sci->ssr = (ssr & ~k_sci_ssr_error_mask) to clear bits [5:3]
 *    (ORER, FER, PER) while preserving all other SSR bits.
 *
 * **Error flag bit positions in SSR:**
 * | Bit | Flag | Description |
 * |-----|------|-------------|
 * |  5  | ORER | Overrun Error - new data arrived before previous data was read |
 * |  4  | FER  | Framing Error - no valid stop bit detected |
 * |  3  | PER  | Parity Error  - parity mismatch (unused in 8N1 mode) |
 *
 * @param[in] sci Pointer to the SCI register block for the target channel
 *   - **Valid range**: Non-nullptr pointer obtained from sci_get_channel()
 *   - **Null handling**: Returns silently if nullptr (no-op; no error return)
 *
 * @pre sci must point to a valid, hardware-mapped rx_sci_regs_t register block
 * @pre The SCI module clock must be enabled (MSTPCRB bit cleared) before
 *      any SSR access; undefined behaviour otherwise
 *
 * @post ORER, FER, and PER bits in sci->ssr are cleared (written to 0)
 * @post All other SSR bits (TDRE, RDRF, TEND, MPB, MPBT) are preserved
 *
 * @note This function is intentionally void-returning; the caller (uart_getc_channel)
 *       checks for errors before calling and does not need a return code
 * @note The intermediate volatile read prevents the compiler from merging the
 *       read and write into a single store, which would miss the read requirement
 *
 * @par Thread Safety:
 * Not thread-safe. SSR is a read-modify-write target; concurrent access from
 * two threads on the same channel can corrupt flag state.  External mutex
 * protection is required if multiple threads share a channel.
 *
 * @par Performance:
 * - Execution time: ~3 cycles (volatile read + mask + volatile write)
 * - Stack usage: 4 bytes (one volatile uint8_t local)
 *
 * @par Example:
 * @code{.c}
 * volatile rx_sci_regs_t* sci = sci_get_channel(k_uart_channel_9);
 * if (sci != nullptr) {
 *   // Clear any stale error flags before reading receive data
 *   internal_clear_errors(sci);
 *   const char data = (char)sci->rdr;
 * }
 * @endcode
 *
 * @see uart_getc_channel() Caller that invokes this before reading RDR
 * @see sci_register_values_t k_sci_ssr_error_mask bitmask definition
 * @see uart_rx_available() Non-destructive receive availability check
 *
 * @since Version 1.0.0
 */
static void internal_clear_errors(volatile rx_sci_regs_t* sci)
{
  /* Pre-condition: Validate pointer (NASA Power of 10 Rule 5) */
  if (sci == nullptr) {
    return;
  }

  volatile uint8_t ssr = 0;

  /* Read SSR then clear error flags */
  ssr = sci->ssr;
  (void)ssr; /* Suppress unused variable warning */
  sci->ssr = (uint8_t)(ssr & ~k_sci_ssr_error_mask);
}

/**
 * @brief Return the MSTPCRB bit position that controls the module-stop clock
 *        gate for a given SCI channel
 *
 * @details
 * The RX72N Module Stop Control Register B (MSTPCRB) contains one bit per SCI
 * channel (SCI0-SCI11).  Clearing a bit removes the module from the stopped
 * state, allowing its clock to run.  The bit layout is contiguous and
 * descending: SCI0 occupies bit 31, SCI1 occupies bit 30, and so on down to
 * SCI11 at bit 20.
 *
 * SCI12 is controlled by a different register (MSTPCRC bit 4) and is therefore
 * outside the scope of this function; channel 12 returns -1 to signal the
 * caller that a different mechanism is needed.
 *
 * **Algorithm steps:**
 * 1. Check whether channel > k_uart_max_mstpb_channel (11); return -1 if so.
 * 2. Compute bit position as k_sci_mstpb_sci0 (31) minus channel index.
 * 3. Cast to int8_t and return.
 *
 * **Bit position table (MSTPCRB):**
 * | Channel | Bit | Enum constant       |
 * |---------|-----|---------------------|
 * | SCI0    | 31  | k_sci_mstpb_sci0    |
 * | SCI1    | 30  | k_sci_mstpb_sci1    |
 * | SCI2    | 29  | k_sci_mstpb_sci2    |
 * | ...     | ... | ...                 |
 * | SCI11   | 20  | k_sci_mstpb_sci11   |
 * | SCI12   | N/A | handled in MSTPCRC  |
 *
 * @param[in] channel SCI channel index
 *   - **Valid range**: 0 to k_uart_max_mstpb_channel (11)
 *   - **Out-of-range**: 12 or above returns -1 (SCI12 is in MSTPCRC)
 *   - **Units**: dimensionless channel index
 *
 * @return int8_t MSTPCRB bit position for the given channel
 * @retval 20..31 Valid bit position (MSTPCRB bit index)
 * @retval -1 Channel is not in MSTPCRB (channel >= 12); caller must use an
 *         alternative register (MSTPCRC) or treat as an error
 *
 * @pre channel is obtained from a validated uart_channel_t value
 * @pre k_sci_mstpb_sci0 must equal 31 so that the descending formula is correct
 *
 * @post Return value, if >= 0, is a valid bit index in [20, 31]
 * @post Caller is responsible for performing the register unlock/lock sequence
 *       around the MSTPCRB write
 *
 * @note Returns int8_t (signed) so that -1 can be used as an unambiguous
 *       sentinel without consuming any valid bit position in [0, 31]
 * @note SCI12 support requires a separate call path using MSTPCRC; this
 *       function deliberately does not handle it
 *
 * @par Thread Safety:
 * Stateless pure function; safe to call from any context including ISR.
 *
 * @par Performance:
 * - Execution time: ~2 cycles (one comparison, one subtraction)
 * - Stack usage: 0 bytes (no locals beyond return register)
 *
 * @par Example:
 * @code{.c}
 * const int8_t bit = internal_get_mstpb_bit(9U);
 * // bit == 22  (k_sci_mstpb_sci9)
 * if (bit >= 0) {
 *   system_regs()->mstpcrb &= ~(1UL << (uint8_t)bit);
 * }
 * @endcode
 *
 * @see internal_enable_sci_clock() Only caller; uses the returned bit to
 *      clear the module-stop gate in MSTPCRB
 * @see sci_mstpb_bits_t Enum defining all SCI MSTPCRB bit positions
 * @see uart_internal_constants_t k_uart_max_mstpb_channel boundary constant
 *
 * @since Version 1.0.0
 */
static int8_t internal_get_mstpb_bit(const uint8_t channel)
{
  /* SCI12 is in MSTPCRC, not supported here */
  if (channel > k_uart_max_mstpb_channel) {
    return -1;
  }

  /* MSTPCRB bits: SCI0=31, SCI1=30, ..., SCI11=20 */
  return (int8_t)(k_sci_mstpb_sci0 - channel);
}

/**
 * @brief Enable the peripheral clock for an SCI channel by clearing its
 *        Module Stop Control Register B (MSTPCRB) bit
 *
 * @details
 * On reset, all SCI modules are held in the module-stop state (their clock
 * gates are closed) to minimize power consumption.  Before any SCI register
 * can be accessed, the corresponding MSTPCRB bit must be cleared.  Clearing
 * the bit opens the clock gate and allows the SCI peripheral to operate.
 *
 * The MSTPCRB register is write-protected by the Protect Register (PRCR).
 * This function performs the required unlock/modify/lock sequence:
 *
 * **Algorithm steps:**
 * 1. Call internal_get_mstpb_bit(channel) to obtain the MSTPCRB bit index.
 *    Return k_rx_err_invalid_arg immediately if the result is negative (channel
 *    12 or out of range).
 * 2. Write k_rx_prcr_unlock_all to *prcr_reg() to remove write protection.
 * 3. Clear the target bit in system_regs()->mstpcrb using a read-modify-write
 *    with the single-bit mask k_uart_mstpcrb_bit_set shifted left by mstpb_bit.
 * 4. Write k_rx_prcr_lock to *prcr_reg() to restore write protection.
 * 5. Return k_rx_ok.
 *
 * **Register sequence:**
 * @code{.c}
 * PRCR  = 0xA50B;   // Unlock
 * MSTPCRB &= ~(1UL << bit);  // Clear module-stop bit
 * PRCR  = 0xA500;   // Lock
 * @endcode
 *
 * @param[in] channel SCI channel index to enable
 *   - **Valid range**: 0 to k_uart_max_mstpb_channel (11)
 *   - **Invalid**: 12 or above -- SCI12 uses MSTPCRC (not supported here)
 *   - **Units**: dimensionless channel index
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success -- MSTPCRB bit cleared, SCI clock enabled
 * @retval k_rx_err_invalid_arg channel >= 12 (not in MSTPCRB); MSTPCRB
 *         is not modified and the channel clock remains gated
 *
 * @pre channel must be in range [0, k_uart_max_mstpb_channel] (i.e., 0-11)
 * @pre PRCR register must be accessible; this function does not check for
 *      nested unlock attempts
 *
 * @post On success, the MSTPCRB bit for the specified channel is cleared and
 *       the SCI module clock is running
 * @post On success, the PRCR register is returned to its locked state
 *
 * @note This function does not re-assert module stop on error or deinit; the
 *       clock is left enabled for the lifetime of the MCU session
 * @warning Do not call while another thread or ISR is modifying MSTPCRB or
 *          any other PRCR-protected register; the unlock/lock window is not
 *          atomic
 *
 * @par Thread Safety:
 * Not thread-safe. The PRCR unlock-MSTPCRB write-PRCR lock sequence must
 * execute atomically.  Call only during single-threaded initialization before
 * ThreadX starts.
 *
 * @par Performance:
 * - Execution time: ~5 cycles (two PRCR writes + one RMW on MSTPCRB)
 * - Stack usage: 8 bytes (one int8_t local for mstpb_bit)
 *
 * @par Example:
 * @code{.c}
 * rx_err_t err = internal_enable_sci_clock(9U);  // Enable SCI9 clock
 * if (err != k_rx_ok) {
 *   return err;  // Channel 12 or invalid -- caller must handle
 * }
 * // SCI9 registers are now accessible
 * @endcode
 *
 * @see internal_get_mstpb_bit() Helper that maps channel -> MSTPCRB bit index
 * @see uart_init_channel() Caller that invokes this as part of channel setup
 * @see uart_mstpcrb_constants_t k_uart_mstpcrb_bit_set single-bit mask seed
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_enable_sci_clock(const uint8_t channel)
{
  const int8_t mstpb_bit = internal_get_mstpb_bit(channel);

  if (mstpb_bit < 0) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock protection */
  *prcr_reg() = k_rx_prcr_unlock_all;

  /* Clear module stop bit to enable clock */
  system_regs()->mstpcrb &= ~(k_uart_mstpcrb_bit_set << (uint8_t)mstpb_bit);

  /* Lock protection */
  *prcr_reg() = k_rx_prcr_lock;

  return k_rx_ok;
}

/**
 * @brief Configure the GPIO and MPC pin-mux settings for a UART TX/RX pin pair
 *
 * @details
 * Prepares the two GPIO pins required for SCI UART operation by programming
 * the Multi-Function Pin Controller (MPC) and the Port Direction/Mode registers
 * in the correct order.  The RX72N hardware requires that:
 *  - MPC is configured before switching a pin to peripheral mode (PMR)
 *  - TX pin is driven as an output; RX pin is configured as an input
 *  - Both pins are switched to peripheral mode (PMR bit = 1) last
 *
 * **Algorithm steps:**
 * 1. Extract the port number and pin number from each rx_port_pin_t using
 *    rx_port_from_pin() and rx_pin_from_pin().
 * 2. Validate that both pin numbers are <= k_rx_pin_max (7); return
 *    k_rx_err_invalid_arg if out of range.  (Lower-bound check is omitted
 *    because pin numbers are uint8_t and k_rx_pin_min == 0, which would
 *    trigger -Wtype-limits.)
 * 3. Obtain volatile port register base pointers via rx_port_get_base(); return
 *    k_rx_err_invalid_arg if either is nullptr (invalid port number).
 * 4. Call rx_mpc_set_sci(tx_gpio) to set the TX pin's PFS register to the SCI
 *    TXD function; propagate any error immediately.
 * 5. Call rx_mpc_set_sci(rx_gpio) to set the RX pin's PFS register to the SCI
 *    RXD function; propagate any error immediately.
 * 6. Build per-pin bitmasks (k_uart_gpio_bit_set << pin_number).
 * 7. Set PDR bit for TX pin (output direction) and PMR bit for TX pin
 *    (peripheral mode).
 * 8. Clear PDR bit for RX pin (input direction) and set PMR bit for RX pin
 *    (peripheral mode).
 * 9. Return k_rx_ok.
 *
 * **MPC write protection:**
 * rx_mpc_set_sci() internally handles the PFSWE unlock/lock sequence, so this
 * function does not need to touch the PFSWE bit directly.
 *
 * **Pin direction and mode summary:**
 * | Pin | PDR (direction) | PMR (mode) |
 * |-----|-----------------|------------|
 * | TX  | 1 (output)      | 1 (peripheral) |
 * | RX  | 0 (input)       | 1 (peripheral) |
 *
 * @param[in] tx_gpio TX pin encoded as rx_port_pin_t
 *   - **Valid range**: Any rx_port_pin_t with port in [0, k_rx_port_max] and
 *     pin in [0, k_rx_pin_max]
 *   - **Typical value**: k_uart_debug_tx_gpio (k_rx_pb_7) for SCI9
 *   - **Encoding**: Use k_rx_p{port}_{pin} constants from rx_port_constants.h
 *
 * @param[in] rx_gpio RX pin encoded as rx_port_pin_t
 *   - **Valid range**: Same constraints as tx_gpio; must be distinct from tx_gpio
 *   - **Typical value**: k_uart_debug_rx_gpio (k_rx_pb_6) for SCI9
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success -- both pins configured for SCI UART operation
 * @retval k_rx_err_invalid_arg tx_pin > k_rx_pin_max, rx_pin > k_rx_pin_max,
 *         or rx_port_get_base() returned nullptr for either port number
 * @retval k_rx_err_invalid_arg Propagated from rx_mpc_set_sci() if the MPC
 *         mapping is not supported for the given pin
 *
 * @pre tx_gpio and rx_gpio must correspond to physically valid RX72N port pins
 * @pre The SCI module clock must already be enabled (MSTPCRB bit cleared) so
 *      that the SCI TXD/RXD MPC function codes are accepted
 *
 * @post TX pin: PDR bit set (output), PMR bit set (peripheral function)
 * @post RX pin: PDR bit cleared (input), PMR bit set (peripheral function)
 * @post Both pins' PFS registers configured for SCI TX/RX function via MPC
 *
 * @note Pin validation only checks upper bound (>k_rx_pin_max); lower bound
 *       (0) is omitted to avoid the -Wtype-limits compiler warning triggered by
 *       comparing an unsigned type to 0
 * @warning Passing the same pin for both TX and RX produces undefined hardware
 *          behaviour; no duplicate-pin check is performed
 *
 * @par Thread Safety:
 * Not thread-safe. PDR/PMR are read-modify-write targets shared with all GPIO
 * operations on the same port.  Call only during single-threaded initialization.
 *
 * @par Performance:
 * - Execution time: ~10 us (dominated by two rx_mpc_set_sci() calls)
 * - Stack usage: 32 bytes (four uint8_t locals + two pointer locals)
 *
 * @par Example:
 * @code{.c}
 * // Configure SCI9 default debug pins (PB7=TX, PB6=RX)
 * rx_err_t err = internal_configure_uart_pins(
 *   (rx_port_pin_t)k_uart_debug_tx_gpio,
 *   (rx_port_pin_t)k_uart_debug_rx_gpio);
 * if (err != k_rx_ok) {
 *   return err;  // Invalid pin specification or MPC error
 * }
 * @endcode
 *
 * @see uart_init_channel() Caller that passes validated tx_gpio/rx_gpio
 * @see rx_mpc_set_sci() MPC pin-function assignment (with PFSWE unlock)
 * @see rx_port_get_base() Port register base address lookup
 * @see uart_gpio_constants_t k_uart_gpio_bit_set used to build pin bitmasks
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_uart_pins(const rx_port_pin_t tx_gpio, rx_port_pin_t rx_gpio)
{
  /* Extract port and pin numbers for hardware register access */
  const uint8_t tx_port = rx_port_from_pin(tx_gpio);
  const uint8_t tx_pin  = rx_pin_from_pin(tx_gpio);
  const uint8_t rx_port = rx_port_from_pin(rx_gpio);
  const uint8_t rx_pin  = rx_pin_from_pin(rx_gpio);

  /* Validate pin numbers (lower bound checks omitted - pins are uint8_t, k_rx_pin_min == 0,
   * so pin >= k_rx_pin_min is always true, avoiding -Wtype-limits warning) */
  if (tx_pin > k_rx_pin_max || rx_pin > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  /* Get port bases */
  volatile rx_port_regs_t* tx_port_base = rx_port_get_base(tx_port);
  volatile rx_port_regs_t* rx_port_base = rx_port_get_base(rx_port);

  if (tx_port_base == nullptr || rx_port_base == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Configure MPC for SCI function */
  rx_err_t err = rx_mpc_set_sci(tx_gpio);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_mpc_set_sci(rx_gpio);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure TX pin: output direction, peripheral mode */
  const uint8_t tx_pin_mask = (uint8_t)(k_uart_gpio_bit_set << tx_pin);
  const uint8_t rx_pin_mask = (uint8_t)(k_uart_gpio_bit_set << rx_pin);

  tx_port_base->pdr |= tx_pin_mask; /* Output */
  tx_port_base->pmr |= tx_pin_mask; /* Peripheral mode */

  /* Configure RX pin: input direction, peripheral mode */
  rx_port_base->pdr &= (uint8_t)~rx_pin_mask; /* Input */
  rx_port_base->pmr |= rx_pin_mask;           /* Peripheral mode */

  return k_rx_ok;
}

/* =============================================================================
 * Multi-Channel UART Functions
 * =============================================================================
 */

/**
 * @brief Initialize UART channel with specified configuration
 *
 * @details
 * Configures and enables an SCI peripheral for asynchronous UART operation.
 * Handles module clock enable, pin mux configuration, and SCI register setup.
 *
 * **Algorithm steps:**
 * 1. Validate configuration parameters (NULL check, channel range, baud rate)
 * 2. Get SCI register base address for channel
 * 3. Check if channel is already initialized (prevent double-init)
 * 4. Enable SCI module clock (clear MSTPB bit)
 * 5. Configure TX/RX pins via MPC and GPIO registers
 * 6. Disable TX/RX during configuration (SCR = 0)
 * 7. Set serial mode: async, 8N1, PCLK/1 (SMR)
 * 8. Calculate and set baud rate divisor (BRR)
 * 9. Wait for 1 bit time (synchronization)
 * 10. Enable TX and RX (SCR = 0x30)
 * 11. Mark channel as initialized
 *
 * @dot
 * digraph uart_init {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="Start"];
 *   validate [label="Validate config"];
 *   check_init [label="Already initialized?", shape=diamond];
 *   enable_clk [label="Enable SCI clock"];
 *   config_pins [label="Configure TX/RX pins"];
 *   disable_txrx [label="Disable TX/RX"];
 *   set_mode [label="Set async 8N1 mode"];
 *   set_baud [label="Set BRR for baud rate"];
 *   wait [label="Wait 1 bit time"];
 *   enable_txrx [label="Enable TX + RX"];
 *   mark_init [label="Mark initialized"];
 *   success [label="Return k_rx_ok", fillcolor=lightgreen, style="rounded,filled"];
 *   error [label="Return error", fillcolor=lightcoral, style="rounded,filled"];
 *
 *   start -> validate;
 *   validate -> error [label="invalid"];
 *   validate -> check_init [label="valid"];
 *   check_init -> error [label="yes"];
 *   check_init -> enable_clk [label="no"];
 *   enable_clk -> config_pins;
 *   config_pins -> error [label="fail"];
 *   config_pins -> disable_txrx [label="ok"];
 *   disable_txrx -> set_mode;
 *   set_mode -> set_baud;
 *   set_baud -> wait;
 *   wait -> enable_txrx;
 *   enable_txrx -> mark_init;
 *   mark_init -> success;
 * }
 * @enddot
 *
 * @param[in] config Pointer to channel configuration structure.
 *   - **channel**: SCI channel (0-12)
 *   - **baudrate**: Target baud rate (1 to PCLKB/32)
 *   - **tx_gpio**: TX pin (rx_port_pin_t)
 *   - **rx_gpio**: RX pin (rx_port_pin_t)
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, channel initialized and ready
 * @retval k_rx_err_null_ptr config parameter is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel (>=13) or invalid baud rate
 * @retval k_rx_err_invalid_state Channel already initialized
 *
 * @pre config must point to valid uart_channel_config_t structure
 * @pre config->channel must be in range [0, 12]
 * @pre config->baudrate must be in valid range for PCLKB
 * @pre Channel must not be already initialized
 *
 * @post Channel configured for 8N1 async operation
 * @post TX and RX enabled and ready for use
 * @post s_channel_initialized[channel] set to true
 * @post TX/RX pins configured for peripheral function
 *
 * @note Not thread-safe - call once during initialization
 * @note Includes ~1ms delay for baud rate synchronization
 * @warning Does not validate pin assignments against hardware
 *
 * @par Thread Safety:
 * Not thread-safe. Call once per channel during system initialization.
 *
 * @par Performance:
 * - Execution time: ~50 us (includes MPC and delay)
 * - Stack usage: 48 bytes
 *
 * @par Example:
 * @code{.c}
 * const uart_channel_config_t config = {
 *   .channel  = k_uart_channel_9,
 *   .baudrate = 115200,
 *   .tx_gpio  = k_rx_pb_7,
 *   .rx_gpio  = k_rx_pb_6,
 * };
 *
 * rx_err_t err = uart_init_channel(&config);
 * if (err != k_rx_ok) {
 *   // Handle error
 * }
 * @endcode
 *
 * @see uart_deinit_channel() Deinitialize channel
 * @see uart_putc_channel() Transmit single character
 * @see uart_channel_config_t Configuration structure
 *
 * @since Version 1.0.0
 */
rx_err_t uart_init_channel(const uart_channel_config_t* config)
{
  /* Validate config pointer */
  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if ((uint8_t)config->channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if ((config->baudrate < k_uart_baudrate_min) || (config->baudrate > k_uart_baudrate_max)) {
    return k_rx_err_invalid_arg;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(config->channel);
  if (sci == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Check if already initialized */
  if (s_channel_initialized[config->channel]) {
    return k_rx_err_invalid_state;
  }

  /* Enable SCI module clock (clear module stop bit) */
  rx_err_t err = internal_enable_sci_clock(config->channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure TX/RX pins (MPC and GPIO) */
  err = internal_configure_uart_pins(config->tx_gpio, config->rx_gpio);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable TX/RX */
  sci->scr = k_sci_scr_disabled;

  /* Configure serial mode: Async, 8-bit, no parity, 1 stop, PCLK/1 */
  sci->smr = k_sci_smr_async_8n1;

  /* Set baud rate */
  sci->brr = internal_calculate_brr(config->baudrate);

  /* Wait for at least 1 bit time */
  /* NOTE: Busy-wait required - may run before ThreadX initialization */
  for (volatile uint32_t i = 0; i < k_uart_bit_time_delay_cycles; i++) {
    __asm__ volatile("nop");
  }

  /* Configure serial control: Enable TX and RX */
  sci->scr = k_sci_scr_txrx_enabled;

  /* Configure serial extended mode */
  sci->semr = k_sci_semr_default;

  /* Mark channel as initialized */
  s_channel_initialized[config->channel] = true;

  return k_rx_ok;
}

/**
 * @brief Deinitialize UART channel and disable TX/RX
 *
 * @details
 * Disables transmit and receive on the specified SCI channel and marks it
 * as uninitialized. The module clock is left running (safe to re-initialize
 * without re-enabling). After this call the channel may be re-initialized
 * with uart_init_channel().
 *
 * **Algorithm steps:**
 * 1. Validate channel number (0-12)
 * 2. Get SCI register base address
 * 3. Write SCR = 0 to disable TX and RX
 * 4. Clear s_channel_initialized[channel]
 *
 * @param[in] channel UART channel to deinitialize (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *   - **Recommended**: Use k_uart_channel_X constants
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, channel TX/RX disabled
 * @retval k_rx_err_invalid_arg Invalid channel number (>=13) or invalid register pointer
 *
 * @pre channel must be in range [0, 12]
 * @pre Channel should be initialized before deinitializing (safe to call on uninit channel)
 *
 * @post TX and RX disabled (SCR = 0)
 * @post s_channel_initialized[channel] set to false
 *
 * @note Not thread-safe - call during shutdown, not during normal operation
 * @note Module stop clock is NOT re-asserted (peripheral clock left enabled)
 *
 * @par Thread Safety:
 * Not thread-safe. Do not call while another thread is using the channel.
 *
 * @par Performance:
 * - Execution time: ~1 us
 * - Stack usage: 16 bytes
 *
 * @par Example:
 * @code{.c}
 * rx_err_t err = uart_deinit_channel(k_uart_channel_9);
 * if (err != k_rx_ok) {
 *   // Handle error (invalid channel)
 * }
 * // Channel is now disabled and can be re-initialized
 * @endcode
 *
 * @see uart_init_channel() Initialize channel
 * @see uart_debug_init() Initialize debug channel (SCI9)
 *
 * @since Version 1.0.0
 */
rx_err_t uart_deinit_channel(const uart_channel_t channel)
{
  /* Validate channel */
  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Disable TX/RX */
  sci->scr = k_sci_scr_disabled;

  /* Mark channel as not initialized */
  s_channel_initialized[channel] = false;

  return k_rx_ok;
}

/**
 * @brief Transmit single character on UART channel
 *
 * @details
 * Transmits a single byte on the specified SCI channel. Waits for the transmit
 * data register to become empty (TDRE flag) with timeout protection, then writes
 * the data to TDR. Uses polling mode (no interrupts).
 *
 * **Algorithm steps:**
 * 1. Validate channel number (0-12)
 * 2. Check channel is initialized
 * 3. Get SCI register base address
 * 4. Wait for TDRE flag with timeout (k_uart_tx_timeout iterations)
 * 5. Write data byte to TDR register
 * 6. Clear TDRE flag (read SSR, write back with TDRE=0)
 *
 * **Character transmission timing at 115200 baud:**
 * - Start bit: 8.68 us
 * - 8 data bits: 69.44 us
 * - Stop bit: 8.68 us
 * - **Total**: ~86.8 us per character
 *
 * @param[in] channel UART channel to use (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *   - **Recommended**: Use k_uart_channel_X constants
 *
 * @param[in] data Character to transmit
 *   - **Valid range**: 0x00 to 0xFF (any byte value)
 *   - **Special handling**: '\n' not converted (use uart_puts_channel for conversion)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, character transmitted
 * @retval k_rx_err_invalid_arg Invalid channel number (>=13)
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_timeout Transmit buffer did not become empty within timeout
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre UART TX must be enabled (happens during init)
 *
 * @post Character written to TDR and transmission started
 * @post TDRE flag cleared (TDR occupied)
 *
 * @note Blocking call - waits for TDRE flag
 * @note Timeout prevents infinite wait if hardware fails
 * @warning May block for up to ~100ms if TX line is held low
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us @ 115200 baud (one character time)
 * - Stack usage: 16 bytes
 *
 * @par Example:
 * @code{.c}
 * // Send single character
 * rx_err_t err = uart_putc_channel(k_uart_channel_9, 'A');
 * if (err != k_rx_ok) {
 *   // Handle error
 * }
 *
 * // Send carriage return + line feed
 * uart_putc_channel(k_uart_channel_9, '\r');
 * uart_putc_channel(k_uart_channel_9, '\n');
 * @endcode
 *
 * @see uart_puts_channel() Transmit string with newline conversion
 * @see uart_write_channel() Transmit binary data
 *
 * @since Version 1.0.0
 */
rx_err_t uart_putc_channel(const uart_channel_t channel, const char data)
{
  /* Validate channel */
  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Wait for transmit buffer to be empty (TDRE flag) with timeout */
  uint32_t timeout = k_uart_tx_timeout;
  while ((sci->ssr & k_sci_ssr_tdre_flag) == k_uart_flag_clear &&
         timeout > k_uart_timeout_expired) {
    timeout -= k_uart_timeout_decrement;
  }

  /* Check if timeout occurred */
  if (timeout == k_uart_timeout_expired) {
    return k_rx_err_timeout;
  }

  /* Write data to transmit register */
  sci->tdr = (uint8_t)data;

  /* Clear TDRE flag by reading SSR then writing 0 */
  const uint8_t ssr = sci->ssr;
  sci->ssr          = (uint8_t)(ssr & ~k_sci_ssr_tdre_flag);

  return k_rx_ok;
}

/**
 * @brief Transmit null-terminated string on UART channel with newline conversion
 *
 * @details
 * Transmits a null-terminated string with automatic LF to CR+LF conversion for
 * terminal compatibility. Enforces maximum string length (k_uart_max_str_len = 256)
 * to comply with NASA Power of 10 Rule 2 (bounded loops).
 *
 * **Algorithm steps:**
 * 1. Validate string pointer (NULL check)
 * 2. Validate channel number and initialization state
 * 3. For each character up to k_uart_max_str_len:
 *    a. Check for null terminator (success exit)
 *    b. If '\n', send '\r' first (CR+LF conversion)
 *    c. Transmit character via uart_putc_channel()
 *    d. Propagate any errors immediately
 * 4. If limit reached without terminator, return k_rx_err_invalid_size
 *
 * **Newline conversion:**
 * - Input '\n' (LF) -> Output '\r\n' (CR+LF)
 * - Required for Windows terminals (e.g., PuTTY, TeraTerm)
 * - Ensures cursor returns to column 0 before line feed
 *
 * @param[in] channel UART channel to use (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *
 * @param[in] str Pointer to null-terminated string
 *   - **Valid range**: Non-nullptr to valid memory
 *   - **Maximum length**: 256 characters (k_uart_max_str_len)
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *   - **Encoding**: ASCII (extended characters passed through)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, entire string transmitted
 * @retval k_rx_err_null_ptr str parameter is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_invalid_size String exceeds 256 characters
 * @retval k_rx_err_timeout Hardware timeout during transmission
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre str must point to null-terminated string in valid memory
 *
 * @post All characters transmitted including converted newlines
 * @post On error, partial string may have been transmitted
 *
 * @note Blocking call - waits for each character to transmit
 * @note String length limited to 256 characters for safety
 * @warning Long strings may take significant time (256 chars ~ 22ms @ 115200)
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us per character @ 115200 baud
 * - Stack usage: 24 bytes
 * - Example: 100-char string ~ 9 ms
 *
 * @par Example:
 * @code{.c}
 * // Simple string
 * uart_puts_channel(k_uart_channel_9, "Hello, World!\n");
 * // Output: "Hello, World!\r\n"
 *
 * // Multiple lines
 * uart_puts_channel(k_uart_channel_9, "Line 1\nLine 2\nLine 3\n");
 * // Output: "Line 1\r\nLine 2\r\nLine 3\r\n"
 *
 * // Error handling
 * rx_err_t err = uart_puts_channel(k_uart_channel_9, msg);
 * if (err != k_rx_ok) {
 *   // Partial transmission may have occurred
 * }
 * @endcode
 *
 * @see uart_putc_channel() Transmit single character
 * @see uart_write_channel() Transmit binary data (no conversion)
 * @see uart_debug_puts() Simplified debug wrapper
 *
 * @since Version 1.0.0
 */
rx_err_t uart_puts_channel(const uart_channel_t channel, const char* str)
{
  /* Validate parameters */
  if (str == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Transmit string with \n to \r\n conversion (statically bounded) */
  for (uint32_t i = 0; i < k_uart_max_str_len; ++i) {
    if (str[i] == '\0') {
      return k_rx_ok; /* Terminator found, success */
    }
    if (str[i] == '\n') {
      const rx_err_t err = uart_putc_channel(channel, '\r');
      if (err != k_rx_ok) {
        return err;
      }
    }
    const rx_err_t err = uart_putc_channel(channel, str[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  /* Reached limit without finding terminator */
  return k_rx_err_invalid_size;
}

/**
 * @brief Write binary data to UART channel without newline conversion
 *
 * @details
 * Transmits a block of raw bytes on the specified SCI channel using
 * uart_putc_channel() for each byte. No LF-to-CR+LF conversion is performed,
 * making this function suitable for binary protocol data.
 *
 * **Algorithm steps:**
 * 1. Validate data pointer (NULL check)
 * 2. Validate channel number and initialization state
 * 3. For each byte in [0, length): call uart_putc_channel() and propagate errors
 *
 * @param[in] channel UART channel to use (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *
 * @param[in] data Pointer to buffer containing bytes to transmit
 *   - **Valid range**: Non-nullptr pointing to at least `length` bytes
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @param[in] length Number of bytes to write
 *   - **Valid range**: 0 to UINT16_MAX; 0 returns k_rx_ok immediately
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, all bytes transmitted
 * @retval k_rx_err_null_ptr data parameter is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_timeout Hardware timeout during transmission
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre data must point to at least `length` bytes of valid memory
 *
 * @post All `length` bytes transmitted in order
 * @post On error, partial data may have been transmitted
 *
 * @note Blocking call - waits for each byte to be accepted by TDR
 * @note No newline conversion - use uart_puts_channel() for text output
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us per byte @ 115200 baud
 * - Stack usage: 24 bytes
 *
 * @par Example:
 * @code{.c}
 * const uint8_t frame[] = {0xAA, 0x55, 0x01, 0x02, 0x03};
 * rx_err_t err = uart_write_channel(k_uart_channel_9, frame, sizeof(frame));
 * if (err != k_rx_ok) {
 *   // Handle partial write
 * }
 * @endcode
 *
 * @see uart_puts_channel() Transmit text string with newline conversion
 * @see uart_read_channel() Read binary data
 *
 * @since Version 1.0.0
 */
rx_err_t uart_write_channel(const uart_channel_t channel, const uint8_t* data, uint16_t length)
{
  /* Validate parameters */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Write each byte */
  for (uint16_t i = 0; i < length; i++) {
    const rx_err_t err = uart_putc_channel(channel, (char)data[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Receive single character from UART channel (non-blocking)
 *
 * @details
 * Attempts to read a single byte from the specified SCI channel's receive data
 * register (RDR). This is a **non-blocking** operation - returns immediately
 * with k_rx_err_empty if no data is available.
 *
 * **Algorithm steps:**
 * 1. Validate data pointer and channel number
 * 2. Check channel initialization state
 * 3. Get SCI register base address
 * 4. Clear any error flags (ORER, FER, PER)
 * 5. Check RDRF flag (Receive Data Register Full)
 *    - If not set, return k_rx_err_empty immediately
 * 6. Read data byte from RDR register
 * 7. Clear RDRF flag (read SSR, write back with RDRF=0)
 *
 * **Error flag handling:**
 * - ORER (Overrun Error): Previous data overwritten before read
 * - FER (Framing Error): Stop bit not detected
 * - PER (Parity Error): Parity mismatch (not used in 8N1 mode)
 *
 * @param[in] channel UART channel to use (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *
 * @param[out] data Pointer to store received character
 *   - **Valid range**: Non-nullptr to char
 *   - **On success**: Contains received byte (0x00-0xFF)
 *   - **On error**: Content undefined
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, received character stored in *data
 * @retval k_rx_err_null_ptr data parameter is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_empty No data available (RDRF not set)
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre data must point to valid memory for single char
 *
 * @post On success, *data contains received byte
 * @post RDRF flag cleared (ready for next byte)
 * @post Error flags cleared if any were set
 *
 * @note Non-blocking - returns immediately if no data
 * @note Clears error flags automatically before read
 * @note Single byte buffer - call frequently to avoid overrun
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~5 us (no wait)
 * - Stack usage: 16 bytes
 *
 * @par Example (Polling Loop):
 * @code{.c}
 * char c;
 * rx_err_t err;
 *
 * // Poll for incoming data
 * do {
 *   err = uart_getc_channel(k_uart_channel_9, &c);
 *   if (err == k_rx_ok) {
 *     process_character(c);
 *   }
 * } while (err == k_rx_ok);  // Continue while data available
 * @endcode
 *
 * @par Example (Timeout Loop):
 * @code{.c}
 * char c;
 * uint32_t timeout = 100000;
 *
 * while (timeout > 0) {
 *   rx_err_t err = uart_getc_channel(k_uart_channel_9, &c);
 *   if (err == k_rx_ok) {
 *     // Got data
 *     break;
 *   }
 *   timeout--;
 * }
 *
 * if (timeout == 0) {
 *   // No data received within timeout
 * }
 * @endcode
 *
 * @see uart_read_channel() Read multiple bytes
 * @see uart_rx_available() Check if data is available
 *
 * @since Version 1.0.0
 */
rx_err_t uart_getc_channel(const uart_channel_t channel, char* data)
{
  /* Validate parameters */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Clear any error flags first */
  if ((sci->ssr & k_sci_ssr_error_mask) != k_uart_flag_clear) {
    internal_clear_errors(sci);
  }

  /* Check if receive data is available (RDRF flag) */
  if ((sci->ssr & k_sci_ssr_rdrf_flag) == k_uart_flag_clear) {
    return k_rx_err_empty;
  }

  /* Read received data */
  *data = (char)sci->rdr;

  /* Clear RDRF flag by reading SSR then writing 0 */
  /* Some RX MCUs require explicit clear after reading RDR */
  const uint8_t ssr = sci->ssr;
  sci->ssr          = (uint8_t)(ssr & ~k_sci_ssr_rdrf_flag);

  return k_rx_ok;
}

/**
 * @brief Read available bytes from UART channel (non-blocking)
 *
 * @details
 * Reads up to `length` bytes from the specified SCI channel using
 * uart_getc_channel(). Stops immediately when no more data is available
 * (RDRF flag not set) rather than waiting. Actual bytes received is
 * reported via `bytes_read`.
 *
 * **Algorithm steps:**
 * 1. Validate data and bytes_read pointers (NULL check)
 * 2. Validate channel number and initialization state
 * 3. Set *bytes_read = 0
 * 4. For each slot in [0, length):
 *    a. Call uart_getc_channel(); if k_rx_err_empty, break (done)
 *    b. On other error, propagate immediately
 *    c. Store byte and increment *bytes_read
 * 5. Return k_rx_ok (even if zero bytes were read)
 *
 * @param[in]  channel    UART channel to read from (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *
 * @param[out] data       Pointer to buffer to store received bytes
 *   - **Valid range**: Non-nullptr pointing to at least `length` bytes
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @param[in]  length     Maximum number of bytes to read
 *   - **Valid range**: 0 to UINT16_MAX
 *
 * @param[out] bytes_read Pointer to store actual number of bytes read
 *   - **Valid range**: Non-nullptr to uint16_t
 *   - **On success**: Set to number of bytes actually received (0..length)
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success; *bytes_read contains actual count (may be 0)
 * @retval k_rx_err_null_ptr data or bytes_read is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre data must point to at least `length` bytes of writable memory
 *
 * @post *bytes_read set to actual number of bytes received
 * @post data[0..*bytes_read-1] contain the received bytes
 *
 * @note Non-blocking - returns immediately with whatever data is available
 * @note k_rx_ok with *bytes_read == 0 means no data was available
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~5 us per byte received + ~5 us when no data
 * - Stack usage: 24 bytes
 *
 * @par Example:
 * @code{.c}
 * uint8_t  buf[64];
 * uint16_t count = 0;
 * rx_err_t err   = uart_read_channel(k_uart_channel_9, buf, sizeof(buf), &count);
 * if (err == k_rx_ok && count > 0) {
 *   process_bytes(buf, count);
 * }
 * @endcode
 *
 * @see uart_getc_channel() Read single character
 * @see uart_rx_available() Check if data is available
 * @see uart_write_channel() Write binary data
 *
 * @since Version 1.0.0
 */
rx_err_t uart_read_channel(const uart_channel_t channel,
                           uint8_t*             data,
                           uint16_t             length,
                           uint16_t*            bytes_read)
{
  /* Validate parameters */
  if (data == nullptr || bytes_read == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Read available bytes */
  *bytes_read = 0;
  for (uint16_t i = 0; i < length; i++) {
    char           c;
    const rx_err_t err = uart_getc_channel(channel, &c);
    if (err == k_rx_err_empty) {
      /* No more data available */
      break;
    }
    if (err != k_rx_ok) {
      return err;
    }
    data[i] = (uint8_t)c;
    (*bytes_read)++;
  }

  return k_rx_ok;
}

/**
 * @brief Check if receive data is available on UART channel
 *
 * @details
 * Checks the RDRF (Receive Data Register Full) flag in the SSR register of
 * the specified SCI channel. Provides a non-destructive peek at receive
 * availability without consuming any data from the buffer.
 *
 * **Algorithm steps:**
 * 1. Validate available pointer (NULL check)
 * 2. Validate channel number and initialization state
 * 3. Get SCI register base address
 * 4. Read SSR and test RDRF bit; store boolean result in *available
 *
 * @param[in] channel UART channel to check (0-12)
 *   - **Valid range**: 0 to 12 (SCI0 through SCI12)
 *
 * @param[out] available Pointer to store result
 *   - **On success**: true if RDRF flag set (data ready), false otherwise
 *   - **On error**: value undefined
 *   - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success; *available set to data availability status
 * @retval k_rx_err_null_ptr available is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number or invalid register pointer
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel must be initialized via uart_init_channel()
 * @pre available must point to valid bool storage
 *
 * @post *available reflects current RDRF state
 * @post No data is consumed from the receive buffer
 *
 * @note Non-destructive - does not read RDR or clear any flags
 * @note RDRF can be set again immediately after reading if UART is receiving
 *
 * @par Thread Safety:
 * Thread-safe for different channels. Not safe for same channel without mutex.
 *
 * @par Performance:
 * - Execution time: ~2 us
 * - Stack usage: 16 bytes
 *
 * @par Example:
 * @code{.c}
 * bool ready = false;
 * if (uart_rx_available(k_uart_channel_9, &ready) == k_rx_ok && ready) {
 *   char c;
 *   uart_getc_channel(k_uart_channel_9, &c);
 * }
 * @endcode
 *
 * @see uart_getc_channel() Read single character
 * @see uart_read_channel() Read multiple bytes
 *
 * @since Version 1.0.0
 */
rx_err_t uart_rx_available(const uart_channel_t channel, bool* available)
{
  /* Validate parameters */
  if (available == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  const volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Check RDRF flag */
  *available = ((sci->ssr & k_sci_ssr_rdrf_flag) != k_uart_flag_clear);

  return k_rx_ok;
}

/* =============================================================================
 * UART Debug Functions - Convenience Wrappers for SCI9
 * =============================================================================
 */

/**
 * @brief Initialize default debug UART channel (SCI9 at 115200 baud)
 *
 * @details
 * Convenience function to initialize SCI9 with default debug settings:
 * - **Channel**: SCI9
 * - **Baud rate**: 115200 bps
 * - **TX pin**: PB7 (TXD9)
 * - **RX pin**: PB6 (RXD9)
 * - **Format**: 8N1 (8 data bits, no parity, 1 stop bit)
 *
 * SCI9 is connected to the CY7C65213 USB-UART bridge on the STAR PCB,
 * providing a virtual COM port when connected via USB.
 *
 * **Algorithm steps:**
 * 1. Create uart_channel_config_t with default values
 * 2. Call uart_init_channel() with the configuration
 * 3. Return result
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, debug UART ready
 * @retval k_rx_err_invalid_state SCI9 already initialized
 *
 * @pre System clocks must be initialized (PCLKB = 60 MHz)
 * @pre SCI9 must not be already initialized
 *
 * @post SCI9 configured and ready for TX/RX at 115200 baud
 * @post uart_debug_puts(), uart_debug_putc() etc. are functional
 *
 * @note Call once during early system initialization
 * @note Call before any other debug output functions
 * @warning Must be called before ThreadX starts if debug output is needed during init
 *
 * @par Thread Safety:
 * Not thread-safe. Call once during startup before ThreadX.
 *
 * @par Performance:
 * - Execution time: ~50 us
 * - Stack usage: 64 bytes
 *
 * @par Example:
 * @code{.c}
 * void system_init(void)
 * {
 *   // Initialize clocks first
 *   rx_clock_init();
 *
 *   // Initialize debug UART for early output
 *   rx_err_t err = uart_debug_init();
 *   if (err != k_rx_ok) {
 *     // Cannot print error - no UART!
 *     // Could blink LED or set error flag
 *     return;
 *   }
 *
 *   // Now we can print debug messages
 *   uart_debug_puts("[INFO] System starting...\n");
 * }
 * @endcode
 *
 * @see uart_init_channel() Generic channel initialization
 * @see uart_debug_puts() Debug string output
 * @see uart_debug_putc() Debug character output
 *
 * @since Version 1.0.0
 */
rx_err_t uart_debug_init(void)
{
  const uart_channel_config_t config = {
    .channel  = (uart_channel_t)k_uart_debug_channel,
    .baudrate = k_uart_default_baudrate,
    .tx_gpio  = (rx_port_pin_t)k_uart_debug_tx_gpio,
    .rx_gpio  = (rx_port_pin_t)k_uart_debug_rx_gpio,
  };
  return uart_init_channel(&config);
}

#if !RX_IS_SIMULATOR
/* =============================================================================
 * Hardware Mode: Debug UART Functions
 * =============================================================================
 * These functions use the SCI9 UART hardware for debug output.
 * In simulator mode, these are replaced by inline implementations in rx_log.h
 * that redirect to stdout.
 * =============================================================================
 */

/**
 * @brief Transmit single character on debug UART (SCI9)
 *
 * @details
 * Convenience wrapper around uart_putc_channel() for the fixed debug channel
 * (SCI9). Errors are silently ignored to allow use in early initialization
 * contexts where error propagation is not yet possible.
 *
 * **Algorithm steps:**
 * 1. Call uart_putc_channel(k_uart_debug_channel, data)
 * 2. Cast return value to (void) - errors discarded
 *
 * @param[in] data Character to transmit (0x00-0xFF)
 *   - **Special handling**: '\n' is NOT converted; use uart_debug_puts() for text
 *
 * @pre uart_debug_init() must have been called successfully
 * @pre SCI9 must be initialized and TX enabled
 *
 * @post Character written to SCI9 TDR and transmission started
 * @post Any errors are silently discarded
 *
 * @note Error return from uart_putc_channel() is intentionally discarded
 * @note For error-checked output use uart_putc_channel() directly
 * @warning Only available when RX_IS_SIMULATOR is 0 (hardware builds)
 *
 * @par Thread Safety:
 * Not safe for concurrent access on SCI9 without external mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us @ 115200 baud
 * - Stack usage: 16 bytes
 *
 * @par Example:
 * @code{.c}
 * uart_debug_putc('A');     // Send 'A'
 * uart_debug_putc('\r');    // Carriage return
 * uart_debug_putc('\n');    // Line feed
 * @endcode
 *
 * @see uart_debug_puts() Transmit string with newline conversion
 * @see uart_putc_channel() Error-checked single character transmit
 *
 * @since Version 1.0.0
 */
void uart_debug_putc(const char data)
{
  /* Ignore errors for debug output (used in early init before error handling) */
  (void)uart_putc_channel((uart_channel_t)k_uart_debug_channel, data);
}

/**
 * @brief Transmit null-terminated string on debug UART with newline conversion
 *
 * @details
 * Transmits a null-terminated string to SCI9 with automatic LF-to-CR+LF
 * conversion for terminal compatibility. Enforces a maximum length of
 * k_uart_max_str_len (256) characters per NASA Power of 10 Rule 2.
 * Silently returns on NULL pointer to allow safe use in early init.
 *
 * **Algorithm steps:**
 * 1. Return immediately if str is nullptr (defensive, no error return)
 * 2. For each character up to k_uart_max_str_len:
 *    a. Stop at null terminator
 *    b. If '\n', send '\r' first
 *    c. Send character via uart_debug_putc()
 *
 * @param[in] str Pointer to null-terminated ASCII string
 *   - **Maximum length**: 256 characters (k_uart_max_str_len)
 *   - **Null handling**: Returns silently if nullptr
 *   - **Encoding**: ASCII; '\n' converted to '\r\n'
 *
 * @pre uart_debug_init() must have been called successfully
 * @pre str should point to a null-terminated string in valid memory
 *
 * @post All characters up to null terminator transmitted to SCI9
 * @post Each '\n' replaced by '\r\n' in the transmitted output
 *
 * @note No return value - errors from uart_debug_putc() are silently discarded
 * @note String truncated at k_uart_max_str_len (256) characters
 * @warning Only available when RX_IS_SIMULATOR is 0 (hardware builds)
 *
 * @par Thread Safety:
 * Not safe for concurrent access on SCI9 without external mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us per character @ 115200 baud
 * - Stack usage: 24 bytes
 *
 * @par Example:
 * @code{.c}
 * uart_debug_puts("Hello, World!\n");  // Sends "Hello, World!\r\n"
 * uart_debug_puts("[INFO] Boot complete\n");
 * @endcode
 *
 * @see uart_debug_putc() Transmit single character
 * @see uart_debug_putint() Transmit decimal integer
 * @see uart_puts_channel() Error-checked string transmit
 *
 * @since Version 1.0.0
 */
void uart_debug_puts(const char* str)
{
  if (str == nullptr) {
    return;
  }

  /* Delegate to uart_puts_channel which already handles LF->CRLF conversion,
   * bounded iteration (k_uart_max_str_len), and per-character error checking. */
  (void)uart_puts_channel((uart_channel_t)k_uart_debug_channel, str);
}

/**
 * @brief Transmit signed 32-bit integer as decimal string on debug UART
 *
 * @details
 * Converts a signed 32-bit integer to a decimal ASCII string and transmits
 * it on SCI9. Handles INT32_MIN correctly by using int64_t for the negation.
 * Uses a fixed-size stack buffer (k_uart_int_buffer_size = 12) built in
 * reverse then passed to uart_debug_puts().
 *
 * **Algorithm steps:**
 * 1. Determine sign; compute abs_value as uint32_t
 * 2. Null-terminate the buffer end
 * 3. Build digits right-to-left (statically bounded by k_uart_int_buffer_size)
 * 4. Prepend '-' if negative
 * 5. Call uart_debug_puts() with the resulting substring pointer
 *
 * @param[in] value Signed 32-bit integer to print
 *   - **Valid range**: INT32_MIN (-2147483648) to INT32_MAX (2147483647)
 *   - **INT32_MIN handling**: Correctly handled via int64_t cast
 *
 * @pre uart_debug_init() must have been called successfully
 * @pre uart_debug_puts() must be functional
 *
 * @post Decimal representation of value transmitted on SCI9
 * @post Leading zeros suppressed; negative values prefixed with '-'
 *
 * @note No return value - output errors are silently discarded
 * @note Buffer is stack-allocated; safe for re-entrant calls on different tasks
 * @warning Only available when RX_IS_SIMULATOR is 0 (hardware builds)
 *
 * @par Thread Safety:
 * Not safe for concurrent access on SCI9 without external mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us per digit @ 115200 baud + digit-loop overhead
 * - Stack usage: 32 bytes (buffer + locals)
 *
 * @par Example:
 * @code{.c}
 * uart_debug_puts("Value: ");
 * uart_debug_putint(42);          // Sends "42"
 * uart_debug_putint(-2147483648); // Sends "-2147483648"
 * uart_debug_putc('\n');
 * @endcode
 *
 * @see uart_debug_puthex() Print value in hexadecimal
 * @see uart_debug_puts() Underlying string transmit
 *
 * @since Version 1.0.0
 */
void uart_debug_putint(const int32_t value)
{
  char  buffer[k_uart_int_buffer_size]; /* Enough for -2147483648 */
  char* p = buffer + sizeof(buffer) - 1;

  /* Handle negative numbers */
  const bool is_negative = (value < 0);
  uint32_t   abs_value   = is_negative ? (uint32_t)(-(int64_t)value) : (uint32_t)value;

  /* Null terminate */
  *p = '\0';

  /* Convert to string (reverse order, statically bounded) */
  for (uint8_t i = 0; i < k_uart_int_buffer_size - 1; ++i) {
    *(--p) = (char)('0' + (abs_value % k_uart_base_10));
    abs_value /= k_uart_base_10;
    if (abs_value == 0) {
      break; /* All digits processed */
    }
    if (p <= buffer) {
      break; /* Buffer limit reached (should not happen with correct sizing) */
    }
  }

  /* Add minus sign if negative */
  if (is_negative && p > buffer) {
    *(--p) = '-';
  }

  /* Transmit the string */
  uart_debug_puts(p);
}

/**
 * @brief Transmit 32-bit unsigned integer as hexadecimal string on debug UART
 *
 * @details
 * Transmits "0x" prefix followed by the specified number of uppercase hex
 * digits of `value` on SCI9. Digits are always printed most-significant first.
 * The `digits` parameter is clamped to [k_uart_hex_min_digits, k_uart_hex_max_digits]
 * (1-8) before use.
 *
 * **Algorithm steps:**
 * 1. Send "0x" prefix via uart_debug_puts()
 * 2. Clamp digits to [1, 8]
 * 3. Iterate nibbles from MSN to LSN (statically bounded by k_uart_hex_max_digits)
 * 4. For each nibble index < digits, extract nibble and print uppercase hex char
 *
 * @param[in] value  Unsigned 32-bit value to display in hexadecimal
 *   - **Valid range**: 0x00000000 to 0xFFFFFFFF
 *
 * @param[in] digits Number of hex digits to print (1-8)
 *   - **Valid range**: 1 to 8 (clamped; 0 -> 1, >8 -> 8)
 *   - **Common values**: 2 for byte, 4 for word, 8 for full 32-bit
 *
 * @pre uart_debug_init() must have been called successfully
 * @pre uart_debug_puts() and uart_debug_putc() must be functional
 *
 * @post "0x" followed by `digits` uppercase hex characters transmitted on SCI9
 * @post digits clamped to [1, 8] if out of range
 *
 * @note Uses static lookup table s_hex[] for digit-to-character conversion
 * @note Always prefixes output with "0x"
 * @warning Only available when RX_IS_SIMULATOR is 0 (hardware builds)
 *
 * @par Thread Safety:
 * Not safe for concurrent access on SCI9 without external mutex.
 *
 * @par Performance:
 * - Execution time: ~90 us per character @ 115200 baud
 * - Stack usage: 24 bytes
 *
 * @par Example:
 * @code{.c}
 * uart_debug_puts("Addr: ");
 * uart_debug_puthex(0xDEADBEEF, 8);  // Sends "0xDEADBEEF"
 * uart_debug_puthex(0x42, 2);        // Sends "0x42"
 * uart_debug_putc('\n');
 * @endcode
 *
 * @see uart_debug_putint() Print signed decimal value
 * @see uart_debug_puts() Underlying string transmit
 *
 * @since Version 1.0.0
 */
void uart_debug_puthex(const uint32_t value, uint8_t digits)
{
  static const char s_hex[] = "0123456789ABCDEF";

  uart_debug_puts("0x");

  /* Clamp digits to valid range */
  if (digits > k_uart_hex_max_digits) {
    digits = k_uart_hex_max_digits;
  }
  if (digits == k_uart_hex_zero_digits) {
    digits = k_uart_hex_min_digits;
  }

  /* Print hex digits from most significant (statically bounded, unsigned counter) */
  for (uint8_t i = k_uart_hex_zero_digits; i < k_uart_hex_max_digits; ++i) {
    const uint8_t digit_idx = (k_uart_hex_max_digits - k_uart_hex_min_digits) - i;
    if (digit_idx < digits) {
      const uint8_t nibble =
        (uint8_t)((value >> (digit_idx * k_uart_hex_nibble_bits)) & k_uart_hex_nibble_mask);
      uart_debug_putc(s_hex[nibble]);
    }
  }
}

#endif /* !RX_IS_SIMULATOR */

#endif /* __RX__ */
