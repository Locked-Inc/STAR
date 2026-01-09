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

#include <string.h>

#include "motor_config.h"
#include "rx_frame.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include "shared_state.h"

/* nanopb Protocol Buffers */
#include "gen/star/v1/motor_control.pb.h"
#include "gen/star/v1/telemetry.pb.h"
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static char* s_tag = "comm_mgr";

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
static rx_err_t comm_manager_init_task(void);
static rx_err_t process_ingress(void);
static rx_err_t decode_and_apply_velocity_command(const rx_frame_t* frame);
static rx_err_t process_egress(void);
static rx_err_t collect_and_encode_telemetry(uint8_t* payload, uint32_t* payload_len);
static rx_err_t
read_telemetry_data(encoder_feedback_t* enc, safety_state_t* safety, system_health_t* health);
static void     populate_telemetry_message(star_v1_TelemetryData*    msg,
                                           const encoder_feedback_t* enc,
                                           const safety_state_t*     safety,
                                           const system_health_t*    health);
static rx_err_t encode_telemetry_to_buffer(const star_v1_TelemetryData* msg,
                                           uint8_t*                     payload,
                                           uint32_t*                    payload_len);
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
  status = tx_thread_create(&s_comm_manager_thread,
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

/**
 * @brief Initialize Comm_Manager task resources
 *
 * Performs watchdog registration, USB communication initialization,
 * and waits for USB CDC enumeration before returning.
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t comm_manager_init_task(void)
{
  rx_err_t ret;

  /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 10ms = 30ms */
  ret = rx_iwdt_register_task("Comm_Manager", 30);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register with watchdog");
  }

  /* Initialize USB communication handle */
  rx_usb_comm_config_t usb_config = {
    .fec_enabled = 0,    /* FEC disabled for now */
    .time_iface  = NULL, /* Use default time interface */
  };

  ret = rx_usb_comm_init(&s_usb_comm_handle, &usb_config);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "USB comm init failed");
    return ret;
  }

  /* Wait for USB CDC to be configured */
  while (!rx_usb_is_configured()) {
    tx_thread_sleep(k_threadx_ticks_100ms); /* Wait 100ms */
  }

  rx_log_info(s_tag, "USB CDC configured, entering main loop");

  /* Initialize last command timestamp */
  s_last_command_timestamp_ms = tx_time_get();

  return k_rx_ok;
}

static void comm_manager_entry(ULONG input)
{
  rx_err_t ret;

  (void)input;

  rx_log_info(s_tag, "Comm_Manager thread started");

  /* Initialize task resources (watchdog, USB, wait for enumeration) */
  ret = comm_manager_init_task();
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Task initialization failed");
    /* Cannot continue without USB comm - infinite sleep */
    while (1) {
      tx_thread_sleep(k_threadx_ticks_1s);
    }
  }

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

/**
 * @brief Decode velocity command and apply to shared setpoint
 * @param frame Received frame with velocity command payload
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t decode_and_apply_velocity_command(const rx_frame_t* frame)
{
  /* Decode Protocol Buffer message using nanopb */
  star_v1_VelocityCommand velocity_cmd = star_v1_VelocityCommand_init_zero;

  /* Create nanopb input stream from frame payload */
  pb_istream_t istream = pb_istream_from_buffer(frame->payload, frame->header.length);

  /* Decode VelocityCommand message */
  const bool decode_status = pb_decode(&istream, star_v1_VelocityCommand_fields, &velocity_cmd);
  if (!decode_status) {
    rx_log_error(s_tag, "Protobuf decode failed");
    rx_usb_comm_send_nack(&s_usb_comm_handle, frame->header.sequence, 0);
    return k_rx_err_protocol_error;
  }

  /* Extract and validate velocities from decoded message */
  float velocities[k_motor_count];
  velocities[k_motor_front_left]  = (float)velocity_cmd.front_left_velocity_mps;
  velocities[k_motor_front_right] = (float)velocity_cmd.front_right_velocity_mps;
  velocities[k_motor_back_left]   = (float)velocity_cmd.back_left_velocity_mps;
  velocities[k_motor_back_right]  = (float)velocity_cmd.back_right_velocity_mps;

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
  shared_state_t* state  = shared_state_get();
  UINT            status = tx_mutex_get(&state->setpoint_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire setpoint mutex");
    return k_rx_err_threadx;
  }

  for (uint8_t i = 0; i < k_motor_count; i++) {
    state->setpoint.velocity_mps[i] = velocities[i];
  }
  state->setpoint.sequence     = frame->header.sequence;
  state->setpoint.timestamp_ms = tx_time_get();
  state->setpoint.valid        = true;

  tx_mutex_put(&state->setpoint_mutex);

  /* Send ACK response */
  rx_err_t ret = rx_usb_comm_send_ack(&s_usb_comm_handle, frame->header.sequence);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "ACK send failed");
  }

  return k_rx_ok;
}

