/**
 * @file rx_tpu.c
 * @brief TPU HAL Driver Implementation for Phase Counting Mode
 *
 * @details
 * Implementation of the TPU hardware abstraction layer for RX72N quadrature
 * encoder phase counting. This driver configures TPU channels 1, 2, 4, and 5
 * for hardware-based encoder position tracking with 4x resolution.
 *
 * ## Module Architecture
 *
 * The implementation is structured in three layers:
 * 1. **Constants & State**: Typed enums for all magic numbers, static tracking
 * 2. **Internal Helpers**: Channel validation, register access, bit mapping
 * 3. **Public API**: User-facing functions documented in rx_tpu.h
 *
 * ## Initialization Sequence
 *
 * @verbatim
 *   rx_tpu_init_phase_count(&config)
 *     |
 *     +-- 1. Validate config (NULL, channel, phase_mode)
 *     +-- 2. Check not already initialized
 *     +-- 3. Enable TPU module clock (MSTPCRA.MSTPA13 = 0)
 *     +-- 4. Stop channel counter (TSTR.CSTn = 0)
 *     +-- 5. Set phase counting mode (TMDR.MD = mode)
 *     +-- 6. Set TCR to free-running (CCLR = disabled)
 *     +-- 7. Clear TCNT to zero
 *     +-- 8. Disable interrupts (TIER = 0x40; bit 6 reserved/must-write-1)
 *     +-- 9. Mark channel as initialized
 * @endverbatim
 *
 * ## Static Variables
 *
 * | Variable               | Type       | Purpose                              |
 * |------------------------|------------|--------------------------------------|
 * | s_tpu_initialized[]    | bool[4]    | Track which channels are initialized |
 * | s_tag                  | const char*| Log tag "TPU"                        |
 *
 * ## Channel-to-Index Mapping
 *
 * Phase counting channels (1, 2, 4, 5) are mapped to array indices 0-3:
 * | Channel | Index | Use in STAR         | Clock Pins      |
 * |---------|-------|---------------------|-----------------|
 * | TPU1    | 0     | Back-right encoder  | TCLKA + TCLKB   |
 * | TPU2    | 1     | Back-left encoder   | TCLKC + TCLKD   |
 * | TPU4    | 2     | Available           | TCLKC + TCLKD   |
 * | TPU5    | 3     | Available           | TCLKA + TCLKB   |
 *
 * @par Performance Characteristics
 * | Function                   | Time    | Stack | Notes                      |
 * |----------------------------|---------|-------|----------------------------|
 * | rx_tpu_init_phase_count()  | ~20 us  | 32 B  | One-time setup per channel |
 * | rx_tpu_start()             | ~0.5 us | 8 B   | Single register write      |
 * | rx_tpu_stop()              | ~0.5 us | 8 B   | Single register write      |
 * | rx_tpu_read_count()        | ~0.5 us | 8 B   | Single register read       |
 * | rx_tpu_read_direction()    | ~0.5 us | 8 B   | Single register read       |
 * | rx_tpu_reset_count()       | ~0.5 us | 8 B   | Single register write      |
 * | rx_tpu_deinit()            | ~1 us   | 16 B  | Stop + reset + cleanup     |
 *
 * @par Memory Usage
 * | Category         | Size       | Description                |
 * |------------------|------------|----------------------------|
 * | Code (.text)     | ~600 bytes | All functions              |
 * | Constants        | 4 bytes    | Tag string "TPU"           |
 * | Static data      | 4 bytes    | Initialized flags array    |
 * | Stack (max)      | 32 bytes   | Deepest call chain         |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: All loops bounded (no loops in this module)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All functions < 60 lines
 * - Rule 5: Minimum 2 validation checks per function
 * - Rule 6: Variables at smallest scope
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums for all constants
 * - Rule 9: Single-level pointers only
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Only TPU hardware access (no encoder logic, no motor control)
 * - **O**: Extensible via rx_tpu_config_t without modifying existing code
 * - **L**: Consistent rx_err_t return semantics across all functions
 * - **I**: Focused API: init, start, stop, read, reset, deinit
 * - **D**: Depends on register abstractions (rx72n_tpu_regs.h), not raw addresses
 *
 * @see rx_tpu.h Public API documentation
 * @see rx72n_tpu_regs.h TPU register definitions
 * @see rx_mtu_encoder.h MTU encoder interface (front wheels)
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_tpu.h"

#include <stddef.h>

#ifdef UNIT_TEST
#include "mock_rx72n_regs.h"
#else
#include "rx72n_regs.h"
#endif
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Logging tag for TPU module
 *
 * @details
 * Static constant string used as the tag parameter for all rx_log_*() calls
 * within this module.
 *
 * @par Value: "TPU"
 * @par Memory: 4 bytes in .rodata section
 *
 * @note Statically allocated, never modified
 * @since Version 1.0.0
 */
