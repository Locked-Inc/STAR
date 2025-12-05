/* lib/star_bus/src/star_bus_dht22_proprietary.c */

#include "star_bus_dht22_proprietary.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <math.h>
#include <rom/ets_sys.h>
#include <string.h>

/* --- Constants --- */

static const char* s_TAG = "STAR_DHT22";

/* Default mutex timeout in milliseconds to prevent infinite wait deadlocks */
static const uint32_t s_dht22_mutex_timeout_ms = 5000;

/* Timing parameters (microseconds) - use constants for type safety
 * Some constants are defined for documentation purposes and future use.
 * The __attribute__((unused)) suppresses warnings for intentionally unused constants. */
static const uint32_t s_timing_start_low_us __attribute__((unused))     = 18000; /* Start signal low (18ms) */
static const uint32_t s_timing_start_high_us                            = 40;    /* Start signal high */
static const uint32_t s_timing_response_low_us                          = 80;    /* Sensor response low */
static const uint32_t s_timing_response_high_us                         = 80;    /* Sensor response high */
static const uint32_t s_timing_bit_start_us                             = 50;    /* Bit start low */
static const uint32_t s_timing_bit_0_high_us __attribute__((unused))    = 28;    /* Bit 0 high (26-28us) */
static const uint32_t s_timing_bit_1_high_us                            = 70;    /* Bit 1 high (70us) */
static const uint32_t s_timing_bit_threshold_us                         = 40;    /* 0/1 threshold */
static const uint32_t s_timing_timeout_us                               = 100;   /* Signal change timeout */

/* Data format - use constants for type safety */
static const uint32_t s_dht22_data_bits  = 40; /* Total bits: 16 humidity + 16 temp + 8 checksum */
static const uint32_t s_dht22_data_bytes = 5;  /* Total bytes */

/* --- Types --- */

/**
 * @brief DHT22 bus state (internal)
 */
typedef struct {
  char                bus_name[32];
  star_dht22_config_t config;
  star_dht22_stats_t  stats;
  star_dht22_data_t   last_data;
  bool                initialized;
  int64_t             last_read_time_us;
} dht22_state_t;

/* --- Static Storage --- */

static dht22_state_t     g_dht22_states[STAR_DHT22_MAX_BUSES] = {0};
static uint8_t           g_num_dht22_states                   = 0;
static SemaphoreHandle_t g_dht22_mutex                        = NULL;
static portMUX_TYPE      g_dht22_init_spinlock                = portMUX_INITIALIZER_UNLOCKED;

/* --- Helper Functions --- */

/**
 * @brief Initialize DHT22 global mutex (thread-safe)
 */
