/* include/motor_config.h */

/**
 * @file motor_config.h
 * @brief Motor system configuration constants
 * @details
 * Defines all motor/encoder/PID constants as enums following NASA Power of 10
 * Rule 8 (no magic numbers). All numeric constants must be named.
 *
 * Motor System:
 * - 4x 6V brushed DC gearmotors (210 RPM @ 6V)
 * - 341 PPR Hall effect encoders (quadrature)
 * - DRV8243S H-bridge motor drivers
 * - MATLAB-tuned PID gains (Kp=0.286, Ki=8.01, Kd=0.0)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H

/* =============================================================================
 * Motor Physical Constants
 * =============================================================================
 */

/**
 * @brief Motor physical parameters
 * @details Mechanical properties of the motor system
 */
typedef enum {
  k_motor_count            = 4,    /**< Number of active motors */
  k_motor_rated_voltage_mv = 6000, /**< Rated voltage (mV) */
  k_motor_rated_rpm        = 210,  /**< Rated RPM at 6V */
  k_motor_stall_current_ma = 3200, /**< Stall current (mA) */
  k_motor_gear_ratio_x100  = 3402, /**< Gear ratio (34.02:1 × 100) */
  k_wheel_radius_mm        = 72,   /**< Wheel radius (mm) = 144mm diameter / 2 */
  k_wheelbase_mm           = 200,  /**< Wheelbase (mm) */
} motor_physical_t;

/**
 * @brief Motor array indices
 * @details Named constants for motor array indexing (NASA Power of 10 - no magic numbers)
 *
 * Motor Layout (top view):
 *   Front
 *  FL   FR
 *  BL   BR
 *   Back
 */
typedef enum {
  k_motor_front_left  = 0, /**< Front left motor index */
  k_motor_front_right = 1, /**< Front right motor index */
  k_motor_back_left   = 2, /**< Back left motor index */
  k_motor_back_right  = 3, /**< Back right motor index */
} motor_idx_t;

/**
 * @brief Encoder configuration
 * @details Quadrature encoder parameters
 */
typedef enum {
  k_encoder_ppr   = 341,                             /**< Pulses per revolution (Hall effect) */
  k_encoder_edges = 4,                               /**< Edges per pulse (quadrature) */
  k_encoder_cpr   = k_encoder_ppr * k_encoder_edges, /**< Counts per rev (1364) */
} encoder_config_t;

/* =============================================================================
 * PID Controller Configuration
 * =============================================================================
 */

/**
 * @brief PID gains (MATLAB-tuned)
 * @details Velocity control PID gains from MATLAB system identification
 *
 * Motor Model: G(s) = 3.665 / (0.075s + 1)
 * Tuning Method: MATLAB pidTuner with phase margin = 60°
 *
 * Gains are scaled by 1000 to avoid floating-point in enums.
 * Divide by 1000 when using: Kp_actual = k_pid_kp_x1000 / 1000.0f
 */
typedef enum {
  k_pid_kp_x1000          = 286,  /**< Proportional gain (0.286 scaled by 1000) */
  k_pid_ki_x1000          = 8010, /**< Integral gain (8.01 scaled by 1000) */
  k_pid_kd_x1000          = 0,    /**< Derivative gain (0.0) */
  k_pid_gain_scale_factor = 1000, /**< PID gain scaling factor (gains stored as x1000) */
} pid_gains_t;

/**
 * @brief PID output limits
 * @details Limits are in percent duty cycle (0-100)
 */
typedef enum {
  k_pid_output_min_percent = 0,   /**< Minimum duty cycle (0%) */
  k_pid_output_max_percent = 100, /**< Maximum duty cycle (100%) */
} pid_output_limits_t;

/**
 * @brief PID integral limits (anti-windup)
 * @details Prevents integral windup during saturation
 */
typedef enum {
  k_pid_integral_min_x100     = -5000, /**< Min integral (-50.0 scaled by 100) */
  k_pid_integral_max_x100     = 5000,  /**< Max integral (50.0 scaled by 100) */
  k_pid_integral_scale_factor = 100,   /**< Integral limit scaling (limits stored as x100) */
} pid_integral_limits_t;

/* =============================================================================
 * Motor Control Loop Timing
 * =============================================================================
 */

/**
 * @brief Control loop timing
 * @details Deterministic timing for Motor_Controller thread
 */