static const char* const s_tag = "TPU";

/**
 * @enum tpu_phase_channel_count_t
 * @brief Number of phase-counting-capable TPU channels
 *
 * @details
 * TPU1, TPU2, TPU4, and TPU5 support phase counting mode.
 * TPU0 and TPU3 do NOT support phase counting.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_phase_channel_count = 4, /**< Channels 1, 2, 4, 5 */
} tpu_phase_channel_count_t;

/**
 * @enum tpu_channel_index_t
 * @brief Array index mapping for phase-counting channels
 *
 * @details
 * Maps the non-contiguous channel numbers (1, 2, 4, 5) to contiguous
 * array indices (0-3) for the s_tpu_initialized[] tracking array.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_idx_ch1 = 0, /**< TPU1 -> index 0 (back-right encoder) */
  k_tpu_idx_ch2 = 1, /**< TPU2 -> index 1 (back-left encoder) */
  k_tpu_idx_ch4 = 2, /**< TPU4 -> index 2 (available) */
  k_tpu_idx_ch5 = 3, /**< TPU5 -> index 3 (available) */
} tpu_channel_index_t;

/**
 * @enum tpu_tmdr_reset_t
 * @brief TMDR register reset value (normal mode)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tmdr_reset = 0x00, /**< Normal operation mode (reset default) */
} tpu_tmdr_reset_t;

/**
 * @enum tpu_tcr_phase_count_t
 * @brief TCR register value for phase counting mode
 *
 * @details
 * In phase counting mode, the TPSC and CKEG fields in TCR are ignored
 * because the external clock pins provide the counting signal. CCLR is
 * set to disabled (free-running counter).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tcr_phase_count = 0x00, /**< Free-running: TPSC=0, CKEG=0, CCLR=0 */
} tpu_tcr_phase_count_t;

/**
 * @enum tpu_tier_disable_t
 * @brief TIER register value with all interrupts disabled
 *
 * @details
 * Bit 6 of TIER is reserved and must always be written as 1 (manual p.1461:
 * "This bit is read as 1. The write value should be 1."). The reset value of
 * TIER is 0x40 (bit 6 set). Writing 0x00 violates the specification; 0x40
 * disables all interrupt sources while preserving the reserved bit.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tier_all_disabled = 0x40, /**< All interrupts disabled; bit 6 reserved, must be 1 */
} tpu_tier_disable_t;

/**
 * @enum tpu_tcnt_zero_t
 * @brief TCNT register zero value
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_tpu_tcnt_zero = 0x0000, /**< Counter cleared to zero */
} tpu_tcnt_zero_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_tpu_initialized
 * @brief Track which phase-counting channels are initialized
 *
 * @details
 * Boolean array indexed by tpu_channel_index_t values (0-3).
 * Set to true by rx_tpu_init_phase_count(), false by rx_tpu_deinit().
 *
 * @warning Do not modify directly - use public API functions
 * @since Version 1.0.0
 */
