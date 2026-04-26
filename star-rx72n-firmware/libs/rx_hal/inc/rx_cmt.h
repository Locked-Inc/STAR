/**
 * @file rx_cmt.h
 * @brief CMT (Compare Match Timer) Driver API for RX72N - Periodic Interrupt Generation
 *
 * @details
 * # Overview
 *
 * The Compare Match Timer (CMT) driver provides periodic interrupt generation for the
 * RX72N microcontroller. CMT is used for system timing, motor control loops, and
 * other periodic tasks requiring precise timing.
 *
 * ## CMT Hardware Overview
 *
 * The RX72N has **4 CMT channels** organized in 2 units:
 *
 * @dot
 * digraph cmt_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_unit0 {
 *     label="CMT Unit 0";
 *     style=filled;
 *     color=lightblue;
 *     cmt0 [label="CMT0\n(ThreadX Tick)", fillcolor=yellow, style=filled];
 *     cmt1 [label="CMT1\n(Motor Control)", fillcolor=lightgreen, style=filled];
 *   }
 *
 *   subgraph cluster_unit1 {
 *     label="CMT Unit 1";
 *     style=filled;
 *     color=lightblue;
 *     cmt2 [label="CMT2\n(Available)"];
 *     cmt3 [label="CMT3\n(Available)"];
 *   }
 *
 *   pclkb [label="PCLKB\n60 MHz", shape=ellipse];
 *   icu [label="ICU\n(Interrupts)", shape=ellipse];
 *
 *   pclkb -> cmt0;
 *   pclkb -> cmt1;
 *   pclkb -> cmt2;
 *   pclkb -> cmt3;
 *
 *   cmt0 -> icu [label="CMI0"];
 *   cmt1 -> icu [label="CMI1"];
 *   cmt2 -> icu [label="CMI2"];
 *   cmt3 -> icu [label="CMI3"];
 * }
 * @enddot
 *
 * ## Channel Allocation
 *
 * | Channel | Usage | Frequency | Description |
 * |---------|-------|-----------|-------------|
 * | **CMT0** | ThreadX | 100 Hz | System tick (RESERVED) |
 * | **CMT1** | Motor | 100-1000 Hz | Motor control loop |
 * | **CMT2** | Available | User-defined | General purpose |
 * | **CMT3** | Available | User-defined | General purpose |
 *
 * @warning **CMT0 is reserved** for ThreadX system tick. Do not use CMT0 with this API.
 *
 * ## CMT Operation Principle
 *
 * @msc
 *   width=600;
 *   Counter, CMCOR, Interrupt;
 *
 *   Counter box Counter [label="16-bit up counter\nStarts at 0"];
 *   Counter -> Counter [label="Count up"];
 *   Counter => CMCOR [label="CMCNT == CMCOR"];
 *   CMCOR box CMCOR [label="Compare match!\nCMCNT = 0"];
 *   CMCOR => Interrupt [label="CMIn interrupt"];
 *   Interrupt >> Counter [label="Continue counting"];
 * @endmsc
 *
 * ## Clock Divider Selection
 *
 * | Divider | CKS Value | Clock Rate | Min Freq | Max Freq | Notes |
 * |---------|-----------|------------|----------|----------|-------|
 * | PCLKB/8 | 0b00 | 7.5 MHz | 115 Hz | 7.5 MHz | High resolution |
 * | PCLKB/32 | 0b01 | 1.875 MHz | 29 Hz | 1.875 MHz | Medium resolution |
 * | PCLKB/128 | 0b10 | 468.75 kHz | 7.2 Hz | 468 kHz | **Recommended** |
 * | PCLKB/512 | 0b11 | 117.2 kHz | 1.8 Hz | 117 kHz | Low frequency |
 *
 * **Calculation example for 250 Hz motor control loop:**
 * ```
 * PCLKB = 60 MHz
 * Divider = 128 -> Counter clock = 468.75 kHz
 * Period = 468750 / 250 = 1875 counts
 * ```
 *
 * ## Memory-Mapped Register Layout
 *
 * | Register | Offset | Description |
 * |----------|--------|-------------|
 * | CMSTR0 | 0x0000 | Start register for CMT0/CMT1 |
 * | CMSTR1 | 0x0010 | Start register for CMT2/CMT3 |
 * | CMCRn | +0x00 | Control register (clock select, interrupt enable) |
 * | CMCNTn | +0x02 | 16-bit counter register |
 * | CMCORn | +0x04 | 16-bit compare match register |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | **Rule 1: Control flow** | [PASS] | No goto, setjmp, or recursion |
 * | **Rule 2: Loop bounds** | [PASS] | Bounded divider search (4 iterations max) |
 * | **Rule 3: No heap** | [PASS] | Static callback arrays only |
 * | **Rule 4: Function length** | [PASS] | All functions under 60 lines |
 * | **Rule 5: Assertions** | [PASS] | Parameter validation in all functions |
 * | **Rule 6: Data scope** | [PASS] | File-scope static with s_ prefix |
 * | **Rule 7: Return checks** | [PASS] | All internal calls checked |
 * | **Rule 8: Preprocessor** | [PASS] | C23 typed enums for all constants |
 * | **Rule 9: Pointers** | [PASS] | Single-level only (callback + user_data) |
 * | **Rule 10: Warnings** | [PASS] | -Wall -Wextra -Werror clean |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S - Single Responsibility** | CMT driver handles only compare match timers |
 * | **O - Open/Closed** | Extended via callback registration |
 * | **L - Liskov Substitution** | All channels use identical API |
 * | **I - Interface Segregation** | Simple 5-function API |
 * | **D - Dependency Inversion** | Callback injection for user code |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   rx_cmt [label="rx_cmt.h", fillcolor=lightgreen, style=filled];
 *   rx_err [label="rx_err.h"];
 *   stdbool [label="stdbool.h"];
 *   stdint [label="stdint.h"];
 *   rx72n_regs [label="rx72n_regs.h\n(implementation)"];
 *   rx_check [label="rx_check.h\n(implementation)"];
 *
 *   rx_cmt -> rx_err [label="Error codes"];
 *   rx_cmt -> stdbool [label="bool type"];
 *   rx_cmt -> stdint [label="uint types"];
 * }
 * @enddot
 *
 * @see rx72n_cmt_regs.h CMT register definitions
 * @see timer.h Timer abstraction (uses CMT internally)
 * @see hardware_init.h Boot sequence context
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

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
 * @enum rx_cmt_channel_t
 * @brief CMT channel identifiers
 *
 * @details
 * Identifies the 4 available CMT channels. Channels are grouped into units:
 * - Unit 0: CMT0, CMT1 (shared CMSTR0)
 * - Unit 1: CMT2, CMT3 (shared CMSTR1)
 *
 * ## Channel Assignment
 *
 * | Channel | Purpose | Notes |
 * |---------|---------|-------|
 * | k_cmt_channel_0 | ThreadX tick | **RESERVED - Do not use** |
 * | k_cmt_channel_1 | Motor control | 100-1000 Hz typical |
 * | k_cmt_channel_2 | User defined | Available |
 * | k_cmt_channel_3 | User defined | Available |
 *
 * @see rx_cmt_init() Channel selection for initialization
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cmt_channel_0 = 0, /**< CMT0 - Reserved for ThreadX system tick (100 Hz) */
  k_cmt_channel_1 = 1, /**< CMT1 - Motor control loop (user configurable) */
  k_cmt_channel_2 = 2, /**< CMT2 - Available for general use */
  k_cmt_channel_3 = 3, /**< CMT3 - Available for general use */
} rx_cmt_channel_t;

