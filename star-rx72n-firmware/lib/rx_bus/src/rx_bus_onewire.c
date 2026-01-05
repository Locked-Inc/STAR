/* lib/rx_bus/src/rx_bus_onewire.c */

/**
 * @file rx_bus_onewire.c
 * @brief OneWire (1-Wire) bus implementation using GPIO bit-banging.
 *
 * Provides reset/bit/byte primitives plus ROM helpers (skip, match, read,
 * search). Uses open-drain style control by switching the GPIO between output
 * low and input (released high via external pull-up).
 *
 * @date 2026-01-03
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_onewire.h"

#include <string.h>

#include "hardware.h"
#include "rx72n_clock.h"
#include "rx72n_cmt_regs.h"
#include "rx72n_regs.h"
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"

static const char* s_tag = "BUS_ONEWIRE";

/* =============================================================================
 * Internal State Tracking
 * =============================================================================
 */

/** @brief OneWire driver constants. */
typedef enum {
  k_onewire_max_instances   = k_max_buses,
  k_onewire_single_bit_mask = 0x01U,
} onewire_driver_constants_t;

/**
 * @brief Runtime state per OneWire bus.
 *
 * Tracks ROM search progress plus cached drive mode to avoid redundant GPIO
 * reconfiguration.
 */
typedef struct {
  uint8_t last_rom[k_onewire_rom_bytes]; /**< Last ROM discovered */
  uint8_t last_discrepancy;              /**< Search tree discrepancy bit */
  bool    last_device_flag;              /**< True when last device enumerated */
  bool    line_is_output;                /**< Current GPIO drive mode */
} onewire_runtime_state_t;

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
typedef enum {
  k_onewire_prcr_unlock         = 0xA50B,
  k_onewire_prcr_lock           = 0xA500,
  k_onewire_mstpb_cmt_bit       = 15, /**< CMT module stop bit in MSTPCRB */
  k_onewire_cmt3_start_bit      = 1,  /**< CMSTR1 bit controlling CMT3 */
  k_onewire_cmt_divider_setting = 0,  /**< CKS = 0 -> PCLKB/8 */
  k_onewire_cmt_divider_value   = 8,  /**< Actual divider value */
  k_onewire_cmt_clk_shift       = 0,  /**< CMCR clock select shift */
  k_onewire_timer_counter_max   = 0xFFFF,
  k_onewire_us_per_second       = 1000000UL,
  k_onewire_timer_rounding      = k_onewire_us_per_second - 1UL,
} onewire_delay_hw_constants_t;

static bool s_delay_timer_initialized = false;

/**
 * @brief Initialize the dedicated CMT3 timer for microsecond delays.
 */
static void internal_delay_timer_init(void)
{
  if (s_delay_timer_initialized) {
    return;
  }

  /* Enable CMT module clock */
  system_regs()->prcr = k_onewire_prcr_unlock;
  system_regs()->mstpcrb &= ~(1UL << k_onewire_mstpb_cmt_bit);
  system_regs()->prcr = k_onewire_prcr_lock;

  /* Stop CMT3 before reconfiguration */
  cmt_ctrl()->cmstr1 &= ~(1U << k_onewire_cmt3_start_bit);

  /* Configure free-running counter (no interrupts) */
  cmt3()->cmcr  = (uint16_t)(k_onewire_cmt_divider_setting << k_onewire_cmt_clk_shift);
  cmt3()->cmcor = k_onewire_timer_counter_max;
  cmt3()->cmcnt = 0;

  /* Start timer */
  cmt_ctrl()->cmstr1 |= (1U << k_onewire_cmt3_start_bit);

  s_delay_timer_initialized = true;
}

/**
 * @brief Delay for the specified number of microseconds using CMT3.
 */