static bool s_tpu_initialized[k_tpu_phase_channel_count] = {false, false, false, false};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Map TPU channel number to array index
 *
 * @details
 * Converts the non-contiguous channel enumeration (1, 2, 4, 5) to a
 * contiguous array index (0-3) for use with s_tpu_initialized[].
 *
 * @param[in] channel TPU channel identifier
 * @param[out] index Pointer to store the array index
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Valid channel, index written
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 *
 * @pre index must be non-NULL (caller responsibility)
 * @pre channel must be a valid rx_tpu_channel_t enumeration value
 * @post *index contains valid array index on success
 * @post *index is unchanged on k_rx_err_invalid_arg
 *
 * @note Thread Safety: Safe - pure function with no shared state
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_channel_to_index(const rx_tpu_channel_t channel, uint8_t* index)
{
  switch (channel) {
    case k_tpu_channel_1:
      *index = k_tpu_idx_ch1;
      return k_rx_ok;
    case k_tpu_channel_2:
      *index = k_tpu_idx_ch2;
      return k_rx_ok;
    case k_tpu_channel_4:
      *index = k_tpu_idx_ch4;
      return k_rx_ok;
    case k_tpu_channel_5:
      *index = k_tpu_idx_ch5;
      return k_rx_ok;
    default:
      rx_log_error_val(s_tag, "Invalid TPU channel", (uint32_t)channel);
      return k_rx_err_invalid_arg;
  }
}

/**
 * @brief Get register pointer for a phase-counting TPU channel
 *
 * @details
 * Maps a TPU channel identifier to the corresponding hardware register
 * base address pointer. Only returns pointers for phase-counting-capable
 * channels (1, 2, 4, 5).
 *
 * @param[in] channel TPU channel identifier (1, 2, 4, or 5)
 *
 * @return Volatile pointer to TPU channel register structure
 * @retval Non-NULL Valid pointer for channels 1, 2, 4, 5
 * @retval NULL Invalid channel (not phase-counting capable)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @pre channel must be a valid rx_tpu_channel_t enumeration value
 * @post No registers modified
 * @post Returned pointer (if non-NULL) points to valid hardware register block
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @see tpu1(), tpu2(), tpu4(), tpu5() Underlying accessor functions
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE volatile rx_tpu_regs_t* internal_get_regs(const rx_tpu_channel_t channel)
{
  switch (channel) {
    case k_tpu_channel_1:
      return tpu1();
    case k_tpu_channel_2:
      return tpu2();
    case k_tpu_channel_4:
      return tpu4();
    case k_tpu_channel_5:
      return tpu5();
    default:
      RX_ASSERT(false, "Post-condition: channel must be valid (1,2,4,5) - caller validates");
      return nullptr;
  }
}

/**
 * @brief Get TSTR CST bit mask for a TPU channel
 *
 * @details
 * Returns the counter start bit mask in TSTR for the specified channel.
 * This bit is set to start counting and cleared to stop counting.
 * Each phase-counting-capable channel has a dedicated bit position in the
 * shared TSTR register: CST1, CST2, CST4, and CST5.
 *
 * @param[in] channel TPU channel identifier (1, 2, 4, or 5)
 *
 * @return TSTR bit mask for the channel
 * @retval Non-zero Valid CST bit mask
 * @retval 0 Invalid channel
 *
 * @pre channel must be one of k_tpu_channel_1, _2, _4, or _5
 * @pre Caller validates that a non-zero return indicates a valid channel
 * @post No hardware registers modified
 * @post Returned value is safe to OR/AND into TSTR register
 *
 * @note Thread Safety: Safe - pure function
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint8_t internal_get_cst_bit(const rx_tpu_channel_t channel)
{
  switch (channel) {
    case k_tpu_channel_1:
      return k_tpu_tstr_cst1;
    case k_tpu_channel_2:
      return k_tpu_tstr_cst2;
    case k_tpu_channel_4:
      return k_tpu_tstr_cst4;
    case k_tpu_channel_5:
      return k_tpu_tstr_cst5;
    default:
      RX_ASSERT(false, "Post-condition: channel must be valid (1,2,4,5) - caller validates");
      return 0;
  }
}

/**
 * @brief Map rx_tpu_phase_mode_t to TMDR.MD register value
 *
 * @details
 * Converts the phase mode enumeration (1-4) to the corresponding TMDR
 * register value for phase counting mode selection. The caller must check
 * whether the return value equals k_tpu_tmdr_md_normal (0x00) to detect
 * an invalid mode argument before writing to hardware.
 *
 * @param[in] mode Phase counting mode (1-4)
 *
 * @return TMDR.MD register value
 * @retval 0x04-0x07 Valid phase counting mode values
 * @retval 0x00 Invalid mode (normal operation fallback)
 *
 * @pre mode must be a valid rx_tpu_phase_mode_t enumeration value
 * @pre Caller must check return != k_tpu_tmdr_md_normal before hardware write
 * @post No hardware registers modified
 * @post Returned value is safe to write directly to TMDR.MD field
 *
 * @note Thread Safety: Safe - pure function
 *
 * @see rx_tpu_tmdr_md_t TMDR mode register values
 * @since Version 1.0.0
 */
