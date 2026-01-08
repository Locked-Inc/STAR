/* src/tasks/motor_controller.c */

/**
 * @file motor_controller.c
 * @brief Motor Controller thread implementation
 * @details
 * Deterministic 250 Hz control loop for 4-motor velocity control.
 * Highest priority thread to prevent jitter from sensor delays.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/motor_controller.h"
#include "hardware_pinout.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_bus_manager.h"
#include "rx_drv8243.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_mtu_encoder.h"
#include "rx_pid.h"
#include <string.h>

/* =============================================================================
 * Private Types
 * =============================================================================
 */

/**
 * @brief Motor subsystem state
 * @details Holds handles for all motor control components
 */
typedef struct {
    rx_bus_manager_t    bus_manager;                    /**< Bus manager for GPIO/ADC access */
    rx_drv8243_handle_t motor_drivers[k_motor_count];   /**< DRV8243S motor driver handles */
    rx_pid_handle_t     pid_controllers[k_motor_count]; /**< PID controller handles */
    rx_encoder_state_t  encoder_states[k_motor_count];  /**< Encoder state tracking */
    bool                initialized;                    /**< True if subsystem initialized */
} motor_subsystem_t;

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static const char* s_tag = "motor_ctrl";

/* Thread control block and stack (statically allocated) */
static TX_THREAD s_motor_controller_thread;
static uint8_t   s_motor_controller_stack[k_stack_motor_controller];

/* Motor subsystem state */
static motor_subsystem_t s_motor_subsystem;

/* =============================================================================
 * Private Function Prototypes
 * =============================================================================
 */

/**
 * @brief Motor Controller thread entry point
 * @param input Thread input parameter (unused)
 */
static void motor_controller_entry(ULONG input);

/**
 * @brief Initialize motor subsystem (motors, encoders, PIDs)
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t motor_subsystem_init(void);

/**
 * @brief Execute one control loop iteration
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t control_loop_iteration(void);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT motor_controller_create(void)
{
    UINT status = TX_SUCCESS;

    /* Zero-initialize motor subsystem */
    memset(&s_motor_subsystem, 0, sizeof(motor_subsystem_t));

    /* Create Motor_Controller thread */
    status = tx_thread_create(
        &s_motor_controller_thread,
        s_tag,
        motor_controller_entry,
        0,
        s_motor_controller_stack,
        k_stack_motor_controller,
        k_priority_motor_controller,
        k_priority_motor_controller,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Motor_Controller thread created (priority 2, 250 Hz)");
    return TX_SUCCESS;
}

/* =============================================================================
 * Private Function Implementations
 * =============================================================================
 */

static void motor_controller_entry(ULONG input)
{
    rx_err_t ret = k_rx_ok;

    (void)input; /* Suppress unused parameter warning */

    rx_log_info(s_tag, "Motor_Controller thread started");

    /* Initialize motor subsystem */
    ret = motor_subsystem_init();
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Motor subsystem init failed");
        /* Cannot continue - halt this thread */
        while (1) {
            tx_thread_sleep(100); /* Sleep forever */
        }
    }

    rx_log_info(s_tag, "Entering 250 Hz control loop");

    /* Main control loop (250 Hz = 4ms period) */
    while (1) {
        /* Execute control loop iteration */
        ret = control_loop_iteration();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Control loop error");
        }

        /* Feed watchdog (critical - prevents reset) */
        rx_iwdt_feed();

        /* Sleep for 4ms (250 Hz control rate)
         * Note: k_control_loop_ticks = 0 because 4ms < 10ms ThreadX tick
         *
         * Phase 1: Using tx_thread_sleep(1) = 10ms for stub implementation
         * Phase 2: Will use dedicated timer interrupt for precise 4ms timing
         *          when actual motor control hardware is integrated */
        tx_thread_sleep(1); /* 10ms tick - Phase 2 will implement precise 4ms timer */
    }
}

