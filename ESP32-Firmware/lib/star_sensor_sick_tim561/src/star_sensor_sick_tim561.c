/* lib/star_sensor_sick_tim561/src/star_sensor_sick_tim561.c */

#include "star_sensor_sick_tim561.h"

#include <esp_log.h>
#include <lwip/sockets.h>
#include <stdlib.h>
#include <string.h>

static const char* s_TAG = "tim561";

esp_err_t star_sensor_tim561_init(tim561_handle_t* handle, const tim561_config_t* config)
{
  if (handle == NULL || config == NULL || config->host == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(tim561_handle_t));

  // Allocate host string
  handle->host = (char*)malloc(strlen(config->host) + 1);
  if (handle->host == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate host string");
    return ESP_ERR_NO_MEM;
  }
  strcpy(handle->host, config->host);
  handle->port = config->port;
  handle->socket_fd = -1;

  // Allocate response buffer
  handle->response_capacity = TIM561_MAX_RESPONSE_LEN;
  handle->response_buffer = (char*)malloc(handle->response_capacity);
  if (handle->response_buffer == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate response buffer");
    free(handle->host);
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    free(handle->response_buffer);
    free(handle->host);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "SICK TiM561 initialized (host=%s, port=%d)", handle->host, handle->port);

  return ESP_OK;
}

esp_err_t star_sensor_tim561_deinit(tim561_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->socket_fd >= 0) {
    star_sensor_tim561_disconnect(handle);
  }

  if (handle->response_buffer != NULL) {
    free(handle->response_buffer);
  }

  if (handle->host != NULL) {
    free(handle->host);
  }

  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_tim561_connect(tim561_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->socket_fd >= 0) {
    ESP_LOGW(s_TAG, "Already connected");
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Connecting to %s:%d", handle->host, handle->port);

  // Create socket
  handle->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (handle->socket_fd < 0) {
    ESP_LOGE(s_TAG, "Failed to create socket");
    return ESP_FAIL;
  }

  // Set socket timeout
  struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
  setsockopt(handle->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(handle->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  // Connect to scanner
  struct sockaddr_in dest_addr;
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(handle->port);
  inet_pton(AF_INET, handle->host, &dest_addr.sin_addr);

  int ret = connect(handle->socket_fd, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  if (ret != 0) {
    ESP_LOGE(s_TAG, "Failed to connect: errno %d", errno);
    close(handle->socket_fd);
    handle->socket_fd = -1;
    return ESP_FAIL;
  }

  ESP_LOGI(s_TAG, "Connected successfully");

  return ESP_OK;
}

esp_err_t star_sensor_tim561_disconnect(tim561_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->socket_fd >= 0) {
    close(handle->socket_fd);
    handle->socket_fd = -1;
    ESP_LOGI(s_TAG, "Disconnected");
  }

  return ESP_OK;
}

esp_err_t star_sensor_tim561_send_command(tim561_handle_t* handle,
                                           const char*      command,
                                           char*            response,
                                           size_t           response_len)
{
  if (handle == NULL || command == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->socket_fd < 0) {
    ESP_LOGE(s_TAG, "Not connected");
    return ESP_ERR_INVALID_STATE;
  }

  // Send command with COLA-A framing (STX command ETX)
  char cmd_buf[256];
  snprintf(cmd_buf, sizeof(cmd_buf), "\x02%s\x03", command);

  int sent = send(handle->socket_fd, cmd_buf, strlen(cmd_buf), 0);
  if (sent < 0) {
    ESP_LOGE(s_TAG, "Failed to send command");
    return ESP_FAIL;
  }

  ESP_LOGD(s_TAG, "TX: %s", command);

  // Receive response if buffer provided
  if (response != NULL && response_len > 0) {
    memset(response, 0, response_len);
    int received = recv(handle->socket_fd, response, response_len - 1, 0);
    if (received < 0) {
      ESP_LOGE(s_TAG, "Failed to receive response");
      return ESP_FAIL;
    }

    ESP_LOGD(s_TAG, "RX: %d bytes", received);
  }

  return ESP_OK;
}

esp_err_t star_sensor_tim561_get_device_info(tim561_handle_t* handle, tim561_device_info_t* info)
{
  if (handle == NULL || info == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[256];
  esp_err_t ret = star_sensor_tim561_send_command(handle, "sRI 0", response, sizeof(response));

  if (ret != ESP_OK) {
    return ret;
  }

  memset(info, 0, sizeof(tim561_device_info_t));
  // Parse response (simplified - real implementation would parse COLA response)
  strncpy(info->device_name, "TiM561", sizeof(info->device_name) - 1);

  return ESP_OK;
}

esp_err_t star_sensor_tim561_start_scan(tim561_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[128];
  esp_err_t ret = star_sensor_tim561_send_command(handle, "sMN LMCstartmeas", response, sizeof(response));

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start measurement");
    return ret;
  }

  // Enable scan data output
  ret = star_sensor_tim561_send_command(handle, "sEN LMDscandata 1", response, sizeof(response));
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to enable scan data");
    return ret;
  }

  handle->scanning = true;
  ESP_LOGI(s_TAG, "Scan started");

  return ESP_OK;
}

esp_err_t star_sensor_tim561_stop_scan(tim561_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[128];
  esp_err_t ret = star_sensor_tim561_send_command(handle, "sEN LMDscandata 0", response, sizeof(response));

  if (ret != ESP_OK) {
    return ret;
  }

  ret = star_sensor_tim561_send_command(handle, "sMN LMCstopmeas", response, sizeof(response));

  handle->scanning = false;
  ESP_LOGI(s_TAG, "Scan stopped");

  return ESP_OK;
}

esp_err_t star_sensor_tim561_read_scan(tim561_handle_t* handle, tim561_scan_data_t* scan_data)
{
  if (handle == NULL || scan_data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->scanning) {
    ESP_LOGE(s_TAG, "Not scanning");
    return ESP_ERR_INVALID_STATE;
  }

  // Receive scan telegram
  int received = recv(handle->socket_fd, handle->response_buffer, handle->response_capacity - 1, 0);
  if (received <= 0) {
    ESP_LOGE(s_TAG, "Failed to receive scan data");
    return ESP_FAIL;
  }

  handle->response_buffer[received] = '\0';

  // Parse telegram (simplified - real implementation would parse binary COLA-B or ASCII COLA-A)
  memset(scan_data, 0, sizeof(tim561_scan_data_t));

  // Example parsing structure (real implementation needs proper COLA parsing)
  scan_data->start_angle_deg = -45.0f;  // TiM561: -45° to +225° (270° total)
  scan_data->angular_resolution_deg = 0.094f;  // 270° / 2880 points
  scan_data->point_count = TIM561_MAX_SCAN_POINTS;

  // In real implementation, parse distance values from telegram
  // Telegram format: STX Type ... DistScale DistStart DistEnd Dist1 Dist2 ... DistN ETX
  // Each distance is typically 16-bit value in mm

  ESP_LOGD(s_TAG, "Scan received: %d points", scan_data->point_count);

  return ESP_OK;
}