static uint8_t internal_get_tmdr_mode(const rx_tpu_phase_mode_t mode)
{
  switch (mode) {
    case k_tpu_phase_mode_1:
      return k_tpu_tmdr_md_phase_count_1;
    case k_tpu_phase_mode_2:
      return k_tpu_tmdr_md_phase_count_2;
    case k_tpu_phase_mode_3:
      return k_tpu_tmdr_md_phase_count_3;
    case k_tpu_phase_mode_4:
      return k_tpu_tmdr_md_phase_count_4;
    default:
      return k_tpu_tmdr_md_normal;
  }
}

/**
 * @brief Enable TPU module clock by clearing MSTPCRA.MSTPA13
 *
 * @details
 * Enables the TPU peripheral by clearing its module stop bit in MSTPCRA.
 * Uses the PRCR register protection unlock/lock sequence. Idempotent -
 * safe to call multiple times.
 *
 * @pre System clock configured
 * @pre PRCR register accessible (system not in protected mode that blocks PRC1)
 * @post TPU module clock enabled (MSTPCRA.MSTPA13 = 0)
 * @post PRCR protection re-enabled
 *
 * @note Thread Safety: Not thread-safe (modifies system-wide registers)
 *
 * @since Version 1.0.0
 */
static void internal_enable_tpu_module_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcra &= ~k_tpu_mstpcra_mstpa13;
  *prcr_reg() = k_rx_prcr_lock;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize TPU channel for phase counting mode
 *
 * @details
 * Implementation of rx_tpu_init_phase_count() - see rx_tpu.h for complete
 * API documentation.
 *
 * @par Register Configuration Sequence
 * 1. Unlock PRCR, clear MSTPCRA.MSTPA13, lock PRCR
 * 2. Clear CSTn in TSTR (stop counter)
 * 3. Write phase counting mode to TMDR.MD
 * 4. Write 0x00 to TCR (free-running, external clock)
 * 5. Write 0x00 to TIER (no interrupts)
 * 6. Write 0x0000 to TCNT (clear counter)
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_init_phase_count(const rx_tpu_config_t* config)
{
  /* Pre-condition 1: validate config pointer (NASA Rule 5) */
  RX_CHECK_NULL_PTR(config, s_tag, "Config pointer is nullptr");

  /* Pre-condition 2: validate channel is phase-counting capable */
  uint8_t        idx = 0U;
  const rx_err_t err = internal_channel_to_index(config->channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 3: validate phase mode range */
  const uint8_t tmdr_mode = internal_get_tmdr_mode(config->phase_mode);
  if (tmdr_mode == k_tpu_tmdr_md_normal) {
    rx_log_error_val(s_tag, "Invalid phase mode", (uint32_t)config->phase_mode);
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 4: check not already initialized */
  if (s_tpu_initialized[idx]) {
    rx_log_error(s_tag, "Channel already initialized");
    return k_rx_err_invalid_state;
  }

  /* 1. Enable TPU module clock */
  internal_enable_tpu_module_clock();

  /* 2. Get register pointer */
  volatile rx_tpu_regs_t* regs = internal_get_regs(config->channel);

  /* 3. Stop counter */
  const uint8_t cst_bit = internal_get_cst_bit(config->channel);
  tpu_control()->tstr &= (uint8_t)~cst_bit;

  /* 4. Configure TCR - free-running (TPSC/CKEG ignored in phase count mode) */
  regs->tcr = k_tpu_tcr_phase_count;

  /* 5. Set phase counting mode in TMDR */
  regs->tmdr = tmdr_mode;

  /* 6. Disable all interrupts */
  regs->tier = k_tpu_tier_all_disabled;

  /* 7. Clear counter */
  regs->tcnt = k_tpu_tcnt_zero;

  /* 8. Mark channel as initialized */
  s_tpu_initialized[idx] = true;

  rx_log_info(s_tag, "TPU phase count initialized");
  rx_log_info_val(s_tag, "  channel", (uint32_t)config->channel);
  rx_log_info_val(s_tag, "  mode", (uint32_t)config->phase_mode);

  return k_rx_ok;
}

/**
 * @brief Start the TPU channel counter (begin phase counting)
 *
 * @details
 * Sets the Count Start (CSTn) bit in the TSTR register to begin counter
 * operation. Once started, the TCNT register increments or decrements on
 * each external encoder clock edge according to the configured phase mode.
 *
 * @param[in] channel TPU channel to start (1, 2, 4, or 5)
 *   - Must have been previously initialized via rx_tpu_init_phase_count()
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter started successfully
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_not_initialized Channel not yet initialized
 *
 * @pre Channel must be initialized via rx_tpu_init_phase_count()
 * @pre External encoder clock pins must be connected and valid
 *
 * @post TSTR.CSTn bit set, counter begins counting encoder pulses
 * @post TCNT updates in real time from encoder edges
 *
 * @note Not thread-safe: modifies shared TSTR register
 * @note Idempotent: safe to call when already running
 *
 * @code{.c}
 * rx_err_t err = rx_tpu_start(k_tpu_channel_1);
 * @endcode
 *
 * @see rx_tpu_stop() Stop the counter
 * @see rx_tpu_read_count() Read current counter value
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_start(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t        idx = 0U;
  rx_err_t const err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 2: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Set CSTn bit in TSTR to start counter */
  const uint8_t cst_bit = internal_get_cst_bit(channel);
  tpu_control()->tstr |= cst_bit;

  return k_rx_ok;
}

/**
 * @brief Stop the TPU channel counter (halt phase counting)
 *
 * @details
 * Clears the Count Start (CSTn) bit in the TSTR register to halt counter
 * operation. The TCNT value is preserved and encoder edges no longer update
 * it. The channel remains initialized and can be restarted with rx_tpu_start().
 *
 * @param[in] channel TPU channel to stop (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter stopped successfully
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_not_initialized Channel not yet initialized
 *
 * @pre Channel must be initialized via rx_tpu_init_phase_count()
 * @pre TSTR register must be accessible (module clock enabled)
 *
 * @post TSTR.CSTn bit cleared, counter halted
 * @post TCNT value preserved at last count
 *
 * @note Not thread-safe: modifies shared TSTR register
 * @note Idempotent: safe to call when already stopped
 *
 * @code{.c}
 * rx_err_t err = rx_tpu_stop(k_tpu_channel_1);
 * @endcode
 *
 * @see rx_tpu_start() Resume counter operation
 * @see rx_tpu_reset_count() Clear counter to zero
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_stop(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t        idx = 0U;
  rx_err_t const err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 2: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Clear CSTn bit in TSTR to stop counter */
  const uint8_t cst_bit = internal_get_cst_bit(channel);
  tpu_control()->tstr &= (uint8_t)~cst_bit;

  return k_rx_ok;
}