static void internal_delay_us(uint32_t microseconds)
{
  if (microseconds == 0U) {
    return;
  }

  internal_delay_timer_init();

  const uint32_t timer_hz = k_pclkb_hz / k_onewire_cmt_divider_value;
  uint64_t       ticks = ((uint64_t)microseconds * (uint64_t)timer_hz + k_onewire_timer_rounding) /
                   k_onewire_us_per_second;
  if (ticks == 0ULL) {
    ticks = 1ULL;
  }

  while (ticks > 0ULL) {
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
 * @brief Ensure runtime state exists for a bus (allocate if needed).
 *
 * @param[in,out] bus_config Bus configuration node
 * @param[out] state Pointer to receive runtime state
 *
 * @return k_rx_ok on success, k_rx_err_no_mem if pool exhausted
 */
static rx_err_t internal_acquire_state(rx_bus_config_t* bus_config, onewire_runtime_state_t** state)
{
  if (bus_config->handle != NULL) {
    *state = (onewire_runtime_state_t*)bus_config->handle;
    return k_rx_ok;
  }

  for (uint32_t i = 0; i < k_onewire_max_instances; ++i) {
    if (!s_state_pool[i].in_use) {
      s_state_pool[i].in_use = true;
      memset(&s_state_pool[i].state, 0, sizeof(onewire_runtime_state_t));
      bus_config->handle = &s_state_pool[i].state;
      *state             = &s_state_pool[i].state;
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
 * @return Pointer to state or NULL if missing
 */
static inline onewire_runtime_state_t* internal_get_state(rx_bus_config_t* bus_config)
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
static rx_err_t
internal_set_drive_mode(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool output)
{
  if (output && !state->line_is_output) {
    rx_err_t err = gpio_set_output(bus_config->proto.onewire.port, bus_config->proto.onewire.pin);
    if (err != k_rx_ok) {
      return err;
    }
    state->line_is_output = true;
  } else if (!output && state->line_is_output) {
    rx_err_t err = gpio_set_input(bus_config->proto.onewire.port, bus_config->proto.onewire.pin);
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
static rx_err_t internal_drive_low(rx_bus_config_t* bus_config, onewire_runtime_state_t* state)
{
  rx_err_t err = internal_set_drive_mode(bus_config, state, true);
  if (err != k_rx_ok) {
    return err;
  }
  return gpio_write_low(bus_config->proto.onewire.port, bus_config->proto.onewire.pin);
}

/**
 * @brief Release the OneWire line (input/high-Z, external pull-up drives high).
 */
static rx_err_t internal_release_line(rx_bus_config_t* bus_config, onewire_runtime_state_t* state)
{
  return internal_set_drive_mode(bus_config, state, false);
}

/**
 * @brief Read current level on the OneWire line.
 *
 * @param[out] high True if line is high
 */
static rx_err_t internal_read_line(rx_bus_config_t* bus_config, bool* high)
{
  return gpio_read(bus_config->proto.onewire.port, bus_config->proto.onewire.pin, high);
}

/**
 * @brief Reset runtime search tracking.
 */
static void internal_reset_search_state(onewire_runtime_state_t* state)
{
  memset(state->last_rom, 0, sizeof(state->last_rom));
  state->last_discrepancy = 0;
  state->last_device_flag = false;
}

/* =============================================================================
 * Protocol Primitive Helpers
 * =============================================================================
 */

/**
 * @brief Issue OneWire reset pulse and check for presence.
 *
 * @param[out] presence True if device responded
 */
static rx_err_t
internal_reset_pulse(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool* presence)
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

  *presence = !line_high;

  internal_delay_us(k_onewire_presence_timeout_us);

  return k_rx_ok;
}

/**
 * @brief Write a single bit on the OneWire bus.
 */
static rx_err_t
internal_write_bit(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool bit)
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
 * @brief Read a single bit from the OneWire bus.
 */
static rx_err_t
internal_read_bit(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool* bit)
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

  uint32_t sample_delay_us = (k_onewire_read_sample_us > k_onewire_read_init_us)
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
static rx_err_t
internal_write_byte(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, uint8_t byte)
{
  for (uint8_t i = 0; i < k_bits_per_byte; ++i) {
    bool     bit = ((byte >> i) & k_onewire_single_bit_mask) != 0U;
    rx_err_t err = internal_write_bit(bus_config, state, bit);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read a byte (LSB first).
 */
static rx_err_t
internal_read_byte(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, uint8_t* byte)
{
  uint8_t value = 0;

  for (uint8_t i = 0; i < k_bits_per_byte; ++i) {
    bool     bit = false;
    rx_err_t err = internal_read_bit(bus_config, state, &bit);
    if (err != k_rx_ok) {
      return err;
    }

    if (bit) {
      value |= (uint8_t)(1U << i);
    }
  }

  *byte = value;
  return k_rx_ok;
}

/**
 * @brief Perform a single pass of the ROM search algorithm.
 *
 * @param[out] rom Newly discovered ROM (if any)
 * @param[out] device_found True if ROM valid
 */
static rx_err_t internal_search_iteration(rx_bus_config_t*         bus_config,
                                          onewire_runtime_state_t* state,
                                          uint8_t                  rom[k_onewire_rom_bytes],
                                          bool*                    device_found)
{
  *device_found = false;

  if (state->last_device_flag) {
    return k_rx_ok;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    return err;
  }

  if (!presence) {
    state->last_discrepancy = 0;
    state->last_device_flag = true;
    return k_rx_ok;
  }

  err = internal_write_byte(bus_config, state, k_onewire_cmd_search_rom);
  if (err != k_rx_ok) {
    return err;
  }

  memset(rom, 0, k_onewire_rom_bytes);

  uint8_t last_zero      = 0;
  uint8_t rom_byte_index = 0;
  uint8_t rom_bit_mask   = 1;
  uint8_t total_bits     = k_onewire_rom_bytes * k_bits_per_byte;

  for (uint8_t bit_number = 1; bit_number <= total_bits; ++bit_number) {
    bool bit = false;
    bool comp_bit;

    err = internal_read_bit(bus_config, state, &bit);
    if (err != k_rx_ok) {
      return err;
    }

    err = internal_read_bit(bus_config, state, &comp_bit);
    if (err != k_rx_ok) {
      return err;
    }

    if (bit && comp_bit) {
      return k_rx_err_hw_error; /* No devices responded */
    }

    bool search_direction;
    if (!bit && !comp_bit) {
      if (bit_number == state->last_discrepancy) {
        search_direction = true;
      } else if (bit_number > state->last_discrepancy) {
        search_direction = false;
      } else {
        search_direction = ((state->last_rom[rom_byte_index] & rom_bit_mask) != 0U);
      }

      if (!search_direction) {
        last_zero = bit_number;
      }
    } else {
      search_direction = bit;
    }

    if (search_direction) {
      rom[rom_byte_index] |= rom_bit_mask;
    } else {
      rom[rom_byte_index] &= (uint8_t)~rom_bit_mask;
    }

    err = internal_write_bit(bus_config, state, search_direction);
    if (err != k_rx_ok) {
      return err;
    }

    rom_bit_mask <<= 1;
    if (rom_bit_mask == 0U) {
      rom_byte_index++;
      rom_bit_mask = 1;
    }
  }

  state->last_discrepancy = last_zero;
  if (state->last_discrepancy == 0) {
    state->last_device_flag = true;
  }

  memcpy(state->last_rom, rom, k_onewire_rom_bytes);

  uint8_t crc = rx_crc8_maxim(rom, k_onewire_rom_bytes - 1U);
  if (crc != rom[k_onewire_rom_bytes - 1U]) {
    return k_rx_err_crc_mismatch;
  }

  *device_found = true;
  return k_rx_ok;
}

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

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

static rx_err_t internal_onewire_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_simple_ctx_t* ctx = (onewire_simple_ctx_t*)user_ctx;

  CHECK_ONEWIRE_BUS(bus_config, ctx, false);

  onewire_runtime_state_t* state = NULL;
  rx_err_t                 err   = internal_acquire_state(bus_config, &state);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  err = gpio_set_input(bus_config->proto.onewire.port, bus_config->proto.onewire.pin);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  state->line_is_output = false;
  internal_reset_search_state(state);

  bus_config->initialized = true;
  ctx->result             = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_onewire_reset_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_reset_ctx_t* ctx = (onewire_reset_ctx_t*)user_ctx;

  CHECK_ONEWIRE_BUS(bus_config, ctx, true);

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  *ctx->presence = presence;
  ctx->result    = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_onewire_write_bit_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_write_bit_ctx_t* ctx = (onewire_write_bit_ctx_t*)user_ctx;

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

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  rx_err_t err = internal_write_bit(bus_config, state, ctx->bit);
  ctx->result  = err;
  return err;
}

static rx_err_t internal_onewire_read_bit_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_read_bit_ctx_t* ctx = (onewire_read_bit_ctx_t*)user_ctx;

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

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  bool     bit = false;
  rx_err_t err = internal_read_bit(bus_config, state, &bit);
  if (err == k_rx_ok) {
    *ctx->bit = bit;
  }
  ctx->result = err;
  return err;
}

static rx_err_t internal_onewire_write_byte_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_write_byte_ctx_t* ctx = (onewire_write_byte_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  rx_err_t err = internal_write_byte(bus_config, state, ctx->byte);
  ctx->result  = err;
  return err;
}

static rx_err_t internal_onewire_read_byte_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_read_byte_ctx_t* ctx = (onewire_read_byte_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  uint8_t  byte = 0;
  rx_err_t err  = internal_read_byte(bus_config, state, &byte);
  if (err == k_rx_ok) {
    *ctx->byte = byte;
  }

  ctx->result = err;
  return err;
}

static rx_err_t internal_onewire_write_buffer_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_write_buf_ctx_t* ctx = (onewire_write_buf_ctx_t*)user_ctx;

  if (ctx->length == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  for (uint32_t i = 0; i < ctx->length; ++i) {
    rx_err_t err = internal_write_byte(bus_config, state, ctx->data[i]);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_onewire_read_buffer_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_read_buf_ctx_t* ctx = (onewire_read_buf_ctx_t*)user_ctx;

  if (ctx->length == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    rx_log_error(s_tag, "Bus invalid or not initialized");
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  for (uint32_t i = 0; i < ctx->length; ++i) {
    uint8_t  byte = 0;
    rx_err_t err  = internal_read_byte(bus_config, state, &byte);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
    ctx->data[i] = byte;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_onewire_skip_rom_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  (void)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    return k_rx_err_invalid_state;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    return k_rx_err_invalid_state;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
  if (err != k_rx_ok) {
    return err;
  }

  if (!presence) {
    return k_rx_err_not_found;
  }

  return internal_write_byte(bus_config, state, k_onewire_cmd_skip_rom);
}

static rx_err_t internal_onewire_match_rom_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_match_rom_ctx_t* ctx = (onewire_match_rom_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
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

static rx_err_t internal_onewire_read_rom_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_read_rom_ctx_t* ctx = (onewire_read_rom_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  bool     presence = false;
  rx_err_t err      = internal_reset_pulse(bus_config, state, &presence);
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

  for (uint8_t i = 0; i < k_onewire_rom_bytes; ++i) {
    uint8_t byte = 0;
    err          = internal_read_byte(bus_config, state, &byte);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }
    ctx->rom[i] = byte;
  }

  uint8_t crc = rx_crc8_maxim(ctx->rom, k_onewire_rom_bytes - 1U);
  if (crc != ctx->rom[k_onewire_rom_bytes - 1U]) {
    ctx->result = k_rx_err_crc_mismatch;
    return ctx->result;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_onewire_search_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  onewire_search_ctx_t* ctx = (onewire_search_ctx_t*)user_ctx;

  *ctx->num_devices = 0;

  if (ctx->max_devices == 0U) {
    ctx->result = k_rx_ok;
    return k_rx_ok;
  }

  if (bus_config->type != k_bus_type_onewire || !bus_config->initialized) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  onewire_runtime_state_t* state = internal_get_state(bus_config);
  if (state == NULL) {
    ctx->result = k_rx_err_invalid_state;
    return ctx->result;
  }

  internal_reset_search_state(state);

  while (*ctx->num_devices < ctx->max_devices) {
    uint8_t  rom[k_onewire_rom_bytes];
    bool     device_found = false;
    rx_err_t err          = internal_search_iteration(bus_config, state, rom, &device_found);
    if (err != k_rx_ok) {
      ctx->result = err;
      return err;
    }

    if (!device_found) {
      break;
    }

    memcpy(&ctx->roms[*ctx->num_devices * k_onewire_rom_bytes], rom, k_onewire_rom_bytes);
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

rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  onewire_simple_ctx_t ctx = {.result = k_rx_err_hw_error};
  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_onewire_init_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(presence, s_tag, "presence pointer is NULL");

  onewire_reset_ctx_t ctx = {.presence = presence, .result = k_rx_err_hw_error};
  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_onewire_reset_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  onewire_write_bit_ctx_t ctx = {.bit = bit, .result = k_rx_err_hw_error};
  rx_err_t                err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_bit_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(bit, s_tag, "bit pointer is NULL");

  onewire_read_bit_ctx_t ctx = {.bit = bit, .result = k_rx_err_hw_error};
  rx_err_t               err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_bit_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  onewire_write_byte_ctx_t ctx = {.byte = byte, .result = k_rx_err_hw_error};
  rx_err_t                 err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_byte_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(byte, s_tag, "byte pointer is NULL");

  onewire_read_byte_ctx_t ctx = {.byte = byte, .result = k_rx_err_hw_error};
  rx_err_t                err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_byte_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              uint32_t          length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  if (length > 0U) {
    RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");
  }

  onewire_write_buf_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  rx_err_t                err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_write_buffer_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t
rx_bus_onewire_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint32_t length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  if (length > 0U) {
    RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");
  }

  onewire_read_buf_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  rx_err_t               err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_buffer_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  return rx_bus_manager_with_bus(manager, bus_name, internal_onewire_skip_rom_callback, NULL);
}

rx_err_t
rx_bus_onewire_match_rom(rx_bus_manager_t* manager, const char* bus_name, const uint8_t rom[8])
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is NULL");

  onewire_match_rom_ctx_t ctx = {.rom = rom, .result = k_rx_err_hw_error};
  rx_err_t                err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_match_rom_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager, const char* bus_name, uint8_t rom[8])
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom pointer is NULL");

  onewire_read_rom_ctx_t ctx = {.rom = rom, .result = k_rx_err_hw_error};
  rx_err_t               err =
    rx_bus_manager_with_bus(manager, bus_name, internal_onewire_read_rom_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}

rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               uint32_t          max_devices,
                               uint32_t*         num_devices)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(roms, s_tag, "roms pointer is NULL");
  RX_CHECK_NULL_PTR(num_devices, s_tag, "num_devices pointer is NULL");

  onewire_search_ctx_t ctx = {
    .roms        = roms,
    .max_devices = max_devices,
    .num_devices = num_devices,
    .result      = k_rx_err_hw_error,
  };

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_onewire_search_callback, &ctx);
  if (err != k_rx_ok) {
    return err;
  }
  return ctx.result;
}