/**
 * @brief CMT interrupt callback function type
 *
 * @details
 * User-provided function called from CMT interrupt context on each compare match.
 * Keep callbacks short and non-blocking to avoid interrupt latency issues.
 *
 * ## Callback Execution Context
 *
 * - **Interrupt level:** IPL equal to configured priority
 * - **Stack:** Interrupt stack (not task stack)
 * - **Timing:** Called every compare match period
 * - **Duration:** Keep under 10 us for real-time performance
 *
 * ## Allowed Operations
 *
 * | Operation | Allowed | Notes |
 * |-----------|---------|-------|
 * | Set flag | [PASS] | Signal task to process |
 * | Write GPIO | [PASS] | Toggle debug pin |
 * | Queue message | [PASS] | Use ThreadX queue |
 * | Blocking call | [FAIL] | Causes interrupt overrun |
 * | Memory allocation | [FAIL] | NASA Rule 3 violation |
 * | Long computation | [FAIL] | Delays other interrupts |
 *
 * @param[in] user_data User data pointer provided during rx_cmt_init()
 *                      May be NULL if no user data needed.
 *
 * @note Callback executes in interrupt context. Keep it fast!
 * @warning Do not call blocking functions from callback.
 * @warning Do not disable interrupts for extended periods.
 *
 * @par Example:
 * @code{.c}
 * static volatile bool s_motor_tick = false;
 *
 * void motor_timer_callback(void* user_data) {
 *     (void)user_data;
 *     s_motor_tick = true;  // Signal motor task
 * }
 * @endcode
 *
 * @see rx_cmt_config_t::callback Callback registration
 *
 * @since Version 1.0.0
 */
