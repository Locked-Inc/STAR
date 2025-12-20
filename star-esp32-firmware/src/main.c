/**
 * @file main.c
 * @brief Application entry point and system initialization for STAR ESP32 firmware
 * @details
 * Main entry point for ESP32 firmware. Initializes the bus manager, motor control system,
 * temperature sensor, ARQ communication layer, and FreeRTOS tasks for robot control.
 *
 * Architecture:
 * - Bus Manager: Unified I2C/SPI/1-Wire abstraction layer
 * - Motor Control: 4x brushed DC motors with DRV8243 H-bridges
 * - Encoders: 4x quadrature encoders (multiplexed via 74HC4052)
 * - PID Control: 4x independent PID controllers running at 250Hz
 * - ARQ Layer: Reliable SPI peripheral communication with RPi5 controller
 * - Motor Control Task: 250Hz control loop for motor PID control
 * - Command Handler Task: Processes velocity commands from RPi5
 * - Telemetry Task: Handles temperature and status reporting
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "main.h"

#include <string.h>

#include "esp_log.h"
#include "motor_shared_state.h"
#include "star_arq.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_encoder_mux.h"
#include "star_motor.h"
#include "star_pid.h"
#include "star_sensor_ds18b20.h"
#include "tasks/command_handler_task.h"
#include "tasks/motor_control_task.h"
#include "tasks/telemetry_task.h"

static const char* s_tag = "MAIN";

void app_main(void)
{
  ESP_LOGI(s_tag, "STAR Motor Control System Starting...");

  /* 1. Initialize bus manager */
  star_bus_manager_t bus_manager;
  esp_err_t          ret = star_bus_manager_init(&bus_manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Bus manager init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 2. Create OneWire bus for DS18B20 */
  star_bus_config_t* onewire =
    star_bus_config_create_onewire("temp_onewire",
                                   CONFIG_STAR_DS18B20_DATA_GPIO,
                                   false /* External power (not parasitic) */);
  star_bus_manager_add_bus(&bus_manager, onewire);

  /* 3. Create SPI3 peripheral for RPi5 communication */
  star_bus_config_t* spi = star_bus_config_create_spi_peripheral(
    "rpi_spi",
    SPI3_HOST,
    CONFIG_STAR_SPI_COPI_GPIO, /* Controller Out, Peripheral In */
    CONFIG_STAR_SPI_CIPO_GPIO, /* Controller In, Peripheral Out */
    CONFIG_STAR_SPI_CLK_GPIO,  /* Serial Clock */
    CONFIG_STAR_SPI_CS_GPIO,   /* Chip Select */
    k_spi_queue_size,
    0 /* SPI mode 0 */);
  star_bus_manager_add_bus(&bus_manager, spi);

  /* 4. Initialize DS18B20 temperature sensor */
  static star_ds18b20_handle_t temp_sensor;
  star_ds18b20_config_t        temp_cfg = {
           .bus_manager = &bus_manager,
           .bus_name    = "temp_onewire",
           .resolution = k_star_ds18b20_resolution_12_bit, /* 0.0625°C precision, 750ms conversion */
           .use_rom = false,                               /* Skip ROM addressing (single sensor) */
  };
  ret = star_sensor_ds18b20_init(&temp_sensor, &temp_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "DS18B20 init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 6. Initialize 4 motors (MCPWM-based control) */
  static star_motor_handle_t motors[4];
  star_motor_config_t        motor_cfgs[4] = {
    /* Motor 0 (Front-Left): EN=IO6, PH=IO7 */
    {.group_id            = 0,
            .timer_resolution_hz = 10000000,   /* 10MHz timer */
            .pwm_freq_hz         = 20000,      /* 20kHz PWM */
            .pin_pwm_a           = GPIO_NUM_6, /* EN (IN1) */
            .pin_pwm_b           = GPIO_NUM_7, /* PH (IN2) */
            .dead_time_ns        = 1000,       /* 1μs dead-time */
            .fault_pin           = -1},
    /* Motor 1 (Rear-Left): EN=IO14, PH=IO15 */
    {.group_id            = 0,
            .timer_resolution_hz = 10000000,
            .pwm_freq_hz         = 20000,
            .pin_pwm_a           = GPIO_NUM_14,
            .pin_pwm_b           = GPIO_NUM_15,
            .dead_time_ns        = 1000,
            .fault_pin           = -1},
    /* Motor 2 (Front-Right): EN=IO16, PH=IO17 */
    {.group_id            = 1,
            .timer_resolution_hz = 10000000,
            .pwm_freq_hz         = 20000,
            .pin_pwm_a           = GPIO_NUM_16,
            .pin_pwm_b           = GPIO_NUM_17,
            .dead_time_ns        = 1000,
            .fault_pin           = -1},
    /* Motor 3 (Rear-Right): EN=IO18, PH=IO21 */
    {.group_id            = 1,
            .timer_resolution_hz = 10000000,
            .pwm_freq_hz         = 20000,
            .pin_pwm_a           = GPIO_NUM_18,
            .pin_pwm_b           = GPIO_NUM_21,
            .dead_time_ns        = 1000,
            .fault_pin           = -1},
  };

  for (int i = 0; i < 4; i++) {
    ret = star_motor_init(&motors[i], &motor_cfgs[i]);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Motor %d init failed: %s", i, esp_err_to_name(ret));
      return;
    }
  }
  ESP_LOGI(s_tag, "4 motors initialized");

  /* 7. Initialize encoder multiplexer (74HC4052) */
  static star_encoder_mux_handle_t encoder_mux;
  star_encoder_mux_config_t        enc_mux_cfg = {
           .pin_sel0         = GPIO_NUM_40, /* Multiplexer select line 0 */
           .pin_sel1         = GPIO_NUM_41, /* Multiplexer select line 1 */
           .pin_out_a        = GPIO_NUM_47, /* Shared channel A output */
           .pin_out_b        = GPIO_NUM_42, /* Shared channel B output */
           .settling_time_us = 10,          /* 10μs settling time */
           .filter_value     = 1000,        /* Glitch filter */
           .high_limit       = 10000,
           .low_limit        = -10000,
  };
  ret = star_encoder_mux_init(&encoder_mux, &enc_mux_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Encoder mux init failed: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Encoder multiplexer initialized");

  /* 8. Initialize 4 PID controllers (tuned gains from MATLAB) */
  static star_pid_handle_t pids[4];
  star_pid_config_t        pid_cfg = {
           .kp           = 0.286f,  /* Proportional gain (from MATLAB tuning) */
           .ki           = 8.01f,   /* Integral gain (from MATLAB tuning) */
           .kd           = 0.0f,    /* Derivative gain (disable for now) */
           .output_min   = -100.0f, /* Min duty cycle (%) */
           .output_max   = 100.0f,  /* Max duty cycle (%) */
           .integral_min = -50.0f,  /* Anti-windup limits */
           .integral_max = 50.0f,
  };

  for (int i = 0; i < 4; i++) {
    ret = star_pid_init(&pids[i], &pid_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "PID %d init failed: %s", i, esp_err_to_name(ret));
      return;
    }
  }
  ESP_LOGI(s_tag, "4 PID controllers initialized (Kp=%.3f, Ki=%.3f)", pid_cfg.kp, pid_cfg.ki);

  /* 9. Initialize motor shared state */
  static motor_shared_state_t shared_state;
  ret = motor_shared_state_init(&shared_state);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Shared state init failed: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Motor shared state initialized");

  /* 10. Initialize ARQ layer */
  static star_arq_handle_t arq;
  star_arq_config_t        arq_cfg = {
           .bus_manager      = &bus_manager,
           .spi_bus_name     = "rpi_spi",
           .max_retries      = k_arq_max_retries,
           .retry_timeout_ms = k_arq_retry_timeout_ms,
  };
  ret = star_arq_init(&arq, &arq_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "ARQ init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 11. Create FreeRTOS tasks */
  motor_control_task_create(&shared_state, motors, &encoder_mux, pids);
  command_handler_task_create(&shared_state, &arq);
  telemetry_task_create(&temp_sensor, &arq);

  ESP_LOGI(s_tag, "Initialization complete. Motor control system running at 250Hz.");

  /* app_main returns, freeing main task stack */
}
