/* src/tasks/comm_manager.c */

/**
 * @file comm_manager.c
 * @brief Communication Manager thread implementation
 * @details
 * Manages USB CDC/SPI communication with RPi5 at 100 Hz.
 * High priority to prevent command lag.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/comm_manager.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_frame.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include <string.h>

/* nanopb Protocol Buffers */
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "gen/star/v1/motor_control.pb.h"
#include "gen/star/v1/telemetry.pb.h"

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static char s_tag[] = "comm_mgr";

/* Thread control block and stack */
static TX_THREAD s_comm_manager_thread;
static uint8_t   s_comm_manager_stack[k_stack_comm_manager];

/* USB communication handle */
static rx_usb_comm_handle_t s_usb_comm_handle;

/* Communication state */
static uint32_t s_last_command_timestamp_ms = 0;

/* =============================================================================
 * Private Function Prototypes
 * =============================================================================
 */

static void     comm_manager_entry(ULONG input);
static rx_err_t process_ingress(void);
static rx_err_t process_egress(void);
static rx_err_t check_comm_timeout(void);
static rx_err_t check_safe_to_clear_estop(void);
static rx_err_t process_clear_estop_command(uint16_t sequence);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT comm_manager_create(void)
{
    UINT status = TX_SUCCESS;

    /* Create Comm_Manager thread */
    status = tx_thread_create(
        &s_comm_manager_thread,
        s_tag,
        comm_manager_entry,
        0,
        s_comm_manager_stack,
        k_stack_comm_manager,
        k_priority_comm_manager,
        k_priority_comm_manager,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Comm_Manager thread created (priority 3, 100 Hz)");
    return TX_SUCCESS;
}

/* =============================================================================
 * Private Function Implementations
 * =============================================================================
 */

static void comm_manager_entry(ULONG input)
{
    rx_err_t ret = k_rx_ok;

    (void)input;

    rx_log_info(s_tag, "Comm_Manager thread started");

    /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 10ms = 30ms */
    ret = rx_iwdt_register_task("Comm_Manager", 30);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to register with watchdog");
    }

    /* Initialize USB communication handle */
    rx_usb_comm_config_t usb_config = {
        .fec_enabled = 0,     /* FEC disabled for now */
        .time_iface  = NULL,  /* Use default time interface */
    };

    ret = rx_usb_comm_init(&s_usb_comm_handle, &usb_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "USB comm init failed");
        /* Cannot continue without USB comm */
        while (1) {
            tx_thread_sleep(k_threadx_ticks_1s);
        }
    }

    /* Wait for USB CDC to be configured */
    while (!rx_usb_is_configured()) {
        tx_thread_sleep(k_threadx_ticks_100ms); /* Wait 100ms */
    }

    rx_log_info(s_tag, "USB CDC configured, entering main loop");

    /* Initialize last command timestamp */
    s_last_command_timestamp_ms = tx_time_get();

    /* Main communication loop (100 Hz = 10ms period) */
    while (1) {
        /* Process incoming commands (ingress) */
        ret = process_ingress();
        if (ret != k_rx_ok && ret != k_rx_err_timeout) {
            rx_log_error(s_tag, "Ingress processing error");
        }

        /* Send telemetry (egress) */
        ret = process_egress();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Egress processing error");
        }

        /* Check communication timeout */
        ret = check_comm_timeout();
        if (ret != k_rx_ok) {
            /* Timeout already logged in check_comm_timeout */
        }

        /* Feed watchdog (critical - prevents reset) */
        rx_iwdt_feed();

        /* Record task heartbeat for deadlock detection */
        rx_iwdt_task_heartbeat("Comm_Manager");

        /* Sleep for 10ms (100 Hz) */
        tx_thread_sleep(k_threadx_ticks_10ms); /* 10ms = 100 Hz communication rate */
    }
}

