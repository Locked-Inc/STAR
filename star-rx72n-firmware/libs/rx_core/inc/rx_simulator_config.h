/**
 * @file rx_simulator_config.h
 * @brief Configuration for e^2 studio simulator support
 *
 * @details
 * This header provides simulator-specific configuration for testing
 * firmware logic without real hardware. The Renesas e^2 studio simulator
 * cannot fully model certain hardware features:
 *
 * **Hardware limitations in simulator**:
 * - External oscillators (24 MHz crystal) - Not modeled
 * - PLL/PPLL lock behavior - Status flags never set
 * - UART/SCI peripheral hardware - TX/RX don't work
 * - USB controller - Limited or no enumeration
 * - SPI transfers - No real external devices
 * - Timing accuracy - Not cycle-accurate
 *
 * **WARNING**: Simulator builds are FOR LOGIC TESTING ONLY.
 * Do not use for timing validation, peripheral verification, or
 * hardware-specific testing. Always validate critical paths on
 * real hardware before deployment.
 *
 * @par Intended Use Cases:
 * - Algorithm validation (PID controllers, state machines)
 * - Protocol parsing and encoding logic
 * - Error handling path coverage
 * - Unit test development
 * - Interactive debugging of control flow
 *
 * @par NOT Suitable For:
 * - Clock tree validation
 * - Interrupt latency measurements
 * - DMA transfer verification
 * - USB enumeration testing
 * - SPI communication reliability
 * - Real-time performance analysis
 *
 * @par Usage:
 * Define RX_SIMULATOR_MODE during compilation to enable simulator support:
 * @code
 * // In e^2 studio:
 * // Project Properties -> C/C++ Build -> Settings -> Compiler -> Preprocessor
 * // Add symbol: RX_SIMULATOR_MODE
 *
 * // Or via command line:
 * gcc -DRX_SIMULATOR_MODE -o firmware.elf ...
 * @endcode
 *
 * @par Example:
 * @code
 * \#include "rx_simulator_config.h"
 *
 * void init_hardware(void)
 * {
 * #if RX_IS_SIMULATOR
 *   // Simulator: Skip hardware polling loops
 *   return k_rx_ok;
 * #else
 *   // Hardware: Wait for PLL stable
 *   while ((OSCOVFSR & PLL_STABLE) == 0) {
 *     // ...
 *   }
 * #endif
 * }
 * @endcode
 *
 * @since Version 1.0.0
 *
 * @see rx_clock_power_init.c Clock initialization with simulator support
 * @see rx_log.h Logging infrastructure with simulator output
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include <stdint.h>

/* =========================================================================
 * Simulator Mode Detection
 * =========================================================================
 */

/**
 * @def RX_IS_SIMULATOR
 * @brief Compile-time flag indicating simulator build
 *
 * @details
 * Evaluates to 1 when RX_SIMULATOR_MODE is defined, 0 otherwise.
 * Use this macro for readable conditional compilation:
 *
 * @code
 * #if RX_IS_SIMULATOR
 *   // Simulator-specific code
 * #else
 *   // Hardware-specific code
 * #endif
 * @endcode
 *
 * @note Prefer RX_IS_SIMULATOR over direct \#ifdef RX_SIMULATOR_MODE
 *       for consistency and readability.
 */
#ifdef RX_SIMULATOR_MODE
#define RX_IS_SIMULATOR 1

/**
   * @warning RX_SIMULATOR_MODE: This build is FOR SIMULATOR ONLY.
   *
   * Do not flash to hardware. Simulator builds skip hardware initialization
   * steps and may not configure peripherals correctly for real operation.
   */
/* Compile-time note: simulator mode active -- do not flash to hardware.
 * See Doxygen @warning above for rationale. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#warning "RX_SIMULATOR_MODE: This build is FOR SIMULATOR ONLY. Do not flash to hardware."
#pragma GCC diagnostic pop
#else
#define RX_IS_SIMULATOR 0
#endif

/* =========================================================================
 * Simulator-Specific Constants
 * =========================================================================
 */

/**
 * @enum simulator_clock_timing_t
 * @brief Simulator-specific clock stabilization timeouts
 *
 * @details
 * Hardware requires multi-millisecond delays for oscillator and PLL
 * stabilization (polling OSCOVFSR register bits until they set).
 * Simulator cannot model these delays as the hardware circuits don't
 * exist, so we provide instant-ready constants.
 *
 * **Hardware behavior**:
 * - Main oscillator: ~10 ms stabilization (2.4M instruction cycles)
 * - PLL: ~200 us stabilization (poll OSCOVFSR bit 2)
 * - PPLL: ~200 us stabilization (poll OSCOVFSR bit 3)
 *
 * **Simulator behavior**:
 * - All clocks: Assume instant ready (skip polling loops)
 *
 * @par Usage:
 * These constants are for documentation and future extensibility.
 * Current implementation simply skips polling loops entirely when
 * RX_IS_SIMULATOR is true.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_sim_clock_instant = 1, /**< Skip delay entirely (assume immediate ready) */
} simulator_clock_timing_t;
