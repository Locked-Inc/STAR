#include "include/shared_data.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char* s_TAG = "SHARED_DATA";

/* Global shared context - single instance per system */
static shared_context_t s_g_global_context;
static bool             s_g_context_initialized = false;
static uint64_t         s_g_init_timestamp      = 0;

/* ========== Private Functions ========== */

static void init_health_data(system_health_t* const health)
{
  memset(health, 0, sizeof(system_health_t));
  health->current_state = k_system_state_initializing;
}

static void init_shared_context(shared_context_t* const ctx)
{
  memset(ctx, 0, sizeof(shared_context_t));

  init_health_data(&ctx->health_data);
  ctx->system_state          = k_system_state_initializing;
  ctx->current_temperature_c = STAR_SYSTEM_CONFIG.health.default_temperature_c;
  ctx->temperature_available = false;
  ctx->system_ready          = false;

  for (uint8_t i = 0; i < STAR_SYSTEM_CONFIG.num_hcsr04_sensors; i++) {
    ctx->latest_sensors[i].sensor_index = i;
    ctx->latest_sensors[i].is_valid     = false;
  }
}

/* ========== Public API Implementation ========== */

esp_err_t shared_data_init(void)
{
  if (s_g_context_initialized) {
    ESP_LOGI(s_TAG, "Shared data already initialized");
    return ESP_OK;
  }

  init_shared_context(&s_g_global_context);

  s_g_global_context.sensor_data_queue =
    xQueueCreate(STAR_SYSTEM_CONFIG.freertos.sensor_data_queue_size, sizeof(sensor_data_t));
  if (s_g_global_context.sensor_data_queue == NULL) {
    ESP_LOGE(s_TAG, "Failed to create sensor data queue");
    return ESP_ERR_NO_MEM;
  }

  s_g_global_context.temperature_data_queue =
    xQueueCreate(STAR_SYSTEM_CONFIG.freertos.temperature_queue_size, sizeof(temperature_data_t));
  if (s_g_global_context.temperature_data_queue == NULL) {
    ESP_LOGE(s_TAG, "Failed to create temperature data queue");
    vQueueDelete(s_g_global_context.sensor_data_queue);
    return ESP_ERR_NO_MEM;
  }

  s_g_global_context.health_mutex = xSemaphoreCreateMutex();
  if (s_g_global_context.health_mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create health mutex");
    vQueueDelete(s_g_global_context.sensor_data_queue);
    vQueueDelete(s_g_global_context.temperature_data_queue);
    return ESP_ERR_NO_MEM;
  }

  s_g_global_context.state_mutex = xSemaphoreCreateMutex();
  if (s_g_global_context.state_mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create state mutex");
    vQueueDelete(s_g_global_context.sensor_data_queue);
    vQueueDelete(s_g_global_context.temperature_data_queue);
    vSemaphoreDelete(s_g_global_context.health_mutex);
    return ESP_ERR_NO_MEM;
  }

  s_g_init_timestamp      = esp_timer_get_time();
  s_g_context_initialized = true;

  ESP_LOGI(s_TAG, "Shared data context initialized successfully");
  return ESP_OK;
}

void shared_data_deinit(void)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (s_g_global_context.sensor_data_queue != NULL) {
    vQueueDelete(s_g_global_context.sensor_data_queue);
  }

  if (s_g_global_context.temperature_data_queue != NULL) {
    vQueueDelete(s_g_global_context.temperature_data_queue);
  }

  if (s_g_global_context.health_mutex != NULL) {
    vSemaphoreDelete(s_g_global_context.health_mutex);
  }

  if (s_g_global_context.state_mutex != NULL) {
    vSemaphoreDelete(s_g_global_context.state_mutex);
  }

  memset(&s_g_global_context, 0, sizeof(shared_context_t));
  s_g_context_initialized = false;
  s_g_init_timestamp      = 0;

  ESP_LOGI(s_TAG, "Shared data context deinitialized");
}

shared_context_t* shared_data_get_context(void)
{
  if (!s_g_context_initialized) {
    ESP_LOGE(s_TAG, "Shared data not initialized");
    return NULL;
  }

  /* Add validation checks to catch corruption early */
  if (s_g_global_context.health_mutex == NULL || s_g_global_context.state_mutex == NULL) {
    ESP_LOGE(s_TAG, "Shared context mutexes are NULL - possible corruption");
    return NULL;
  }

  if (s_g_global_context.sensor_data_queue == NULL ||
      s_g_global_context.temperature_data_queue == NULL) {
    ESP_LOGE(s_TAG, "Shared context queues are NULL - possible corruption");
    return NULL;
  }

  return &s_g_global_context;
}