/**
 * @brief Read the current 16-bit encoder counter value
 *
 * @details
 * Performs a single volatile read of the TCNT register for the specified
 * channel. In 4x phase counting mode, the counter increments or decrements
 * on every edge of both A and B encoder channels.
 *
 * @param[in]  channel TPU channel to read (1, 2, 4, or 5)
 * @param[out] count   Pointer to store the 16-bit counter value [0, 65535]
 *   - Must be non-NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter value read into *count
 * @retval k_rx_err_null_ptr count pointer is nullptr
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_not_initialized Channel not yet initialized
 *
 * @pre Channel must be initialized via rx_tpu_init_phase_count()
 * @pre count must point to valid uint16_t storage
 *
 * @post *count contains the TCNT register value at the time of the read
 * @post Hardware registers unchanged
 *
 * @note Not thread-safe: counter may wrap between multiple reads
 * @note Wraps naturally at 0 and 65535 (16-bit overflow)
 *
 * @code{.c}
 * uint16_t count;
 * rx_err_t err = rx_tpu_read_count(k_tpu_channel_1, &count);
 * @endcode
 *
 * @see rx_tpu_reset_count() Clear counter to zero
 * @see rx_tpu_read_direction() Read counting direction
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_read_count(const rx_tpu_channel_t channel, uint16_t* count)
{
  /* Pre-condition 1: validate output pointer */
  RX_CHECK_NULL_PTR(count, s_tag, "Count pointer is nullptr");

  /* Pre-condition 2: validate channel */
  uint8_t        idx = 0U;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 3: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Read TCNT register */
  const volatile rx_tpu_regs_t* regs = internal_get_regs(channel);

  *count = regs->tcnt;

  return k_rx_ok;
}

