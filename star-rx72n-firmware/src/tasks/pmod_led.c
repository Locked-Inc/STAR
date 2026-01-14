/* src/tasks/pmod_led.c */

/**
 * @file pmod_led.c
 * @brief PMOD LED Status Display Task Implementation
 * @details
 * Displays system status on Pmod8LD (8 LEDs, GPIO output).
 * Updates at 10 Hz to show real-time system health.
 *
 * LED Assignments:
 * - LED 0-3: Motor fault indicators (one per motor)
 * - LED 4: Emergency stop active
 * - LED 5: Communication timeout
 * - LED 6: Obstacle detected
 * - LED 7: Battery low warning
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/pmod_led.h"

#include <string.h>

#include "system_config.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "shared_state.h"

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* Module name for logging */
static const char* s_tag       = "pmod_led";
static const char* s_task_name = "pmod_led";

/* ThreadX thread control block and stack */
static TX_THREAD s_pmod_led_thread;
static uint8_t   s_pmod_led_stack[2048];

/* Thread priority and timing */
typedef enum {
  k_priority_pmod_led       = 25, /**< Low priority (status display) */
  k_update_rate_hz          = 10, /**< 10 Hz = 100ms period */
  k_watchdog_timeout_ms     = 300, /**< 3x period = 300ms */
  k_led_count               = 8,  /**< Total number of LEDs */
  k_motor_led_count         = 4,  /**< LEDs 0-3 for motor faults */
  k_led_idx_emergency_stop  = 4,  /**< LED 4: Emergency stop */
  k_led_idx_comm_timeout    = 5,  /**< LED 5: Communication timeout */
  k_led_idx_obstacle        = 6,  /**< LED 6: Obstacle detected */
  k_led_idx_battery_low     = 7,  /**< LED 7: Battery low warning */
} pmod_led_config_t;

/* =============================================================================
 * Hardware Interface
 * =============================================================================
 */

/**
 * @brief Initialize GPIO pins for 8 LEDs
 *
 * Note: GPIO pin configuration will be implemented when hardware_pinout.h
 * is available. For now, this is a placeholder that logs initialization.
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t init_led_gpio(void)
{
  /* Note: Actual GPIO initialization will be added when hardware pins are defined
   * Example implementation:
   *   gpio_set_output(k_port_b, 0);  // LED 0
   *   gpio_set_output(k_port_b, 1);  // LED 1
   *   ... (repeat for all 8 LEDs)
   */

  rx_log_info(s_tag, "LED GPIO initialized (8 LEDs)");
  return k_rx_ok;
}

/**
 * @brief Update LED state based on system status
 *
 * Reads shared_state and sets LED outputs accordingly:
 * - LEDs 0-3: Individual motor fault indicators
 * - LED 4: Emergency stop active
 * - LED 5: Communication timeout
 * - LED 6: Obstacle detected
 * - LED 7: Battery low warning
 */
static void update_led_display(void)
{
  shared_state_t* state = shared_state_get();

  /* Read safety state (mutex-protected) */
  UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire safety mutex");
    return;
  }

  const bool emergency_stop    = state->safety.emergency_stop;
  const bool obstacle_detected = state->safety.obstacle_detected;
  const bool comm_timeout      = state->safety.comm_timeout;
  const fault_flags_t faults   = state->safety.fault_flags;

  tx_mutex_put(&state->safety_mutex);

  /* Read health state (mutex-protected) */
  status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire health mutex");
    return;
  }

  const bool battery_low = state->health.battery_low;

  tx_mutex_put(&state->health_mutex);

  /* Update LEDs 0-3: Motor fault indicators
   * Note: Actual GPIO writes will be added when hardware pins are defined
   * Example:
   *   for (uint8_t i = 0; i < k_motor_led_count; i++) {
   *     if (faults & (1 << i)) {
   *       gpio_write_high(k_port_b, i);
   *     } else {
   *       gpio_write_low(k_port_b, i);
   *     }
   *   }
   */
  (void)faults; /* Suppress unused variable warning until GPIO is implemented */

  /* Update LED 4: Emergency stop
   * Example: gpio_write(k_port_b, k_led_idx_emergency_stop, emergency_stop); */
  (void)emergency_stop;

  /* Update LED 5: Communication timeout
   * Example: gpio_write(k_port_b, k_led_idx_comm_timeout, comm_timeout); */
  (void)comm_timeout;

  /* Update LED 6: Obstacle detected
   * Example: gpio_write(k_port_b, k_led_idx_obstacle, obstacle_detected); */
  (void)obstacle_detected;

  /* Update LED 7: Battery low warning
   * Example: gpio_write(k_port_b, k_led_idx_battery_low, battery_low); */
  (void)battery_low;
}

/* =============================================================================
 * Thread Entry Point
 * =============================================================================
 */

/**
 * @brief PMOD LED thread entry point
 *
 * Main loop for LED status display:
 * 1. Register with watchdog (3x period timeout = 300ms)
 * 2. Initialize GPIO pins for 8 LEDs
 * 3. Main update loop (10 Hz):
 *    - Read system status from shared_state
 *    - Update LED outputs to reflect status
 *    - Send heartbeat to watchdog
 *    - Sleep until next update
 *
 * @param[in] input Thread input parameter (unused)
 */
static void pmod_led_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "PMOD LED thread started");

  /* Register with watchdog for task-level monitoring
   * Timeout = 3x period = 300ms */
  rx_err_t ret = rx_iwdt_register_task(s_task_name, k_watchdog_timeout_ms);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register with watchdog");
  }

  /* Initialize GPIO pins for LED output */
  ret = init_led_gpio();
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "LED GPIO initialization failed");

    /* Cannot continue - halt this thread */
    while (1) {
      tx_thread_sleep(k_threadx_ticks_1s);
    }
  }

  rx_log_info(s_tag, "Entering main LED update loop (10 Hz)");

  /* Main update loop (10 Hz = 100ms period) */
  while (1) {
    /* Update LED display based on system status */
    update_led_display();

    /* Record task heartbeat for deadlock detection */
    rx_iwdt_task_heartbeat(s_task_name);

    /* Sleep until next update
     * ThreadX tick = 10ms (100 Hz)
     * 10 Hz → 100ms period → 10 ticks */
    const uint32_t period_ms   = 1000 / k_update_rate_hz;
    const uint32_t sleep_ticks = period_ms / 10;
    tx_thread_sleep(sleep_ticks);
  }
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

UINT pmod_led_create(void)
{
  /* Create PMOD LED thread */
  UINT status = tx_thread_create(&s_pmod_led_thread,
                                 s_tag,
                                 pmod_led_entry,
                                 0,
                                 s_pmod_led_stack,
                                 sizeof(s_pmod_led_stack),
                                 k_priority_pmod_led,
                                 k_priority_pmod_led,
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);

  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Thread creation failed");
    return status;
  }

  rx_log_info(s_tag, "PMOD_LED thread created (priority 25, 10 Hz)");
  return TX_SUCCESS;
}
