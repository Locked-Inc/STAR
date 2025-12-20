/**
 * @file command_handler_task.c
 * @brief FreeRTOS task implementation for command handling
 * @details
 * Implements the command handler task that receives motor control commands from
 * the Raspberry Pi 5 over SPI using ARQ and Protocol Buffers. The task processes
 * velocity commands and emergency stop requests, updating the shared state that
 * the motor control task reads.
 *
 * Message Flow:
 * RPi5 → SPI → ARQ → Command Handler → Shared State → Motor Control Task → Motors
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/command_handler_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_log.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "star/v1/common.pb.h"
#include "star/v1/motor_control.pb.h"

/* --- Constants --- */

/**
 * @brief Logging tag for ESP-IDF logging macros
 */
static const char* s_tag = "COMMAND_HANDLER_TASK";

/* --- Type Definitions --- */

/**
 * @brief Task parameters structure
 */
typedef struct {
  motor_shared_state_t* shared_state; /**< Pointer to shared state */
  star_arq_handle_t*    arq;          /**< Pointer to ARQ handle */
} command_task_params_t;

/* --- Private Function Prototypes --- */

static void internal_handle_set_velocity_request(motor_shared_state_t* shared_state,
                                                 star_arq_handle_t*    arq,
                                                 const uint8_t*        request_buf,
                                                 size_t                request_len,
                                                 uint8_t*              response_buf,
                                                 size_t                response_buf_size);
static void internal_handle_emergency_stop_request(motor_shared_state_t* shared_state,
                                                   star_arq_handle_t*    arq,
                                                   const uint8_t*        request_buf,
                                                   size_t                request_len,
                                                   uint8_t*              response_buf,
                                                   size_t                response_buf_size);
static void internal_handle_requests(motor_shared_state_t* shared_state, star_arq_handle_t* arq);
static void internal_command_handler_task(void* pvParameters);

/* --- Public Functions --- */

void command_handler_task_create(motor_shared_state_t* shared_state, star_arq_handle_t* arq)
{
  /* Allocate task parameters (static to persist after function returns) */
  static command_task_params_t task_params;
  task_params.shared_state = shared_state;
  task_params.arq          = arq;

  /* Create command handler task */
  BaseType_t task_ret = xTaskCreate(internal_command_handler_task,
                                    "cmd_handler",
                                    k_command_task_stack_size,
                                    &task_params,
                                    k_command_task_priority,
                                    NULL);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_tag, "Failed to create command handler task");
    return;
  }

  ESP_LOGI(s_tag,
           "Command handler task created successfully (priority %d)",
           k_command_task_priority);
}

/* --- Private Functions --- */

/**
 * @brief Handle SetVelocityRequest message
 *
 * Processes a velocity command from the RPi5, distributing the left/right
 * velocities to the appropriate motors based on skid-steer kinematics:
 * - Motors 0, 1 (left side) track left_velocity_mps
 * - Motors 2, 3 (right side) track right_velocity_mps
 *
 * @param[in]  shared_state      Pointer to shared state
 * @param[in]  arq               Pointer to ARQ handle
 * @param[in]  request_buf       Request buffer
 * @param[in]  request_len       Request length
 * @param[out] response_buf      Response buffer
 * @param[in]  response_buf_size Response buffer size
 */
