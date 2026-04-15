/**
 * @file hardware_config.h
 * @brief BeagleBone Blue hardware configuration constants.
 *
 * @details
 * Defines board-level constants for motor channels, encoder channels, servo
 * channels, IMU configuration, and USB serial device path. All values match
 * the BeagleBone Blue / OSD3358 hardware as documented in the librobotcontrol
 * user guide.
 *
 * @see https://beagleboard.org/blue
 * @see librobotcontrol API documentation
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * USB CDC serial device
 * ---------------------------------------------------------------------------*/

/**
 * @def BB_USB_CDC_DEVICE
 * @brief Default tty path for USB CDC connection to RPi5 gateway.
 *
 * @details
 * The BeagleBone Blue enumerates as a USB gadget device when connected to the
 * RPi5 gateway via USB-A to USB-A cable. The host (RPi5) sees it as ttyACM0.
 * This define specifies the device node on the BBB side.
 *
 * @since Version 1.0.0
 */
#define BB_USB_CDC_DEVICE "/dev/ttyGS0"

/* ---------------------------------------------------------------------------
 * Motor channels (librobotcontrol 1-indexed, matches BBB silkscreen)
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_motor_channel_t
 * @brief DC motor channel assignments on the BeagleBone Blue.
 *
 * @details
 * The BBB provides 4 bidirectional DC motor channels via the DRV8838 drivers.
 * librobotcontrol uses 1-based indexing (channels 1-4).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_motor_all         = 0U, /**< Channel 0: all motors (librobotcontrol convention) */
    k_bb_motor_front_left  = 1U, /**< Motor 1: front-left drive */
    k_bb_motor_front_right = 2U, /**< Motor 2: front-right drive */
    k_bb_motor_rear_left   = 3U, /**< Motor 3: rear-left drive */
    k_bb_motor_rear_right  = 4U, /**< Motor 4: rear-right drive */
    k_bb_motor_count       = 4U, /**< Total number of motor channels */
} bb_motor_channel_t;

/* ---------------------------------------------------------------------------
 * Encoder channels (librobotcontrol 1-indexed)
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_encoder_channel_t
 * @brief Quadrature encoder channel assignments on the BeagleBone Blue.
 *
 * @details
 * The BBB provides 4 quadrature encoder inputs via the eQEP hardware.
 * librobotcontrol uses 1-based indexing (channels 1-4).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_encoder_front_left  = 1U, /**< Encoder 1: front-left motor */
    k_bb_encoder_front_right = 2U, /**< Encoder 2: front-right motor */
    k_bb_encoder_rear_left   = 3U, /**< Encoder 3: rear-left motor */
    k_bb_encoder_rear_right  = 4U, /**< Encoder 4: rear-right motor */
    k_bb_encoder_count       = 4U, /**< Total number of encoder channels */
} bb_encoder_channel_t;

/* ---------------------------------------------------------------------------
 * Task loop rates
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_task_rate_hz_t
 * @brief Loop rates for each task thread.
 *
 * @details
 * All rates in Hz. Tasks use clock_nanosleep() for period control.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_bb_rate_comm_hz      = 100U, /**< comm_task loop rate (100 Hz) */
    k_bb_rate_motor_hz     = 100U, /**< motor_control_task loop rate (100 Hz) */
    k_bb_rate_imu_hz       = 100U, /**< imu_task loop rate (100 Hz) */
    k_bb_rate_telemetry_hz = 10U,  /**< telemetry_task loop rate (10 Hz) */
    k_bb_rate_watchdog_hz  = 10U,  /**< watchdog_task loop rate (10 Hz) */
} bb_task_rate_hz_t;

/* ---------------------------------------------------------------------------
 * Watchdog timeout
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_watchdog_t
 * @brief Watchdog timeout configuration.
 *
 * @details
 * If no valid frame is received from the gateway within k_bb_watchdog_ms,
 * watchdog_task commands all motors to stop.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_bb_watchdog_ms = 500U, /**< 500 ms comm watchdog timeout */
} bb_watchdog_t;

/* ---------------------------------------------------------------------------
 * Time unit constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_time_units_t
 * @brief Named constants for time unit conversions.
 *
 * @details
 * Used by all task loops and timestamp calculations to avoid magic numbers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint64_t {
    k_bb_ns_per_sec = 1000000000ULL, /**< Nanoseconds per second */
    k_bb_us_per_sec = 1000000ULL,    /**< Microseconds per second */
    k_bb_ns_per_us  = 1000ULL,       /**< Nanoseconds per microsecond */
} bb_time_units_t;

/**
 * @enum bb_task_period_ns_t
 * @brief Task loop periods in nanoseconds, derived from rate constants.
 *
 * @since Version 1.0.0
 */
typedef enum : uint64_t {
    k_bb_comm_period_ns      = k_bb_ns_per_sec / k_bb_rate_comm_hz,
    k_bb_motor_period_ns     = k_bb_ns_per_sec / k_bb_rate_motor_hz,
    k_bb_imu_period_ns       = k_bb_ns_per_sec / k_bb_rate_imu_hz,
    k_bb_telemetry_period_ns = k_bb_ns_per_sec / k_bb_rate_telemetry_hz,
    k_bb_watchdog_period_ns  = k_bb_ns_per_sec / k_bb_rate_watchdog_hz,
} bb_task_period_ns_t;