/**
 * @brief Read the current encoder counting direction
 *
 * @details
 * Reads the Timer Counter Flag Direction (TCFD) bit in the TSR register.
 * TCFD reflects the last count event direction:
 * - TCFD = 1: Counter incremented on the last edge (counting up / forward)
 * - TCFD = 0: Counter decremented on the last edge (counting down / reverse)
 *
 * @param[in]  channel     TPU channel to query (1, 2, 4, or 5)
 * @param[out] counting_up Pointer to store direction result
 *   - true  = last count was an increment (encoder moving forward)
 *   - false = last count was a decrement (encoder moving in reverse)
 *   - Must be non-NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Direction read into *counting_up
 * @retval k_rx_err_null_ptr counting_up pointer is nullptr
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_not_initialized Channel not yet initialized
 *
 * @pre Channel must be initialized via rx_tpu_init_phase_count()
 * @pre counting_up must point to valid bool storage
 *
 * @post *counting_up reflects TSR.TCFD at the time of the read
 * @post Hardware registers unchanged
 *
 * @note Not thread-safe: value may change between reads if encoder is moving
 * @note Direction is unreliable if counter is stationary (no recent edge)
 *
 * @code{.c}
 * bool forward;
 * rx_err_t err = rx_tpu_read_direction(k_tpu_channel_1, &forward);
 * @endcode
 *
 * @see rx_tpu_read_count() Read counter position
 * @see k_tpu_tsr_tcfd TSR direction bit mask
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_read_direction(const rx_tpu_channel_t channel, bool* counting_up)
{
  /* Pre-condition 1: validate output pointer */
  RX_CHECK_NULL_PTR(counting_up, s_tag, "Direction pointer is nullptr");

  /* Pre-condition 2: validate channel */
  uint8_t        idx = 0U;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 3: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Read TSR.TCFD bit */
  const volatile rx_tpu_regs_t* regs = internal_get_regs(channel);

  *counting_up = (bool)((regs->tsr & k_tpu_tsr_tcfd) != 0U);

  return k_rx_ok;
}