static void internal_handle_set_velocity_request(motor_shared_state_t* shared_state,
                                                 star_arq_handle_t*    arq,
                                                 const uint8_t*        request_buf,
                                                 size_t                request_len,
                                                 uint8_t*              response_buf,
                                                 size_t                response_buf_size)
{
  /* Decode the request */
  pb_istream_t               input_stream = pb_istream_from_buffer(request_buf, request_len);
  star_v1_SetVelocityRequest request      = star_v1_SetVelocityRequest_init_zero;

  bool decode_ret = pb_decode(&input_stream, star_v1_SetVelocityRequest_fields, &request);

  if (!decode_ret) {
    ESP_LOGE(s_tag, "Failed to decode SetVelocityRequest: %s", PB_GET_ERROR(&input_stream));
  }

  /* Update shared state with velocity setpoints */
  esp_err_t ret = ESP_OK;

  if (decode_ret && request.has_command) {
    /* Skid-steer kinematics: distribute left/right to motors */
    /* Left side: motors 0, 1 */
    ret = motor_shared_state_set_velocity(shared_state, 0, request.command.left_velocity_mps);
    ret |= motor_shared_state_set_velocity(shared_state, 1, request.command.left_velocity_mps);

    /* Right side: motors 2, 3 */
    ret |= motor_shared_state_set_velocity(shared_state, 2, request.command.right_velocity_mps);
    ret |= motor_shared_state_set_velocity(shared_state, 3, request.command.right_velocity_mps);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to update velocity setpoints");
    } else {
      ESP_LOGD(s_tag,
               "Velocity command: L=%.3f m/s, R=%.3f m/s",
               request.command.left_velocity_mps,
               request.command.right_velocity_mps);
    }
  }

  /* Build response */
  star_v1_SetVelocityResponse response = star_v1_SetVelocityResponse_init_zero;
  response.has_header                  = true;

  /* Set status */
  if (!decode_ret) {
    response.header.status = star_v1_Status_STATUS_INVALID_REQUEST;
  } else if (ret != ESP_OK) {
    response.header.status = star_v1_Status_STATUS_INTERNAL_ERROR;
  } else {
    response.header.status = star_v1_Status_STATUS_OK;
  }

  /* Copy request_id for correlation */
  if (decode_ret && request.has_header) {
    response.header.request_id = request.header.request_id;
  }

  /* TODO: Populate motor_status array from shared state */
  /* This would read motor status from shared state and encode into response */

  /* Encode response */
  pb_ostream_t output_stream = pb_ostream_from_buffer(response_buf, response_buf_size);

  bool encode_ok = pb_encode(&output_stream, star_v1_SetVelocityResponse_fields, &response);

  if (!encode_ok) {
    ESP_LOGE(s_tag, "Failed to encode SetVelocityResponse: %s", PB_GET_ERROR(&output_stream));
    return;
  }

  size_t response_len = output_stream.bytes_written;

  /* Send response */
  esp_err_t send_ret =
    star_arq_send(arq, response_buf, response_len, k_command_arq_send_timeout_ms);

  if (send_ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to send SetVelocityResponse: %s", esp_err_to_name(send_ret));
  }
}

/**
 * @brief Handle EmergencyStopRequest message
 *
 * Processes an emergency stop request from the RPi5, setting the emergency
 * stop flag in shared state. The motor control task will read this flag
 * and immediately brake all motors.
 *
 * @param[in]  shared_state      Pointer to shared state
 * @param[in]  arq               Pointer to ARQ handle
 * @param[in]  request_buf       Request buffer
 * @param[in]  request_len       Request length
 * @param[out] response_buf      Response buffer
 * @param[in]  response_buf_size Response buffer size
 */
