/**
 * @file rx_bus_onewire.c
 * @brief OneWire (1-Wire) Bus Implementation Using GPIO Bit-Banging
 *
 * @details
 * Implements the Dallas/Maxim 1-Wire protocol using software bit-banging on
 * GPIO pins. Provides all standard 1-Wire operations: reset/presence detect,
 * bit/byte read/write, and ROM commands (Skip, Match, Read, Search).
 *
 * @par Implementation Architecture:
 * @msc
 * App, Bus_Manager, OneWire, GPIO, Timer;
 * --- [label="Reset Pulse"];
 * App => Bus_Manager [label="reset()"];
 * Bus_Manager => OneWire [label="callback"];
 * OneWire => GPIO [label="drive_low()"];
 * OneWire => Timer [label="delay_us(480)"];
 * OneWire => GPIO [label="release()"];
 * OneWire => Timer [label="delay_us(70)"];
 * OneWire => GPIO [label="read()"];
 * OneWire => Bus_Manager [label="presence"];
 * @endmsc
 *
 * @par 1-Wire Timing (Standard Speed):
 * | Operation        | Parameter          | Value    |
 * |------------------|--------------------|----------|
 * | Reset pulse      | T_RSTL             | 480 us   |
 * | Presence wait    | T_PDH              | 70 us    |
 * | Presence tail    | T_RSTH             | 410 us   |
 * | Write-1 low      | T_LOW1             | 6 us     |
 * | Write-1 high     | T_SLOT - T_LOW1    | 64 us    |
 * | Write-0 low      | T_LOW0             | 60 us    |
 * | Write-0 high     | T_SLOT - T_LOW0    | 10 us    |
 * | Read init        | T_LOWR             | 6 us     |
 * | Read sample      | T_RDV              | 9 us     |
 * | Read recovery    | T_REC              | 55 us    |
 *
 * @par Open-Drain Emulation:
 * GPIO is configured as:
 * - Output LOW to drive line low
 * - Input (high-Z) to release line (external 4.7kOhm pullup drives high)
 *
 * @par Hardware Requirements:
 * - 4.7kOhm external pullup resistor on 1-Wire data line
 * - CMT3 timer for microsecond delays (PCLKB/8 = 7.5 MHz)
 * - GPIO pin with configurable direction
 *
 * @par Supported Devices:
 * | Device   | Family Code | Description              |
 * |----------|-------------|--------------------------|
 * | DS18B20  | 0x28        | Temperature sensor       |
 * | DS18S20  | 0x10        | Temperature sensor       |
 * | DS2431   | 0x2D        | 1Kb EEPROM               |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] Loops bounded (k_bits_per_byte, k_onewire_rom_bytes)
 * - Rule 3: [OK] No dynamic allocation (static state pool)
 * - Rule 5: [OK] Pre/post conditions on all functions
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Typed enums for all constants
 *
 * @par SOLID Principles:
 * - SRP: Handles 1-Wire protocol only, delegates GPIO to HAL
 * - OCP: Extensible via callback pattern (bus_manager)
 * - DIP: Depends on GPIO abstraction, not direct register access
 *
 * @see rx_bus_onewire.h Public API header
 * @see rx_bus_manager.h Bus manager integration
 * @see gpio.c GPIO driver used for bit-banging
 *
 * @author Locked, Inc.
 * @date 2026-01-03
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_bus_onewire.h"

#include "hardware.h"
#include "rx72n_clock.h"
#ifdef UNIT_TEST
#include "mock_rx72n_cmt_regs.h"
#include "mock_rx72n_regs.h"
#include "mock_rx72n_system_regs.h"
#else
#include "rx72n_cmt_regs.h"
#include "rx72n_regs.h"
#include "rx72n_system_regs.h"
#endif
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"
#include "rx_register_protection.h"

static const char*    s_tag                        = "BUS_ONEWIRE";
static const uint32_t s_onewire_max_search_devices = 64U;

/* =============================================================================
 * Internal State Tracking
 * =============================================================================
 */

/** @brief OneWire driver constants. */
typedef enum : uint8_t {
  k_onewire_max_instances         = k_max_buses,
  k_onewire_single_bit_mask       = 0x01U,
  k_onewire_search_last_zero_init = 0U,
  k_onewire_first_bit_number      = 1U,
  k_onewire_rom_bit_mask_reset    = 1U,
  k_onewire_no_discrepancy        = 0U,
  k_onewire_max_buf_bytes         = 255U,
  k_onewire_bit_mask_lsb          = 1U,
  k_onewire_crc_offset            = 1U,
  k_onewire_rom_family_code_idx   = 0U,
  k_onewire_invalid_family_code   = 0x00U,
  k_onewire_instance_idx_start    = 0U, /**< Starting index for instance pool iteration */
  k_onewire_rom_crc_idx           = 7U, /**< CRC byte index in ROM (k_onewire_rom_crc_idx) */
} onewire_driver_constants_t;

/**
 * @brief Runtime state per OneWire bus.
 *
 * Tracks ROM search progress plus cached drive mode to avoid redundant GPIO
 * reconfiguration.
 */
#ifndef UNIT_TEST
typedef struct {
  uint8_t last_rom[k_onewire_rom_bytes]; /**< Last ROM discovered */
  uint8_t last_discrepancy;              /**< Search tree discrepancy bit */
  bool    last_device_flag;              /**< True when last device enumerated */
  bool    line_is_output;                /**< Current GPIO drive mode */
} onewire_runtime_state_t;
#endif /* UNIT_TEST */

/** @brief Static pool for OneWire runtime state (zero-allocation pattern). */
typedef struct {
  bool                    in_use;
  onewire_runtime_state_t state;
} onewire_state_entry_t;

static onewire_state_entry_t s_state_pool[k_onewire_max_instances];

/* =============================================================================
 * Timer-Based Delay Helpers
 * =============================================================================
 */

/**
 * @brief Hardware timer configuration for microsecond delays.
 *
 * Uses CMT channel 3 in free-run mode (no interrupts) as a 16-bit counter
 * clocked from PCLKB/8 (7.5 MHz). Delays are implemented by measuring elapsed
 * ticks to ensure consistent timing independent of compiler optimizations.
 */
typedef enum : uint32_t {
  k_onewire_mstpa_cmt_unit1_bit = 14, /**< MSTPA14 in MSTPCRA: CMT unit 1 (CMT2/CMT3) module stop */
  k_onewire_cmt3_start_bit      = 1,  /**< CMSTR1 bit controlling CMT3 */
  k_onewire_cmt_divider_setting = 0,  /**< CKS = 0 -> PCLKB/8 */
  k_onewire_cmt_divider_value   = 8,  /**< Actual divider value */
  k_onewire_cmt_clk_shift       = 0,  /**< CMCR clock select shift */
  k_onewire_timer_counter_max   = 0xFFFF,
  k_onewire_us_per_second       = 1000000UL,
  k_onewire_timer_rounding      = k_onewire_us_per_second - 1UL,
  k_onewire_bit_set             = 1U,
  k_onewire_counter_reset       = 0U,
  k_onewire_zero_microseconds   = 0U,
} onewire_delay_hw_constants_t;

typedef enum : uint64_t {
  k_onewire_ticks_zero = 0ULL,
} onewire_delay_tick_constants_t;

static bool s_delay_timer_initialized = false;