static rx_err_t process_ingress(void)
{
  /* -------------------------------------------------------------------------
     * Issue 11: Command Processing (Ingress - Refactored)
     * -------------------------------------------------------------------------
     * Receive frames via USB CDC (100ms timeout = non-blocking poll)
     * Decode and apply velocity commands via helper function
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

    /* Decode and apply velocity command */
    ret = decode_and_apply_velocity_command(&frame);
    if (ret != k_rx_ok) {
      return ret;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read telemetry data from shared state (mutex-protected)
 * @param enc Output encoder feedback data
 * @param safety Output safety state data
 * @param health Output system health data
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t
read_telemetry_data(encoder_feedback_t* enc, safety_state_t* safety, system_health_t* health)
{
  shared_state_t* state = shared_state_get();

  /* Read encoder feedback (mutex-protected) */
  UINT status = tx_mutex_get(&state->encoder_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire encoder mutex");
    return k_rx_err_threadx;
  }

  *enc = state->encoders;
  tx_mutex_put(&state->encoder_mutex);

  /* Read safety state (mutex-protected) */
  status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire safety mutex");
    return k_rx_err_threadx;
  }

  *safety = state->safety;
  tx_mutex_put(&state->safety_mutex);

  /* Read health data (mutex-protected) */
  status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire health mutex");
    return k_rx_err_threadx;
  }

  *health = state->health;
  tx_mutex_put(&state->health_mutex);

  return k_rx_ok;
}

/**
 * @brief Populate telemetry message from collected data
 * @param msg Output Protocol Buffer message
 * @param enc Encoder feedback data
 * @param safety Safety state data
 * @param health System health data
 */
static void populate_telemetry_message(star_v1_TelemetryData*    msg,
                                       const encoder_feedback_t* enc,
                                       const safety_state_t*     safety,
                                       const system_health_t*    health)
{
  /* Fill in timestamp */
  const uint32_t timestamp_ms = tx_time_get();
  msg->timestamp_us           = (int64_t)timestamp_ms * 1000LL; /* Convert ms to us */

  /* Encoder data (4 motors) - individual fields for embedded (no dynamic allocation) */
  msg->encoder_front_left.motor_id     = k_motor_front_left;
  msg->encoder_front_left.ticks        = enc->motors[k_motor_front_left].ticks;
  msg->encoder_front_left.velocity_mps = enc->motors[k_motor_front_left].velocity_mps;
  msg->encoder_front_left.timestamp_us = msg->timestamp_us;
  msg->has_encoder_front_left          = true;

  msg->encoder_front_right.motor_id     = k_motor_front_right;
  msg->encoder_front_right.ticks        = enc->motors[k_motor_front_right].ticks;
  msg->encoder_front_right.velocity_mps = enc->motors[k_motor_front_right].velocity_mps;
  msg->encoder_front_right.timestamp_us = msg->timestamp_us;
  msg->has_encoder_front_right          = true;

  msg->encoder_back_left.motor_id     = k_motor_back_left;
  msg->encoder_back_left.ticks        = enc->motors[k_motor_back_left].ticks;
  msg->encoder_back_left.velocity_mps = enc->motors[k_motor_back_left].velocity_mps;
  msg->encoder_back_left.timestamp_us = msg->timestamp_us;
  msg->has_encoder_back_left          = true;

  msg->encoder_back_right.motor_id     = k_motor_back_right;
  msg->encoder_back_right.ticks        = enc->motors[k_motor_back_right].ticks;
  msg->encoder_back_right.velocity_mps = enc->motors[k_motor_back_right].velocity_mps;
  msg->encoder_back_right.timestamp_us = msg->timestamp_us;
  msg->has_encoder_back_right          = true;

  /* Safety state */
  msg->emergency_stop = safety->emergency_stop;
  msg->fault_flags    = safety->fault_flags;

  /* Health data */
  msg->battery_voltage_v   = health->battery_voltage_v;
  msg->battery_soc_percent = (uint32_t)health->battery_soc_percent;
  msg->temperature_celsius = health->temperature_c;
}

