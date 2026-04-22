/**
 * @file led_status_task.c
 * @brief LED Status Indicator Task Implementation
 *
 * @details
 * Implements a low-priority ThreadX task that drives 6 status LEDs at 20 Hz
 * to provide visual system health feedback. Each LED is mapped to a specific
 * system function and driven by state read from shared_data.
 *
 * ## LED Assignments
 *
 * | LED | Pin | Port | Function               | Pattern                        |
 * |-----|-----|------|------------------------|--------------------------------|
 * | 0   | PA7 | A    | System Heartbeat       | 1 Hz toggle (500ms on/off)     |
 * | 1   | PB0 | B    | Error Indicator        | Fast blink on any fault        |
 * | 2   | P71 | 7    | Motor Active           | Solid on when motors running   |
 * | 3   | P72 | 7    | Communication Activity | 100ms pulse on command rx      |
 * | 4   | PB1 | B    | Obstacle Detected      | Solid on when obstacle near    |
 * | 5   | PB2 | B    | E-Stop Active          | Solid on during emergency stop |
 *
 * ## Task Characteristics
 *
 * | Metric           | Value    |
 * |------------------|----------|
 * | Priority         | 17       |
 * | Stack            | 512 B    |
 * | Update Rate      | 20 Hz    |
 * | CPU Utilization  | < 0.01%  |
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "led_status_task.h"

#include "hardware.h"
#include "hardware_config.h"
#include "rx_check.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_poeg.h"
#include "rx_port_utils.h"
#include "shared_data.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum led_task_constants_t
 * @brief LED status task configuration constants
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_led_task_stack_size   = 512, /**< Stack size in bytes (GPIO writes only) */
  k_led_task_priority     = 17,  /**< ThreadX priority (low - visual only) */
  k_led_task_input        = 0,   /**< Thread entry input parameter */
  k_led_task_period_ticks = 5,   /**< 50ms period = 20 Hz (5 ticks @ 100 Hz) */
} led_task_constants_t;

/**
 * @enum led_timing_constants_t
 * @brief LED blink timing constants in task ticks (50ms per tick)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_led_heartbeat_half_period = 10, /**< 10 ticks x 50ms = 500ms half-period (1 Hz) */
  k_led_error_half_period     = 3,  /**< 3 ticks x 50ms = 150ms half-period (fast blink) */
  k_led_comm_pulse_duration   = 2,  /**< 2 ticks x 50ms = 100ms comm activity pulse */
} led_timing_constants_t;

/**
 * @enum led_timing_divisor_t
 * @brief Divisor constants for LED blink period calculations
 *
 * @details
 * Provides named constants for arithmetic performed on LED timing values.
 * Using a named divisor instead of a bare literal prevents magic numbers
 * and makes the intent of half-period calculations self-documenting.
 *
 * @invariant k_led_half_period_divisor must equal 2 (half-period = full/2)
 *
 * @code
 * // Convert a full-period counter to half-period comparison:
 * bool led_on = (s_heartbeat_counter < (k_led_heartbeat_half_period
 *                                       / k_led_half_period_divisor));
 * @endcode
 *
 * @see led_timing_constants_t Full-period and half-period tick counts
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_led_half_period_divisor = 2, /**< Divisor to convert full period to half period (full / 2) */
} led_timing_divisor_t;

/**
 * @enum led_index_t
 * @brief LED index assignments for clarity
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_led_idx_heartbeat = 0, /**< LED 0: System heartbeat */
  k_led_idx_error     = 1, /**< LED 1: Error indicator */
  k_led_idx_motor     = 2, /**< LED 2: Motor active */
  k_led_idx_comm      = 3, /**< LED 3: Communication activity */
  k_led_idx_obstacle  = 4, /**< LED 4: Obstacle detected */
  k_led_idx_estop     = 5, /**< LED 5: E-stop active */
} led_index_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief ThreadX thread control block */
static TX_THREAD s_led_thread;