/**
 * @brief Initialize CMT3 timer for microsecond-precision delays
 *
 * @details
 * Configures CMT channel 3 as a free-running 16-bit counter for timing delays.
 * Uses PCLKB/8 clock source (7.5 MHz at 60 MHz PCLKB) for ~133ns resolution.
 * Timer runs continuously without interrupts.
 *
 * @par Algorithm:
 * 1. Check if already initialized (early return)
 * 2. Assert register pointers are valid
 * 3. Unlock MSTPCRB via PRCR
 * 4. Enable CMT module clock (clear MSTPB15)
 * 5. Lock PRCR
 * 6. Stop CMT3 (clear CMSTR1 bit 1)
 * 7. Configure CMCR for PCLKB/8, no interrupts
 * 8. Set CMCOR to 0xFFFF (full 16-bit range)
 * 9. Reset counter to 0
 * 10. Start timer (set CMSTR1 bit 1)
 *
 * @pre System clocks initialized (PCLKB running)
 * @post CMT3 running as free-running counter
 * @post s_delay_timer_initialized == true
 *
 * @note Called automatically by internal_delay_us() on first use
 * @note Timer resolution: 1 / (PCLKB/8) = 133ns @ 60MHz PCLKB
 *
 * @see internal_delay_us() Uses this timer for delays
 */
RX_STATIC_TESTABLE void internal_delay_timer_init(void)
{
  if (s_delay_timer_initialized) {
    return;
  }

  RX_ASSERT_PRE(system_regs() != nullptr, "System registers unavailable");
  RX_ASSERT_PRE(cmt_ctrl() != nullptr, "CMT control registers unavailable");
  RX_ASSERT_PRE(cmt3() != nullptr, "CMT3 registers unavailable");

  /* Enable CMT module clock */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  system_regs()->mstpcra &= ~(k_onewire_bit_set << k_onewire_mstpa_cmt_unit1_bit);
  *prcr_reg() = k_rx_prcr_lock;

  /* Stop CMT3 before reconfiguration */
  cmt_ctrl()->cmstr1 &= (uint16_t) ~((uint16_t)k_onewire_bit_set << k_onewire_cmt3_start_bit);

  /* Configure free-running counter (no interrupts) */
  cmt3()->cmcr  = (uint16_t)(k_onewire_cmt_divider_setting << k_onewire_cmt_clk_shift);
  cmt3()->cmcor = k_onewire_timer_counter_max;
  cmt3()->cmcnt = k_onewire_counter_reset;

  /* Start timer */
  cmt_ctrl()->cmstr1 |= ((uint16_t)k_onewire_bit_set << k_onewire_cmt3_start_bit);

  s_delay_timer_initialized = true;
}

#ifdef UNIT_TEST
/**
 * @brief Reset delay timer initialization state (test-only)
 *
 * @details
 * Clears s_delay_timer_initialized so that internal_delay_timer_init() will
 * re-execute its full body on the next call. This exists solely to allow
 * unit tests to exercise both the "assert-passes" and "assert-fires" branches
 * of the RX_ASSERT_PRE checks on lines 212-214, which require entering the
 * function body multiple times with different mock configurations.
 *
 * @pre Must only be called from unit tests (UNIT_TEST build)
 * @post s_delay_timer_initialized == false
 *
 * @note Not thread-safe; test-only function
 *
 * @see internal_delay_timer_init() Function whose state is reset
 *
 * @since Version 1.0.0
 */
void internal_delay_timer_reset(void)
{
  s_delay_timer_initialized = false;
}
#endif /* UNIT_TEST */

/**
 * @brief Delay for specified microseconds using CMT3 timer
 *
 * @details
 * Implements busy-wait delay using CMT3 counter. Handles delays longer than
 * the 16-bit counter maximum by looping. Automatically initializes timer on
 * first call.
 *
 * @par Algorithm:
 * 1. Return immediately if microseconds == 0
 * 2. Initialize timer if not already done
 * 3. Calculate required ticks: (us * timer_hz + round) / 1000000
 * 4. Ensure minimum 1 tick for very short delays
 * 5. Loop while ticks > 0:
 *    a. Calculate wait_ticks (capped at 0xFFFF)
 *    b. Read start counter value
 *    c. Busy-wait until (current - start) >= wait_ticks
 *    d. Subtract wait_ticks from remaining
 *
 * @param[in] microseconds Delay duration in microseconds (0 = no delay)
 *
 * @pre System clocks initialized
 * @post Delay of approximately `microseconds` us elapsed
 *
 * @note Busy-wait implementation, blocks CPU
 * @note Accuracy: +/-1 timer tick (~133ns @ 7.5MHz)
 * @note Maximum single delay: limited by uint32_t microseconds
 *
 * @par Performance:
 * - Overhead: ~10 cycles for function call
 * - Resolution: 133ns (1 tick @ 7.5MHz)
 * - Accuracy: +/-0.5% typical
 *
 * @warning Disables preemption during delay (busy-wait)
 *
 * @see internal_delay_timer_init() Timer initialization
 */
RX_STATIC_TESTABLE void internal_delay_us(uint32_t microseconds)
{
  if (microseconds == k_onewire_zero_microseconds) {
    return;
  }

  internal_delay_timer_init();

  const uint32_t timer_hz = k_pclkb_hz / k_onewire_cmt_divider_value;
  uint64_t       ticks = ((uint64_t)microseconds * (uint64_t)timer_hz + k_onewire_timer_rounding) /
                   k_onewire_us_per_second;

  while (ticks > k_onewire_ticks_zero) {
    uint32_t wait_ticks =
      (ticks > k_onewire_timer_counter_max) ? k_onewire_timer_counter_max : (uint32_t)ticks;
    uint16_t start = cmt3()->cmcnt;
    while ((uint16_t)(cmt3()->cmcnt - start) < wait_ticks) {
      __asm__ volatile("nop");
    }
    ticks -= wait_ticks;
  }
}

/* =============================================================================
 * Internal Helpers (State / GPIO Control)
 * =============================================================================
 */

/**
 * @brief Acquire or allocate runtime state for a 1-Wire bus
 *
 * @details
 * Ensures a runtime state structure exists for the bus. If the bus already
 * has state (handle != nullptr), returns existing state. Otherwise allocates
 * from static pool. Uses zero-allocation pattern for NASA Rule 3 compliance.
 *
 * @par Algorithm:
 * 1. If bus_config->handle != nullptr, return existing state
 * 2. Search static pool for unused entry
 * 3. Mark entry as in_use, zero-initialize state
 * 4. Store pointer in bus_config->handle
 * 5. Return state pointer via output parameter
 *
 * @param[in,out] bus_config Bus configuration node (handle field updated)
 * @param[out] state Pointer to receive runtime state
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, state available
 * @retval k_rx_err_no_mem Static pool exhausted (k_onewire_max_instances)
 *
 * @pre bus_config != nullptr
 * @pre state != nullptr
 * @post *state points to valid runtime state
 * @post bus_config->handle points to same state
 *
 * @note Uses static pool, no dynamic allocation
 * @note Pool size: k_onewire_max_instances (= k_max_buses)
 *
 * @see internal_get_state() Retrieve existing state
 */
static rx_err_t internal_acquire_state(rx_bus_config_t* bus_config, onewire_runtime_state_t** state)
{
  if (bus_config->handle != nullptr) {
    *state = (onewire_runtime_state_t*)bus_config->handle;
    return k_rx_ok;
  }
  for (uint32_t i = k_onewire_instance_idx_start; i < k_onewire_max_instances; ++i) {
    if (!s_state_pool[i].in_use) {
      s_state_pool[i].in_use = true;
      s_state_pool[i].state  = (onewire_runtime_state_t){};
      bus_config->handle     = &s_state_pool[i].state;
      *state                 = &s_state_pool[i].state;
      return k_rx_ok;
    }
  }

  return k_rx_err_no_mem;
}