static rx_err_t process_ingress(void)
{
    /* -------------------------------------------------------------------------
     * Issue 11: Command Processing (Ingress)
     * -------------------------------------------------------------------------
     * Receive frames via USB CDC (100ms timeout = non-blocking poll)
     * Decode SetVelocityRequest (nanopb deserialize)
     * Update shared setpoint
     * Send ACK response
     */
    rx_frame_t frame = {0};

    /* Non-blocking receive (100ms timeout) */
    rx_err_t ret = rx_usb_comm_receive(&s_usb_comm_handle, &frame, 100);

    if (ret == k_rx_err_timeout) {
        /* No data available - not an error */
        return k_rx_err_timeout;
    }

    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Frame receive error");
        return ret;
    }

    /* Update last command timestamp */
    s_last_command_timestamp_ms = tx_time_get();

    /* Process frame based on type */
    if (frame.header.type == k_frame_type_command) {
        /* Check for clear E-STOP command (empty payload = clear E-STOP) */
        if (frame.header.length == 0) {
            ret = process_clear_estop_command(frame.header.sequence);
            return ret;
        }

        /* Decode Protocol Buffer message using nanopb */
        star_v1_VelocityCommand velocity_cmd = star_v1_VelocityCommand_init_zero;

        /* Create nanopb input stream from frame payload */
        pb_istream_t istream = pb_istream_from_buffer(frame.payload, frame.header.length);

        /* Decode VelocityCommand message */
        const bool decode_status = pb_decode(&istream, star_v1_VelocityCommand_fields, &velocity_cmd);
        if (!decode_status) {
            rx_log_error(s_tag, "Protobuf decode failed");
            rx_usb_comm_send_nack(&s_usb_comm_handle, frame.header.sequence, 0);
            return k_rx_err_protocol_error;
        }

        /* Extract and validate velocities from decoded message */
        float velocities[k_motor_count];
        velocities[0] = (float)velocity_cmd.motor_0_velocity_mps;
        velocities[1] = (float)velocity_cmd.motor_1_velocity_mps;
        velocities[2] = (float)velocity_cmd.motor_2_velocity_mps;
        velocities[3] = (float)velocity_cmd.motor_3_velocity_mps;

        /* Validate and clamp velocities to ±2.0 m/s */
        for (uint8_t i = 0; i < k_motor_count; i++) {
            const float max_vel = (float)k_motor_max_velocity_x100 / 100.0f;
            const float min_vel = (float)k_motor_min_velocity_x100 / 100.0f;

            if (velocities[i] > max_vel) {
                velocities[i] = max_vel;
            } else if (velocities[i] < min_vel) {
                velocities[i] = min_vel;
            }
        }

        /* Update shared setpoint (mutex-protected) */
        shared_state_t* state = shared_state_get();
        UINT status = tx_mutex_get(&state->setpoint_mutex, TX_WAIT_FOREVER);
        if (status != TX_SUCCESS) {
            rx_log_error(s_tag, "Failed to acquire setpoint mutex");
            return k_rx_err_threadx;
        }

        for (uint8_t i = 0; i < k_motor_count; i++) {
            state->setpoint.velocity_mps[i] = velocities[i];
        }
        state->setpoint.sequence = frame.header.sequence;
        state->setpoint.timestamp_ms = tx_time_get();
        state->setpoint.valid = true;

        tx_mutex_put(&state->setpoint_mutex);

        /* Send ACK response */
        ret = rx_usb_comm_send_ack(&s_usb_comm_handle, frame.header.sequence);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "ACK send failed");
        }
    }

    return k_rx_ok;
}