/** @brief Static thread stack (no dynamic allocation) */
static uint8_t s_led_stack[k_led_task_stack_size];

/** @brief Task creation guard flag */
static bool s_led_created = false;

/** @brief Log tag for this module */
static const char* const s_tag = "LED";

/** @brief LED port numbers lookup table (indexed by LED number) */
static const uint8_t s_led_ports[k_led_count] = {
  k_led_0_port,
  k_led_1_port,
  k_led_2_port,
  k_led_3_port,
  k_led_4_port,
  k_led_5_port,
};

/** @brief LED pin numbers lookup table (indexed by LED number) */
static const uint8_t s_led_pins[k_led_count] = {
  k_led_0_pin,
  k_led_1_pin,
  k_led_2_pin,
  k_led_3_pin,
  k_led_4_pin,
  k_led_5_pin,
};

/** @brief Heartbeat blink counter (incremented each task tick) */
static uint8_t s_heartbeat_counter = 0;

/** @brief Error blink counter */
static uint8_t s_error_counter = 0;

/** @brief Communication pulse countdown (ticks remaining) */
static uint8_t s_comm_pulse_remaining = 0;

/** @brief Last seen motor command sequence number for comm activity detection */
static uint32_t s_last_comm_sequence = 0;

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_led_task_entry(ULONG input);
static void internal_led_set(uint8_t led_index, bool on);
static void internal_led_init_gpio(void);
static void internal_update_heartbeat_led(void);
static void internal_update_error_and_motor_leds(void);
static void internal_update_comm_led(void);
static void internal_update_obstacle_and_estop_leds(void);

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create the LED status indicator task
 *
 * @details
 * Creates and starts the LED ThreadX task with low priority (17) for
 * visual system health feedback at 20 Hz. GPIO pins are initialized
 * as outputs inside the task entry to keep creation lightweight.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_invalid_state Already created
 * @retval k_rx_err_rtos_thread_create ThreadX error
 *
 * @pre ThreadX kernel running
 * @pre shared_data_init() called
 *
 * @post LEDTask created and running at 20 Hz
 * @post s_led_created set to true
 *
 * @note Thread Safety: Not thread-safe, call from tx_application_define only
 *
 * @since Version 1.0.0
 */