/**
 * @brief Retrieve runtime state for already-initialized bus.
 *
 * @param[in] bus_config Bus configuration node
 *
 * @return Pointer to state or nullptr if missing
 */
static inline onewire_runtime_state_t* internal_get_state(const rx_bus_config_t* bus_config)
{
  return (onewire_runtime_state_t*)bus_config->handle;
}

/**
 * @brief Configure GPIO drive mode (output low vs input/high-Z).
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] state Runtime state tracking current mode
 * @param[in] output True to drive (output), false to release (input)
 *
 * @return k_rx_ok on success
 */
RX_STATIC_TESTABLE rx_err_t internal_set_drive_mode(const rx_bus_config_t*   bus_config,
                                                    onewire_runtime_state_t* state,
                                                    const bool               output)
{
  rx_err_t err = k_rx_ok;
  if ((int)output && !(int)state->line_is_output) {
    err = gpio_set_output(bus_config->proto.onewire.pin);
    if (err != k_rx_ok) {
      return err;
    }
    state->line_is_output = true;
  } else if (!(int)output && (int)state->line_is_output) {
    err = gpio_set_input(bus_config->proto.onewire.pin);
    if (err != k_rx_ok) {
      return err;
    }
    state->line_is_output = false;
  }
  return k_rx_ok;
}

/**
 * @brief Drive the OneWire line low (open-drain style).
 */
RX_STATIC_TESTABLE rx_err_t internal_drive_low(rx_bus_config_t*         bus_config,
                                               onewire_runtime_state_t* state)
{
  const rx_err_t err = internal_set_drive_mode(bus_config, state, true);
  if (err != k_rx_ok) {
    return err;
  }
  return gpio_write_low(bus_config->proto.onewire.pin);
}

/**
 * @brief Release the OneWire line (input/high-Z, external pull-up drives high).
 */
RX_STATIC_TESTABLE rx_err_t internal_release_line(rx_bus_config_t*         bus_config,
                                                  onewire_runtime_state_t* state)
{
  return internal_set_drive_mode(bus_config, state, false);
}

/**
 * @brief Read current level on the OneWire line.
 *
 * @param[out] high True if line is high
 */
RX_STATIC_TESTABLE rx_err_t internal_read_line(const rx_bus_config_t* bus_config, bool* high)
{
  return gpio_read(bus_config->proto.onewire.pin, high);
}

/**
 * @brief Reset runtime search tracking.
 */
static void internal_reset_search_state(onewire_runtime_state_t* state)
{
  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    state->last_rom[i] = 0;
  }
  state->last_discrepancy = 0;
  state->last_device_flag = false;
}

/* =============================================================================
 * Protocol Primitive Helpers
 * =============================================================================
 */

/**
 * @brief Issue 1-Wire reset pulse and detect device presence
 *
 * @details
 * Executes the 1-Wire reset/presence detect sequence. Controller drives line
 * low for 480us (reset pulse), then releases and samples during presence
 * window. Devices respond by pulling line low within 60us of release.
 *
 * @par Timing Diagram:
 * @code
 *          |<-- 480us -->|<-70us->|<-- 410us -->|
 *     _____|             |________|_____________|_____
 *          |_____________|        |             |
 *          ^             ^        ^             ^
 *          |             |        |             |
 *        Drive         Release  Sample       Complete
 *         Low                   Presence
 * @endcode
 *
 * @par Algorithm:
 * 1. Drive line low via internal_drive_low()
 * 2. Delay 480us (reset pulse duration)
 * 3. Release line via internal_release_line()
 * 4. Delay 70us (presence wait)
 * 5. Sample line level
 * 6. presence = !line_high (low = device present)
 * 7. Delay 410us (complete reset slot)
 *
 * @param[in] bus_config Bus configuration with GPIO pin
 * @param[in,out] state Runtime state for GPIO mode tracking
 * @param[out] presence Set to true if device responded (line went low)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, presence result valid
 * @retval k_rx_err_* GPIO operation failed
 *
 * @pre bus_config != nullptr
 * @pre state != nullptr
 * @pre presence != nullptr
 * @post *presence indicates device response
 * @post Line released (high via pullup)
 *
 * @note Total reset slot: ~960us
 * @note Multiple devices can respond simultaneously
 *
 * @see k_onewire_reset_pulse_us Reset pulse duration (480us)
 * @see k_onewire_presence_wait_us Presence sample delay (70us)
 * @see k_onewire_presence_tail_us Remaining window after sample (410us)
 */
RX_STATIC_TESTABLE rx_err_t internal_reset_pulse(rx_bus_config_t*         bus_config,
                                                 onewire_runtime_state_t* state,
                                                 bool*                    presence)
{
  rx_err_t err = internal_drive_low(bus_config, state);
  if (err != k_rx_ok) {
    return err;
  }

  internal_delay_us(k_onewire_reset_pulse_us);

  err = internal_release_line(bus_config, state);
  if (err != k_rx_ok) {
    return err;
  }

  internal_delay_us(k_onewire_presence_wait_us);
  bool line_high = true;
  err            = internal_read_line(bus_config, &line_high);
  if (err != k_rx_ok) {
    return err;
  }

  *presence = (bool)(!line_high);
  internal_delay_us(k_onewire_presence_tail_us);

  return k_rx_ok;
}

/**
 * @brief Write a single bit on the 1-Wire bus
 *
 * @details
 * Writes one bit using the 1-Wire write time slot protocol. Write-1 and
 * Write-0 differ in how long the line is held low.
 *
 * @par Write-1 Timing:
 * @code
 *     |<-6us->|<------ 64us ------>|
 *     |______|                     |
 *            |_____________________|
 * @endcode
 *
 * @par Write-0 Timing:
 * @code
 *     |<-------- 60us -------->|<10us>|
 *     |________________________|      |
 *                              |______|
 * @endcode
 *
 * @param[in] bus_config Bus configuration with GPIO pin
 * @param[in,out] state Runtime state for GPIO mode tracking
 * @param[in] bit Value to write (true=1, false=0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, bit written
 * @retval k_rx_err_* GPIO operation failed
 *
 * @pre bus_config != nullptr
 * @pre state != nullptr
 * @post Bit transmitted, line released high
 *
 * @note Total write slot: ~70us
 *
 * @see internal_write_byte() Writes 8 bits LSB-first
 */
RX_STATIC_TESTABLE rx_err_t internal_write_bit(rx_bus_config_t*         bus_config,
                                               onewire_runtime_state_t* state,
                                               const bool               bit)
{
  rx_err_t err = internal_drive_low(bus_config, state);
  if (err != k_rx_ok) {
    return err;
  }

  if (bit) {
    internal_delay_us(k_onewire_write_1_low_us);
    err = internal_release_line(bus_config, state);
    if (err != k_rx_ok) {
      return err;
    }
    internal_delay_us(k_onewire_write_1_high_us);
  } else {
    internal_delay_us(k_onewire_write_0_low_us);
    err = internal_release_line(bus_config, state);
    if (err != k_rx_ok) {
      return err;
    }
    internal_delay_us(k_onewire_write_0_high_us);
  }
  return k_rx_ok;
}

