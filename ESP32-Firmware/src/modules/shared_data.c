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

static void init_health_data(system_health_t* health)
{
  memset(health, 0, sizeof(system_health_t));
  health->current_state = SYSTEM_STATE_INITIALIZING;
}

static void init_shared_context(shared_context_t* ctx)
{
  memset(ctx, 0, sizeof(shared_context_t));

  init_health_data(&ctx->health_data);
  ctx->system_state          = SYSTEM_STATE_INITIALIZING;
  ctx->current_temperature_c = DEFAULT_TEMPERATURE_C;
  ctx->temperature_available = false;
  ctx->system_ready          = false;

  for (uint8_t i = 0; i < NUM_HCSR04; i++) {
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
    xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
  if (s_g_global_context.sensor_data_queue == NULL) {
    ESP_LOGE(s_TAG, "Failed to create sensor data queue");
    return ESP_ERR_NO_MEM;
  }

  s_g_global_context.temperature_data_queue =
    xQueueCreate(TEMPERATURE_QUEUE_SIZE, sizeof(temperature_data_t));
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
  
  // Add validation checks to catch corruption early
  if (s_g_global_context.health_mutex == NULL || s_g_global_context.state_mutex == NULL) {
    ESP_LOGE(s_TAG, "Shared context mutexes are NULL - possible corruption");
    return NULL;
  }
  
  if (s_g_global_context.sensor_data_queue == NULL || s_g_global_context.temperature_data_queue == NULL) {
    ESP_LOGE(s_TAG, "Shared context queues are NULL - possible corruption");
    return NULL;
  }
  
  return &s_g_global_context;
}

void shared_data_update_health(uint8_t sensor_index, bool success)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.health_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    if (sensor_index < NUM_HCSR04) {
      s_g_global_context.health_data.total_sensor_reads++;
      if (!success) {
        s_g_global_context.health_data.sensor_read_failures[sensor_index]++;
      }
    } else if (sensor_index == NUM_HCSR04) {
      s_g_global_context.health_data.total_temperature_reads++;
      if (!success) {
        s_g_global_context.health_data.temperature_read_failures++;
      }
    }

    s_g_global_context.health_data.uptime_ms = shared_data_get_uptime_ms();

    xSemaphoreGive(s_g_global_context.health_mutex);
  }
}

system_state_t shared_data_get_system_state(void)
{
  if (!s_g_context_initialized) {
    return SYSTEM_STATE_FAULT;
  }

  system_state_t state = SYSTEM_STATE_FAULT;

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    state = s_g_global_context.system_state;
    xSemaphoreGive(s_g_global_context.state_mutex);
  }

  return state;
}

void shared_data_set_system_state(system_state_t state)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    if (s_g_global_context.system_state != state) {
      ESP_LOGI(s_TAG, "System state changed: %d -> %d", s_g_global_context.system_state, state);
      s_g_global_context.system_state              = state;
      s_g_global_context.health_data.current_state = state;
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  }
}

bool shared_data_get_temperature(float* temperature_c)
{
  if (!s_g_context_initialized || temperature_c == NULL) {
    return false;
  }

  bool available = false;

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    *temperature_c = s_g_global_context.current_temperature_c;
    available      = s_g_global_context.temperature_available;
    xSemaphoreGive(s_g_global_context.state_mutex);
  }

  return available;
}

void shared_data_set_temperature(float temperature_c, bool valid)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    if (valid && VALIDATE_TEMPERATURE(temperature_c)) {
      s_g_global_context.current_temperature_c = temperature_c;
      s_g_global_context.temperature_available = true;
    } else if (!valid) {
      s_g_global_context.temperature_available = false;
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  }
}

bool shared_data_get_sensor_reading(uint8_t sensor_index, sensor_data_t* data)
{
  if (!s_g_context_initialized || data == NULL || !VALIDATE_SENSOR_INDEX(sensor_index)) {
    return false;
  }

  bool valid = false;

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    *data = s_g_global_context.latest_sensors[sensor_index];
    valid = data->is_valid;
    xSemaphoreGive(s_g_global_context.state_mutex);
  }

  return valid;
}

void shared_data_set_sensor_reading(uint8_t sensor_index, const sensor_data_t* data)
{
  if (!s_g_context_initialized || data == NULL || !VALIDATE_SENSOR_INDEX(sensor_index)) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    s_g_global_context.latest_sensors[sensor_index] = *data;
    xSemaphoreGive(s_g_global_context.state_mutex);
  }
}

sensor_data_t
shared_data_create_sensor_data(uint8_t sensor_index, float distance_cm, esp_err_t result)
{
  sensor_data_t data = {.sensor_index = sensor_index,
                        .distance_cm  = distance_cm,
                        .read_result  = result,
                        .timestamp_ms = shared_data_get_uptime_ms(),
                        .is_valid     = (result == ESP_OK && VALIDATE_DISTANCE(distance_cm))};
  return data;
}

temperature_data_t shared_data_create_temperature_data(float     temperature_c,
                                                       float     humidity_percent,
                                                       esp_err_t result,
                                                       bool      checksum_valid)
{
  temperature_data_t data = {
    .temperature_c    = temperature_c,
    .humidity_percent = humidity_percent,
    .read_result      = result,
    .timestamp_ms     = shared_data_get_uptime_ms(),
    .is_valid         = (result == ESP_OK && checksum_valid && VALIDATE_TEMPERATURE(temperature_c)),
    .checksum_valid   = checksum_valid};
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
  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    ready = s_g_global_context.system_ready;
    xSemaphoreGive(s_g_global_context.state_mutex);
  }

  return ready;
}

void shared_data_mark_system_ready(void)
{
  if (!s_g_context_initialized) {
    return;
  }

  if (xSemaphoreTake(s_g_global_context.state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    s_g_global_context.system_ready = true;
    if (s_g_global_context.system_state == SYSTEM_STATE_INITIALIZING) {
      s_g_global_context.system_state              = SYSTEM_STATE_RUNNING;
      s_g_global_context.health_data.current_state = SYSTEM_STATE_RUNNING;
      ESP_LOGI(s_TAG, "System marked as ready - entering RUNNING state");
    }
    xSemaphoreGive(s_g_global_context.state_mutex);
  }
}