static void internal_handle_emergency_stop_request(motor_shared_state_t* shared_state,
                                                   star_arq_handle_t*    arq,
                                                   const uint8_t*        request_buf,
                                                   size_t                request_len,
                                                   uint8_t*              response_buf,
                                                   size_t                response_buf_size)
{
  /* Decode the request */
  pb_istream_t                 input_stream = pb_istream_from_buffer(request_buf, request_len);
  star_v1_EmergencyStopRequest request      = star_v1_EmergencyStopRequest_init_zero;

  bool decode_ret = pb_decode(&input_stream, star_v1_EmergencyStopRequest_fields, &request);

  if (!decode_ret) {
    ESP_LOGE(s_tag, "Failed to decode EmergencyStopRequest: %s", PB_GET_ERROR(&input_stream));
  }

  /* Activate emergency stop */
  esp_err_t ret = motor_shared_state_set_estop(shared_state, true);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to set emergency stop: %s", esp_err_to_name(ret));
  }

  /* Build response */
  star_v1_EmergencyStopResponse response = star_v1_EmergencyStopResponse_init_zero;
  response.has_header                    = true;

  /* Set status */
  if (!decode_ret) {
    response.header.status = star_v1_Status_STATUS_INVALID_REQUEST;
    response.estop_engaged = false;
  } else if (ret != ESP_OK) {
    response.header.status = star_v1_Status_STATUS_INTERNAL_ERROR;
    response.estop_engaged = false;
  } else {
    response.header.status = star_v1_Status_STATUS_OK;
    response.estop_engaged = true;
  }

  /* Copy request_id for correlation */
  if (decode_ret && request.has_header) {
    response.header.request_id = request.header.request_id;
  }

  /* Encode response */
  pb_ostream_t output_stream = pb_ostream_from_buffer(response_buf, response_buf_size);

  bool encode_ok = pb_encode(&output_stream, star_v1_EmergencyStopResponse_fields, &response);

  if (!encode_ok) {
    ESP_LOGE(s_tag, "Failed to encode EmergencyStopResponse: %s", PB_GET_ERROR(&output_stream));
    return;
  }

  size_t response_len = output_stream.bytes_written;

  /* Send response */
  esp_err_t send_ret =
    star_arq_send(arq, response_buf, response_len, k_command_arq_send_timeout_ms);

  if (send_ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to send EmergencyStopResponse: %s", esp_err_to_name(send_ret));
  }
}

/**
 * @brief Main request/response loop
 *
 * Continuously receives messages via ARQ and dispatches to appropriate handlers
 * based on message type. Currently handles:
 * - SetVelocityRequest
 * - EmergencyStopRequest
 *
 * @param[in] shared_state Pointer to shared state
 * @param[in] arq          Pointer to ARQ handle
 */
static void internal_handle_requests(motor_shared_state_t* shared_state, star_arq_handle_t* arq)
{
  uint8_t rx_buffer[k_command_rx_buffer_size];
  uint8_t tx_buffer[k_command_tx_buffer_size];

  while (true) {
    /* Receive request from RPi5 (blocking) */
    size_t    rx_len;
    esp_err_t ret =
      star_arq_receive(arq, rx_buffer, k_command_rx_buffer_size, &rx_len, portMAX_DELAY);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Receive failed: %s", esp_err_to_name(ret));
      vTaskDelay(pdMS_TO_TICKS(k_command_arq_error_backoff_ms));
      continue;
    }

    /* TODO: Add message type detection to dispatch to correct handler */
    /* For now, assume all messages are SetVelocityRequest */
    /* In production, would parse message type field or use message envelope */

    /* Process as SetVelocityRequest (most common) */
    internal_handle_set_velocity_request(shared_state,
                                         arq,
                                         rx_buffer,
                                         rx_len,
                                         tx_buffer,
                                         k_command_tx_buffer_size);

    /* If implementing message type detection:
     * - Parse message type from envelope or first bytes
     * - Switch based on type:
     *   - SetVelocityRequest → internal_handle_set_velocity_request()
     *   - EmergencyStopRequest → internal_handle_emergency_stop_request()
     *   - SetMotorPowerRequest → internal_handle_set_motor_power_request()
     */
  }
}

/**
 * @brief Command handler task entry point
 *
 * FreeRTOS task entry point. Extracts parameters and calls the main request loop.
 *
 * @param[in] pvParameters Pointer to command_task_params_t
 */
static void internal_command_handler_task(void* pvParameters)
{
  command_task_params_t* params = (command_task_params_t*)pvParameters;

  ESP_LOGI(s_tag, "Command handler task started");

  /* Run request/response loop */
  internal_handle_requests(params->shared_state, params->arq);

  /* Should never reach here */
  ESP_LOGE(s_tag, "Command handler task exited unexpectedly");
  vTaskDelete(NULL);
}