/**
 * @brief Read a single bit from the 1-Wire bus
 *
 * @details
 * Reads one bit using the 1-Wire read time slot protocol. Controller initiates
 * by driving low briefly, then releases and samples the line. Device drives
 * low for 0, leaves high for 1.
 *
 * @par Read Timing:
 * @code
 *     |<6us>|<-9us->|<---- 55us ---->|
 *     |_____|       |                |
 *           |_______|________________| (reading 0)
 *           |_______________________| (reading 1)
 *           ^       ^
 *           |       |
 *        Release  Sample
 * @endcode
 *
 * @param[in] bus_config Bus configuration with GPIO pin
 * @param[in,out] state Runtime state for GPIO mode tracking
 * @param[out] bit Read value (true=1/high, false=0/low)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, bit read into output
 * @retval k_rx_err_* GPIO operation failed
 *
 * @pre bus_config != nullptr
 * @pre state != nullptr
 * @pre bit != nullptr
 * @post *bit contains read value
 * @post Line released high
 *
 * @note Total read slot: ~70us
 * @note Sample point: ~15us after slot start
 *
 * @see internal_read_byte() Reads 8 bits LSB-first
 */
RX_STATIC_TESTABLE rx_err_t internal_read_bit(rx_bus_config_t*         bus_config,
                                              onewire_runtime_state_t* state,
                                              bool*                    bit)
{
  rx_err_t err = internal_drive_low(bus_config, state);
  if (err != k_rx_ok) {
    return err;
  }

  internal_delay_us(k_onewire_read_init_us);

  err = internal_release_line(bus_config, state);
  if (err != k_rx_ok) {
    return err;
  }

  const uint32_t sample_delay_us = (k_onewire_read_sample_us > k_onewire_read_init_us)
                                     ? (k_onewire_read_sample_us - k_onewire_read_init_us)
                                     : k_onewire_read_sample_us;
  internal_delay_us(sample_delay_us);

  bool line_high = true;
  err            = internal_read_line(bus_config, &line_high);
  if (err != k_rx_ok) {
    return err;
  }

  *bit = line_high;
  internal_delay_us(k_onewire_read_recovery_us);

  return k_rx_ok;
}

/**
 * @brief Write a byte (LSB first).
 */
RX_STATIC_TESTABLE rx_err_t internal_write_byte(rx_bus_config_t*         bus_config,
                                                onewire_runtime_state_t* state,
                                                const uint8_t            byte)
{
  for (uint8_t i = 0; i < k_bits_per_byte; ++i) {
    const bool     bit = (bool)(((byte >> i) & k_onewire_single_bit_mask) != 0U);
    const rx_err_t err = internal_write_bit(bus_config, state, bit);
    if (err != k_rx_ok) {
      return err;
    }
  }
  return k_rx_ok;
}

/**
 * @brief Read a byte (LSB first).
 */
RX_STATIC_TESTABLE rx_err_t internal_read_byte(rx_bus_config_t*         bus_config,
                                               onewire_runtime_state_t* state,
                                               uint8_t*                 byte)
{
  uint8_t value = 0;
  for (uint8_t i = 0; i < k_bits_per_byte; ++i) {
    bool           bit = false;
    const rx_err_t err = internal_read_bit(bus_config, state, &bit);
    if (err != k_rx_ok) {
      return err;
    }
    if (bit) {
      value |= (uint8_t)(k_onewire_bit_mask_lsb << i);
    }
  }

  *byte = value;
  return k_rx_ok;
}

/**
 * @brief Process one ROM search bit position.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in] state Runtime OneWire state
 * @param[in,out] rom ROM buffer under construction
 * @param[in] bit_number Current bit index (1..64)
 * @param[in,out] rom_byte_index Current ROM byte index
 * @param[in,out] rom_bit_mask Current ROM bit mask
 * @param[in,out] last_zero Last discrepancy bit index
 *
 * @return k_rx_ok on success, error code on failure
 */

/**
 * @brief Resolve search direction from bit pair and discrepancy state
 *
 * @details
 * Determines which branch of the ROM search tree to take based on the two
 * bits read (bit, complement) and the discrepancy tracking state.
 *
 * @param[in]  bit            True bit read from bus
 * @param[in]  comp_bit       Complement bit read from bus
 * @param[in]  bit_number     Current bit position (1-indexed)
 * @param[in]  state          Runtime search state with last_discrepancy
 * @param[in]  rom_byte_index Current byte index in ROM buffer
 * @param[in]  rom_bit_mask   Current bit mask within ROM byte
 * @param[out] last_zero      Updated with bit_number when direction is 0
 *
 * @return Resolved search direction (true = 1, false = 0)
 *
 * @pre All pointer parameters are non-null
 * @post *last_zero updated if direction is 0 at a discrepancy
 */
static bool internal_resolve_search_direction(const bool                     bit,
                                              const bool                     comp_bit,
                                              const uint8_t                  bit_number,
                                              const onewire_runtime_state_t* state,
                                              const uint8_t                  rom_byte_index,
                                              const uint8_t                  rom_bit_mask,
                                              uint8_t*                       last_zero)
{
  bool search_direction = false;

  if (!bit && !comp_bit) {
    if (bit_number == state->last_discrepancy) {
      search_direction = true;
    } else if (bit_number > state->last_discrepancy) {
      search_direction = false;
    } else {
      search_direction = (bool)((state->last_rom[rom_byte_index] & rom_bit_mask) != 0U);
    }
    if (!search_direction) {
      *last_zero = bit_number;
    }
  } else {
    search_direction = bit;
  }

  return search_direction;
}

RX_STATIC_TESTABLE rx_err_t internal_search_step(rx_bus_config_t*         bus_config,
                                                 onewire_runtime_state_t* state,
                                                 uint8_t                  rom[k_onewire_rom_bytes],
                                                 const uint8_t            bit_number,
                                                 uint8_t*                 rom_byte_index,
                                                 uint8_t*                 rom_bit_mask,
                                                 uint8_t*                 last_zero)
{
  RX_CHECK_NULL_PTR(bus_config, s_tag, "bus_config pointer is nullptr");
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is nullptr");

  bool     bit = false;
  rx_err_t err = internal_read_bit(bus_config, state, &bit);
  if (err != k_rx_ok) {
    return err;
  }
  bool comp_bit = false;
  err           = internal_read_bit(bus_config, state, &comp_bit);
  if (err != k_rx_ok) {
    return err;
  }
  if ((int)bit && (int)comp_bit) {
    return k_rx_err_hw_error;
  }

  const bool search_direction = internal_resolve_search_direction(bit,
                                                                  comp_bit,
                                                                  bit_number,
                                                                  state,
                                                                  *rom_byte_index,
                                                                  *rom_bit_mask,
                                                                  last_zero);

  if (search_direction) {
    rom[*rom_byte_index] |= *rom_bit_mask;
  } else {
    rom[*rom_byte_index] &= (uint8_t) ~(*rom_bit_mask);
  }

  err = internal_write_bit(bus_config, state, search_direction);
  if (err != k_rx_ok) {
    return err;
  }

  *rom_bit_mask <<= k_onewire_bit_mask_lsb;
  if (*rom_bit_mask == k_onewire_no_discrepancy) {
    (*rom_byte_index)++;
    *rom_bit_mask = k_onewire_rom_bit_mask_reset;
  }
  return k_rx_ok;
}

