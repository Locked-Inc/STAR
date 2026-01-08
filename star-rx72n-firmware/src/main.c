/* src/main.c */

/**
 * @file main.c
 * @brief Main entry point for STAR RX72N Motor Controller Firmware
 * @details
 * Initializes hardware and ThreadX RTOS, then starts the scheduler.
 * This is the application entry point that ties together all subsystems.
 *
 * System Flow:
 * 1. Hardware initialization (clocks, peripherals, watchdog)
 * 2. ThreadX kernel initialization
 * 3. Shared state initialization (mutexes)
 * 4. Task creation (motor control, communication, sensors, health)
 * 5. Start ThreadX scheduler (never returns)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tx_api.h"
#include "hardware.h"
#include "hardware_init.h"
#include "shared_state.h"
#include "diagnostics.h"
#include "rx_log.h"
#include "rx_iwdt.h"
#include "tasks/motor_controller.h"
#include "tasks/comm_manager.h"
#include "tasks/env_monitor.h"
#include "tasks/system_health.h"

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

/**
 * @brief ThreadX application definition function
 *
 * Called by ThreadX kernel after initialization.
 * Creates all application tasks and starts the system.
 *
 * @param first_unused_memory Pointer to first unused memory (for dynamic allocation)
 *
 * @note We don't use dynamic allocation (safety-critical), so this parameter is ignored
 */
void tx_application_define(void* first_unused_memory);

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static const char* s_s_tag_tx = "main";

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */

/**
 * @brief Main entry point
 *
 * Initializes hardware and enters ThreadX kernel.
 * Never returns after entering ThreadX.
 *
 * @return Should never return (ThreadX scheduler takes over)
 */
int main(void)
{
  rx_err_t ret = k_rx_ok;

  /* Initialize all hardware peripherals */
  ret = hardware_init_all();
  if (ret != k_rx_ok) {
    /* Hardware init failed - halt system
     * Enter low-power wait state (cannot continue without hardware) */
    rx_log_error(s_s_tag_tx, "Hardware initialization failed");
    while (1) {
      __asm__ volatile("wait");
    }
  }

  /* Check if watchdog caused last reset */
  if (rx_iwdt_was_reset()) {
    rx_log_error(s_s_tag_tx, "System recovered from watchdog reset");
  }

  /* Enter ThreadX kernel (never returns) */
  tx_kernel_enter();

  /* Should never reach here - ThreadX scheduler failed to start */
  while (1) {
    __asm__ volatile("wait");
  }

  return 0;
}

/* =============================================================================
 * ThreadX Application Definition
 * =============================================================================
 */

void tx_application_define(void* first_unused_memory)
{
  UINT             status = TX_SUCCESS;
  shared_state_t*  state  = NULL;
  static const char* s_tag_tx = "tx_app_def";

  /* Suppress unused parameter warning */
  (void)first_unused_memory;

  rx_log_info(s_tag_tx, "ThreadX application definition starting");

  /* -------------------------------------------------------------------------
   * Initialize Diagnostics
   * -------------------------------------------------------------------------
   */
  diagnostics_init();
  rx_log_info(s_tag_tx, "Diagnostics initialized");

  /* -------------------------------------------------------------------------
   * Initialize Shared State
   * -------------------------------------------------------------------------
   * Create all mutexes and zero-initialize state structures
   */
  state = shared_state_get();
  status = shared_state_init(state);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag_tx, "Shared state init failed");
    return;
  }
  rx_log_info(s_tag_tx, "Shared state initialized");

  /* -------------------------------------------------------------------------
   * Create Application Tasks
   * -------------------------------------------------------------------------
   * Priority hierarchy (lower number = higher priority):
   * - Motor_Controller (2): Deterministic 250 Hz control loop
   * - Comm_Manager (3): High priority for RPi5 commands
   * - Env_Monitor (5): Medium priority for sensors
   * - System_Health (9): Low priority for diagnostics
   */

  /* Create Motor_Controller thread (priority 2, 250 Hz) */
  status = motor_controller_create();
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag_tx, "Motor_Controller creation failed");
    return;
  }

  /* Create Comm_Manager thread (priority 3, 100 Hz) */
  status = comm_manager_create();
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag_tx, "Comm_Manager creation failed");
    return;
  }

  /* Create Env_Monitor thread (priority 5, 50 Hz) */
  status = env_monitor_create();
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag_tx, "Env_Monitor creation failed");
    return;
  }

  /* Create System_Health thread (priority 9, 1 Hz) */
  status = system_health_create();
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag_tx, "System_Health creation failed");
    return;
  }

  rx_log_info(s_tag_tx, "ThreadX application definition complete");
}