typedef enum {
  k_control_loop_hz        = 250,  /**< Control loop frequency (Hz) */
  k_control_loop_period_ms = 4,    /**< Control loop period (ms) = 1000/250 */
  k_control_loop_period_us = 4000, /**< Control loop period (us) */
  k_threadx_tick_hz        = 100,  /**< ThreadX tick rate (Hz) */
  k_control_loop_ticks     = 0,    /**< ThreadX ticks per loop (4ms < 10ms) */
} control_loop_timing_t;

/**
 * @brief Watchdog timing
 * @details IWDT configuration for control loop monitoring
 */
typedef enum {
  k_iwdt_timeout_ms = 1000, /**< Watchdog timeout (ms) */
  k_iwdt_feed_hz    = 250,  /**< Feed rate (same as control loop) */
} watchdog_timing_t;

/* =============================================================================
 * Motor Driver Configuration (DRV8243S)
 * =============================================================================
 */

/**
 * @brief DRV8243S current limits
 * @details Current sensing and protection thresholds
 */
typedef enum {
  k_drv8243_max_current_ma      = 3000,  /**< Maximum continuous current (mA) */
  k_drv8243_overcurrent_ma      = 3500,  /**< Overcurrent threshold (mA) */
  k_drv8243_ipropi_scaling_x100 = 52500, /**< IPROPI scaling (525 A/V × 100) */
} drv8243_config_t;

/**
 * @brief PWM configuration for motor drivers
 * @details GPTW PWM generation parameters
 */
typedef enum {
  k_pwm_frequency_hz  = 20000, /**< PWM frequency (20 kHz) */
  k_pwm_resolution_us = 50,    /**< PWM period (50 us = 1/20kHz) */
  k_pwm_deadtime_ns   = 1000,  /**< PWM deadtime (1000 ns = 1 μs) */
} pwm_config_t;

/* =============================================================================
 * Velocity Limits
 * =============================================================================
 */

/**
 * @brief Motor velocity limits
 * @details Safety limits for velocity commands (scaled by 100 for enums)
 */
typedef enum {
  k_motor_max_velocity_x100 = 200,  /**< Max velocity (2.0 m/s × 100) */
  k_motor_min_velocity_x100 = -200, /**< Min velocity (-2.0 m/s × 100) */
} velocity_limits_t;

/* =============================================================================
 * Obstacle Detection Configuration
 * =============================================================================
 */

/**
 * @brief HC-SR04 ultrasonic sensor configuration
 * @details Obstacle detection thresholds
 */
typedef enum {
  k_hcsr04_count            = 4,  /**< Number of HC-SR04 sensors */
  k_obstacle_threshold_cm   = 30, /**< Obstacle detection threshold (cm) */
  k_obstacle_scan_hz        = 50, /**< Scan rate (Hz) */
  k_obstacle_scan_period_ms = 20, /**< Scan period (ms) = 1000/50 */
} obstacle_config_t;

/* =============================================================================
 * Temperature Monitoring Configuration
 * =============================================================================
 */

/**
 * @brief DS18B20 temperature sensor configuration
 * @details Thermal protection thresholds
 */
typedef enum {
  k_temp_warning_threshold_c  = 70, /**< Thermal warning (°C) */
  k_temp_shutdown_threshold_c = 85, /**< Thermal shutdown (°C) */
  k_temp_sample_hz            = 50, /**< Sample rate (Hz) */
} temperature_config_t;

/* =============================================================================
 * Communication Configuration
 * =============================================================================
 */

/**
 * @brief Communication timing
 * @details RPi5 communication via USB CDC / SPI
 */
typedef enum {
  k_comm_rate_hz         = 100, /**< Communication rate (Hz) */
  k_comm_period_ms       = 10,  /**< Communication period (ms) = 1000/100 */
  k_comm_timeout_ms      = 500, /**< Timeout before E-STOP (ms) */
  k_comm_max_frame_bytes = 256, /**< Maximum frame size (bytes) */
} comm_config_t;

/* =============================================================================
 * Peripheral Hardware Configuration
 * =============================================================================
 */

/**
 * @brief UART configuration
 * @details Debug UART (SCI9) settings
 */
typedef enum {
  k_uart_baudrate = 115200, /**< Debug UART baud rate */
} uart_config_t;