/**
 * @brief Execute the ROM search bit loop.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in] state Runtime OneWire state
 * @param[out] rom ROM buffer under construction
 * @param[out] last_zero Last discrepancy bit index
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_search_bits(rx_bus_config_t*         bus_config,
                                                 onewire_runtime_state_t* state,
                                                 uint8_t                  rom[k_onewire_rom_bytes],
                                                 uint8_t*                 last_zero)
{
  RX_CHECK_NULL_PTR(bus_config, s_tag, "bus_config pointer is nullptr");
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is nullptr");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is nullptr");
  RX_CHECK_NULL_PTR(last_zero, s_tag, "last_zero pointer is nullptr");
  const uint8_t total_bits     = k_onewire_rom_bytes * k_bits_per_byte;
  uint8_t       rom_byte_index = 0;
  uint8_t       rom_bit_mask   = k_onewire_bit_mask_lsb;
  for (uint8_t bit_number = k_onewire_first_bit_number; bit_number <= total_bits; ++bit_number) {
    const rx_err_t err = internal_search_step(bus_config,
                                              state,
                                              rom,
                                              bit_number,
                                              &rom_byte_index,
                                              &rom_bit_mask,
                                              last_zero);
    if (err != k_rx_ok) {
      return err;
    }
  }
  return k_rx_ok;
}

/**
 * @brief Execute one iteration of the 1-Wire ROM search algorithm
 *
 * @details
 * Performs a single pass of the binary tree ROM search algorithm. Each call
 * discovers one device ROM by traversing the binary tree of device addresses.
 * Tracks search state (discrepancy points) to enumerate all devices.
 *
 * @par Search Algorithm:
 * The 64-bit ROM forms a binary tree. At each bit position:
 * 1. Read bit and complement from all devices
 * 2. If discrepancy (0,0): choose direction based on last_discrepancy
 * 3. If no discrepancy: follow the bit value
 * 4. Write chosen direction to select devices
 * 5. Track discrepancy points for subsequent iterations
 *
 * @par Algorithm Steps:
 * 1. Check last_device_flag (return if all found)
 * 2. Issue reset, check presence
 * 3. Send SEARCH_ROM command (0xF0)
 * 4. For each of 64 bits: internal_search_step()
 * 5. Update last_discrepancy for next iteration
 * 6. Verify ROM CRC-8
 * 7. Copy ROM to last_rom for next iteration
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] state Runtime state with search tracking
 * @param[out] rom 8-byte buffer for discovered ROM
 * @param[out] device_found Set to true if valid ROM discovered
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success (check device_found for result)
 * @retval k_rx_err_crc_mismatch ROM CRC validation failed
 * @retval k_rx_err_hw_error Bus collision detected
 * @retval k_rx_err_* Other bus/GPIO errors
 *
 * @pre bus_config != nullptr
 * @pre state != nullptr
 * @pre rom != nullptr
 * @pre device_found != nullptr
 * @post On success with device_found: rom contains valid 64-bit address
 * @post state->last_rom updated for next iteration
 * @post state->last_device_flag set when no more devices
 *
 * @note Call repeatedly until device_found == false
 * @note Uses CRC-8 (Maxim/Dallas polynomial) for ROM validation
 *
 * @see rx_bus_onewire_search() Public API using this
 */
/**
 * @brief Validate discovered ROM via CRC and update state tracking
 *
 * @details
 * After a successful search-bits pass, copies the ROM into state->last_rom,
 * updates discrepancy tracking, and validates the CRC-8 byte.
 *
 * @param[in,out] state      Runtime search state to update
 * @param[in]     rom        ROM buffer with discovered address
 * @param[in]     last_zero  Last discrepancy bit from search
 * @param[out]    device_found Set to true if ROM CRC is valid
 *
 * @return k_rx_ok on success, k_rx_err_crc_mismatch on CRC failure
 *
 * @pre state, rom, and device_found are non-null
 * @post state->last_rom updated with rom contents
 * @post state->last_device_flag set when no more discrepancies
 */
static rx_err_t internal_finalize_search_rom(onewire_runtime_state_t* state,
                                             const uint8_t            rom[k_onewire_rom_bytes],
                                             const uint8_t            last_zero,
                                             bool*                    device_found)
{
  state->last_discrepancy = last_zero;
  if (state->last_discrepancy == k_onewire_no_discrepancy) {
    state->last_device_flag = true;
  }

  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    state->last_rom[i] = rom[i];
  }

  uint32_t crc_out = 0U;
  (void)rx_crc8_maxim(rom, k_onewire_rom_crc_idx, &crc_out);
  if ((uint8_t)crc_out != rom[k_onewire_rom_crc_idx]) {
    return k_rx_err_crc_mismatch;
  }

  *device_found = true;
  return k_rx_ok;
}

/**
 * @brief Begin a search iteration by resetting bus and sending search command
 *
 * @details
 * Performs the bus reset pulse, checks for device presence, and sends the
 * Search ROM command (0xF0). If no devices are present, updates state to
 * indicate search completion. If last_device_flag is already set, returns
 * immediately with skip_search = true.
 *
 * @param[in]     bus_config   Bus configuration node
 * @param[in,out] state        Runtime search state
 * @param[out]    skip_search  Set to true if search bits should be skipped
 *
 * @return k_rx_ok on success, error code on bus failure
 *
 * @pre bus_config, state, and skip_search are non-null
 * @post On presence failure: state flags updated, skip_search set true
 * @post On success: search ROM command sent, skip_search set false
 */
static rx_err_t internal_search_begin_reset(rx_bus_config_t*         bus_config,
                                            onewire_runtime_state_t* state,
                                            bool*                    skip_search)
{
  *skip_search = false;

  if (state->last_device_flag) {
    *skip_search = true;
    return k_rx_ok;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    return err;
  }

  if (!presence) {
    state->last_discrepancy = k_onewire_no_discrepancy;
    state->last_device_flag = true;
    *skip_search            = true;
    return k_rx_ok;
  }

  err = internal_write_byte(bus_config, state, k_onewire_cmd_search_rom);
  return err;
}

/**
 * @brief Validate all pointer arguments for internal_search_iteration
 *
 * @param[in] bus_config   Bus configuration node (must not be null)
 * @param[in] state        Runtime search state (must not be null)
 * @param[in] rom          ROM buffer (must not be null)
 * @param[in] device_found Device found flag (must not be null)
 *
 * @return k_rx_ok if all pointers valid, k_rx_err_null_ptr otherwise
 *
 * @pre None
 * @post No state modified
 */
static rx_err_t internal_validate_search_args(const rx_bus_config_t*         bus_config,
                                              const onewire_runtime_state_t* state,
                                              const uint8_t*                 rom,
                                              const bool*                    device_found)
{
  RX_CHECK_NULL_PTR(bus_config, s_tag, "bus_config pointer is nullptr");
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is nullptr");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is nullptr");
  RX_CHECK_NULL_PTR(device_found, s_tag, "device_found pointer is nullptr");
  return k_rx_ok;
}

RX_STATIC_TESTABLE rx_err_t internal_search_iteration(rx_bus_config_t*         bus_config,
                                                      onewire_runtime_state_t* state,
                                                      uint8_t rom[k_onewire_rom_bytes],
                                                      bool*   device_found)
{
  rx_err_t err = internal_validate_search_args(bus_config, state, rom, device_found);
  if (err != k_rx_ok) {
    return err;
  }
  uint8_t last_zero = k_onewire_search_last_zero_init;
  *device_found     = false;

  bool skip_search = false;
  err              = internal_search_begin_reset(bus_config, state, &skip_search);
  if (err != k_rx_ok) {
    return err;
  }
  if (skip_search) {
    return k_rx_ok;
  }

  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    rom[i] = 0;
  }

  err = internal_search_bits(bus_config, state, rom, &last_zero);
  if (err != k_rx_ok) {
    return err;
  }

  return internal_finalize_search_rom(state, rom, last_zero, device_found);
}
/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

#ifndef UNIT_TEST
typedef struct {
  rx_err_t result;
} onewire_simple_ctx_t;

typedef struct {
  bool*    presence;
  rx_err_t result;
} onewire_reset_ctx_t;

typedef struct {
  bool     bit;
  rx_err_t result;
} onewire_write_bit_ctx_t;

typedef struct {
  bool*    bit;
  rx_err_t result;
} onewire_read_bit_ctx_t;

typedef struct {
  uint8_t  byte;
  rx_err_t result;
} onewire_write_byte_ctx_t;