rx_err_t led_status_task_create(void)
{
  /* Check if already created */
  RX_ASSERT(!s_led_created, "LED task already created");
  if (s_led_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  UINT tx_status = tx_thread_create(&s_led_thread,
                                    "LEDTask",
                                    internal_led_task_entry,
                                    k_led_task_input,
                                    s_led_stack,
                                    k_led_task_stack_size,
                                    k_led_task_priority,
                                    k_led_task_priority,
                                    TX_NO_TIME_SLICE,
                                    TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_led_created = true;
  rx_log_info(s_tag, "LED status task created (priority 17, 20 Hz)");

  return k_rx_ok;
}

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Initialize LED GPIO pins as outputs with all LEDs off
 *
 * @details
 * Iterates over all k_led_count LED entries in the s_led_ports and s_led_pins
 * lookup tables and configures each pin as a GPIO output set low (LED off).
 * For each pin the function:
 * 1. Retrieves the PORT register base via rx_port_get_base()
 * 2. Clears the PMR bit to set the pin to GPIO mode
 * 3. Sets the PDR bit to configure the pin as an output
 * 4. Clears the PODR bit to drive the pin low (LED off)
 *
 * Invalid port entries (rx_port_get_base returns nullptr) are skipped with
 * an error log; remaining valid pins continue to be initialized.
 *
 * @pre PORT register space must be accessible (clock and power initialized)
 * @pre s_led_ports[] and s_led_pins[] must contain valid PORT/pin values
 *
 * @post All valid LED pins are configured as GPIO outputs, driven low
 * @post Invalid LED port entries are skipped (error logged, not fatal)
 *
 * @note Thread Safety: Call once during task initialization only; not
 *       re-entrant due to read-modify-write on PORT registers
 *
 * @code
 * // Called internally during LED task startup:
 * static void internal_led_task_entry(ULONG input) {
 *     (void)input;
 *     internal_led_init_gpio();   // All LEDs off after this
 *     while (true) { ... }        // Main blink loop
 * }
 * @endcode
 *
 * @see internal_led_task_entry() Only caller of this function
 * @see internal_led_set() Runtime LED state control after initialization
 * @see rx_port_get_base() PORT register accessor used for each pin
 *
 * @since Version 1.0.0
 */
static void internal_led_init_gpio(void)
{
  for (uint8_t i = 0; i < k_led_count; i++) {
    volatile rx_port_regs_t* port = rx_port_get_base(s_led_ports[i]);
    if (port == nullptr) {
      rx_log_error_val(s_tag, "Invalid LED port", (uint32_t)s_led_ports[i]);
      continue;
    }

    const uint8_t pin_mask = (uint8_t)(1U << s_led_pins[i]);

    /* Set pin to GPIO mode (clear PMR bit) */
    port->pmr &= (uint8_t)~pin_mask;

    /* Set pin as output (set PDR bit) */
    port->pdr |= pin_mask;

    /* Set pin low - LED off (clear PODR bit) */
    port->podr &= (uint8_t)~pin_mask;
  }
}

/**
 * @brief Set an LED on or off by writing to its PORT output data register
 *
 * @details
 * Drives a single LED high (on) or low (off) by writing to the PODR bit of
 * the corresponding PORT register. Out-of-range led_index values or invalid
 * port pointers return silently to prevent crashes from defensive callers.
 *
 * Algorithm:
 * 1. Bounds-check led_index against k_led_count; return silently if invalid
 * 2. Retrieve PORT register base from s_led_ports[led_index]
 * 3. Return silently if port is nullptr (invalid port configuration)
 * 4. Compute pin_mask = (1U << s_led_pins[led_index])
 * 5. Set (on=true) or clear (on=false) the PODR bit for the pin
 *
 * @param[in] led_index LED index (0 to k_led_count - 1, inclusive)
 *   - 0: Heartbeat LED
 *   - 1: Error LED
 *   - 2: Motor active LED
 *   - 3: Communication activity LED
 *   - 4: Obstacle detected LED
 *   - 5: E-stop active LED
 * @param[in] on true to turn LED on (PODR bit set), false to turn off
 *
 * @pre LED GPIO initialized via internal_led_init_gpio()
 * @pre led_index < k_led_count (silently ignored if out of range)
 *
 * @post LED PODR bit set or cleared to match the requested on/off state
 * @post No change if led_index is out of range or port is invalid
 *
 * @note Thread Safety: Safe from single task context; do not call from
 *       multiple tasks without external synchronization
 *
 * @code
 * // Turn heartbeat LED on:
 * internal_led_set(k_led_idx_heartbeat, true);
 *
 * // Turn error LED off:
 * internal_led_set(k_led_idx_error, false);
 * @endcode
 *
 * @see internal_led_init_gpio() Must be called first to configure pins as outputs
 * @see led_index_t Named constants for led_index values
 * @see internal_led_task_entry() Main loop that calls this function at 20 Hz
 *
 * @since Version 1.0.0
 */
static void internal_led_set(uint8_t led_index, bool on)
{
  if (led_index >= k_led_count) {
    return;
  }

  volatile rx_port_regs_t* port = rx_port_get_base(s_led_ports[led_index]);
  if (port == nullptr) {
    return;
  }

  const uint8_t pin_mask = (uint8_t)(1U << s_led_pins[led_index]);

  if (on) {
    port->podr |= pin_mask;
  } else {
    port->podr &= (uint8_t)~pin_mask;
  }
}

/**
 * @brief Update LED 0 (heartbeat): 1 Hz toggle (10 ticks on, 10 ticks off)
 *
 * @details
 * Increments s_heartbeat_counter each call (50 ms tick), wraps at
 * k_led_heartbeat_half_period, and drives LED 0 on during the first half
 * of the period.
 *
 * @pre internal_led_init_gpio() called
 * @post s_heartbeat_counter incremented and wrapped
 * @post LED 0 state updated
 *
 * @note Not thread-safe; called only from internal_led_task_entry()
 * @since Version 1.0.0
 */
static void internal_update_heartbeat_led(void)
{
  s_heartbeat_counter++;
  if (s_heartbeat_counter >= k_led_heartbeat_half_period) {
    s_heartbeat_counter = 0;
  }
  internal_led_set(
    k_led_idx_heartbeat,
    (bool)(s_heartbeat_counter < (k_led_heartbeat_half_period / k_led_half_period_divisor)));
}

/**
 * @brief Update LED 1 (error) and LED 2 (motor active) from motor state
 *
 * @details
 * Reads motor_state from shared_data once and drives:
 * - LED 1 (error): fast blink when any fault_flags[i] != 0
 * - LED 2 (motor): solid on when any duty_cycle_percent[i] > 0
 *
 * @pre internal_led_init_gpio() called
 * @post LED 1 and LED 2 states updated
 * @post s_error_counter reset to 0 when no fault present
 *
 * @note Not thread-safe; called only from internal_led_task_entry()
 * @since Version 1.0.0
 */
static void internal_update_error_and_motor_leds(void)
{
  motor_state_t motor_state;
  (void)shared_data_get_motor_state(&motor_state);

  bool any_fault = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (motor_state.fault_flags[i] != 0) {
      any_fault = true;
    }
  }

  if (any_fault) {
    s_error_counter++;
    if (s_error_counter >= k_led_error_half_period) {
      s_error_counter = 0;
    }
    internal_led_set(
      k_led_idx_error,
      (bool)(s_error_counter < (k_led_error_half_period / k_led_half_period_divisor)));
  } else {
    s_error_counter = 0;
    internal_led_set(k_led_idx_error, false);
  }

  bool any_motor_active = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (motor_state.duty_cycle_percent[i] > 0.0F) {
      any_motor_active = true;
    }
  }
  internal_led_set(k_led_idx_motor, any_motor_active);
}