/**
 * @brief I2C configuration
 * @details RIIC settings for battery management (BQ4050)
 */
typedef enum {
  k_i2c_frequency_hz = 400000, /**< I2C clock frequency (400 kHz) */
  k_riic_channel     = 0,      /**< RIIC channel for BQ4050 */
} i2c_config_t;

/**
 * @brief SPI configuration
 * @details RSPI settings for motor drivers and RPi5 communication
 */
typedef enum {
  k_rspi_channel   = 0, /**< RSPI channel for motor drivers */
  k_rspi_mode      = 0, /**< SPI mode 0 (CPOL=0, CPHA=0) */
  k_rspi_use_16bit = 0, /**< Use 8-bit frames (false) */
} spi_config_t;

/**
 * @brief ADC configuration
 * @details S12ADFa settings for current sensing
 */
typedef enum {
  k_adc_unit       = 0,  /**< ADC unit 0 */
  k_adc_resolution = 12, /**< 12-bit ADC resolution */
} adc_config_t;

/* =============================================================================
 * System Health Monitoring Configuration
 * =============================================================================
 */

/**
 * @brief Battery monitoring configuration (BQ4050)
 * @details Battery health thresholds
 */
typedef enum {
  k_battery_low_soc_percent  = 20, /**< Low battery threshold (%) */
  k_battery_critical_percent = 10, /**< Critical battery threshold (%) */
  k_battery_sample_hz        = 1,  /**< Sample rate (Hz) */
} battery_config_t;

/* =============================================================================
 * Thread Priorities
 * =============================================================================
 */

/**
 * @brief ThreadX thread priorities
 * @details Lower number = higher priority
 *
 * Priority Rationale:
 * - Motor_Controller (2): Highest - deterministic control loop
 * - Comm_Manager (3): High - RPi5 is "High-Level Commander"
 * - Env_Monitor (5): Medium - Isolated from motor timing
 * - System_Health (9): Low - Battery changes slowly
 * - PMOD tasks (10-31): User-defined priorities
 */
typedef enum {
  k_priority_motor_controller = 2,  /**< Highest (deterministic) */
  k_priority_comm_manager     = 3,  /**< High (RPi5 commands) */
  k_priority_env_monitor      = 5,  /**< Medium (sensors) */
  k_priority_system_health    = 9,  /**< Low (diagnostics) */
  k_priority_pmod_start       = 10, /**< PMOD priority range start */
  k_priority_pmod_end         = 31, /**< PMOD priority range end */
} thread_priorities_t;

/* =============================================================================
 * Thread Stack Sizes
 * =============================================================================
 */

/**
 * @brief ThreadX thread stack sizes (bytes)
 * @details Statically allocated stacks (no dynamic memory)
 */
typedef enum {
  k_stack_motor_controller = 4096, /**< Motor control + PID */
  k_stack_comm_manager     = 4096, /**< USB/SPI + Protocol Buffers */
  k_stack_env_monitor      = 3072, /**< Sensors (HC-SR04, DS18B20) */
  k_stack_system_health    = 2048, /**< Battery monitoring + diagnostics */
} thread_stack_sizes_t;

/* =============================================================================
 * ThreadX Tick Conversion Constants
 * =============================================================================
 */

/**
 * @brief ThreadX tick conversion constants
 * @details Convert time periods to ThreadX ticks (100 Hz = 10ms per tick)
 *
 * Usage: tx_thread_sleep(k_threadx_ticks_20ms);
 *
 * NASA Power of 10 Rule 8: No magic numbers - all tick values must use
 * these named constants for clarity and maintainability.
 *
 * Note: For infinite timeout use TX_WAIT_FOREVER macro directly
 */
typedef enum {
  k_threadx_ticks_10ms  = 1,   /**< 10ms = 1 tick at 100Hz */
  k_threadx_ticks_20ms  = 2,   /**< 20ms = 2 ticks (Env_Monitor period) */
  k_threadx_ticks_80ms  = 8,   /**< 80ms = 8 ticks (DS18B20 conversion) */
  k_threadx_ticks_100ms = 10,  /**< 100ms = 10 ticks (Comm_Manager period) */
  k_threadx_ticks_1s    = 100, /**< 1 second = 100 ticks (System_Health period) */
} threadx_tick_constants_t;

#endif /* MOTOR_CONFIG_H */