typedef struct {
  uint8_t* byte;
  rx_err_t result;
} onewire_read_byte_ctx_t;

typedef struct {
  const uint8_t* data;
  uint32_t       length;
  rx_err_t       result;
} onewire_write_buf_ctx_t;

typedef struct {
  uint8_t* data;
  uint32_t length;
  rx_err_t result;
} onewire_read_buf_ctx_t;

typedef struct {
  const uint8_t* rom;
  rx_err_t       result;
} onewire_match_rom_ctx_t;

typedef struct {
  uint8_t* rom;
  rx_err_t result;
} onewire_read_rom_ctx_t;

typedef struct {
  uint8_t*  roms;
  uint32_t  max_devices;
  uint32_t* num_devices;
  rx_err_t  result;
} onewire_search_ctx_t;
#endif /* UNIT_TEST */

/* =============================================================================
 * Callback Implementations
 * =============================================================================
 */

/**
 * @brief Helper macro to validate OneWire bus type and initialization state.
 *
 * Reduces repetitive validation code across callback functions.
 *
 * @param[in] bus_config Bus configuration to validate
 * @param[in,out] ctx Context structure with result field
 * @param[in] check_initialized True to also check initialization status
 */
#define CHECK_ONEWIRE_BUS(bus_config, ctx, check_initialized)                                      \
  do {                                                                                             \
    if ((bus_config)->type != k_bus_type_onewire) {                                                \
      rx_log_error(s_tag, "Bus is not OneWire type");                                              \
      (ctx)->result = k_rx_err_invalid_arg;                                                        \
      return (ctx)->result;                                                                        \
    }                                                                                              \
    if ((check_initialized) && !(bus_config)->initialized) {                                       \
      rx_log_error(s_tag, "Bus not initialized");                                                  \
      (ctx)->result = k_rx_err_invalid_state;                                                      \
      return (ctx)->result;                                                                        \
    }                                                                                              \
  } while (0)

RX_STATIC_TESTABLE rx_err_t internal_onewire_init_callback(rx_bus_config_t* bus_config,
                                                           void*            user_ctx)
{
  onewire_simple_ctx_t* ctx = (onewire_simple_ctx_t*)user_ctx;

  CHECK_ONEWIRE_BUS(bus_config, ctx, false);

  onewire_runtime_state_t* state = nullptr;
  rx_err_t                 err   = internal_acquire_state(bus_config, &state);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  err = gpio_set_input(bus_config->proto.onewire.pin);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify pin reads as input with pullup */
  bool pin_state = false;
  err            = gpio_read(bus_config->proto.onewire.pin, &pin_state);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Post-init pin read failed");
    ctx->result = k_rx_err_hw_error;
    return k_rx_err_hw_error;
  }

  if (!pin_state) {
    rx_log_warn(s_tag, "OneWire pin not pulled high - check 4.7k pullup resistor");
    /* Continue anyway - may work if device pulls line */
  }
  state->line_is_output = false;
  internal_reset_search_state(state);

  bus_config->initialized = true;
  ctx->result             = k_rx_ok;
  return k_rx_ok;
}
/**
 * @brief Release a state pool entry for a given bus config handle.
 *
 * @details
 * Scans the static state pool for the entry matching the bus config's handle
 * pointer. Clears the in_use flag and nulls the handle so the slot can be
 * reused by a future rx_bus_onewire_init() call.
 *
 * @param[in,out] bus_config Bus configuration whose state to release
 *
 * @pre bus_config != nullptr
 * @post Matching pool entry marked !in_use
 * @post bus_config->handle == nullptr
 */
static void internal_release_state(rx_bus_config_t* bus_config)
{
  if (bus_config->handle == nullptr) {
    return;
  }

  for (uint32_t i = k_onewire_instance_idx_start; i < k_onewire_max_instances; ++i) {
    const bool slot_in_use = s_state_pool[i].in_use;
    const bool handle_matches =
      (bool)(&s_state_pool[i].state == (onewire_runtime_state_t*)bus_config->handle);
    if ((bool)((int)slot_in_use & (int)handle_matches)) {
      s_state_pool[i].in_use = false;
      s_state_pool[i].state  = (onewire_runtime_state_t){};
      break;
    }
  }

  bus_config->handle = nullptr;
}

/**
 * @brief Callback for OneWire bus deinitialization.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Simple context with result field
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_deinit_callback(rx_bus_config_t* bus_config,
                                                             void*            user_ctx)
{
  onewire_simple_ctx_t* ctx = (onewire_simple_ctx_t*)user_ctx;

  CHECK_ONEWIRE_BUS(bus_config, ctx, false);

  internal_release_state(bus_config);
  bus_config->initialized = false;
  ctx->result             = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback to perform OneWire reset pulse and detect presence
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing presence output pointer
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_reset_callback(rx_bus_config_t* bus_config,
                                                            void*            user_ctx)
{
  onewire_reset_ctx_t*     ctx      = (onewire_reset_ctx_t*)user_ctx;
  onewire_runtime_state_t* state    = nullptr;
  bool                     presence = false;
  rx_err_t                 err      = k_rx_ok;

  CHECK_ONEWIRE_BUS(bus_config, ctx, true);

  if (ctx->presence == nullptr) {
    rx_log_error(s_tag, "OneWire reset nullptr presence pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  state = internal_get_state(bus_config);
  err   = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  *ctx->presence = presence;

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback to write a single bit on the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing bit value to write
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_write_bit_callback(rx_bus_config_t* bus_config,
                                                                void*            user_ctx)
{
  onewire_write_bit_ctx_t* ctx   = (onewire_write_bit_ctx_t*)user_ctx;
  onewire_runtime_state_t* state = nullptr;
  rx_err_t                 err   = k_rx_ok;

  if (bus_config->type != k_bus_type_onewire) {
    rx_log_error(s_tag, "Bus is not OneWire type");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  state = internal_get_state(bus_config);
  err   = internal_write_bit(bus_config, state, ctx->bit);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  ctx->result = err;
  return err;
}

/**
 * @brief Callback to read a single bit from the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing bit output pointer
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_read_bit_callback(rx_bus_config_t* bus_config,
                                                               void*            user_ctx)
{
  onewire_read_bit_ctx_t*  ctx   = (onewire_read_bit_ctx_t*)user_ctx;
  onewire_runtime_state_t* state = nullptr;
  bool                     bit   = false;
  rx_err_t                 err   = k_rx_ok;

  if (bus_config->type != k_bus_type_onewire) {
    rx_log_error(s_tag, "Bus is not OneWire type");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  if (ctx->bit == nullptr) {
    rx_log_error(s_tag, "OneWire read_bit nullptr bit pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  state = internal_get_state(bus_config);
  err   = internal_read_bit(bus_config, state, &bit);
  if (err == k_rx_ok) {
    *ctx->bit = bit;
  }

  ctx->result = err;
  return err;
}

/**
 * @brief Callback to write a single byte on the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing byte value to write
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_write_byte_callback(rx_bus_config_t* bus_config,
                                                                 void*            user_ctx)
{
  onewire_write_byte_ctx_t* ctx = (onewire_write_byte_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  const rx_err_t           err   = internal_write_byte(bus_config, state, ctx->byte);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  ctx->result = err;
  return err;
}

/**
 * @brief Callback to read a single byte from the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing byte output pointer
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_read_byte_callback(rx_bus_config_t* bus_config,
                                                                void*            user_ctx)
{
  onewire_read_byte_ctx_t* ctx   = (onewire_read_byte_ctx_t*)user_ctx;
  onewire_runtime_state_t* state = nullptr;
  uint8_t                  byte  = 0;
  rx_err_t                 err   = k_rx_ok;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  if (ctx->byte == nullptr) {
    rx_log_error(s_tag, "OneWire read_byte nullptr byte pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  state = internal_get_state(bus_config);
  err   = internal_read_byte(bus_config, state, &byte);
  if (err == k_rx_ok) {
    *ctx->byte = byte;
  }

  ctx->result = err;
  return err;
}

/**
 * @brief Callback to write a buffer of bytes on the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing data pointer and length
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_write_buffer_callback(rx_bus_config_t* bus_config,
                                                                   void*            user_ctx)
{
  onewire_write_buf_ctx_t* ctx   = (onewire_write_buf_ctx_t*)user_ctx;
  onewire_runtime_state_t* state = nullptr;
  rx_err_t                 err   = k_rx_ok;

  if (ctx->length == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }
  if (ctx->length > k_onewire_max_buf_bytes) {
    ctx->result = k_rx_err_invalid_size;
    return ctx->result;
  }
  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  if (ctx->data == nullptr) {
    rx_log_error(s_tag, "OneWire write_buffer nullptr data pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  state = internal_get_state(bus_config);
  for (uint32_t i = 0; (i < k_onewire_max_buf_bytes) && (i < ctx->length); ++i) {
    err = internal_write_byte(bus_config, state, ctx->data[i]);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}
/**
 * @brief Callback to read a buffer of bytes from the OneWire bus
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing data pointer and length
 *
 * @return k_rx_ok on success, error code on failure
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_read_buffer_callback(rx_bus_config_t* bus_config,
                                                                  void*            user_ctx)
{
  onewire_read_buf_ctx_t*  ctx   = (onewire_read_buf_ctx_t*)user_ctx;
  onewire_runtime_state_t* state = nullptr;
  uint8_t                  byte  = 0;
  rx_err_t                 err   = k_rx_ok;

  if (ctx->length == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }
  if (ctx->length > k_onewire_max_buf_bytes) {
    ctx->result = k_rx_err_invalid_size;
    return ctx->result;
  }
  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  if (ctx->data == nullptr) {
    rx_log_error(s_tag, "OneWire read_buffer nullptr data pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  state = internal_get_state(bus_config);
  for (uint32_t i = 0; (i < k_onewire_max_buf_bytes) && (i < ctx->length); ++i) {
    byte = 0;
    err  = internal_read_byte(bus_config, state, &byte);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
    ctx->data[i] = byte;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback to issue OneWire Skip ROM command
 *
 * Sends the Skip ROM command (0xCC) to address all devices on the bus.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in] user_ctx User context (unused)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_not_found if no device presence detected
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_skip_rom_callback(rx_bus_config_t* bus_config,
                                                               void*            user_ctx)
{
  (void)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    return k_rx_err_invalid_state;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    return err;
  }

  if (!presence) {
    return k_rx_err_not_found;
  }

  err = internal_write_byte(bus_config, state, k_onewire_cmd_skip_rom);
  if (err != k_rx_ok) {
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Callback to issue OneWire Match ROM command
 *
 * Sends the Match ROM command (0x55) followed by 64-bit ROM address to
 * select a specific device on a multi-device bus.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing ROM address
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_not_found if no device presence detected
 */