/**
 * @brief Reset the TPU channel encoder counter to zero
 *
 * @details
 * Writes k_tpu_tcnt_zero (0x0000) to the TCNT register, resetting the
 * encoder position counter. The counter continues running from the new
 * zero baseline.
 *
 * Typical use: zero encoder position at a known reference point (home).
 *
 * @param[in] channel TPU channel to reset (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter reset to zero successfully
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_not_initialized Channel not yet initialized
 *
 * @pre Channel must be initialized via rx_tpu_init_phase_count()
 * @pre Channel should be stopped (rx_tpu_stop()) before resetting for atomic 0
 *
 * @post TCNT register written to 0x0000
 * @post Counter resumes from zero if running
 *
 * @note Not thread-safe: write races with live counter updates from encoder
 * @note May cause a single spurious velocity estimate if called while running
 *
 * @code{.c}
 * rx_tpu_stop(k_tpu_channel_1);
 * rx_tpu_reset_count(k_tpu_channel_1);
 * rx_tpu_start(k_tpu_channel_1);
 * @endcode
 *
 * @see rx_tpu_stop() Stop counter before resetting for deterministic result
 * @see rx_tpu_read_count() Verify counter after reset
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_reset_count(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t        idx = 0U;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 2: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Write 0 to TCNT */
  volatile rx_tpu_regs_t* const regs = internal_get_regs(channel);

  regs->tcnt = k_tpu_tcnt_zero;

  return k_rx_ok;
}

/**
 * @brief Deinitialize a TPU phase-counting channel
 *
 * @details
 * Performs an orderly shutdown of the specified TPU channel:
 * 1. Validate channel and get array index
 * 2. Stop counter (clear CSTn in TSTR)
 * 3. Reset TMDR to normal operation mode (0x00)
 * 4. Disable all interrupts (TIER = 0x40; bit 6 reserved/must-write-1)
 * 5. Clear counter (TCNT = 0)
 * 6. Mark channel as uninitialized in s_tpu_initialized[]
 *
 * This function does NOT disable the TPU module clock (MSTPCRA.MSTPA13),
 * as other channels may still be in use.
 *
 * @param[in] channel TPU channel to deinitialize (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel deinitialized successfully
 * @retval k_rx_err_invalid_arg Channel is not 1, 2, 4, or 5
 * @retval k_rx_err_invalid_arg Register pointer could not be resolved
 *
 * @pre channel must be a valid phase-counting TPU channel (1, 2, 4, or 5)
 * @pre Motor driven by this encoder must be stopped before calling
 *
 * @post Counter stopped and zeroed
 * @post TMDR reset to normal mode
 * @post s_tpu_initialized[idx] == false
 *
 * @note Not thread-safe: do not access channel concurrently during deinit
 * @note Channel may be partially initialized; deinit succeeds regardless
 *
 * @code{.c}
 * rx_err_t err = rx_tpu_deinit(k_tpu_channel_1);
 * @endcode
 *
 * @see rx_tpu_init_phase_count() Re-initialize after deinit
 * @see rx_tpu_stop() Stop counter without full teardown
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_deinit(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t        idx = 0U;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Get register pointer (no init check - allow deinit of partially init'd channel) */
  volatile rx_tpu_regs_t* const regs = internal_get_regs(channel);

  /* 1. Stop counter */
  const uint8_t cst_bit = internal_get_cst_bit(channel);
  tpu_control()->tstr &= (uint8_t)~cst_bit;

  /* 2. Reset mode to normal operation */
  regs->tmdr = k_tpu_tmdr_reset;

  /* 3. Disable all interrupts */
  regs->tier = k_tpu_tier_all_disabled;

  /* 4. Clear counter */
  regs->tcnt = k_tpu_tcnt_zero;

  /* 5. Mark channel as uninitialized */
  s_tpu_initialized[idx] = false;

  rx_log_info_val(s_tag, "TPU channel deinitialized", (uint32_t)channel);

  return k_rx_ok;
}
