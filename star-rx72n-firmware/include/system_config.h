/* include/system_config.h */

/**
 * @file system_config.h
 * @brief System-level configuration constants for RX72N STAR firmware
 * @details
 * Defines system-wide configuration parameters including PID gains, control loop
 * parameters, safety limits, and runtime behavior settings. These values are tuned
 * for the STAR robot platform and should be modified carefully.
 *
 * Configuration Categories:
 * - Motor control (PID gains, limits)
 * - Safety and fault handling
 * - Communication timeouts
 * - Task priorities and stack sizes
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Motor Control Configuration
 * =============================================================================
 */

/**
 * @brief PID controller gains for velocity control
 *
 * These values were tuned using MATLAB system identification and
 * Ziegler-Nichols method for the 6V gearmotor with 75ms time constant.
 * See: matlab/pid_design_velocity.m
 */
typedef enum {
  k_pid_kp_default = 286,  /**< Proportional gain × 1000 (0.286 actual) */
  k_pid_ki_default = 8010, /**< Integral gain × 1000 (8.01 actual) */
  k_pid_kd_default = 0,    /**< Derivative gain × 1000 (0.0 actual) */
} pid_gains_config_t;

/**
 * @brief PID output limits (duty cycle percentage)
 */
typedef enum {
  k_pid_output_min   = -10000, /**< -100.0% duty cycle × 100 */
  k_pid_output_max   = 10000,  /**< +100.0% duty cycle × 100 */
  k_pid_integral_min = -5000,  /**< -50.0 integral limit × 100 */
  k_pid_integral_max = 5000,   /**< +50.0 integral limit × 100 */
} pid_limits_config_t;

/**
 * @brief Motor velocity limits (m/s)
 */
typedef enum {
  k_motor_max_velocity_mps = 2500, /**< 2.5 m/s × 1000 (absolute max) */
  k_motor_min_velocity_mps = 10,   /**< 0.01 m/s × 1000 (deadband threshold) */
} motor_velocity_config_t;

/**
 * @brief Motor current limits (mA)
 */
typedef enum {
  k_motor_current_limit_ma     = 2000, /**< 2.0A software current limit */
  k_motor_current_warning_ma   = 1800, /**< 1.8A warning threshold */
  k_motor_overcurrent_delay_ms = 100,  /**< Delay before triggering overcurrent fault */
} motor_current_config_t;

/* =============================================================================
 * Control Loop Configuration
 * =============================================================================
 */

/**
 * @brief Control loop timing parameters
 */
typedef enum {
  k_control_loop_freq_hz   = 250,  /**< 250 Hz control loop */
  k_control_loop_period_ms = 4,    /**< 4ms period */
  k_control_loop_period_us = 4000, /**< 4000μs period */
} control_loop_config_t;

/**
 * @brief Control loop performance targets
 */
typedef enum {
  k_control_loop_max_execution_us = 500, /**< Max execution time (12.5% of period) */
  k_control_loop_max_jitter_us    = 100, /**< Max timing jitter */
} control_loop_performance_t;

/* =============================================================================
 * Safety and Fault Handling
 * =============================================================================
 */

/**
 * @brief Emergency stop configuration
 */
typedef enum {
  k_estop_debounce_ms       = 50,   /**< E-stop button debounce time */
  k_estop_brake_delay_ms    = 0,    /**< Delay before braking (0 = immediate) */
  k_estop_recovery_delay_ms = 1000, /**< Delay before allowing recovery */
} estop_config_t;

/**
 * @brief Fault detection thresholds
 */
typedef enum {
  k_fault_encoder_timeout_ms  = 100,   /**< Encoder read timeout */
  k_fault_motor_stall_time_ms = 500,   /**< Time at high current + low speed = stall */
  k_fault_temperature_max_c   = 85,    /**< Maximum motor/driver temperature */
  k_fault_voltage_min_mv      = 10000, /**< Minimum battery voltage (10V) */
  k_fault_voltage_max_mv      = 30000, /**< Maximum battery voltage (30V) */
} fault_detection_config_t;

/**
 * @brief Fault recovery behavior
 */
typedef enum {
  k_fault_auto_recovery_enabled = 1,    /**< Enable automatic fault recovery */
  k_fault_recovery_retry_count  = 3,    /**< Max retry attempts */
  k_fault_recovery_delay_ms     = 1000, /**< Delay between retries */
} fault_recovery_config_t;

/* =============================================================================
 * Communication Configuration
 * =============================================================================
 */

/**
 * @brief SPI communication parameters (Raspberry Pi)
 */
typedef enum {
  k_spi_clock_hz        = 10000000, /**< 10 MHz SPI clock */
  k_spi_max_packet_size = 256,      /**< Maximum packet size (bytes) */
  k_spi_timeout_ms      = 100,      /**< SPI transaction timeout */
  k_spi_retry_count     = 3,        /**< Number of retries on error */
} spi_comm_config_t;