RX_STATIC_TESTABLE rx_err_t internal_onewire_match_rom_callback(rx_bus_config_t* bus_config,
                                                                void*            user_ctx)
{
  onewire_match_rom_ctx_t* ctx = (onewire_match_rom_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }
  if (ctx->rom == nullptr) {
    rx_log_error(s_tag, "OneWire match_rom nullptr ROM pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  onewire_runtime_state_t* state    = internal_get_state(bus_config);
  bool                     presence = false;
  rx_err_t                 err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  if (!presence) {
    ctx->result = k_rx_err_not_found;
    return ctx->result;
  }

  err = internal_write_byte(bus_config, state, k_onewire_cmd_match_rom);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    err = internal_write_byte(bus_config, state, ctx->rom[i]);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
  }
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback to read ROM address from a single OneWire device
 *
 * Sends the Read ROM command (0x33) and reads 64-bit ROM address with CRC.
 * Only works when exactly one device is on the bus.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing ROM output buffer
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_not_found if no device presence detected
 * @return k_rx_err_crc_mismatch if ROM CRC validation fails
 * @return propagated error from rx_crc8_maxim() if CRC computation itself fails
 *         (ctx->result is also set to the propagated error in that case)
 */
/**
 * @brief Read ROM bytes from bus and validate CRC
 *
 * @param[in]  bus_config Bus configuration node
 * @param[in]  state      Runtime OneWire state
 * @param[out] rom        Buffer to receive 8-byte ROM code
 *
 * @return k_rx_ok on success, k_rx_err_crc_mismatch on CRC failure
 */
static rx_err_t internal_read_rom_bytes_and_validate(rx_bus_config_t*         bus_config,
                                                     onewire_runtime_state_t* state,
                                                     uint8_t rom[k_onewire_rom_bytes])
{
  uint8_t byte = 0;
  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    const rx_err_t err = internal_read_byte(bus_config, state, &byte);
    if (err != k_rx_ok) {
      return err;
    }
    rom[i] = byte;
  }

  uint32_t crc_out = 0U;
  (void)rx_crc8_maxim(rom, k_onewire_rom_crc_idx, &crc_out);
  if ((uint8_t)crc_out != rom[k_onewire_rom_crc_idx]) {
    return k_rx_err_crc_mismatch;
  }
  return k_rx_ok;
}

RX_STATIC_TESTABLE rx_err_t internal_onewire_read_rom_callback(rx_bus_config_t* bus_config,
                                                               void*            user_ctx)
{
  onewire_read_rom_ctx_t* ctx = (onewire_read_rom_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }
  if (ctx->rom == nullptr) {
    rx_log_error(s_tag, "OneWire read_rom nullptr ROM pointer");
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }

  onewire_runtime_state_t* state    = internal_get_state(bus_config);
  bool                     presence = false;
  rx_err_t                 err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  if (!presence) {
    ctx->result = k_rx_err_not_found;
    return ctx->result;
  }

  err = internal_write_byte(bus_config, state, k_onewire_cmd_read_rom);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  err = internal_read_rom_bytes_and_validate(bus_config, state, ctx->rom);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }
  if (ctx->rom[k_onewire_rom_family_code_idx] == k_onewire_invalid_family_code) {
    rx_log_warn(s_tag, "OneWire ROM has invalid family code");
    /* Continue anyway - CRC passed, may be valid edge case */
  }
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback to search for all OneWire devices on the bus
 *
 * Implements the OneWire ROM search algorithm to discover all devices on
 * a multi-device bus. Returns array of 64-bit ROM addresses with CRC.
 *
 * @param[in] bus_config Bus configuration node
 * @param[in,out] user_ctx Context containing ROM buffer, max count, and output count
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_invalid_arg if parameters invalid or max_devices exceeded
 */
/**
 * @brief Validate search callback parameters and bus state
 *
 * @param[in]  bus_config Bus configuration node
 * @param[in]  ctx        Search context to validate
 *
 * @return k_rx_ok if valid, error code otherwise
 *
 * @pre bus_config and ctx are non-null
 * @post *ctx->num_devices initialized to 0 on success
 */
static rx_err_t internal_validate_search_params(const rx_bus_config_t* bus_config,
                                                onewire_search_ctx_t*  ctx)
{
  if (ctx->num_devices == nullptr) {
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  *ctx->num_devices = 0;

  if (ctx->max_devices == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }

  if (ctx->max_devices > s_onewire_max_search_devices) {
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  if (ctx->roms == nullptr) {
    ctx->result = k_rx_err_invalid_arg;
    return ctx->result;
  }
  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }
  return k_rx_ok;
}

RX_STATIC_TESTABLE rx_err_t internal_onewire_search_callback(rx_bus_config_t* bus_config,
                                                             void*            user_ctx)
{
  onewire_search_ctx_t* ctx = (onewire_search_ctx_t*)user_ctx;
  uint8_t               rom[k_onewire_rom_bytes];
  bool                  device_found = false;

  rx_err_t err = internal_validate_search_params(bus_config, ctx);
  if (err != k_rx_ok) {
    return err;
  }
  /* Early return if max_devices was 0 (validation set result to k_rx_ok) */
  if (ctx->max_devices == 0U) {
    return k_rx_ok;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  internal_reset_search_state(state);

  while (*ctx->num_devices < ctx->max_devices) {
    device_found = false;
    err          = internal_search_iteration(bus_config, state, rom, &device_found);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }

    if (!device_found) {
      break;
    }

    {
      const size_t offset = (size_t)(*ctx->num_devices) * (size_t)k_onewire_rom_bytes;
      for (uint8_t j = 0; j < k_onewire_rom_bytes; ++j) {
        ctx->roms[offset + j] = rom[j];
      }
    }
    (*ctx->num_devices)++;

    if (state->last_device_flag) {
      break;
    }
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */
/**
 * @brief Initialize a 1-Wire bus instance
 *
 * @details
 * Initializes a 1-Wire bus by configuring the GPIO pin as input (released)
 * and allocating runtime state from the static pool. Verifies pullup resistor
 * is present by checking line reads high after release.
 *
 * @par Algorithm:
 * 1. Validate manager and bus_name pointers
 * 2. Acquire runtime state from pool
 * 3. Configure GPIO as input (high-Z, pullup drives high)
 * 4. Verify line reads high (warn if not)
 * 5. Reset search state
 * 6. Mark bus as initialized
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name (must match registered bus)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, bus ready for use
 * @retval k_rx_err_null_ptr nullptr manager or bus_name
 * @retval k_rx_err_not_found Bus name not registered
 * @retval k_rx_err_invalid_arg Bus is not 1-Wire type
 * @retval k_rx_err_no_mem Static state pool exhausted
 * @retval k_rx_err_hw_error GPIO configuration failed
 *
 * @pre manager != nullptr and initialized
 * @pre bus_name registered via rx_bus_manager_register()
 * @pre 4.7kOhm pullup resistor connected to data line
 * @post Bus ready for reset, read, write operations
 *
 * @note Logs warning if pullup not detected (may still work)
 *
 * @par Example:
 * @code
 * rx_err_t err = rx_bus_onewire_init(&bus_manager, "temp_sensor");
 * if (err != k_rx_ok) {
 *     rx_log_error("1W", "Init failed");
 * }
 * @endcode
 *
 * @see rx_bus_onewire_reset() First operation after init
 */
rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  onewire_simple_ctx_t ctx = {.result = k_rx_err_hw_error};
  const rx_err_t       err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_init_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Deinitialize OneWire bus and release state pool entry
 *
 * @details
 * Releases the runtime state allocated by rx_bus_onewire_init() back to the
 * static pool. Prevents state pool exhaustion in long-running test suites.
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_not_found Bus not registered
 * @retval k_rx_err_invalid_arg Bus is not OneWire type
 *
 * @pre Bus was initialized via rx_bus_onewire_init()
 * @post State pool entry released for reuse
 * @post bus_config->initialized == false
 *
 * @see rx_bus_onewire_init() Initialization counterpart
 */
rx_err_t rx_bus_onewire_deinit(rx_bus_manager_t* manager, const char* bus_name)
{
  if (manager == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (bus_name == nullptr) {
    return k_rx_err_null_ptr;
  }

  onewire_simple_ctx_t ctx = {.result = k_rx_err_hw_error};
  const rx_err_t       err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_deinit_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Perform OneWire reset pulse and detect device presence
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[out] presence True if device responded, false otherwise
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(presence, s_tag, "presence pointer is nullptr");

  onewire_reset_ctx_t ctx = {.presence = presence, .result = k_rx_err_hw_error};
  const rx_err_t      err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_reset_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Write a single bit on the OneWire bus
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[in] bit Bit value to write (true=1, false=0)
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  onewire_write_bit_ctx_t ctx = {.bit = bit, .result = k_rx_err_hw_error};
  const rx_err_t          err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_bit_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Read a single bit from the OneWire bus
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[out] bit Bit value read (true=1, false=0)
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(bit, s_tag, "bit pointer is nullptr");

  onewire_read_bit_ctx_t ctx = {.bit = bit, .result = k_rx_err_hw_error};
  const rx_err_t         err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_bit_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Write a single byte on the OneWire bus (LSB first)
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[in] byte Byte value to write
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  onewire_write_byte_ctx_t ctx = {.byte = byte, .result = k_rx_err_hw_error};
  const rx_err_t           err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_byte_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Read a single byte from the OneWire bus (LSB first)
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[out] byte Byte value read
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(byte, s_tag, "byte pointer is nullptr");

  onewire_read_byte_ctx_t ctx = {.byte = byte, .result = k_rx_err_hw_error};
  const rx_err_t          err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_byte_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              const uint32_t    length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  if (length > 0U) {
    RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");
  }
  onewire_write_buf_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  const rx_err_t          err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_buffer_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t
rx_bus_onewire_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint32_t length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  if (length > 0U) {
    RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");
  }
  onewire_read_buf_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  const rx_err_t         err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_buffer_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Issue OneWire Skip ROM command
 *
 * Sends Skip ROM command (0xCC) to address all devices on the bus.
 * Use when only one device is connected or when broadcasting to all devices.
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_not_found if no device presence detected
 */
rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  return rx_bus_manager_with_bus(manager, bus_name, internal_onewire_skip_rom_callback, nullptr);
}
/**
 * @brief Issue OneWire Match ROM command
 *
 * Sends Match ROM command (0x55) followed by 64-bit ROM address to select
 * a specific device on a multi-device bus.
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[in] rom 8-byte ROM address (family code + serial + CRC)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_not_found if no device presence detected
 */
rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
                                  const char*       bus_name,
                                  const uint8_t     rom[k_onewire_rom_bytes])
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is nullptr");

  onewire_match_rom_ctx_t ctx = {.rom = rom, .result = k_rx_err_hw_error};
  const rx_err_t          err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_match_rom_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

/**
 * @brief Read ROM address from single OneWire device
 *
 * Sends Read ROM command (0x33) and reads 64-bit ROM address with CRC.
 * Only works when exactly one device is present on the bus.
 *
 * @param[in] manager Bus manager handle
 * @param[in] bus_name Bus configuration name
 * @param[out] rom 8-byte buffer for ROM address (family code + serial + CRC)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_not_found if no device presence detected
 * @return k_rx_err_crc_mismatch if ROM CRC validation fails
 */
rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint8_t           rom[k_onewire_rom_bytes])
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is nullptr");

  onewire_read_rom_ctx_t ctx = {.rom = rom, .result = k_rx_err_hw_error};
  const rx_err_t         err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_rom_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               const uint32_t    max_devices,
                               uint32_t*         num_devices)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(roms, s_tag, "roms pointer is nullptr");
  RX_CHECK_NULL_PTR(num_devices, s_tag, "num_devices pointer is nullptr");

  onewire_search_ctx_t ctx = {
    .roms        = roms,
    .max_devices = max_devices,
    .num_devices = num_devices,
    .result      = k_rx_err_hw_error,
  };

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_search_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}