void shared_data_update_health(const uint8_t sensor_index, const bool success)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.health_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    if (sensor_index < STAR_SYSTEM_CONFIG.num_hcsr04_sensors) {
      s_g_global_context.health_data.total_sensor_reads++;
      if (!success) {
        s_g_global_context.health_data.sensor_read_failures[sensor_index]++;
      }
    } else if (sensor_index == STAR_SYSTEM_CONFIG.num_hcsr04_sensors) {
      s_g_global_context.health_data.total_temperature_reads++;
      if (!success) {
        s_g_global_context.health_data.temperature_read_failures++;
      }
    }

    s_g_global_context.health_data.uptime_ms = shared_data_get_uptime_ms();

    xSemaphoreGive(s_g_global_context.health_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire health mutex - update skipped");
  }
}

system_state_t shared_data_get_system_state(void)
{
  if (!s_g_context_initialized) {
    return k_system_state_fault;
  }

  system_state_t state = k_system_state_fault;

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    state = s_g_global_context.system_state;
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in get_system_state");
  }

  return state;
}

void shared_data_set_system_state(const system_state_t state)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    if (s_g_global_context.system_state != state) {
      ESP_LOGI(s_TAG, "System state changed: %d -> %d", s_g_global_context.system_state, state);
      s_g_global_context.system_state              = state;
      s_g_global_context.health_data.current_state = state;
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in set_system_state");
  }
}

bool shared_data_get_temperature(float* const temperature_c)
{
  if (!s_g_context_initialized || temperature_c == NULL) {
    return false;
  }

  bool available = false;

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    *temperature_c = s_g_global_context.current_temperature_c;
    available      = s_g_global_context.temperature_available;
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in get_temperature");
  }

  return available;
}

void shared_data_set_temperature(const float temperature_c, const bool valid)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    if (valid && star_validate_temperature(temperature_c)) {
      s_g_global_context.current_temperature_c = temperature_c;
      s_g_global_context.temperature_available = true;
    } else if (!valid) {
      s_g_global_context.temperature_available = false;
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in set_temperature");
  }
}

bool shared_data_get_sensor_reading(const uint8_t sensor_index, sensor_data_t* const data)
{
  if (!s_g_context_initialized || data == NULL || !star_validate_sensor_index(sensor_index)) {
    return false;
  }

  bool valid = false;

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    *data = s_g_global_context.latest_sensors[sensor_index];
    valid = data->is_valid;
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in get_sensor_reading");
  }

  return valid;
}

void shared_data_set_sensor_reading(const uint8_t sensor_index, const sensor_data_t* const data)
{
  if (!s_g_context_initialized || data == NULL || !star_validate_sensor_index(sensor_index)) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    s_g_global_context.latest_sensors[sensor_index] = *data;
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in set_sensor_reading");
  }
}

sensor_data_t shared_data_create_sensor_data(const uint8_t   sensor_index,
                                             const float     distance_cm,
                                             const esp_err_t result)
{
  const sensor_data_t data = {.sensor_index = sensor_index,
                              .distance_cm  = distance_cm,
                              .read_result  = result,
                              .timestamp_ms = shared_data_get_uptime_ms(),
                              .is_valid =
                                (result == ESP_OK && star_validate_distance(distance_cm))};
  return data;
}

temperature_data_t shared_data_create_temperature_data(const float     temperature_c,
                                                       const float     humidity_percent,
                                                       const esp_err_t result,
                                                       const bool      checksum_valid)
{
  const temperature_data_t data = {
    .temperature_c    = temperature_c,
    .humidity_percent = humidity_percent,
    .read_result      = result,
    .timestamp_ms     = shared_data_get_uptime_ms(),
    .is_valid = (result == ESP_OK && checksum_valid && star_validate_temperature(temperature_c)),
    .checksum_valid = checksum_valid};
  return data;
}

uint32_t shared_data_get_uptime_ms(void)
{
  if (s_g_init_timestamp == 0) {
    return 0;
  }
  return (uint32_t)((esp_timer_get_time() - s_g_init_timestamp) / 1000);
}

bool shared_data_is_system_ready(void)
{
  if (!s_g_context_initialized) {
    return false;
  }

  bool ready = false;
  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    ready = s_g_global_context.system_ready;
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in is_system_ready");
  }

  return ready;
}

void shared_data_mark_system_ready(void)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    s_g_global_context.system_ready = true;
    if (s_g_global_context.system_state == k_system_state_initializing) {
      s_g_global_context.system_state              = k_system_state_running;
      s_g_global_context.health_data.current_state = k_system_state_running;
      ESP_LOGI(s_TAG, "System marked as ready - entering RUNNING state");
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  } else {
    ESP_LOGW(s_TAG, "Failed to acquire state mutex in mark_system_ready");
  }
}