/**
 * @brief Update LED 3 (comm activity): 100 ms pulse on new motor command
 *
 * @details
 * Detects a new motor command by comparing sequence numbers. On a new
 * command, loads s_comm_pulse_remaining with k_led_comm_pulse_duration
 * (2 ticks = 100 ms) and counts down each call.
 *
 * @pre internal_led_init_gpio() called
 * @post LED 3 state updated
 * @post s_comm_pulse_remaining decremented or loaded
 * @post s_last_comm_sequence updated on new command
 *
 * @note Not thread-safe; called only from internal_led_task_entry()
 * @since Version 1.0.0
 */
static void internal_update_comm_led(void)
{
  motor_command_t cmd;
  (void)shared_data_get_motor_command(&cmd);

  if ((bool)((int)cmd.valid && (cmd.sequence != s_last_comm_sequence))) {
    s_last_comm_sequence   = cmd.sequence;
    s_comm_pulse_remaining = k_led_comm_pulse_duration;
  }

  if (s_comm_pulse_remaining > 0) {
    s_comm_pulse_remaining--;
    internal_led_set(k_led_idx_comm, true);
  } else {
    internal_led_set(k_led_idx_comm, false);
  }
}

/**
 * @brief Update LED 4 (obstacle) and LED 5 (e-stop) from shared state
 *
 * @details
 * - LED 4: solid on when obs_state.any_obstacle is true
 * - LED 5: solid on when shared_data_is_estop_active() returns true
 *
 * @pre internal_led_init_gpio() called
 * @post LED 4 and LED 5 states updated
 *
 * @note Not thread-safe; called only from internal_led_task_entry()
 * @since Version 1.0.0
 */
