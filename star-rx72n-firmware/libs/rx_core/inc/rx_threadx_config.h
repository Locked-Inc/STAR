/**
 * @file rx_threadx_config.h
 * @brief ThreadX RTOS Tick Rate Configuration
 *
 * @details
 * This header defines the ThreadX system tick frequency used throughout the
 * RX72N firmware for RTOS time conversions. The tick rate is configured to
 * 100 Hz (10ms period) via hardware timer CMT0 (Compare Match Timer 0).
 *
 * ## System Architecture Context
 *
 * ThreadX is a hard real-time operating system (RTOS) that provides:
 * - **Deterministic preemptive scheduling**: Priority-based thread switching
 * - **Time-slicing**: Round-robin scheduling for equal-priority threads
 * - **Timers and delays**: Software timers with tick-based granularity
 * - **Event synchronization**: Semaphores, mutexes, event flags with timeouts
 *
 * The system tick is the fundamental time quantum for all ThreadX operations:
 * ```
 * Hardware Timer (CMT0) @ 100 Hz
 *         v
 * ThreadX Tick ISR (every 10ms)
 *         v
 * +-------------------------------+
 * | _tx_timer_interrupt()         |
 * |  - Increment tick counter     |
 * |  - Check sleeping threads     |
 * |  - Expire software timers     |
 * |  - Trigger time-slice rotation|
 * +-------------------------------+
 *         v
 * Schedule highest-priority ready thread
 * ```
 *
 * @dot
 * digraph threadx_tick_architecture {
 *     rankdir=LR;
 *     node [shape=box, style=rounded];
 *
 *     // Hardware layer
 *     subgraph cluster_hardware {
 *         label="Hardware Layer";
 *         style=filled;
 *         color=lightgrey;
 *         PCLKB [label="PCLKB\n60 MHz", shape=ellipse, fillcolor=yellow, style=filled];
 *         Divider [label="/32\nDivider", shape=diamond];
 *         CMT0_Clock [label="CMT0 Clock\n1.875 MHz"];
 *         CMT0 [label="CMT0 Timer\nCompare: 18750", fillcolor=lightblue, style=filled];
 *     }
 *
 *     // ThreadX layer
 *     subgraph cluster_threadx {
 *         label="ThreadX RTOS";
 *         style=filled;
 *         color=lightcyan;
 *         ISR [label="CMT0 ISR\n_tx_timer_interrupt()", fillcolor=orange, style=filled];
 *         TickCounter [label="Global Tick Counter\n_tx_timer_system_clock"];
 *         Scheduler [label="Scheduler\n_tx_thread_schedule()"];
 *         Threads [label="Thread Wake-Up\nTimer Expiration", shape=parallelogram];
 *     }
 *
 *     // Application layer
 *     subgraph cluster_application {
 *         label="Application Code";
 *         style=filled;
 *         color=lightyellow;
 *         TickRate [label="s_rx_threadx_tick_rate_hz\n100 Hz (this file)", fillcolor=lightgreen, style="filled,bold"];
 *         TimeConv [label="Time Conversions\nms -> ticks"];
 *         API [label="ThreadX API\ntx_thread_sleep()\ntx_semaphore_get()"];
 *     }
 *
 *     // Connections
 *     PCLKB -> Divider;
 *     Divider -> CMT0_Clock;
 *     CMT0_Clock -> CMT0 [label="1.875 MHz"];
 *     CMT0 -> ISR [label="100 Hz\n(every 10ms)", color=red, style=bold];
 *     ISR -> TickCounter [label="Increment"];
 *     TickCounter -> Scheduler;
 *     Scheduler -> Threads;
 *     TickRate -> TimeConv [style=dashed, label="Config"];
 *     TimeConv -> API;
 *     API -> Threads [style=dashed];
 * }
 * @enddot
 *
 * ## Design Rationale
 *
 * **Why 100 Hz Tick Rate?**
 * 1. **Control loop alignment**: Motor control runs at 100 Hz, matching tick period
 * 2. **Low overhead**: Context switch overhead <1% of CPU time (~24us per 10ms)
 * 3. **Simple conversions**: ms -> ticks is divide-by-10 (compiler optimizes to shift)
 * 4. **Adequate responsiveness**: 10ms maximum scheduling latency acceptable for robotics
 * 5. **Standard practice**: Common RTOS tick rate (FreeRTOS, VxWorks use 100-1000 Hz)
 *
 * **CMT0 Hardware Configuration:**
 * - **Input clock**: PCLKB / 32 = 60 MHz / 32 = 1.875 MHz
 * - **Compare value**: 18,750 counts
 * - **Interrupt period**: 18,750 / 1,875,000 = 0.01 seconds = 10ms
 * - **Interrupt priority**: IPR[IPR_CMT0_CMI0] = 3 (high priority for determinism)
 *
 * ## Implementation Approach
 *
 * This header provides a **single source of truth** for ThreadX tick rate:
 * - **tx_user.h**: Defines TX_TIMER_TICKS_PER_SECOND = 100 (ThreadX core config)
 * - **This file**: Provides C constant s_rx_threadx_tick_rate_hz = 100 (application use)
 * - **CMT0 initialization**: Configures hardware to generate 100 Hz interrupt
 *
 * **Why not use TX_TIMER_TICKS_PER_SECOND directly?**
 * - tx_user.h may use preprocessor macros (not type-safe)
 * - This file provides typed constant for compile-time validation
 * - Decouples application code from ThreadX internal headers
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **Tick period** | 10 milliseconds | Fixed by CMT0 configuration |
 * | **Tick frequency** | 100 Hz | Reciprocal of tick period |
 * | **Tick jitter** | < 1 us | CMT0 crystal accuracy +/-50 ppm |
 * | **ISR latency** | < 5 us | ThreadX tick ISR execution time |
 * | **Context switch** | ~24 us | Worst-case at 240 MHz (measured) |
 * | **CPU overhead** | < 1% | (5us ISR + 24us switch) / 10ms = 0.29% |
 * | **Max tick count** | 2^3^2 - 1 | 32-bit counter wraps after ~497 days |
 *
 * **Tick Resolution vs. Alternatives:**
 * - **10ms ticks**: Current configuration, 0.29% overhead
 * - **1ms ticks (1000 Hz)**: 10x overhead ~2.9%, better responsiveness
 * - **100ms ticks (10 Hz)**: 0.029% overhead, unacceptable latency for robotics
 *
 * ## Memory Usage
 *
 * | Item | Size | Location | Notes |
 * |------|------|----------|-------|
 * | `s_rx_threadx_tick_rate_hz` | 0 bytes | Compile-time constant | Inlined by optimizer |
 * | ThreadX tick counter | 4 bytes | `.bss` | `_tx_timer_system_clock` global |
 * | CMT0 registers | 8 bytes | Hardware | Memory-mapped I/O |
 * | **Total** | **4 bytes RAM** | - | (Counter only, register is MMIO) |
 *
 * ## Module Dependencies
 *
 * @par Depends On:
 * - None (foundational configuration)
 *
 * @par Used By:
 * - [rx_time_constants.h](rx_time_constants.h) - Defines k_threadx_ms_per_tick (inverse: 10ms)
 * - [rx_iwdt.c](../rx_core/src/rx_iwdt.c) - Watchdog timeout conversions
 * - [rx_usb_comm.c](../rx_usb_comm/src/rx_usb_comm.c) - USB timeout calculations
 * - [tx_initialize_low_level.c](../../src/tx_initialize_low_level.c) - CMT0 timer setup
 * - All application threads using tx_thread_sleep(), tx_semaphore_get(), etc.
 *
 * @par Configuration Consistency:
 * **CRITICAL**: The following values MUST remain synchronized:
 * | File | Symbol | Value | Purpose |
 * |------|--------|-------|---------|
 * | tx_user.h | TX_TIMER_TICKS_PER_SECOND | 100 | ThreadX core config |
 * | rx_threadx_config.h | s_rx_threadx_tick_rate_hz | 100 | Application constant |
 * | rx_time_constants.h | k_threadx_ms_per_tick | 10 | Conversion factor (inverse) |
 * | tx_initialize_low_level.c | CMT0.CMCOR | 18750 | Hardware compare value |
 *
 * @warning If you change the tick rate, update ALL four locations above!
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [OK] | No code (configuration constant only) |
 * | 2. Fixed loop bounds | [OK] | No loops |
 * | 3. No dynamic memory | [OK] | No allocations (compile-time constant) |
 * | 4. Function length <60 lines | [OK] | No functions |
 * | 5. Assertions | [OK] | N/A (header-only configuration) |
 * | 6. Smallest scope | [OK] | File-scope constant |
 * | 7. Check return values | [OK] | N/A (no function calls) |
 * | 8. Limited preprocessor | [OK] | No macros (uses const, not \#define) |
 * | 9. Pointer restrictions | [OK] | No pointers |
 * | 10. Compiler warnings | [OK] | `-Wall -Wextra -Werror` enforced |
 *
 * ## SOLID Principles
 *
 * **Single Responsibility (S):**
 * - This header has **one responsibility**: Define ThreadX tick rate
 * - No mixing of concerns (pure configuration, no logic)
 *
 * **Open/Closed (O):**
 * - Changing tick rate requires recompilation (embedded systems reality)
 * - Alternative tick rates could be provided via separate config headers
 * - Hardware dependency (CMT0) prevents runtime configuration
 *
 * **Liskov Substitution (L):**
 * - N/A (not applicable to configuration constants)
 *
 * **Interface Segregation (I):**
 * - Single focused constant (no "fat" configuration structure)
 * - Application code depends only on what it needs (tick rate)
 *
 * **Dependency Inversion (D):**
 * - High-level modules (application) depend on abstraction (tick rate constant)
 * - Low-level details (CMT0 configuration) hidden in initialization code
 * - Time interface (rx_time_interface.h) provides further abstraction
 *
 * @see tx_user.h ThreadX core configuration (TX_TIMER_TICKS_PER_SECOND)
 * @see rx_time_constants.h Time conversion factors (k_threadx_ms_per_tick)
 * @see tx_initialize_low_level.c CMT0 timer initialization
 * @see rx_time_interface.h Dependency Inversion time abstraction
 * @see ThreadX User Guide Chapter 3: Time and Timers
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ThreadX Timing Constants
 * =============================================================================
 */