typedef void (*rx_cmt_callback_t)(void* user_data);

/**
 * @struct rx_cmt_config_t
 * @brief CMT channel configuration structure
 *
 * @details
 * Configuration parameters for initializing a CMT channel. All fields must be
 * set before calling rx_cmt_init().
 *
 * ## Memory Layout
 *
 * | Offset | Field | Size | Description |
 * |--------|-------|------|-------------|
 * | 0x00 | frequency_hz | 4 | Target interrupt frequency |
 * | 0x04 | callback | 4 | Interrupt callback pointer |
 * | 0x08 | user_data | 4 | User data for callback |
 * | 0x0C | priority | 1 | Interrupt priority level |
 *
 * **Total size:** 13 bytes (16 with padding)
 *
 * ## Frequency Calculation
 *
 * The driver automatically selects the best clock divider for the requested
 * frequency. Valid range depends on PCLKB (60 MHz):
 * - Minimum: ~1.8 Hz (with /512 divider)
 * - Maximum: ~7.5 MHz (with /8 divider)
 *
 * ## Example Configuration
 *
 * @code{.c}
 * rx_cmt_config_t config = {
 *     .frequency_hz = 250,            // 250 Hz motor control
 *     .callback = motor_tick_handler,
 *     .user_data = &motor_context,
 *     .priority = 8                   // Medium priority
 * };
 * @endcode
 *
 * @see rx_cmt_init() Uses this configuration
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Desired interrupt frequency in Hz
   * @details
   * Target frequency for periodic interrupts. Driver selects optimal divider.
   * Valid range: 1-7500000 Hz (depends on PCLKB).
   * @note Actual frequency may differ slightly due to integer division.
   */
  uint32_t frequency_hz;

  /**
   * @brief Interrupt callback function
   * @details
   * Function called on each compare match interrupt. May be NULL if no
   * callback needed (timer runs but no notification).
   * @see rx_cmt_callback_t Callback function signature
   */
  rx_cmt_callback_t callback;

  /**
   * @brief User data passed to callback
   * @details
   * Opaque pointer passed to callback on each invocation. Typically used
   * to pass context or state to the callback handler.
   * @note May be NULL if callback doesn't need user data.
   */
  void* user_data;

  /**
   * @brief Interrupt priority level (0-15)
   * @details
   * ICU interrupt priority. Higher values = higher priority.
   * - 0: Lowest priority (may be masked)
   * - 15: Highest priority
   * @note Motor control typically uses priority 8-12.
   * @warning Priority 0 disables the interrupt.
   */
  uint8_t priority;
} rx_cmt_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize CMT channel for periodic interrupts
 *
 * @details
 * Configures a CMT channel for periodic interrupt generation at the specified
 * frequency. The timer starts immediately after initialization.
 *
 * ## Initialization Sequence
 *
 * @msc
 *   width=600;
 *   Caller, Init, Module, Timer, ICU;
 *
 *   Caller => Init [label="rx_cmt_init(ch, config)"];
 *   Init box Init [label="Validate parameters"];
 *   Init => Module [label="Clear MSTPCRB.CMT"];
 *   Module >> Init [label="Module enabled"];
 *   Init => Timer [label="Stop timer (CMSTR)"];
 *   Timer >> Init;
 *   Init => Timer [label="Configure CMCR, CMCOR"];
 *   Timer >> Init;
 *   Init => ICU [label="Set priority, enable IRQ"];
 *   ICU >> Init;
 *   Init box Init [label="Save callback"];
 *   Init => Timer [label="Start timer (CMSTR)"];
 *   Timer >> Init;
 *   Init >> Caller [label="k_rx_ok"];
 * @endmsc
 *
 * ## Divider Auto-Selection
 *
 * The driver automatically selects the smallest divider that produces a
 * period fitting in the 16-bit compare register:
 *
 * | Frequency | Divider | Period | Actual Freq |
 * |-----------|---------|--------|-------------|
 * | 1000 Hz | /32 | 1875 | 1000.00 Hz |
 * | 250 Hz | /128 | 1875 | 250.00 Hz |
 * | 100 Hz | /128 | 4687 | 100.02 Hz |
 * | 10 Hz | /512 | 11719 | 10.00 Hz |
 *
 * @param[in] channel CMT channel to initialize (1-3, CMT0 reserved)
 *                    Valid range: k_cmt_channel_1 to k_cmt_channel_3
 * @param[in] config Pointer to configuration structure (must not be NULL)
 *
 * @return rx_err_t Initialization status
 *
 * @retval k_rx_ok Channel initialized and started successfully
 * @retval k_rx_err_null_ptr config is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel, frequency out of range, or invalid priority
 * @retval k_rx_err_conflict Channel is CMT0 (reserved for ThreadX)
 *
 * @pre Module stop cleared for CMT (or will be cleared by this function)
 * @pre Channel not currently in use (call rx_cmt_deinit() first if reinitializing)
 * @pre config->frequency_hz > 0 and within achievable range
 * @pre config->priority in range [1, 15] (0 disables interrupt)
 *
 * @post CMT channel running at specified frequency
 * @post Callback registered and will be called on each compare match
 * @post Interrupt enabled at specified priority
 *
 * @note Timer starts automatically. Use rx_cmt_stop() to pause.
 * @note CMT0 is reserved for ThreadX - returns k_rx_err_conflict.
 *
 * @warning Do not initialize CMT0 - it's managed by ThreadX.
 * @warning High frequencies (>100 kHz) may cause excessive interrupt load.
 *
 * @par Thread Safety:
 * Not thread-safe. Call from single thread only, typically during init.
 *
 * @par Performance:
 * - Execution time: ~50 us @ 240 MHz
 * - Stack usage: ~64 bytes
 *
 * @par Example - Motor Control Timer:
 * @code{.c}
 * static void motor_tick(void* user_data) {
 *     motor_context_t* ctx = (motor_context_t*)user_data;
 *     ctx->tick_count++;
 * }
 *
 * void motor_timer_init(void) {
 *     static motor_context_t s_motor_ctx;
 *
 *     rx_cmt_config_t config = {
 *         .frequency_hz = 250,        // 250 Hz control loop
 *         .callback = motor_tick,
 *         .user_data = &s_motor_ctx,
 *         .priority = 10              // High priority
 *     };
 *
 *     rx_err_t err = rx_cmt_init(k_cmt_channel_1, &config);
 *     if (err != k_rx_ok) {
 *         rx_log_error("MOTOR", "Failed to init CMT");
 *     }
 * }
 * @endcode
 *
 * @see rx_cmt_stop() Pause timer
 * @see rx_cmt_start() Resume timer
 * @see rx_cmt_deinit() Release channel
 *
 * @since Version 1.0.0
 *
 * @callgraph
 * @callergraph
 */