static esp_err_t internal_init_dht22_mutex(void)
{
  if (g_dht22_mutex != NULL) {
    return ESP_OK;
  }

  portENTER_CRITICAL(&g_dht22_init_spinlock);

  if (g_dht22_mutex == NULL) {
    g_dht22_mutex = xSemaphoreCreateMutex();
    if (g_dht22_mutex == NULL) {
      portEXIT_CRITICAL(&g_dht22_init_spinlock);
      ESP_LOGE(s_TAG, "Failed to create DHT22 global mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  portEXIT_CRITICAL(&g_dht22_init_spinlock);
  return ESP_OK;
}

/**
 * @brief Get DHT22 state for a bus (must be called with mutex held)
 */
static dht22_state_t* internal_get_dht22_state(const char* bus_name, bool create)
{
  for (uint8_t i = 0; i < g_num_dht22_states; i++) {
    if (strcmp(g_dht22_states[i].bus_name, bus_name) == 0) {
      return &g_dht22_states[i];
    }
  }

  if (create && g_num_dht22_states < STAR_DHT22_MAX_BUSES) {
    dht22_state_t* state = &g_dht22_states[g_num_dht22_states];
    memset(state, 0, sizeof(dht22_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';
    g_num_dht22_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Get DHT22 state for a bus (thread-safe)
 */
static dht22_state_t* get_dht22_state(const char* bus_name, bool create)
{
  if (internal_init_dht22_mutex() != ESP_OK) {
    return NULL;
  }

  if (xSemaphoreTake(g_dht22_mutex, pdMS_TO_TICKS(s_dht22_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take DHT22 mutex");
    return NULL;
  }

  dht22_state_t* state = internal_get_dht22_state(bus_name, create);

  xSemaphoreGive(g_dht22_mutex);
  return state;
}

/**
 * @brief Delay in microseconds
 */
static inline void delay_us(uint32_t us)
{
  ets_delay_us(us);
}

/**
 * @brief Set GPIO output low
 */
static inline void gpio_low(gpio_num_t pin)
{
  gpio_set_level(pin, 0);
}

/**
 * @brief Set GPIO output high
 */
static inline void gpio_high(gpio_num_t pin)
{
  gpio_set_level(pin, 1);
}

/**
 * @brief Read GPIO input
 */
static inline int gpio_read(gpio_num_t pin)
{
  return gpio_get_level(pin);
}

/**
 * @brief Set GPIO to output mode
 */
static inline void gpio_set_output(gpio_num_t pin)
{
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

/**
 * @brief Set GPIO to input mode
 */
static inline void gpio_set_input(gpio_num_t pin)
{
  gpio_set_direction(pin, GPIO_MODE_INPUT);
}

/**
 * @brief Wait for GPIO to reach expected level with timeout
 *
 * @param[in] pin           GPIO pin
 * @param[in] expected_level Expected level (0 or 1)
 * @param[in] timeout_us    Timeout in microseconds
 *
 * @return Duration in microseconds, or -1 on timeout
 */
static int32_t wait_for_level(gpio_num_t pin, int expected_level, uint32_t timeout_us)
{
  int64_t  start = esp_timer_get_time();
  int64_t  elapsed;
  uint32_t yield_count = 0;

  while (gpio_read(pin) != expected_level) {
    elapsed = esp_timer_get_time() - start;
    if (elapsed > timeout_us) {
      return -1;
    }

    /* Aggressive yielding to prevent watchdog timeout */
    yield_count++;
    if (yield_count >= 50) { /* Yield every 50 iterations */
      yield_count = 0;
      /* For very short timeouts, use taskYIELD, for longer ones allow a brief delay */
      if (timeout_us < 200) {
        taskYIELD();
      } else {
        vTaskDelay(0); /* Minimal delay but ensures other tasks run */
      }
    }
  }

  return (int32_t)(esp_timer_get_time() - start);
}

/**
 * @brief Wait for GPIO level change and measure duration
 *
 * @param[in] pin           GPIO pin
 * @param[in] current_level Current level to wait for change from
 * @param[in] timeout_us    Timeout in microseconds
 *
 * @return Duration of current level in microseconds, or -1 on timeout
 */
static int32_t measure_pulse(gpio_num_t pin, int current_level, uint32_t timeout_us)
{
  int64_t  start = esp_timer_get_time();
  int64_t  elapsed;
  uint32_t yield_count = 0;

  while (gpio_read(pin) == current_level) {
    elapsed = esp_timer_get_time() - start;
    if (elapsed > timeout_us) {
      return -1;
    }

    /* Aggressive yielding to prevent watchdog timeout */
    yield_count++;
    if (yield_count >= 50) { /* Yield every 50 iterations */
      yield_count = 0;
      /* For very short timeouts, use taskYIELD, for longer ones allow a brief delay */
      if (timeout_us < 200) {
        taskYIELD();
      } else {
        vTaskDelay(0); /* Minimal delay but ensures other tasks run */
      }
    }
  }

  return (int32_t)(esp_timer_get_time() - start);
}

/**
 * @brief Read raw data from DHT22 sensor
 *
 * @param[in]  state    DHT22 state
 * @param[out] data     Array to store 5 bytes of raw data
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t read_raw_data(dht22_state_t* state, uint8_t data[s_dht22_data_bytes])
{
  gpio_num_t pin          = state->config.gpio_pin;
  uint32_t   start_low_us = state->config.start_low_ms * 1000;
  uint32_t   timeout_us   = state->config.read_timeout_ms * 1000;

  /*
   * Step 1: Send start signal
   *
   * The DHT22 protocol requires:
   * 1. Pull data line low for at least 1ms (we use 18ms by default)
   * 2. Pull high for 20-40us
   * 3. Switch to input mode
   *
   * The 18ms delay does NOT need to be in a critical section - only the
   * transition timing between low->high->input is critical. Holding the
   * critical section for 18ms would block interrupts and cause watchdog
   * timeouts.
   */
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  /* Enter critical section only for the GPIO mode change and initial pull low */
  portENTER_CRITICAL(&mux);
  gpio_set_output(pin);
  gpio_low(pin);
  portEXIT_CRITICAL(&mux);

  /* Long delay OUTSIDE critical section - interrupts can fire during this time */
  delay_us(start_low_us);

  /*
   * Enter critical section for the timing-critical transition:
   * Pull high for 20-40us, then switch to input mode.
   * This transition timing is important for sensor to recognize the start signal.
   */
  portENTER_CRITICAL(&mux);
  gpio_high(pin);
  delay_us(s_timing_start_high_us);

  /* Step 2: Switch to input and wait for sensor response */
  gpio_set_input(pin);
  portEXIT_CRITICAL(&mux);

  /* Wait for sensor to pull low (response start) */
  if (wait_for_level(pin, 0, timeout_us) < 0) {
    ESP_LOGD(s_TAG, "No response from sensor (wait for low)");
    return ESP_ERR_TIMEOUT;
  }

  /* Wait for sensor to pull high (response end of low pulse ~80us) */
  if (wait_for_level(pin, 1, s_timing_response_low_us + 50) < 0) {
    ESP_LOGD(s_TAG, "No response from sensor (wait for high)");
    return ESP_ERR_TIMEOUT;
  }

  /* Wait for sensor to pull low (start of first data bit) */
  if (wait_for_level(pin, 0, s_timing_response_high_us + 50) < 0) {
    ESP_LOGD(s_TAG, "No response from sensor (wait for data start)");
    return ESP_ERR_TIMEOUT;
  }

  /* Step 3: Read 40 bits of data */
  memset(data, 0, s_dht22_data_bytes);

  /* Use short critical sections only for the most timing-sensitive parts */
  for (int i = 0; i < s_dht22_data_bits; i++) {
    /* Yield periodically to prevent watchdog */
    if (i % 8 == 0 && i > 0) {
      taskYIELD();
    }

    /* Wait for high (end of bit start pulse ~50us) */
    if (wait_for_level(pin, 1, s_timing_bit_start_us + 30) < 0) {
      ESP_LOGD(s_TAG, "Timeout waiting for bit %d high", i);
      return ESP_ERR_TIMEOUT;
    }

    /* Measure high pulse duration to determine bit value */
    int32_t high_duration = measure_pulse(pin, 1, s_timing_bit_1_high_us + 30);
    if (high_duration < 0) {
      ESP_LOGD(s_TAG, "Timeout measuring bit %d", i);
      return ESP_ERR_TIMEOUT;
    }

    /* Bit is 1 if high duration > threshold, 0 otherwise */
    if (high_duration > s_timing_bit_threshold_us) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

  return ESP_OK;
}

/**
 * @brief Default read operation implementation
 */
static esp_err_t dht22_read_default(const star_bus_config_t* config, star_dht22_data_t* data)
{
  if (config == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* This is called through the bus manager, so we need to find the state */
  dht22_state_t* state = get_dht22_state(config->name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t   raw_data[s_dht22_data_bytes];
  esp_err_t ret = read_raw_data(state, raw_data);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Parse raw data */
  data->raw_humidity    = (raw_data[0] << 8) | raw_data[1];
  data->raw_temperature = (raw_data[2] << 8) | raw_data[3];
  data->checksum        = raw_data[4];

  /* Verify checksum */
  uint8_t calc_checksum =
    star_bus_dht22_calculate_checksum(data->raw_humidity, data->raw_temperature);
  data->checksum_valid = (calc_checksum == data->checksum);

  /* Convert to physical values */
  data->humidity_percent = star_bus_dht22_raw_to_humidity(data->raw_humidity, state->config.model);
  data->temperature_c = star_bus_dht22_raw_to_celsius(data->raw_temperature, state->config.model);
  data->timestamp_us  = esp_timer_get_time();

  return ESP_OK;
}

/* --- Public Functions --- */

esp_err_t star_bus_dht22_init(star_bus_manager_t*        manager,
                              const char*                bus_name,
                              const star_dht22_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->gpio_pin < 0) {
    ESP_LOGE(s_TAG, "Invalid GPIO pin");
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(s_TAG, "Failed to create state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  memcpy(&state->config, config, sizeof(star_dht22_config_t));
  memset(&state->stats, 0, sizeof(star_dht22_stats_t));
  memset(&state->last_data, 0, sizeof(star_dht22_data_t));
  state->last_read_time_us = 0;

  /* Configure GPIO as open-drain output with pull-up */
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << config->gpio_pin),
    .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };

  esp_err_t result = gpio_config(&io_conf);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure GPIO: %s", esp_err_to_name(result));
    return result;
  }

  /* Release bus (high) */
  gpio_high(config->gpio_pin);

  state->initialized = true;

  ESP_LOGI(s_TAG,
           "DHT22 bus '%s' initialized on GPIO %d (model: %d)",
           bus_name,
           config->gpio_pin,
           config->model);

  return ESP_OK;
}

esp_err_t star_bus_dht22_deinit(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Reset GPIO to default state */
  gpio_reset_pin(state->config.gpio_pin);

  state->initialized = false;

  ESP_LOGI(s_TAG, "DHT22 bus '%s' deinitialized", bus_name);

  return ESP_OK;
}

void star_bus_dht22_init_default_ops(star_dht22_ops_t* ops)
{
  if (ops == NULL) {
    ESP_LOGE(s_TAG, "Cannot init default DHT22 ops: ops pointer is NULL");
    return;
  }

  ops->read = dht22_read_default;

  ESP_LOGD(s_TAG, "DHT22 default operations initialized");
}

esp_err_t
star_bus_dht22_read(star_bus_manager_t* manager, const char* bus_name, star_dht22_data_t* data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  int64_t now     = esp_timer_get_time();
  int64_t elapsed = now - state->last_read_time_us;

  /* Check if minimum interval has passed */
  if (state->last_read_time_us > 0 && elapsed < (STAR_DHT22_MIN_INTERVAL_MS * 1000)) {
    /* Return cached data */
    memcpy(data, &state->last_data, sizeof(star_dht22_data_t));
    ESP_LOGD(s_TAG, "Returning cached data (elapsed: %" PRId64 " us)", elapsed);
    return ESP_OK;
  }

  state->stats.total_reads++;
  uint32_t start_time = esp_timer_get_time();

  uint8_t   raw_data[s_dht22_data_bytes];
  esp_err_t ret = read_raw_data(state, raw_data);

  uint32_t read_time             = (uint32_t)(esp_timer_get_time() - start_time);
  state->stats.last_read_time_us = read_time;

  if (ret != ESP_OK) {
    state->stats.failed_reads++;
    if (ret == ESP_ERR_TIMEOUT) {
      state->stats.timeout_errors++;
    } else {
      state->stats.no_response_errors++;
    }
    ESP_LOGD(s_TAG, "Read failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Parse raw data */
  data->raw_humidity    = (raw_data[0] << 8) | raw_data[1];
  data->raw_temperature = (raw_data[2] << 8) | raw_data[3];
  data->checksum        = raw_data[4];

  /* Verify checksum */
  uint8_t calc_checksum =
    star_bus_dht22_calculate_checksum(data->raw_humidity, data->raw_temperature);
  data->checksum_valid = (calc_checksum == data->checksum);

  if (!data->checksum_valid) {
    state->stats.checksum_errors++;
    state->stats.failed_reads++;
    ESP_LOGD(s_TAG,
             "Checksum mismatch: calculated 0x%02X, received 0x%02X",
             calc_checksum,
             data->checksum);
    return ESP_ERR_INVALID_CRC;
  }

  /* Convert to physical values */
  data->humidity_percent = star_bus_dht22_raw_to_humidity(data->raw_humidity, state->config.model);
  data->temperature_c = star_bus_dht22_raw_to_celsius(data->raw_temperature, state->config.model);
  data->timestamp_us  = esp_timer_get_time();

  /* Update state */
  state->stats.successful_reads++;
  state->stats.last_humidity          = data->humidity_percent;
  state->stats.last_temperature       = data->temperature_c;
  state->stats.last_read_timestamp_us = data->timestamp_us;
  state->last_read_time_us            = now;
  memcpy(&state->last_data, data, sizeof(star_dht22_data_t));

  ESP_LOGD(s_TAG,
           "Read successful: T=%.1fC, H=%.1f%%, time=%lu us",
           data->temperature_c,
           data->humidity_percent,
           (unsigned long)read_time);

  return ESP_OK;
}

esp_err_t star_bus_dht22_read_temperature(star_bus_manager_t* manager,
                                          const char*         bus_name,
                                          float*              temperature)
{
  if (temperature == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_dht22_data_t data;
  esp_err_t         ret = star_bus_dht22_read(manager, bus_name, &data);
  if (ret == ESP_OK) {
    *temperature = data.temperature_c;
  }
  return ret;
}

esp_err_t
star_bus_dht22_read_humidity(star_bus_manager_t* manager, const char* bus_name, float* humidity)
{
  if (humidity == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_dht22_data_t data;
  esp_err_t         ret = star_bus_dht22_read(manager, bus_name, &data);
  if (ret == ESP_OK) {
    *humidity = data.humidity_percent;
  }
  return ret;
}

esp_err_t star_bus_dht22_force_read(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    star_dht22_data_t*  data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Reset last read time to force a new read */
  state->last_read_time_us = 0;

  return star_bus_dht22_read(manager, bus_name, data);
}

esp_err_t
star_bus_dht22_check_presence(star_bus_manager_t* manager, const char* bus_name, bool* present)
{
  if (manager == NULL || bus_name == NULL || present == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  gpio_num_t pin          = state->config.gpio_pin;
  uint32_t   start_low_us = state->config.start_low_ms * 1000;

  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  /* Enter critical section only for GPIO mode change and initial pull low */
  portENTER_CRITICAL(&mux);
  gpio_set_output(pin);
  gpio_low(pin);
  portEXIT_CRITICAL(&mux);

  /* Long delay OUTSIDE critical section to avoid watchdog timeout */
  delay_us(start_low_us);

  /* Enter critical section for timing-critical transition */
  portENTER_CRITICAL(&mux);
  gpio_high(pin);
  delay_us(s_timing_start_high_us);

  /* Switch to input and check for response */
  gpio_set_input(pin);
  portEXIT_CRITICAL(&mux);

  /* Wait for sensor to pull low - outside critical section */
  *present = (wait_for_level(pin, 0, s_timing_timeout_us * 10) >= 0);

  /* Release bus */
  gpio_set_output(pin);
  gpio_high(pin);
  gpio_set_input(pin);

  return ESP_OK;
}

esp_err_t star_bus_dht22_get_stats(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   star_dht22_stats_t*       stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_dht22_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_dht22_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  dht22_state_t* state = get_dht22_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  memset(&state->stats, 0, sizeof(star_dht22_stats_t));

  ESP_LOGI(s_TAG, "Statistics reset for DHT22 bus '%s'", bus_name);

  return ESP_OK;
}

void star_bus_dht22_print_stats(const char* bus_name, const star_dht22_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== DHT22 Statistics: %s ===\n", bus_name);
  printf("Reads:\n");
  printf("  Total:      %" PRIu64 "\n", stats->total_reads);
  printf("  Successful: %" PRIu64 "\n", stats->successful_reads);
  printf("  Failed:     %" PRIu64 "\n", stats->failed_reads);

  printf("\nErrors:\n");
  printf("  Checksum:    %" PRIu64 "\n", stats->checksum_errors);
  printf("  Timeout:     %" PRIu64 "\n", stats->timeout_errors);
  printf("  No Response: %" PRIu64 "\n", stats->no_response_errors);

  printf("\nLast Successful Read:\n");
  printf("  Temperature: %.1f C\n", stats->last_temperature);
  printf("  Humidity:    %.1f %%\n", stats->last_humidity);
  printf("  Read Time:   %" PRIu32 " us\n", stats->last_read_time_us);

  if (stats->total_reads > 0) {
    float success_rate = (float)stats->successful_reads / stats->total_reads * 100.0f;
    printf("\nSuccess Rate: %.1f%%\n", success_rate);
  }

  printf("==================================\n\n");
}

/* --- Utility Functions --- */

float star_bus_dht22_raw_to_celsius(uint16_t raw_value, star_dht22_model_t model)
{
  float temperature;

  if (model == k_star_dht22_model_dht11) {
    /* DHT11: integer values only, high byte is whole degrees */
    temperature = (float)(raw_value >> 8);
  } else {
    /* DHT22/AM2302: 0.1 degree resolution, bit 15 is sign */
    bool is_negative = (raw_value & 0x8000) != 0;
    temperature      = (float)(raw_value & 0x7FFF) / 10.0f;
    if (is_negative) {
      temperature = -temperature;
    }
  }

  return temperature;
}

float star_bus_dht22_raw_to_humidity(uint16_t raw_value, star_dht22_model_t model)
{
  float humidity;

  if (model == k_star_dht22_model_dht11) {
    /* DHT11: integer values only, high byte is whole percent */
    humidity = (float)(raw_value >> 8);
  } else {
    /* DHT22/AM2302: 0.1% resolution */
    humidity = (float)raw_value / 10.0f;
  }

  /* Clamp to valid range */
  if (humidity > 100.0f) {
    humidity = 100.0f;
  }
  if (humidity < 0.0f) {
    humidity = 0.0f;
  }

  return humidity;
}

uint8_t star_bus_dht22_calculate_checksum(uint16_t raw_humidity, uint16_t raw_temperature)
{
  return (uint8_t)((raw_humidity >> 8) + (raw_humidity & 0xFF) + (raw_temperature >> 8) +
                   (raw_temperature & 0xFF));
}

float star_bus_dht22_calculate_heat_index(float temperature, float humidity)
{
  /* Convert to Fahrenheit for calculation */
  float t = star_bus_dht22_celsius_to_fahrenheit(temperature);
  float h = humidity;

  /* Simple formula for temperatures below 80F */
  if (t < 80.0f) {
    float hi = 0.5f * (t + 61.0f + ((t - 68.0f) * 1.2f) + (h * 0.094f));
    return (hi - 32.0f) * 5.0f / 9.0f; /* Convert back to Celsius */
  }

  /* Rothfusz regression equation */
  float hi = -42.379f + 2.04901523f * t + 10.14333127f * h - 0.22475541f * t * h -
             0.00683783f * t * t - 0.05481717f * h * h + 0.00122874f * t * t * h +
             0.00085282f * t * h * h - 0.00000199f * t * t * h * h;

  /* Adjustments for low/high humidity */
  if (h < 13.0f && t >= 80.0f && t <= 112.0f) {
    hi -= ((13.0f - h) / 4.0f) * sqrtf((17.0f - fabsf(t - 95.0f)) / 17.0f);
  } else if (h > 85.0f && t >= 80.0f && t <= 87.0f) {
    hi += ((h - 85.0f) / 10.0f) * ((87.0f - t) / 5.0f);
  }

  return (hi - 32.0f) * 5.0f / 9.0f; /* Convert back to Celsius */
}

float star_bus_dht22_calculate_dew_point(float temperature, float humidity)
{
  /* Magnus formula constants for temperature range -40 to +50C */
  const float a = 17.27f;
  const float b = 237.7f;

  float gamma     = (a * temperature) / (b + temperature) + logf(humidity / 100.0f);
  float dew_point = (b * gamma) / (a - gamma);

  return dew_point;
}