/**
 * @brief UART debug console parameters
 */
typedef enum {
  k_uart_baud_rate      = 115200, /**< 115200 baud */
  k_uart_tx_buffer_size = 512,    /**< TX buffer size (bytes) */
  k_uart_rx_buffer_size = 256,    /**< RX buffer size (bytes) */
} uart_debug_config_t;

/**
 * @brief I2C/SMBUS parameters (BMS)
 */
typedef enum {
  k_i2c_clock_hz     = 100000, /**< 100 kHz I2C clock */
  k_i2c_timeout_ms   = 50,     /**< I2C transaction timeout */
  k_smbus_timeout_ms = 100,    /**< SMBUS transaction timeout */
} i2c_comm_config_t;

/* =============================================================================
 * ThreadX Task Configuration
 * =============================================================================
 */

/**
 * @brief Task priorities (higher number = higher priority)
 *
 * Priority Guidelines:
 * - 0-3: Low priority (telemetry, logging)
 * - 4-7: Medium priority (communication)
 * - 8-11: High priority (motor control, safety)
 * - 12-15: Critical (emergency stop, fault handling)
 */
typedef enum {
  k_task_priority_motor_control   = 10, /**< Motor control task */
  k_task_priority_command_handler = 8,  /**< Command handler task */
  k_task_priority_telemetry       = 5,  /**< Telemetry reporting task */
  k_task_priority_bms             = 7,  /**< Battery management task */
  k_task_priority_fault_monitor   = 12, /**< Fault monitoring task */
} task_priority_config_t;

/**
 * @brief Task stack sizes (bytes)
 */
typedef enum {
  k_task_stack_motor_control   = 4096, /**< Motor control task */
  k_task_stack_command_handler = 4096, /**< Command handler task */
  k_task_stack_telemetry       = 2048, /**< Telemetry task */
  k_task_stack_bms             = 2048, /**< BMS task */
  k_task_stack_fault_monitor   = 2048, /**< Fault monitor task */
} task_stack_config_t;

/**
 * @brief ThreadX configuration
 */
typedef enum {
  k_threadx_tick_rate_hz    = 100,   /**< 100 Hz tick rate (10ms per tick) */
  k_threadx_total_heap_size = 65536, /**< 64KB heap for ThreadX */
} threadx_config_t;

/* =============================================================================
 * Telemetry and Logging
 * =============================================================================
 */

/**
 * @brief Telemetry reporting rates (Hz)
 */
typedef enum {
  k_telemetry_motor_status_hz   = 10, /**< Motor status @ 10 Hz */
  k_telemetry_battery_status_hz = 1,  /**< Battery status @ 1 Hz */
  k_telemetry_system_stats_hz   = 1,  /**< System stats @ 1 Hz */
} telemetry_rate_config_t;

/**
 * @brief Logging configuration
 */
typedef enum {
  k_log_level_default = 3,    /**< 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG */
  k_log_buffer_size   = 1024, /**< Log buffer size (bytes) */
  k_log_uart_enabled  = 1,    /**< Enable UART logging */
} logging_config_t;

/* =============================================================================
 * Battery Management
 * =============================================================================
 */

/**
 * @brief Battery monitoring thresholds
 */
typedef enum {
  k_battery_voltage_nominal_mv  = 14800, /**< 14.8V nominal (4S LiPo) */
  k_battery_voltage_full_mv     = 16800, /**< 16.8V full charge */
  k_battery_voltage_low_mv      = 13200, /**< 13.2V low battery warning */
  k_battery_voltage_critical_mv = 12000, /**< 12.0V critical (shutdown) */
} battery_voltage_config_t;

/**
 * @brief Battery capacity and SOC
 */
typedef enum {
  k_battery_capacity_mah         = 5000, /**< 5000 mAh nominal capacity */
  k_battery_soc_low_percent      = 20,   /**< 20% SOC low warning */
  k_battery_soc_critical_percent = 10,   /**< 10% SOC critical warning */
} battery_capacity_config_t;

/**
 * @brief Charging configuration
 */
typedef enum {
  k_charge_current_max_ma    = 2000,  /**< 2A max charge current */
  k_charge_voltage_max_mv    = 16800, /**< 16.8V max charge voltage (4.2V × 4) */
  k_charge_temperature_max_c = 45,    /**< 45°C max charge temperature */
} charging_config_t;

/* =============================================================================
 * Performance Monitoring
 * =============================================================================
 */

/**
 * @brief Watchdog timer configuration
 */
typedef enum {
  k_watchdog_timeout_ms = 1000, /**< 1 second watchdog timeout */
  k_watchdog_enabled    = 1,    /**< Enable watchdog */
} watchdog_config_t;

/**
 * @brief Stack overflow detection
 */
typedef enum {
  k_stack_check_enabled   = 1,   /**< Enable stack checking */
  k_stack_watermark_bytes = 256, /**< Minimum free stack required */
} stack_monitor_config_t;

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONFIG_H */
