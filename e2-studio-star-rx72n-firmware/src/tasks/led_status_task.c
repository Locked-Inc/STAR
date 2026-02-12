/* src/tasks/led_status_task.c */

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
 * | 0   | P32 | 3    | System Heartbeat       | 1 Hz toggle (500ms on/off)     |
 * | 1   | P87 | 8    | Error Indicator        | Fast blink on any fault        |
 * | 2   | P56 | 5    | Motor Active           | Solid on when motors running   |
 * | 3   | P55 | 5    | Communication Activity | 100ms pulse on command rx      |
 * | 4   | P54 | 5    | Obstacle Detected      | Solid on when obstacle near    |
 * | 5   | P52 | 5    | E-Stop Active          | Solid on during emergency stop |
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
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "led_status_task.h"

#include "hardware_config.h"
#include "rx_check.h"
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
  k_led_heartbeat_half_period = 10, /**< 10 ticks × 50ms = 500ms half-period (1 Hz) */
  k_led_error_half_period     = 3,  /**< 3 ticks × 50ms = 150ms half-period (fast blink) */
  k_led_comm_pulse_duration   = 2,  /**< 2 ticks × 50ms = 100ms comm activity pulse */
} led_timing_constants_t;

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
  UINT tx_status;

  /* Check if already created */
  RX_ASSERT(!s_led_created, "LED task already created");
  if (s_led_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  tx_status = tx_thread_create(&s_led_thread,
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
 * Configures PORT3 pin 2, PORT8 pin 7, and PORT5 pins 2/4/5/6 as
 * GPIO outputs set low (LEDs off). Uses rx_port_get_base for safe
 * register access.
 *
 * @pre PORT register space accessible
 * @post LED pins configured as GPIO outputs, all low
 *
 * @note Thread Safety: Call once during task init only
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
 * @brief Set an LED on or off
 *
 * @param[in] led_index LED index (0-5)
 * @param[in] on true to turn LED on, false to turn off
 *
 * @pre LED GPIO initialized via internal_led_init_gpio()
 * @post LED output state changed
 *
 * @note Thread Safety: Safe from single task context
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
 * @brief LED status task entry point - infinite loop updating LEDs at 20 Hz
 *
 * @details
 * Reads system state from shared_data each tick and updates 6 LEDs:
 * - LED 0: Heartbeat toggle at 1 Hz (10 ticks on, 10 ticks off)
 * - LED 1: Fast blink when any motor fault or BMS fault active
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
    /* ---- LED 0: Heartbeat (1 Hz toggle) ---- */
    s_heartbeat_counter++;
    if (s_heartbeat_counter >= k_led_heartbeat_half_period) {
      s_heartbeat_counter = 0;
    }
    internal_led_set(k_led_idx_heartbeat,
                     (s_heartbeat_counter < (k_led_heartbeat_half_period / 2)));

    /* ---- LED 1: Error indicator (fast blink on fault) ---- */
    {
      motor_state_t motor_state;
      (void)shared_data_get_motor_state(&motor_state);

      bool any_fault = false;
      for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
        if (motor_state.fault_flags[i] != 0) {
          any_fault = true;
        }
      }

      /* Also check BMS faults */
      bms_state_t bms_state;
      (void)shared_data_get_bms(&bms_state);
      if (bms_state.fault_flags != 0) {
        any_fault = true;
      }

      if (any_fault) {
        s_error_counter++;
        if (s_error_counter >= k_led_error_half_period) {
          s_error_counter = 0;
        }
        internal_led_set(k_led_idx_error, (s_error_counter < (k_led_error_half_period / 2)));
      } else {
        s_error_counter = 0;
        internal_led_set(k_led_idx_error, false);
      }

      /* ---- LED 2: Motor active (solid on if any motor running) ---- */
      {
        bool any_motor_active = false;
        for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
          if (motor_state.duty_cycle_percent[i] > 0.0F) {
            any_motor_active = true;
          }
        }
        internal_led_set(k_led_idx_motor, any_motor_active);
      }
    }

    /* ---- LED 3: Communication activity (100ms pulse on new command) ---- */
    {
      motor_command_t cmd;
      (void)shared_data_get_motor_command(&cmd);

      if (cmd.valid && (cmd.sequence != s_last_comm_sequence)) {
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

    /* ---- LED 4: Obstacle detected (solid on) ---- */
    {
      obstacle_state_t obs_state;
      (void)shared_data_get_obstacle(&obs_state);
      internal_led_set(k_led_idx_obstacle, obs_state.any_obstacle);
    }

    /* ---- LED 5: E-stop active (solid on) ---- */
    internal_led_set(k_led_idx_estop, shared_data_is_estop_active());

    /* Sleep until next tick */
    (void)tx_thread_sleep(k_led_task_period_ticks);
  }
}
