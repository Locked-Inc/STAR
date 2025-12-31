/* src/main.c */

/**
 * @file main.c
 * @brief STAR RX72N Motor Control Firmware - Main Entry Point
 * @details
 * Complete 4-motor control system with:
 * - 250Hz PID velocity control
 * - Hardware encoder feedback (4 dedicated MTU channels)
 * - Thread-safe shared state
 * - Emergency stop support
 * - Battery management system integration
 *
 * Hardware: Renesas RX72N (R5F572NNHGFP#30)
 * RTOS: ThreadX (Azure RTOS / Eclipse ThreadX)
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "tx_api.h"

/* Motor control system */
#include "hardware_config.h"
#include "motor_shared_state.h"
#include "rx_drv8243.h"
#include "rx_motor.h"
#include "rx_mtu_encoder.h"
#include "rx_pid.h"
#include "system_config.h"
#include "tasks/motor_control_task.h"
#include "tasks/spi_comm_task.h"

/* Infrastructure */
#include "rx_bus_manager.h"
#include "rx_log.h"

static const char* s_tag = "MAIN";

/* =============================================================================
 * Global Motor Control Objects
 * =============================================================================
 */

/* Motor controllers */
static rx_motor_handle_t s_motors[k_motor_count];

/* PID controllers */
static rx_pid_handle_t s_pids[k_motor_count];

/* Encoder channels */
static rx_mtu_channel_t s_encoder_channels[k_motor_count];

/* Shared state */
static motor_shared_state_t s_shared_state;

/* =============================================================================
 * Motor System Initialization
 * =============================================================================
 */

/**
 * @brief Initialize all 4 motors with MTU PWM
 *
 * @return RX_OK on success
 */
static rx_err_t internal_init_motors(void)
{
  rx_err_t err;

  RX_LOG_INFO(s_tag, "Initializing 4 motors...");

  /* Motor 0 configuration */
  rx_motor_config_t motor0_config = {
    .channel      = k_motor_0_mtu_channel,
    .output_a     = k_motor_0_pwm_ph,
    .output_b     = k_motor_0_pwm_en,
    .pwm_freq_hz  = k_motor_pwm_freq_hz,
    .dead_time_ns = k_motor_dead_time_ns,
    .invert_pwm   = false,
  };
  err = rx_motor_init(&s_motors[0], &motor0_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init motor 0");
    return err;
  }

  /* Motor 1 configuration */
  rx_motor_config_t motor1_config = {
    .channel      = k_motor_1_mtu_channel,
    .output_a     = k_motor_1_pwm_ph,
    .output_b     = k_motor_1_pwm_en,
    .pwm_freq_hz  = k_motor_pwm_freq_hz,
    .dead_time_ns = k_motor_dead_time_ns,
    .invert_pwm   = false,
  };
  err = rx_motor_init(&s_motors[1], &motor1_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init motor 1");
    return err;
  }

  /* Motor 2 configuration */
  rx_motor_config_t motor2_config = {
    .channel      = k_motor_2_mtu_channel,
    .output_a     = k_motor_2_pwm_ph,
    .output_b     = k_motor_2_pwm_en,
    .pwm_freq_hz  = k_motor_pwm_freq_hz,
    .dead_time_ns = k_motor_dead_time_ns,
    .invert_pwm   = false,
  };
  err = rx_motor_init(&s_motors[2], &motor2_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init motor 2");
    return err;
  }

  /* Motor 3 configuration */
  rx_motor_config_t motor3_config = {
    .channel      = k_motor_3_mtu_channel,
    .output_a     = k_motor_3_pwm_ph,
    .output_b     = k_motor_3_pwm_en,
    .pwm_freq_hz  = k_motor_pwm_freq_hz,
    .dead_time_ns = k_motor_dead_time_ns,
    .invert_pwm   = false,
  };
  err = rx_motor_init(&s_motors[3], &motor3_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init motor 3");
    return err;
  }

  RX_LOG_INFO(s_tag, "All 4 motors initialized successfully");
  return RX_OK;
}

/**
 * @brief Initialize all 4 encoders with dedicated MTU channels
 *
 * @return RX_OK on success
 */
static rx_err_t internal_init_encoders(void)
{
  rx_err_t err;

  RX_LOG_INFO(s_tag, "Initializing 4 encoders...");

  /* Store encoder channels for motor control task */
  s_encoder_channels[0] = k_motor_0_encoder_ch;
  s_encoder_channels[1] = k_motor_1_encoder_ch;
  s_encoder_channels[2] = k_motor_2_encoder_ch;
  s_encoder_channels[3] = k_motor_3_encoder_ch;

  /* Initialize encoder 0 */
  rx_encoder_config_t enc0_config = {
    .channel          = k_motor_0_encoder_ch,
    .counts_per_rev   = k_encoder_ppr,
    .invert_direction = false,
  };
  err = rx_encoder_init(&enc0_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init encoder 0");
    return err;
  }

  /* Initialize encoder 1 */
  rx_encoder_config_t enc1_config = {
    .channel          = k_motor_1_encoder_ch,
    .counts_per_rev   = k_encoder_ppr,
    .invert_direction = false,
  };
  err = rx_encoder_init(&enc1_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init encoder 1");
    return err;
  }

  /* Initialize encoder 2 */
  rx_encoder_config_t enc2_config = {
    .channel          = k_motor_2_encoder_ch,
    .counts_per_rev   = k_encoder_ppr,
    .invert_direction = false,
  };
  err = rx_encoder_init(&enc2_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init encoder 2");
    return err;
  }

  /* Initialize encoder 3 */
  rx_encoder_config_t enc3_config = {
    .channel          = k_motor_3_encoder_ch,
    .counts_per_rev   = k_encoder_ppr,
    .invert_direction = false,
  };
  err = rx_encoder_init(&enc3_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init encoder 3");
    return err;
  }

  RX_LOG_INFO(s_tag, "All 4 encoders initialized successfully");
  return RX_OK;
}

