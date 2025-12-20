/**
 * @file telemetry_task.c
 * @brief FreeRTOS task implementation for telemetry data handling and transmission
 * @details
 * Implements the telemetry task that handles temperature sensor readings from the DS18B20
 * and processes Protocol Buffer communication with the Raspberry Pi 5 over SPI using the
 * ARQ (Automatic Repeat Request) protocol for reliable data delivery.
 *
 * The task continuously monitors for incoming telemetry requests, reads sensor data,
 * and sends responses using nanopb-encoded Protocol Buffer messages.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/telemetry_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_log.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "star/v1/common.pb.h"
#include "star/v1/telemetry.pb.h"

static const char* s_tag = "TELEMETRY_TASK";

/* Task parameters structure */
typedef struct {
  star_ds18b20_handle_t* sensor;
  star_arq_handle_t*     arq;
} telemetry_task_params_t;

/* --- Private Function Prototypes --- */

static void internal_handle_telemetry_request(star_ds18b20_handle_t* sensor,
                                              star_arq_handle_t*     arq,
                                              const uint8_t*         request_buf,
                                              size_t                 request_len,
                                              uint8_t*               response_buf,
                                              size_t                 response_buf_size);
static void internal_handle_requests(star_ds18b20_handle_t* sensor, star_arq_handle_t* arq);
static void internal_telemetry_task(void* pvParameters);

/* --- Public Functions --- */

void telemetry_task_create(star_ds18b20_handle_t* sensor, star_arq_handle_t* arq)
{
  /* Allocate task parameters (static to persist after function returns) */
  static telemetry_task_params_t task_params;
  task_params.sensor = sensor;
  task_params.arq    = arq;

  /* Create telemetry task */
  BaseType_t task_ret = xTaskCreate(internal_telemetry_task,
                                    "telemetry",
                                    k_telemetry_task_stack_size,
                                    &task_params,
                                    k_telemetry_task_priority,
                                    NULL);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_tag, "Failed to create telemetry task");
    return;
  }

  ESP_LOGI(s_tag, "Telemetry task created successfully");
}

/* --- Private Functions --- */

/* TODO(human): Implement the telemetry request handler
 *
 * This function should:
 * 1. Decode the incoming GetTelemetryRequest using pb_decode()
 * 2. Read temperature from the DS18B20 sensor
 * 3. Build a GetTelemetryResponse with the temperature data
 * 4. Encode the response using pb_encode()
 * 5. Send the response via star_arq_send()
 *
 * Key considerations:
 * - Error handling: What if decode fails? What if sensor read fails?
 * - Response status: Should use star_v1_Status_STATUS_OK on success
 * - Request ID: Must be copied from request to response for correlation
 * - Timestamp: Optional but useful for debugging (can set to 0 for now)
 */
static void internal_handle_telemetry_request(star_ds18b20_handle_t* sensor,
                                              star_arq_handle_t*     arq,
                                              const uint8_t*         request_buf,
                                              size_t                 request_len,
                                              uint8_t*               response_buf,
                                              size_t                 response_buf_size)
{
  /* Decode the request */
  pb_istream_t                input_stream = pb_istream_from_buffer(request_buf, request_len);
  star_v1_GetTelemetryRequest request      = star_v1_GetTelemetryRequest_init_zero;

  bool decode_ret = pb_decode(&input_stream, star_v1_GetTelemetryRequest_fields, &request);

  if (!decode_ret) {
    ESP_LOGE(s_tag, "Failed to decode telemetry request: %s", PB_GET_ERROR(&input_stream));
  }

  /* Read sensor */
  float     temp_c     = 0.0f;
  esp_err_t sensor_ret = ESP_FAIL;

  if (decode_ret) {
    sensor_ret = star_sensor_ds18b20_read_temp(sensor, &temp_c);

    if (sensor_ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to read temperature from sensor: %s", esp_err_to_name(sensor_ret));
    }
  }

  /* Build response */
  star_v1_GetTelemetryResponse response = star_v1_GetTelemetryResponse_init_zero;
  response.has_header                   = true;

  /* Set status */
  if (!decode_ret) {
    response.header.status = star_v1_Status_STATUS_INVALID_REQUEST;
  } else if (sensor_ret != ESP_OK) {
    response.header.status = star_v1_Status_STATUS_INTERNAL_ERROR;
  } else {
    response.header.status = star_v1_Status_STATUS_OK;
  }

  /* Copy request_id for correlation */
  if (decode_ret && request.has_header) {
    response.header.request_id = request.header.request_id;
  }

  /* Set telemetry data */
  if (sensor_ret == ESP_OK) {
    response.has_telemetry                 = true;
    response.telemetry.temperature_celsius = temp_c;
  }

  /* Encode response */
  pb_ostream_t output_stream = pb_ostream_from_buffer(response_buf, response_buf_size);

  bool encode_ok = pb_encode(&output_stream, star_v1_GetTelemetryResponse_fields, &response);

  if (!encode_ok) {
    ESP_LOGE(s_tag, "Failed to encode response: %s", PB_GET_ERROR(&output_stream));
    return;
  }

  size_t response_len = output_stream.bytes_written;

  /* Send response */
  esp_err_t send_ret = star_arq_send(arq, response_buf, response_len, k_arq_send_timeout_ms);

  if (send_ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to send telemetry response: %s", esp_err_to_name(send_ret));
  }
}

static void internal_handle_requests(star_ds18b20_handle_t* sensor, star_arq_handle_t* arq)
{
  uint8_t rx_buffer[k_rx_buffer_size];
  uint8_t tx_buffer[k_tx_buffer_size];

  while (true) {
    /* Receive request from RPi5 */
    size_t    rx_len;
    esp_err_t ret = star_arq_receive(arq, rx_buffer, k_rx_buffer_size, &rx_len, portMAX_DELAY);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Receive failed: %s", esp_err_to_name(ret));
      vTaskDelay(pdMS_TO_TICKS(k_arq_error_backoff_ms));
      continue;
    }

    /* Process telemetry request */
    internal_handle_telemetry_request(sensor, arq, rx_buffer, rx_len, tx_buffer, k_tx_buffer_size);
  }
}

static void internal_telemetry_task(void* pvParameters)
{
  telemetry_task_params_t* params = (telemetry_task_params_t*)pvParameters;

  ESP_LOGI(s_tag, "Telemetry task started");

  /* Run request/response loop */
  internal_handle_requests(params->sensor, params->arq);

  /* Should never reach here */
  ESP_LOGE(s_tag, "Telemetry task exited unexpectedly");
  vTaskDelete(NULL);
}