/**
 * @brief Encode telemetry message to buffer using nanopb
 * @param msg Protocol Buffer message to encode
 * @param payload Output buffer for encoded data
 * @param payload_len Output length of encoded payload
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t encode_telemetry_to_buffer(const star_v1_TelemetryData* msg,
                                           uint8_t*                     payload,
                                           uint32_t*                    payload_len)
{
  /* Encode message to buffer */
  pb_ostream_t ostream = pb_ostream_from_buffer(payload, star_v1_TelemetryData_size);

  const bool encode_status = pb_encode(&ostream, star_v1_TelemetryData_fields, msg);
  if (!encode_status) {
    rx_log_error(s_tag, "Protobuf encode failed");
    return k_rx_err_protocol_error;
  }

  *payload_len = (uint32_t)ostream.bytes_written;
  return k_rx_ok;
}

/**
 * @brief Collect telemetry data and encode to Protocol Buffer
 * @param payload Output buffer for encoded telemetry
 * @param payload_len Output length of encoded payload
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t collect_and_encode_telemetry(uint8_t* payload, uint32_t* payload_len)
{
  /* Refactored into 3 focused helper functions for NASA Rule 4 compliance */
  encoder_feedback_t    enc;
  safety_state_t        safety;
  system_health_t       health;
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;

  /* 1. Read all telemetry data from shared state (mutex-protected) */
  rx_err_t ret = read_telemetry_data(&enc, &safety, &health);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 2. Populate Protocol Buffer message (pure data transformation) */
  populate_telemetry_message(&msg, &enc, &safety, &health);

  /* 3. Encode message to buffer using nanopb */
  return encode_telemetry_to_buffer(&msg, payload, payload_len);
}

static rx_err_t process_egress(void)
{
  /* -------------------------------------------------------------------------
     * Issue 12: Telemetry Streaming (Egress - Refactored)
     * -------------------------------------------------------------------------
     * Collect telemetry and encode via helper function
     * Transmit via USB CDC at 100 Hz
     */
  uint8_t  payload[star_v1_TelemetryData_size]; /* 418 bytes - prevents buffer overflow */
  uint32_t payload_len = 0;

  /* Collect telemetry data and encode to Protocol Buffer */
  rx_err_t ret = collect_and_encode_telemetry(payload, &payload_len);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* Send telemetry frame */
  ret = rx_usb_comm_send(&s_usb_comm_handle,
                         k_frame_type_response,
                         0, /* flags */
                         payload,
                         payload_len);

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
  const uint32_t elapsed_ms      = current_time_ms - s_last_command_timestamp_ms;

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
      state->safety.comm_timeout   = true;
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

  const bool    obstacle_detected = state->safety.obstacle_detected;
  const bool    comm_timeout      = state->safety.comm_timeout;
  const uint8_t fault_flags       = state->safety.fault_flags;

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
