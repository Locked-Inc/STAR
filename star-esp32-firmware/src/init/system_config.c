/* src/init/system_config.c - System-wide pin and constant definitions */

#include "system_config.h"

/* ========================================================================= */
/*                    LOGGING TAG                                            */
/* ========================================================================= */

const char* const s_TAG = "STAR_MAIN";

/* ========================================================================= */
/*                    MOTOR CONTROL PINS (4 MOTORS)                          */
/* ========================================================================= */

/* Motor 1 */
const gpio_num_t s_motor1_pwm_en_pin = GPIO_NUM_6;  /* Enable/Speed PWM */
const gpio_num_t s_motor1_pwm_ph_pin = GPIO_NUM_7;  /* Phase/Direction */
const adc_channel_t s_motor1_ipropi_adc = ADC_CHANNEL_0; /* GPIO1 current sense */

/* Motor 2 */
const gpio_num_t s_motor2_pwm_en_pin = GPIO_NUM_14; /* Enable/Speed PWM */
const gpio_num_t s_motor2_pwm_ph_pin = GPIO_NUM_15; /* Phase/Direction */
const adc_channel_t s_motor2_ipropi_adc = ADC_CHANNEL_1; /* GPIO2 current sense */

/* Motor 3 */
const gpio_num_t s_motor3_pwm_en_pin = GPIO_NUM_16; /* Enable/Speed PWM */
const gpio_num_t s_motor3_pwm_ph_pin = GPIO_NUM_17; /* Phase/Direction */
const adc_channel_t s_motor3_ipropi_adc = ADC_CHANNEL_3; /* GPIO4 current sense */

/* Motor 4 */
const gpio_num_t s_motor4_pwm_en_pin = GPIO_NUM_18; /* Enable/Speed PWM */
const gpio_num_t s_motor4_pwm_ph_pin = GPIO_NUM_21; /* Phase/Direction */
const adc_channel_t s_motor4_ipropi_adc = ADC_CHANNEL_4; /* GPIO5 current sense */

/* ========================================================================= */
/*                    ENCODER MULTIPLEXER CONTROL                            */
/* ========================================================================= */

const gpio_num_t s_encoder_sel0_pin = GPIO_NUM_40; /* Address bit 0 (controls U3 & U4) */
const gpio_num_t s_encoder_sel1_pin = GPIO_NUM_41; /* Address bit 1 (controls U3 & U4) */
const gpio_num_t s_encoder_out0_pin = GPIO_NUM_42; /* Selected encoder A signal */
const gpio_num_t s_encoder_out1_pin = GPIO_NUM_47; /* Selected encoder B signal */

/* ========================================================================= */
/*                    RPI5 COMMUNICATION (SPI3)                              */
/* ========================================================================= */

const gpio_num_t s_rpi_spi_cs_pin   = GPIO_NUM_10; /* Chip Select */
const gpio_num_t s_rpi_spi_copi_pin = GPIO_NUM_11; /* COPI (Controller Out, Peripheral In) */
const gpio_num_t s_rpi_spi_sclk_pin = GPIO_NUM_12; /* CLK */
const gpio_num_t s_rpi_spi_cipo_pin = GPIO_NUM_13; /* CIPO (Controller In, Peripheral Out) */

/* ========================================================================= */
/*                    I2C SENSORS                                            */
/* ========================================================================= */

const gpio_num_t s_i2c_sda_pin      = GPIO_NUM_8;  /* Fuel gauge SDA */
const gpio_num_t s_i2c_scl_pin      = GPIO_NUM_9;  /* Fuel gauge SCL */
const gpio_num_t s_temp_sensor_pin  = GPIO_NUM_48; /* DS18B20 temperature */

/* ========================================================================= */
/*                    MOTOR CONTROL TIMING                                   */
/* ========================================================================= */

const float s_motor_control_dt_sec = 0.004f;       /* 4ms = 250Hz per motor */
const uint32_t s_motor_control_period_ms = 4;
const uint32_t s_encoder_mux_settle_us = 1;        /* 1us settling time for 74HC157 */

/* ========================================================================= */
/*                    ENCODER CONFIGURATION                                  */
/* ========================================================================= */

const uint32_t s_encoder_edges_per_rev = 2000;    /* 500 PPR encoder * 4 edges */
const int32_t s_encoder_high_limit = 10000;
const int32_t s_encoder_low_limit = -10000;
const uint32_t s_encoder_filter_cycles = 1000;    /* Glitch filter APB cycles */

/* ========================================================================= */
/*                    ADC CONFIGURATION                                      */
/* ========================================================================= */

const float s_adc_max_value = 4095.0f;             /* 12-bit ADC */
const float s_adc_vref_volts = 3.3f;               /* ESP32 reference voltage */

/* ========================================================================= */
/*                    DRV8243 CURRENT SENSING                                */
/* ========================================================================= */

const float s_drv8243_ipropi_ratio = 525.0f;       /* DRV8243 IPROPI current ratio */
const float s_drv8243_ipropi_resistor_kohm = 1.5f; /* IPROPI resistor value */
const float s_motor_overcurrent_ma = 2500.0f;      /* Overcurrent threshold */

/* ========================================================================= */
/*                    MCPWM CONFIGURATION                                    */
/* ========================================================================= */

const uint32_t s_mcpwm_timer_resolution_hz = 10000000; /* 10MHz timer */
const uint32_t s_mcpwm_pwm_freq_hz = 20000;            /* 20kHz PWM frequency */
const uint32_t s_mcpwm_dead_time_ns = 1000;            /* 1us dead-time */

/* ========================================================================= */
/*                    PID CONTROLLER DEFAULTS                                */
/* ========================================================================= */

const float s_pid_kp_default = 1.0f;
const float s_pid_ki_default = 0.5f;
const float s_pid_kd_default = 0.1f;
const float s_pid_output_min = -100.0f;
const float s_pid_output_max = 100.0f;
const float s_pid_integral_min = -50.0f;
const float s_pid_integral_max = 50.0f;

/* ========================================================================= */
/*                    FREERTOS TASK CONFIGURATION                            */
/* ========================================================================= */

const uint32_t s_motor_task_stack_size = 8192;     /* 8KB stack */
const uint32_t s_telemetry_task_stack_size = 4096; /* 4KB stack */
const uint32_t s_comm_task_stack_size = 4096;      /* 4KB stack */
const uint8_t s_motor_task_priority = 10;          /* Highest priority */
const uint8_t s_telemetry_task_priority = 5;       /* Medium priority */
const uint8_t s_comm_task_priority = 7;            /* High priority */

/* ========================================================================= */
/*                    TELEMETRY AND COMMUNICATION TIMING                     */
/* ========================================================================= */

const uint32_t s_telemetry_period_ms = 100;        /* 10Hz telemetry */
const uint32_t s_comm_period_ms = 10;              /* 100Hz communication */
const uint32_t s_heartbeat_period_ms = 1000;       /* 1Hz heartbeat */
const uint32_t s_state_mutex_timeout_ms = 100;     /* Mutex timeout */

/* ========================================================================= */
/*                    ERROR HANDLER CONFIGURATION                            */
/* ========================================================================= */

const uint8_t s_error_max_retries = 3;
const uint32_t s_error_initial_delay_ms = 100;
const uint32_t s_error_max_delay_ms = 5000;

/* ========================================================================= */
/*                    UNIT CONVERSION                                        */
/* ========================================================================= */

const float s_seconds_per_minute = 60.0f;