[[nodiscard]] rx_err_t rx_cmt_init(rx_cmt_channel_t channel, const rx_cmt_config_t* config);

/**
 * @brief Start CMT timer
 *
 * @details
 * Starts the timer counter and interrupt generation. The timer is automatically
 * started during rx_cmt_init(), so this function is only needed after calling
 * rx_cmt_stop() to resume operation.
 *
 * @param[in] channel CMT channel to start
 *                    Valid range: k_cmt_channel_0 to k_cmt_channel_3
 *
 * @return rx_err_t Start status
 *
 * @retval k_rx_ok Timer started successfully
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_hw_error Hardware failed to start (CMSTR readback failed)
 *
 * @pre Channel initialized via rx_cmt_init()
 *
 * @post Timer counter incrementing
 * @post Interrupts generating at configured frequency
 *
 * @note Counter resumes from current value (not reset to 0).
 *
 * @par Thread Safety:
 * Not thread-safe. Call from single thread or protect with mutex.
 *
 * @see rx_cmt_stop() Stop timer
 * @see rx_cmt_init() Initialize channel
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cmt_start(rx_cmt_channel_t channel);

/**
 * @brief Stop CMT timer
 *
 * @details
 * Stops the timer counter and interrupt generation. Counter value is preserved
 * and can be read with rx_cmt_get_count(). Use rx_cmt_start() to resume.
 *
 * @param[in] channel CMT channel to stop
 *                    Valid range: k_cmt_channel_0 to k_cmt_channel_3
 *
 * @return rx_err_t Stop status
 *
 * @retval k_rx_ok Timer stopped successfully
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_hw_error Hardware failed to stop (CMSTR readback failed)
 *
 * @post Timer counter frozen at current value
 * @post No further interrupts until rx_cmt_start() called
 *
 * @note Counter value preserved. Call rx_cmt_start() to resume counting.
 *
 * @par Thread Safety:
 * Not thread-safe. Call from single thread or protect with mutex.
 *
 * @see rx_cmt_start() Resume timer
 * @see rx_cmt_get_count() Read frozen counter value
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cmt_stop(rx_cmt_channel_t channel);

/**
 * @brief Get CMT counter value
 *
 * @details
 * Reads the current 16-bit counter value. Useful for timing measurements
 * or debugging. Counter resets to 0 on each compare match.
 *
 * @param[in] channel CMT channel to read
 *                    Valid range: k_cmt_channel_0 to k_cmt_channel_3
 * @param[out] count Pointer to store counter value (0-65535)
 *
 * @return rx_err_t Read status
 *
 * @retval k_rx_ok Counter value read successfully
 * @retval k_rx_err_null_ptr count pointer is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized via rx_cmt_init()
 * @pre count != nullptr
 *
 * @post *count contains current counter value
 *
 * @note Counter may change immediately after reading if timer running.
 *
 * @par Thread Safety:
 * Safe to call from multiple threads (atomic 16-bit read).
 *
 * @see rx_cmt_stop() Stop timer to freeze counter
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cmt_get_count(rx_cmt_channel_t channel, uint16_t* count);

/**
 * @brief Deinitialize CMT channel
 *
 * @details
 * Stops timer, disables interrupts, clears callback, and marks channel as
 * uninitialized. Channel can be reinitialized with rx_cmt_init() after this.
 *
 * @param[in] channel CMT channel to deinitialize
 *                    Valid range: k_cmt_channel_0 to k_cmt_channel_3
 *
 * @return rx_err_t Deinitialization status
 *
 * @retval k_rx_ok Channel deinitialized successfully
 * @retval k_rx_err_invalid_arg Invalid channel number
 * @retval k_rx_err_hw_error Hardware failed to stop
 *
 * @post Timer stopped
 * @post Interrupt disabled
 * @post Callback cleared
 * @post Channel marked as uninitialized
 *
 * @note Does not re-enable module stop bit (CMT module remains powered).
 *
 * @par Thread Safety:
 * Not thread-safe. Ensure no callbacks executing during deinit.
 *
 * @see rx_cmt_init() Reinitialize channel
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cmt_deinit(rx_cmt_channel_t channel);

#ifdef __cplusplus
}
#endif