static rx_err_t motor_subsystem_init(void)
{
    /* -------------------------------------------------------------------------
     * Initialize Bus Manager
     * -------------------------------------------------------------------------
     * Create bus manager with no error/pin interfaces (use defaults)
     */
    rx_err_t ret = rx_bus_manager_init(&s_motor_subsystem.bus_manager, s_tag, NULL, NULL);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Bus manager init failed");
        return ret;
    }
    rx_log_info(s_tag, "Bus manager initialized");

    /* -------------------------------------------------------------------------
     * Initialize PID Controllers (4 motors)
     * -------------------------------------------------------------------------
     * MATLAB-tuned gains: Kp=0.286, Ki=8.01, Kd=0.0
     * Output limits: ±100% duty cycle
     * Integral limits: ±50% (anti-windup)
     */
    const float kp      = (float)k_pid_kp_x1000 / 1000.0f;
    const float ki      = (float)k_pid_ki_x1000 / 1000.0f;
    const float kd      = (float)k_pid_kd_x1000 / 1000.0f;
    const float out_min = -(float)k_pid_output_max_percent;
    const float out_max = (float)k_pid_output_max_percent;
    const float int_min = (float)k_pid_integral_min_x100 / 100.0f;
    const float int_max = (float)k_pid_integral_max_x100 / 100.0f;

    for (uint8_t i = 0; i < k_motor_count; i++) {
        rx_pid_config_t pid_config = {
            .kp           = kp,
            .ki           = ki,
            .kd           = kd,
            .output_min   = out_min,
            .output_max   = out_max,
            .integral_min = int_min,
            .integral_max = int_max,
        };

        ret = rx_pid_init(&s_motor_subsystem.pid_controllers[i], &pid_config);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "PID init failed");
            return ret;
        }
    }
    rx_log_info(s_tag, "PID controllers initialized (Kp=0.286, Ki=8.01)");

    /* -------------------------------------------------------------------------
     * Initialize DRV8243S Motor Drivers (4 motors)
     * -------------------------------------------------------------------------
     * Motor driver configuration:
     * - GPTW channels 0-3 for PWM generation (20 kHz)
     * - Port E pins for Phase/Enable control
     * - Port 4 ADC pins for current sensing (IPROPI)
     * - Port 4 GPIO pins for fault detection (nFAULT)
     */
    const gpio_pin_t motor_pins_ph[k_motor_count] = {
        k_pin_motor0_ph,
        k_pin_motor1_ph,
        k_pin_motor2_ph,
        k_pin_motor3_ph,
    };

    const gpio_pin_t motor_pins_en[k_motor_count] = {
        k_pin_motor0_en,
        k_pin_motor1_en,
        k_pin_motor2_en,
        k_pin_motor3_en,
    };

    const uint8_t adc_pins_ipropi[k_motor_count] = {
        k_adc_motor0_current,
        k_adc_motor1_current,
        k_adc_motor2_current,
        k_adc_motor3_current,
    };

    const gpio_pin_t fault_pins[k_motor_count] = {
        k_pin_motor0_nfault,
        k_pin_motor1_nfault,
        k_pin_motor2_nfault,
        k_pin_motor3_nfault,
    };

    for (uint8_t i = 0; i < k_motor_count; i++) {
        rx_drv8243_config_t motor_config = {
            .bus_manager     = &s_motor_subsystem.bus_manager,
            .gpio_bus_name   = "motor_gpio",
            .adc_bus_name    = "motor_adc",
            .gptw_channel    = (rx_gptw_channel_t)i,
            .output_ph       = k_gptw_output_a,
            .output_en       = k_gptw_output_b,
            .pin_ipropi      = adc_pins_ipropi[i],
            .port_nfault     = (uint8_t)((fault_pins[i] >> 8) & 0xFF),
            .pin_nfault      = (uint8_t)(fault_pins[i] & 0xFF),
            .pwm_freq_hz     = k_pwm_frequency_hz,
            .current_limit_ma = k_drv8243_max_current_ma,
            .ki_propi        = (uint16_t)(k_drv8243_ipropi_scaling_x100 / 100),
        };

        ret = rx_drv8243_init(&s_motor_subsystem.motor_drivers[i], &motor_config);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Motor driver init failed");
            return ret;
        }
    }
    rx_log_info(s_tag, "Motor drivers initialized (4x DRV8243S, 20 kHz PWM)");

    /* -------------------------------------------------------------------------
     * Initialize Encoder State Tracking
     * -------------------------------------------------------------------------
     * MTU encoders already initialized in hardware_init.c
     * Just zero-initialize encoder state structures
     */
    for (uint8_t i = 0; i < k_motor_count; i++) {
        memset(&s_motor_subsystem.encoder_states[i], 0, sizeof(rx_encoder_state_t));
    }
    rx_log_info(s_tag, "Encoder state tracking initialized");

    s_motor_subsystem.initialized = true;
    rx_log_info(s_tag, "Motor subsystem initialization complete");

    return k_rx_ok;
}

static rx_err_t control_loop_iteration(void)
{
    /* Stub implementation - Issues 7-9:
     * - Control loop with encoder feedback and PID (Issue 7)
     * - Current sensing and fault detection (Issue 8)
     * - Emergency stop integration (Issue 9) */

    return k_rx_ok;
}