static rx_err_t process_egress(void)
{
    /* -------------------------------------------------------------------------
     * Issue 12: Telemetry Streaming (Egress)
     * -------------------------------------------------------------------------
     * Read encoder feedback (4 motors) from shared state
     * Read battery/temperature from health state
     * Encode TelemetryData message (nanopb serialize)
     * Transmit via USB CDC at 100 Hz
     */
    shared_state_t* state = shared_state_get();

    /* Read encoder feedback (mutex-protected) */
    UINT status = tx_mutex_get(&state->encoder_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire encoder mutex");
        return k_rx_err_threadx;
    }

    encoder_feedback_t encoder_data = state->encoders;
    tx_mutex_put(&state->encoder_mutex);

    /* Read safety state (mutex-protected) */
    status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire safety mutex");
        return k_rx_err_threadx;
    }

    safety_state_t safety_data = state->safety;
    tx_mutex_put(&state->safety_mutex);

    /* Read health data (mutex-protected) */
    status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire health mutex");
        return k_rx_err_threadx;
    }

    system_health_t health_data = state->health;
    tx_mutex_put(&state->health_mutex);

    /* Encode Protocol Buffer message using nanopb */
    star_v1_TelemetryData telemetry_msg = star_v1_TelemetryData_init_zero;

    /* Fill in telemetry data */
    const uint32_t timestamp_ms = tx_time_get();
    telemetry_msg.timestamp_us = (int64_t)timestamp_ms * 1000LL; /* Convert ms to us */

    /* Encoder data (4 motors) - individual fields for embedded (no dynamic allocation) */
    telemetry_msg.encoder_0.motor_id = 0;
    telemetry_msg.encoder_0.ticks = encoder_data.motors[0].ticks;
    telemetry_msg.encoder_0.velocity_mps = encoder_data.motors[0].velocity_mps;
    telemetry_msg.encoder_0.timestamp_us = telemetry_msg.timestamp_us;

    telemetry_msg.encoder_1.motor_id = 1;
    telemetry_msg.encoder_1.ticks = encoder_data.motors[1].ticks;
    telemetry_msg.encoder_1.velocity_mps = encoder_data.motors[1].velocity_mps;
    telemetry_msg.encoder_1.timestamp_us = telemetry_msg.timestamp_us;

    telemetry_msg.encoder_2.motor_id = 2;
    telemetry_msg.encoder_2.ticks = encoder_data.motors[2].ticks;
    telemetry_msg.encoder_2.velocity_mps = encoder_data.motors[2].velocity_mps;
    telemetry_msg.encoder_2.timestamp_us = telemetry_msg.timestamp_us;

    telemetry_msg.encoder_3.motor_id = 3;
    telemetry_msg.encoder_3.ticks = encoder_data.motors[3].ticks;
    telemetry_msg.encoder_3.velocity_mps = encoder_data.motors[3].velocity_mps;
    telemetry_msg.encoder_3.timestamp_us = telemetry_msg.timestamp_us;

    /* Safety state */
    telemetry_msg.emergency_stop = safety_data.emergency_stop;
    telemetry_msg.fault_flags = safety_data.fault_flags;

    /* Health data */
    telemetry_msg.battery_voltage_v = health_data.battery_voltage_v;
    telemetry_msg.battery_soc_percent = (uint32_t)health_data.battery_soc_percent;
    telemetry_msg.temperature_celsius = health_data.temperature_c;

    /* Encode message to buffer */
    uint8_t payload[256];
    pb_ostream_t ostream = pb_ostream_from_buffer(payload, sizeof(payload));

    const bool encode_status = pb_encode(&ostream, star_v1_TelemetryData_fields, &telemetry_msg);
    if (!encode_status) {
        rx_log_error(s_tag, "Protobuf encode failed");
        return k_rx_err_protocol_error;
    }

    /* Send telemetry frame */
    rx_err_t ret = rx_usb_comm_send(
        &s_usb_comm_handle,
        k_frame_type_response,
        0, /* flags */
        payload,
        ostream.bytes_written
    );

    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Telemetry send failed");
        return ret;
    }

    return k_rx_ok;
}