static void internal_update_obstacle_and_estop_leds(void)
{
  obstacle_state_t obs_state;
  (void)shared_data_get_obstacle(&obs_state);
  internal_led_set(k_led_idx_obstacle, obs_state.any_obstacle);
  internal_led_set(k_led_idx_estop, shared_data_is_estop_active());
}

/**
 * @brief LED status task entry point - infinite loop updating LEDs at 20 Hz
 *
 * @details
 * Reads system state from shared_data each tick and updates 6 LEDs:
 * - LED 0: Heartbeat toggle at 1 Hz (10 ticks on, 10 ticks off)
 * - LED 1: Fast blink when any motor fault active
 * - LED 2: Solid on when any motor has nonzero duty cycle
 * - LED 3: 100ms pulse when new motor command received
 * - LED 4: Solid on when any obstacle detected
 * - LED 5: Solid on when e-stop is active
 *
 * @param[in] input Thread input parameter (unused)
 *
 * @pre led_status_task_create() called successfully
 * @pre ThreadX scheduler started
 * @pre shared_data initialized
 *
 * @post Infinite loop - never returns
 * @post LEDs updated at 20 Hz based on system state
 *
 * @note Thread Safety: Reads shared_data via thread-safe APIs
 *
 * @see internal_update_heartbeat_led() LED 0 update
 * @see internal_update_error_and_motor_leds() LED 1 and 2 update
 * @see internal_update_comm_led() LED 3 update
 * @see internal_update_obstacle_and_estop_leds() LED 4 and 5 update
 *
 * @since Version 1.0.0
 */
static void internal_led_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "LED status task starting");

  /* Initialize LED GPIO pins */
  internal_led_init_gpio();

  rx_log_info(s_tag, "LED status running @ 20 Hz (6 LEDs)");

  /* Main loop */
  while (true) {
    /* BISECT: re-assert D14 at top of loop so it lights if we enter the
     * loop AT ALL, even if one of the update functions blocks.  If the
     * update_* calls run, D14 keeps flipping on; if the task is stuck
     * in a mutex wait, D14 stays off (nothing to re-assert it). */
    (void)led_write_high(k_led_d14_estop);

    internal_update_heartbeat_led();
    internal_update_error_and_motor_leds();
    internal_update_comm_led();
    internal_update_obstacle_and_estop_leds();

    /* BISECT: re-assert D14 again AFTER updates.  If D14 is visibly on,
     * ALL update functions returned and the loop is cycling cleanly. */
    (void)led_write_high(k_led_d14_estop);

    /* BISECT(loop-rate): toggle D11 AFTER updates (so update_error_motor
     * doesn't clobber it).  At 20 Hz task period this produces a 10 Hz
     * flicker (visible as dim-bright to the eye).  If D11 stays fully dark
     * the loop only ran once before hanging.  If bright-ish flicker, loop
     * runs at expected rate. */
    static bool s_d11_toggle = false;
    s_d11_toggle             = !s_d11_toggle;
    if (s_d11_toggle) {
      (void)led_write_high(k_led_d11_motor);
    } else {
      (void)led_write_low(k_led_d11_motor);
    }

    /* BISECT: D10 HIGH right before iwdt_heartbeat, LOW right after.
     * If D10 ends up solid ON, iwdt_heartbeat is hanging.
     * If D10 ends up solid OFF, hang is in tx_thread_sleep. */
    (void)led_write_high(k_led_d10_error);

    /* Report task heartbeat to IWDT (must execute within 150ms timeout) */
    rx_err_t err = rx_iwdt_task_heartbeat("LEDStatus");

    (void)led_write_low(k_led_d10_error);

    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep until next tick */
    (void)tx_thread_sleep(k_led_task_period_ticks);
  }
}
