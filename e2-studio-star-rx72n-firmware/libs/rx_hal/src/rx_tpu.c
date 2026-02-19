/* lib/rx_hal/src/rx_tpu.c */

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
 *     +-- 8. Disable interrupts (TIER = 0)
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
 * | TPU1    | 0     | Rear-left encoder   | TCLKA + TCLKB   |
 * | TPU2    | 1     | Rear-right encoder  | TCLKC + TCLKD   |
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
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "rx_tpu.h"

#include <stddef.h>

#include "rx72n_regs.h"
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
static const char* s_tag = "TPU";

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
  k_tpu_idx_ch1 = 0, /**< TPU1 -> index 0 (rear-left encoder) */
  k_tpu_idx_ch2 = 1, /**< TPU2 -> index 1 (rear-right encoder) */
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
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tier_all_disabled = 0x00, /**< All interrupt sources disabled */
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
 * @post *index contains valid array index on success
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
 * @post No registers modified
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @see tpu1(), tpu2(), tpu4(), tpu5() Underlying accessor functions
 * @since Version 1.0.0
 */
static volatile rx_tpu_regs_t* internal_get_regs(const rx_tpu_channel_t channel)
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
      return nullptr;
  }
}

/**
 * @brief Get TSTR CST bit mask for a TPU channel
 *
 * @details
 * Returns the counter start bit mask in TSTR for the specified channel.
 * This bit is set to start counting and cleared to stop counting.
 *
 * @param[in] channel TPU channel identifier (1, 2, 4, or 5)
 *
 * @return TSTR bit mask for the channel
 * @retval Non-zero Valid CST bit mask
 * @retval 0 Invalid channel
 *
 * @note Thread Safety: Safe - pure function
 *
 * @since Version 1.0.0
 */
static uint8_t internal_get_cst_bit(const rx_tpu_channel_t channel)
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
      return 0;
  }
}

/**
 * @brief Map rx_tpu_phase_mode_t to TMDR.MD register value
 *
 * @details
 * Converts the phase mode enumeration (1-4) to the corresponding TMDR
 * register value for phase counting mode selection.
 *
 * @param[in] mode Phase counting mode (1-4)
 *
 * @return TMDR.MD register value
 * @retval 0x04-0x07 Valid phase counting mode values
 * @retval 0x00 Invalid mode (normal operation fallback)
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
  system_regs()->mstpcra &= ~(uint32_t)k_tpu_mstpcra_mstpa13;
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
  uint8_t  idx;
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
  if (regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

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
 * @brief Start TPU channel counter
 *
 * @details
 * Implementation of rx_tpu_start() - see rx_tpu.h for complete documentation.
 * Sets the CSTn bit in TSTR to begin counter operation.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_start(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t  idx;
  rx_err_t err;

  err = internal_channel_to_index(channel, &idx);
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
 * @brief Stop TPU channel counter
 *
 * @details
 * Implementation of rx_tpu_stop() - see rx_tpu.h for complete documentation.
 * Clears the CSTn bit in TSTR to stop counter operation.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_stop(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t  idx;
  rx_err_t err;

  err = internal_channel_to_index(channel, &idx);
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
 * @brief Read TPU channel 16-bit counter value
 *
 * @details
 * Implementation of rx_tpu_read_count() - see rx_tpu.h for complete
 * documentation. Single volatile register read of TCNT.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_read_count(const rx_tpu_channel_t channel, uint16_t* count)
{
  /* Pre-condition 1: validate output pointer */
  RX_CHECK_NULL_PTR(count, s_tag, "Count pointer is nullptr");

  /* Pre-condition 2: validate channel */
  uint8_t  idx;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 3: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Read TCNT register */
  const volatile rx_tpu_regs_t* regs = internal_get_regs(channel);
  if (regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *count = regs->tcnt;

  return k_rx_ok;
}

/**
 * @brief Read TPU channel counting direction
 *
 * @details
 * Implementation of rx_tpu_read_direction() - see rx_tpu.h for complete
 * documentation. Reads the TCFD bit (bit 7) in the TSR register.
 * TCFD = 1 means counting up (forward), TCFD = 0 means counting down.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_read_direction(const rx_tpu_channel_t channel, bool* counting_up)
{
  /* Pre-condition 1: validate output pointer */
  RX_CHECK_NULL_PTR(counting_up, s_tag, "Direction pointer is nullptr");

  /* Pre-condition 2: validate channel */
  uint8_t  idx;
  const rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 3: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Read TSR.TCFD bit */
  const volatile rx_tpu_regs_t* regs = internal_get_regs(channel);
  if (regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *counting_up = (regs->tsr & k_tpu_tsr_tcfd) != 0;

  return k_rx_ok;
}

/**
 * @brief Reset TPU channel counter to zero
 *
 * @details
 * Implementation of rx_tpu_reset_count() - see rx_tpu.h for complete
 * documentation. Writes zero to the TCNT register.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_reset_count(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t  idx;
  rx_err_t err;

  err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Pre-condition 2: check initialized */
  RX_VALIDATE_INIT(s_tpu_initialized[idx], s_tag, "Channel not initialized");

  /* Write 0 to TCNT */
  volatile rx_tpu_regs_t* regs = internal_get_regs(channel);
  if (regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

  regs->tcnt = k_tpu_tcnt_zero;

  return k_rx_ok;
}

/**
 * @brief Deinitialize TPU channel
 *
 * @details
 * Implementation of rx_tpu_deinit() - see rx_tpu.h for complete documentation.
 * Stops the channel counter, resets mode to normal, clears the counter,
 * and marks the channel as uninitialized.
 *
 * @see rx_tpu.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_deinit(const rx_tpu_channel_t channel)
{
  /* Pre-condition 1: validate channel */
  uint8_t  idx;
  rx_err_t err;

  err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return err;
  }

  /* Get register pointer (no init check - allow deinit of partially init'd channel) */
  volatile rx_tpu_regs_t* regs = internal_get_regs(channel);
  if (regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

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