/**
 * @brief Initialize all 4 PID controllers
 *
 * @return RX_OK on success
 */
static rx_err_t internal_init_pids(void)
{
  rx_err_t err;

  RX_LOG_INFO(s_tag, "Initializing 4 PID controllers...");

  /* Common PID configuration for all motors */
  rx_pid_config_t pid_config = {
    .kp           = k_pid_kp_default / 1000.0f,  /* Convert from x 1000 */
    .ki           = k_pid_ki_default / 1000.0f,  /* Convert from x 1000 */
    .kd           = k_pid_kd_default / 1000.0f,  /* Convert from x 1000 */
    .output_min   = k_pid_output_min / 100.0f,   /* Convert from x 100 */
    .output_max   = k_pid_output_max / 100.0f,   /* Convert from x 100 */
    .integral_min = k_pid_integral_min / 100.0f, /* Convert from x 100 */
    .integral_max = k_pid_integral_max / 100.0f, /* Convert from x 100 */
  };

  /* Initialize all 4 PID controllers with same gains */
  for (int32_t i = 0; i < k_motor_count; i++) {
    err = rx_pid_init(&s_pids[i], &pid_config);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to init PID controller");
      return err;
    }
  }

  RX_LOG_INFO(s_tag, "All 4 PIDs initialized successfully");

  return RX_OK;
}

/* =============================================================================
 * ThreadX Application Definition
 * =============================================================================
 */

/**
 * @brief ThreadX application definition
 *
 * Called by tx_kernel_enter() to create all application objects.
 */
void tx_application_define(void* first_unused_memory)
{
  (void)first_unused_memory;

  RX_LOG_INFO(s_tag, "Creating motor control system...");

  /* Initialize shared state */
  rx_err_t err = motor_shared_state_init(&s_shared_state);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to init shared state");
    return;
  }

  /* Create motor control task (250Hz control loop) */
  err = motor_control_task_create(&s_shared_state, s_motors, s_encoder_channels, s_pids);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to create motor control task");
    return;
  }

  /* Create SPI communication task (100Hz polling for RPi5 commands) */
  err = spi_comm_task_create(&s_shared_state);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to create SPI comm task");
    return;
  }

  RX_LOG_INFO(s_tag, "Motor control system created successfully");
}

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */

/**
 * @brief Application entry point
 *
 * Initializes all hardware and starts the ThreadX kernel.
 * This function never returns.
 */
int main(void)
{
  rx_err_t err;

  /* Initialize RX72N hardware (clocks, 240MHz CPU) */
  err = system_init();
  RX_ERROR_CHECK(err);

  /* Initialize UART first for logging */
  err = uart_init();
  RX_ERROR_CHECK(err);

  /* Print startup banner */
  uart_puts("\r\n");
  uart_puts("========================================\r\n");
  uart_puts("STAR RX72N Motor Control Firmware v1.0\r\n");
  uart_puts("========================================\r\n");
  uart_puts("CPU:     240 MHz RXv3 Core\r\n");
  uart_puts("RTOS:    ThreadX (Azure RTOS)\r\n");
  uart_puts("Control: 250Hz PID Loop (4 motors)\r\n");
  uart_puts("========================================\r\n\r\n");

  RX_LOG_INFO(s_tag, "System clocks initialized (240MHz CPU, 120MHz PCLKA, 60MHz PCLKB)");

  /* Initialize global error handling and pin validation */
  err = rx_infrastructure_init();
  RX_ERROR_CHECK(err);
  RX_LOG_INFO(s_tag, "Infrastructure initialized");

  /* Initialize system tick timer (CMT0 reserved for ThreadX) */
  err = timer_init();
  RX_ERROR_CHECK(err);
  RX_LOG_INFO(s_tag, "System tick initialized");

  /* Initialize motor control hardware */
  RX_LOG_INFO(s_tag, "Initializing motor control system...");

  err = internal_init_motors();
  RX_ERROR_CHECK(err);

  err = internal_init_encoders();
  RX_ERROR_CHECK(err);

  err = internal_init_pids();
  RX_ERROR_CHECK(err);

  RX_LOG_INFO(s_tag, "Motor control hardware initialized");
  RX_LOG_INFO(s_tag, "  Motors:   4 x DRV8243 H-bridge");
  RX_LOG_INFO(s_tag, "  Encoders: 4 x MTU phase counting");
  RX_LOG_INFO(s_tag, "  PIDs:     4 x velocity control");
  RX_LOG_INFO(s_tag, "  Control:  250 Hz (4ms period)");

  /* All systems ready */
  uart_puts("\r\n");
  RX_LOG_INFO(s_tag, "All systems initialized successfully");
  RX_LOG_INFO(s_tag, "Starting ThreadX RTOS...");
  uart_puts("\r\n");

  /* Enter ThreadX kernel - this never returns */
  tx_kernel_enter();

  /* Should never reach here */
  RX_LOG_ERROR(s_tag, "ThreadX kernel exited unexpectedly");
  while (1) {
    __asm__ volatile("wait");
  }

  return 0;
}
