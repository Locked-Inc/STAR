/* include/system_config.h - System-wide pin and constant definitions */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "driver/adc.h"
#include "driver/gpio.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*                    MOTOR CONTROL PINS (4 MOTORS)                          */
/* ========================================================================= */

/* Motor 1 */
extern const gpio_num_t s_motor1_pwm_en_pin;
extern const gpio_num_t s_motor1_pwm_ph_pin;
extern const adc_channel_t s_motor1_ipropi_adc;

/* Motor 2 */
extern const gpio_num_t s_motor2_pwm_en_pin;
extern const gpio_num_t s_motor2_pwm_ph_pin;
extern const adc_channel_t s_motor2_ipropi_adc;

/* Motor 3 */
extern const gpio_num_t s_motor3_pwm_en_pin;
extern const gpio_num_t s_motor3_pwm_ph_pin;
extern const adc_channel_t s_motor3_ipropi_adc;

/* Motor 4 */
extern const gpio_num_t s_motor4_pwm_en_pin;
extern const gpio_num_t s_motor4_pwm_ph_pin;
extern const adc_channel_t s_motor4_ipropi_adc;

/* ========================================================================= */
/*                    ENCODER MULTIPLEXER CONTROL                            */
/* ========================================================================= */

extern const gpio_num_t s_encoder_sel0_pin;
extern const gpio_num_t s_encoder_sel1_pin;
extern const gpio_num_t s_encoder_out0_pin;
extern const gpio_num_t s_encoder_out1_pin;

/* ========================================================================= */
/*                    RPI5 COMMUNICATION (SPI3)                              */
/* ========================================================================= */

extern const gpio_num_t s_rpi_spi_cs_pin;
extern const gpio_num_t s_rpi_spi_copi_pin;
extern const gpio_num_t s_rpi_spi_sclk_pin;
extern const gpio_num_t s_rpi_spi_cipo_pin;

/* ========================================================================= */
/*                    I2C SENSORS                                            */
/* ========================================================================= */

extern const gpio_num_t s_i2c_sda_pin;
extern const gpio_num_t s_i2c_scl_pin;
extern const gpio_num_t s_temp_sensor_pin;

/* ========================================================================= */
/*                    SYSTEM CONFIGURATION CONSTANTS                         */
/* ========================================================================= */

/* Array Sizes (must be macros for C array declarations) */
#define NUM_MOTORS   (4)
#define NUM_MUX_PINS (2)

/* Motor Control Timing */
extern const float s_motor_control_dt_sec;
extern const uint32_t s_motor_control_period_ms;
extern const uint32_t s_encoder_mux_settle_us;

/* Encoder Configuration */
extern const uint32_t s_encoder_edges_per_rev;
extern const int32_t s_encoder_high_limit;
extern const int32_t s_encoder_low_limit;
extern const uint32_t s_encoder_filter_cycles;

/* ADC Configuration */
extern const float s_adc_max_value;
extern const float s_adc_vref_volts;

/* DRV8243 Current Sensing */
extern const float s_drv8243_ipropi_ratio;
extern const float s_drv8243_ipropi_resistor_kohm;
extern const float s_motor_overcurrent_ma;

/* MCPWM Configuration */
extern const uint32_t s_mcpwm_timer_resolution_hz;
extern const uint32_t s_mcpwm_pwm_freq_hz;
extern const uint32_t s_mcpwm_dead_time_ns;

/* PID Controller Defaults */
extern const float s_pid_kp_default;
extern const float s_pid_ki_default;
extern const float s_pid_kd_default;
extern const float s_pid_output_min;
extern const float s_pid_output_max;
extern const float s_pid_integral_min;
extern const float s_pid_integral_max;

/* FreeRTOS Task Configuration */
extern const uint32_t s_motor_task_stack_size;
extern const uint32_t s_telemetry_task_stack_size;
extern const uint32_t s_comm_task_stack_size;
extern const uint8_t s_motor_task_priority;
extern const uint8_t s_telemetry_task_priority;
extern const uint8_t s_comm_task_priority;

/* Telemetry and Communication Timing */
extern const uint32_t s_telemetry_period_ms;
extern const uint32_t s_comm_period_ms;
extern const uint32_t s_heartbeat_period_ms;
extern const uint32_t s_state_mutex_timeout_ms;

/* Error Handler Configuration */
extern const uint8_t s_error_max_retries;
extern const uint32_t s_error_initial_delay_ms;
extern const uint32_t s_error_max_delay_ms;

/* Unit Conversion */
extern const float s_seconds_per_minute;

/* Logging Tag */
extern const char* const s_TAG;

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONFIG_H */