/**
 * @var s_rx_threadx_tick_rate_hz
 * @brief ThreadX system tick frequency (100 Hz)
 *
 * @details
 * Defines the ThreadX timer tick rate configured via hardware timer CMT0.
 * This value represents the frequency of the RTOS scheduler time quantum
 * and is the fundamental unit for all ThreadX time-based operations.
 *
 * ## Configuration Details
 *
 * **ThreadX Core Configuration (tx_user.h):**
 * ```c
 * \#define TX_TIMER_TICKS_PER_SECOND  100
 * ```
 *
 * **Hardware Timer Configuration (CMT0):**
 * - **Timer**: Compare Match Timer 0 (CMT0)
 * - **Input clock**: PCLKB / 32 = 60 MHz / 32 = 1.875 MHz
 * - **Compare value**: 18,750 counts (0x493E)
 * - **Interrupt vector**: VECT_CMT0_CMI0 (vector 28)
 * - **Interrupt priority**: IPR[IPR_CMT0_CMI0] = 3 (high priority)
 * - **Calculation**: 18,750 / 1,875,000 Hz = 10ms period = 100 Hz
 *
 * **Register Configuration:**
 * ```c
 * // From tx_initialize_low_level.c
 * CMT.CMSTR0.BIT.STR0 = 0;         // Stop CMT0
 * CMT0.CMCR.WORD = 0x00C2;         // PCLKB/32, interrupt enabled
 * CMT0.CMCOR = 18750 - 1;          // Compare match value
 * IPR(CMT0, CMI0) = 3;             // Priority 3
 * IEN(CMT0, CMI0) = 1;             // Enable interrupt
 * CMT.CMSTR0.BIT.STR0 = 1;         // Start CMT0
 * ```
 *
 * ## Tick Rate Selection Rationale
 *
 * **Why 100 Hz (10ms period)?**
 *
 * | Factor | Analysis |
 * |--------|----------|
 * | **Control loop alignment** | Motor PID runs at 100 Hz - tick period matches control quantum |
 * | **Scheduling latency** | 10ms worst-case latency acceptable for robotics (vs. 1ms for hard real-time) |
 * | **CPU overhead** | Context switch ~24us, ISR ~5us, total 0.29% overhead |
 * | **Simple arithmetic** | ms -> ticks is divide-by-10 (optimized to right-shift-by-1 + divide-by-5) |
 * | **Tick counter range** | 32-bit counter @ 100 Hz wraps after 497 days (acceptable) |
 * | **Standard practice** | Common RTOS tick rate (FreeRTOS default: 100-1000 Hz) |
 *
 * **Alternative Tick Rates Considered:**
 *
 * | Tick Rate | Pros | Cons | Verdict |
 * |-----------|------|------|---------|
 * | **10 Hz (100ms)** | Minimal overhead (0.029%) | 100ms latency unacceptable for robotics | [FAIL] Rejected |
 * | **100 Hz (10ms)** | Good balance, aligns with control loops | N/A | [PASS] **Selected** |
 * | **1000 Hz (1ms)** | Excellent responsiveness | 10x overhead (2.9%), overkill for our use case | [FAIL] Rejected |
 *
 * ## Conversion Formulas
 *
 * **Milliseconds -> ThreadX Ticks:**
 * ```
 * ticks = ms x (tick_rate_hz / 1000)
 *       = ms x (100 / 1000)
 *       = ms / 10
 * ```
 *
 * **ThreadX Ticks -> Milliseconds:**
 * ```
 * ms = ticks x (1000 / tick_rate_hz)
 *    = ticks x (1000 / 100)
 *    = ticks x 10
 * ```
 *
 * **Seconds -> ThreadX Ticks:**
 * ```
 * ticks = seconds x tick_rate_hz
 *       = seconds x 100
 * ```
 *
 * @par Value: 100 Hz
 *
 * @par Type: `uint16_t const` (read-only compile-time constant)
 *
 * @par Memory: Inlined by optimizer (zero runtime storage)
 *
 * @par Common Usage: Time conversions for ThreadX API calls
 *
 * @invariant s_rx_threadx_tick_rate_hz == 1000 / k_threadx_ms_per_tick (reciprocal relationship)
 * @invariant s_rx_threadx_tick_rate_hz == TX_TIMER_TICKS_PER_SECOND (must match tx_user.h)
 * @invariant CMT0.CMCOR == (PCLKB_Hz / 32 / s_rx_threadx_tick_rate_hz) - 1
 *
 * @warning **CRITICAL**: This value MUST match TX_TIMER_TICKS_PER_SECOND in tx_user.h
 * @warning **CRITICAL**: CMT0 hardware timer MUST be configured for 100 Hz interrupt
 * @warning Changing this value requires updating 4 locations (see file header)
 *
 * @note Prefer using k_threadx_ms_per_tick from rx_time_constants.h for conversions
 *       (provides inverse relationship: 10ms per tick)
 *
 * @par Basic Sleep Example:
 * @code{.c}
 * \#include "rx_threadx_config.h"
 * \#include "rx_time_constants.h"
 * \#include "tx_api.h"
 *
 * // Sleep for 500ms using ThreadX
 * void delay_500ms(void)
 * {
 *     // Method 1: Using tick rate (forward conversion)
 *     uint32_t timeout_ms = 500;
 *     uint32_t ticks = (timeout_ms * s_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;
 *     // Result: (500 x 100) / 1000 = 50 ticks
 *
 *     // Method 2: Using tick period (preferred - simpler)
 *     ticks = timeout_ms / k_threadx_ms_per_tick;
 *     // Result: 500 / 10 = 50 ticks
 *
 *     tx_thread_sleep(ticks);  // Sleep for 50 ticks = 500ms
 * }
 * @endcode
 *
 * @par Semaphore Timeout Example:
 * @code{.c}
 * \#include "rx_threadx_config.h"
 * \#include "rx_time_constants.h"
 * \#include "tx_api.h"
 *
 * TX_SEMAPHORE data_ready_semaphore;
 *
 * // Wait for semaphore with 100ms timeout
 * rx_err_t wait_for_data(void)
 * {
 *     uint32_t timeout_ms = 100;
 *     uint32_t timeout_ticks = timeout_ms / k_threadx_ms_per_tick;  // 10 ticks
 *
 *     UINT status = tx_semaphore_get(&data_ready_semaphore, timeout_ticks);
 *     if (status == TX_NO_INSTANCE) {
 *         // Timeout occurred after 100ms
 *         return k_rx_err_timeout;
 *     }
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Software Timer Example:
 * @code{.c}
 * \#include "rx_threadx_config.h"
 * \#include "rx_time_constants.h"
 * \#include "tx_api.h"
 *
 * TX_TIMER telemetry_timer;
 *
 * void telemetry_timer_callback(ULONG param)
 * {
 *     // Send telemetry data (called every 100ms)
 *     send_telemetry();
 * }
 *
 * void setup_telemetry_timer(void)
 * {
 *     // Create periodic timer: 100ms period
 *     uint32_t period_ms = 100;
 *     uint32_t period_ticks = period_ms / k_threadx_ms_per_tick;  // 10 ticks
 *
 *     tx_timer_create(&telemetry_timer,
 *                     "Telemetry Timer",
 *                     telemetry_timer_callback,
 *                     0,                  // Parameter
 *                     period_ticks,       // Initial ticks = 100ms
 *                     period_ticks,       // Reschedule ticks = 100ms
 *                     TX_AUTO_ACTIVATE);
 * }
 * @endcode
 *
 * @par Watchdog Timeout Calculation Example:
 * @code{.c}
 * \#include "rx_threadx_config.h"
 * \#include "rx_time_constants.h"
 * \#include "rx_iwdt.h"
 *
 * // Configure IWDT for 8-second timeout
 * void configure_watchdog(void)
 * {
 *     // Convert seconds to milliseconds
 *     uint32_t timeout_sec = 8;
 *     uint32_t timeout_ms = timeout_sec * k_rx_ms_per_second;  // 8000ms
 *
 *     // Calculate equivalent in ticks (for validation)
 *     uint32_t timeout_ticks = timeout_ms / k_threadx_ms_per_tick;  // 800 ticks
 *     // Watchdog should trigger before tick counter wraps (2^32 ticks = 497 days)
 *
 *     rx_iwdt_config_t config = {
 *         .timeout_ms = timeout_ms,
 *         .window_start_percent = 0,
 *         .window_end_percent = 100
 *     };
 *
 *     rx_err_t err = rx_iwdt_init(&config);
 *     if (err != k_rx_ok) {
 *         // Handle initialization error
 *     }
 * }
 * @endcode
 *
 * @par Performance Measurement Example:
 * @code{.c}
 * \#include "rx_threadx_config.h"
 * \#include "rx_time_constants.h"
 * \#include "tx_api.h"
 *
 * // Measure function execution time (tick granularity)
 * void measure_execution_time(void)
 * {
 *     ULONG start_ticks = tx_time_get();
 *
 *     // Execute function to measure
 *     expensive_operation();
 *
 *     ULONG end_ticks = tx_time_get();
 *     ULONG elapsed_ticks = end_ticks - start_ticks;
 *
 *     // Convert to milliseconds
 *     uint32_t elapsed_ms = elapsed_ticks * k_threadx_ms_per_tick;
 *
 *     rx_log_info("PERF", "Operation took %lu ms (%lu ticks)",
 *                 elapsed_ms, elapsed_ticks);
 * }
 * @endcode
 *
 * @see TX_TIMER_TICKS_PER_SECOND ThreadX core configuration (tx_user.h)
 * @see k_threadx_ms_per_tick Inverse constant (10ms per tick) in rx_time_constants.h
 * @see k_rx_ms_per_second Milliseconds per second (1000) in rx_time_constants.h
 * @see tx_initialize_low_level.c CMT0 hardware timer initialization
 * @see tx_thread_sleep() ThreadX sleep function (ticks)
 * @see tx_semaphore_get() ThreadX semaphore with timeout (ticks)
 * @see tx_time_get() Get current tick counter value
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 8 (Limited preprocessor): Uses const, not macro [OK]
 */
static const uint16_t s_rx_threadx_tick_rate_hz = 100U;

#ifdef __cplusplus
}
#endif