/**
 * @enum bb_motor_idx_t
 * @brief Zero-based motor indices for shared_data array indexing.
 *
 * @details
 * Maps to motor/encoder channel assignments. Use with duty_percent[]
 * and ticks[] arrays in bb_motor_cmd_t and bb_encoder_data_t.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_motor_idx_fl = 0U, /**< Front left array index */
    k_bb_motor_idx_fr = 1U, /**< Front right array index */
    k_bb_motor_idx_rl = 2U, /**< Rear left array index */
    k_bb_motor_idx_rr = 3U, /**< Rear right array index */
} bb_motor_idx_t;

/**
 * @enum bb_imu_axis_t
 * @brief IMU axis indices for accel/gyro/mag arrays.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_axis_x = 0U, /**< X axis index */
    k_bb_axis_y = 1U, /**< Y axis index */
    k_bb_axis_z = 2U, /**< Z axis index */
} bb_imu_axis_t;

/**
 * @enum bb_duty_scale_t
 * @brief Duty cycle scaling constants.
 *
 * @since Version 1.0.0
 */
static const float s_bb_duty_percent_scale = 100.0F;

/* ---------------------------------------------------------------------------
 * Motor voltage protection
 * ---------------------------------------------------------------------------*/

/**
 * @brief Motor rated voltage (DFRobot FIT0520: 6V brushed DC gearmotor).
 *
 * The DRV8838 H-bridge passes V_battery directly to motors. With a 2S LiPo
 * (up to 8.4V), 100% duty would deliver 40% overvoltage to 6V motors.
 * The motor_control_task clamps duty to: 0.95 * (6.0 / V_batt).
 *
 * @since Version 1.1.0
 */
static const float s_bb_motor_rated_v = 6.0F;

/**
 * @brief Duty cycle derating factor (5% margin for ADC tolerance).
 * @since Version 1.1.0
 */
static const float s_bb_duty_derating = 0.95F;

/**
 * @brief Fallback max duty when battery voltage is unknown (USB power).
 *
 * Assumes worst-case 8.8V (freshly charged 2S with surface charge).
 * 0.95 * 6.0 / 8.8 = 0.648, rounded down to 0.65.
 *
 * @since Version 1.1.0
 */
static const float s_bb_duty_fallback_max = 0.65F;

/**
 * @brief ADC battery voltage deadzone threshold (volts).
 *
 * librobotcontrol's rc_adc_batt() returns 0.0 when the raw reading is
 * below 1.0V (disconnected battery or USB-only power). We use the same
 * threshold to decide whether battery monitoring is available.
 *
 * @since Version 1.1.0
 */
static const float s_bb_batt_deadzone_v = 1.0F;

/* ---------------------------------------------------------------------------
 * Wheel geometry (used for tick -> velocity conversion in telemetry_task)
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_wheel_geometry_t
 * @brief Wheel geometry constants for encoder velocity computation.
 *
 * @details
 * Encoder ticks per output-shaft revolution for the goBILDA + DFRobot
 * gearmotor stack used by the BBB target. Used by motor_control_task to
 * convert tick deltas into output-shaft angular velocity (rad/s) for
 * the velocity PID, and by telemetry_task to convert ticks into wheel
 * surface velocity (m/s) for upstream odometry.
 *
 * 341 PPR Hall encoder * 34.02:1 gearbox = 11599 ticks per output rev.
 *
 * @invariant k_bb_ticks_per_rev MUST match the `ticks_per_rev` ROS2
 *            parameter declared in
 *            star-ros2/.../star_gateway_bridge_node.cpp and
 *            star-ros2/.../star_spi_driver_node.cpp. If the values
 *            disagree, the gateway's pose integration and the BBB's
 *            twist computation will diverge.
 *
 * @code
 * const float rad_per_tick = (2.0F * (float)M_PI)
 *                          / (float)k_bb_ticks_per_rev;
 * @endcode
 *
 * @see s_bb_wheel_radius_m The wheel radius companion constant.
 * @see star_gateway_bridge_node.cpp The ROS2 source of truth.
 *
 * @since Version 1.2.0
 */
typedef enum : uint32_t {
    k_bb_ticks_per_rev = 11599U, /**< Quadrature ticks per gearbox output revolution */
} bb_wheel_geometry_t;

/**
 * @brief Wheel rolling radius in meters.
 *
 * @details
 * goBILDA 3616-0014-0144 Wasteland 144 mm wheel, diameter / 2 = 0.072 m.
 * Used by motor_control_task to convert m/s setpoints into output-shaft
 * rad/s setpoints for the velocity PID, and by telemetry_task to
 * convert tick-derived rad/s into wheel surface m/s for upstream
 * odometry.
 *
 * Must match the `wheel_radius` ROS2 parameter declared in the gateway
 * and SPI bridge nodes; see the @invariant note on @ref bb_wheel_geometry_t.
 *
 * @note Float, not enum, because C enums cannot hold non-integral values.
 *
 * @since Version 1.2.0
 */
static const float s_bb_wheel_radius_m = 0.072F;

#ifdef __cplusplus
}
#endif