static rx_err_t check_comm_timeout(void)
{
    /* -------------------------------------------------------------------------
     * Issue 17: Communication Timeout Handling
     * -------------------------------------------------------------------------
     * Check if last command was received within k_comm_timeout_ms (500ms)
     * If timeout, trigger E-STOP by setting safety.comm_timeout flag
     */
    const uint32_t current_time_ms = tx_time_get();
    const uint32_t elapsed_ms = current_time_ms - s_last_command_timestamp_ms;

    if (elapsed_ms > k_comm_timeout_ms) {
        /* Communication timeout - trigger E-STOP */
        shared_state_t* state = shared_state_get();

        UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
        if (status != TX_SUCCESS) {
            rx_log_error(s_tag, "Failed to acquire safety mutex");
            return k_rx_err_threadx;
        }

        /* Only log once when timeout first detected */
        if (!state->safety.comm_timeout) {
            rx_log_error(s_tag, "Communication timeout - E-STOP triggered");
            state->safety.comm_timeout = true;
            state->safety.emergency_stop = true;
        }

        tx_mutex_put(&state->safety_mutex);

        return k_rx_err_timeout;
    }

    /* Communication is healthy - clear timeout flag if it was set */
    shared_state_t* state = shared_state_get();

    UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire safety mutex");
        return k_rx_err_threadx;
    }

    if (state->safety.comm_timeout) {
        rx_log_info(s_tag, "Communication restored");
        state->safety.comm_timeout = false;
        /* Note: E-STOP flag remains set until manual clearance */
    }

    tx_mutex_put(&state->safety_mutex);

    return k_rx_ok;
}

static rx_err_t check_safe_to_clear_estop(void)
{
    /* -------------------------------------------------------------------------
     * Issue 18: Manual Emergency Stop Clearance - Safety Check
     * -------------------------------------------------------------------------
     * Check all safety conditions before allowing E-STOP clearance:
     * - No obstacle detected
     * - No motor faults
     * - No communication timeout
     */
    shared_state_t* state = shared_state_get();

    /* Read safety state (mutex-protected) */
    UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire safety mutex");
        return k_rx_err_threadx;
    }

    const bool obstacle_detected = state->safety.obstacle_detected;
    const bool comm_timeout = state->safety.comm_timeout;
    const uint8_t fault_flags = state->safety.fault_flags;

    tx_mutex_put(&state->safety_mutex);

    /* Check safety conditions */
    if (obstacle_detected) {
        rx_log_error(s_tag, "Cannot clear E-STOP: obstacle still detected");
        return k_rx_err_invalid_state;
    }

    if (comm_timeout) {
        rx_log_error(s_tag, "Cannot clear E-STOP: communication timeout");
        return k_rx_err_invalid_state;
    }

    if (fault_flags != 0) {
        rx_log_error(s_tag, "Cannot clear E-STOP: motor faults present");
        return k_rx_err_hw_error;
    }

    /* All safety conditions met */
    return k_rx_ok;
}

static rx_err_t process_clear_estop_command(uint16_t sequence)
{
    /* -------------------------------------------------------------------------
     * Issue 18: Manual Emergency Stop Clearance - Command Handler
     * -------------------------------------------------------------------------
     * Handle ClearEmergencyStop command from RPi5
     * Only clear E-STOP if all safety conditions are met
     */
    shared_state_t* state = shared_state_get();

    /* Check if it's safe to clear E-STOP */
    rx_err_t ret = check_safe_to_clear_estop();
    if (ret != k_rx_ok) {
        /* Safety check failed - send NACK with error code */
        rx_usb_comm_send_nack(&s_usb_comm_handle, sequence, (uint8_t)ret);
        return ret;
    }

    /* Safe to clear - clear emergency stop flag */
    UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire safety mutex");
        rx_usb_comm_send_nack(&s_usb_comm_handle, sequence, (uint8_t)k_rx_err_threadx);
        return k_rx_err_threadx;
    }

    state->safety.emergency_stop = false;
    tx_mutex_put(&state->safety_mutex);

    rx_log_info(s_tag, "Emergency stop cleared (manual clearance)");

    /* Send ACK response */
    ret = rx_usb_comm_send_ack(&s_usb_comm_handle, sequence);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to send clearance ACK");
    }

    return k_rx_ok;
}
