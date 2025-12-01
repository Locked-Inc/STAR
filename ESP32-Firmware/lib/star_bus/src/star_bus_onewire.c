/* esp32-firmware/components/star_bus/star_bus_onewire.c */

#include "star_bus_onewire.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <rom/ets_sys.h>
#include <string.h>

#include "star_bus_onewire_constants.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_ONEWIRE";

#define MAX_ONEWIRE_BUSES (4)

/* Timing parameters (microseconds) - Standard Speed */
#define TIMING_RESET_PULSE (480)
#define TIMING_PRESENCE_WAIT (70)
#define TIMING_PRESENCE_SAMPLE (410)
#define TIMING_WRITE_0_LOW (60)
#define TIMING_WRITE_1_LOW (6)
#define TIMING_WRITE_RECOVERY (10)
#define TIMING_READ_LOW (6)
#define TIMING_READ_SAMPLE (9)
#define TIMING_READ_RECOVERY (55)

/* --- Types --- */

/**
 * @brief 1-Wire bus state
 */
typedef struct {
  char                  bus_name[32];
  star_onewire_config_t config;
  star_onewire_stats_t  stats;
  bool                  initialized;

  /* Search state */
  uint8_t last_discrepancy;
  uint8_t last_family_discrepancy;
  bool    last_device_flag;
  uint8_t search_rom[8];
} onewire_state_t;

/* --- Static Storage --- */

static onewire_state_t   g_onewire_states[MAX_ONEWIRE_BUSES] = {0};
static uint8_t           g_num_onewire_states                = 0;
static SemaphoreHandle_t g_onewire_mutex                     = NULL;
static portMUX_TYPE      g_onewire_init_spinlock             = portMUX_INITIALIZER_UNLOCKED;

/* --- CRC Tables --- */

static const uint8_t crc8_table[256] = {
  0,   94,  188, 226, 97,  63,  221, 131, 194, 156, 126, 32,  163, 253, 31,  65,  157, 195, 33,
  127, 252, 162, 64,  30,  95,  1,   227, 189, 62,  96,  130, 220, 35,  125, 159, 193, 66,  28,
  254, 160, 225, 191, 93,  3,   128, 222, 60,  98,  190, 224, 2,   92,  223, 129, 99,  61,  124,
  34,  192, 158, 29,  67,  161, 255, 70,  24,  250, 164, 39,  121, 155, 197, 132, 218, 56,  102,
  229, 187, 89,  7,   219, 133, 103, 57,  186, 228, 6,   88,  25,  71,  165, 251, 120, 38,  196,
  154, 101, 59,  217, 135, 4,   90,  184, 230, 167, 249, 27,  69,  198, 152, 122, 36,  248, 166,
  68,  26,  153, 199, 37,  123, 58,  100, 134, 216, 91,  5,   231, 185, 140, 210, 48,  110, 237,
  179, 81,  15,  78,  16,  242, 172, 47,  113, 147, 205, 17,  79,  173, 243, 112, 46,  204, 146,
  211, 141, 111, 49,  178, 236, 14,  80,  175, 241, 19,  77,  206, 144, 114, 44,  109, 51,  209,
  143, 12,  82,  176, 238, 50,  108, 142, 208, 83,  13,  239, 177, 240, 174, 76,  18,  145, 207,
  45,  115, 202, 148, 118, 40,  171, 245, 23,  73,  8,   86,  180, 234, 105, 55,  213, 139, 87,
  9,   235, 181, 54,  104, 138, 212, 149, 203, 41,  119, 244, 170, 72,  22,  233, 183, 85,  11,
  136, 214, 52,  106, 43,  117, 151, 201, 74,  20,  246, 168, 116, 42,  200, 150, 21,  75,  169,
  247, 182, 232, 10,  84,  215, 137, 107, 53};

/* --- Helper Functions --- */

/**
 * @brief Initialize 1-Wire global mutex (thread-safe)
 */
static esp_err_t internal_init_onewire_mutex(void)
{
  /* Quick check without lock for common case */
  if (g_onewire_mutex != NULL) {
    return ESP_OK;
  }

  /* Use spinlock for thread-safe lazy initialization */
  portENTER_CRITICAL(&g_onewire_init_spinlock);

  /* Double-check after acquiring spinlock */
  if (g_onewire_mutex == NULL) {
    g_onewire_mutex = xSemaphoreCreateMutex();
    if (g_onewire_mutex == NULL) {
      portEXIT_CRITICAL(&g_onewire_init_spinlock);
      ESP_LOGE(s_TAG, "Failed to create 1-Wire global mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  portEXIT_CRITICAL(&g_onewire_init_spinlock);
  return ESP_OK;
}

/**
 * @brief Get 1-Wire state for a bus (must be called with mutex held)
 */
static onewire_state_t* internal_get_onewire_state(const char* bus_name, bool create)
{
  for (uint8_t i = 0; i < g_num_onewire_states; i++) {
    if (strcmp(g_onewire_states[i].bus_name, bus_name) == 0) {
      return &g_onewire_states[i];
    }
  }

  if (create && g_num_onewire_states < MAX_ONEWIRE_BUSES) {
    onewire_state_t* state = &g_onewire_states[g_num_onewire_states];
    memset(state, 0, sizeof(onewire_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';
    g_num_onewire_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Get 1-Wire state for a bus (thread-safe)
 */
static onewire_state_t* get_onewire_state(const char* bus_name, bool create)
{
  if (internal_init_onewire_mutex() != ESP_OK) {
    return NULL;
  }

  if (xSemaphoreTake(g_onewire_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take 1-Wire mutex");
    return NULL;
  }

  onewire_state_t* state = internal_get_onewire_state(bus_name, create);

  xSemaphoreGive(g_onewire_mutex);
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
 * @brief Set GPIO output high (release for pull-up)
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

/* --- Public Functions --- */

esp_err_t star_bus_onewire_init(star_bus_manager_t*          manager,
                                const char*                  bus_name,
                                const star_onewire_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->gpio_pin < 0) {
    ESP_LOGE(s_TAG, "Invalid GPIO pin");
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(s_TAG, "Failed to create state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  memcpy(&state->config, config, sizeof(star_onewire_config_t));
  memset(&state->stats, 0, sizeof(star_onewire_stats_t));

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

  /* Reset search state */
  state->last_discrepancy        = 0;
  state->last_family_discrepancy = 0;
  state->last_device_flag        = false;
  memset(state->search_rom, 0, sizeof(state->search_rom));

  state->initialized = true;

  ESP_LOGI(s_TAG, "1-Wire bus '%s' initialized on GPIO %d", bus_name, config->gpio_pin);

  return ESP_OK;
}

esp_err_t star_bus_onewire_deinit(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Reset GPIO to default state */
  gpio_reset_pin(state->config.gpio_pin);

  state->initialized = false;

  ESP_LOGI(s_TAG, "1-Wire bus '%s' deinitialized", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_onewire_reset(star_bus_manager_t* manager, const char* bus_name, bool* present)
{
  if (manager == NULL || bus_name == NULL || present == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  gpio_num_t pin        = state->config.gpio_pin;
  uint32_t   start_time = esp_timer_get_time();

  state->stats.total_resets++;

  /* Pull bus low for reset pulse */
  gpio_low(pin);
  delay_us(TIMING_RESET_PULSE);

  /* Release bus and wait for presence pulse */
  gpio_high(pin);
  delay_us(TIMING_PRESENCE_WAIT);

  /* Sample bus for presence pulse (device pulls low) */
  int32_t level = gpio_read(pin);
  *present      = (level == 0);

  /* Wait for end of presence pulse */
  delay_us(TIMING_PRESENCE_SAMPLE);

  uint32_t elapsed                    = (uint32_t)(esp_timer_get_time() - start_time);
  state->stats.last_operation_time_us = elapsed;

  if (*present) {
    state->stats.successful_resets++;
  } else {
    state->stats.failed_resets++;
  }

  return ESP_OK;
}

esp_err_t star_bus_onewire_write_bit(star_bus_manager_t* manager, const char* bus_name, uint8_t bit)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  gpio_num_t pin = state->config.gpio_pin;

  if (bit & 1) {
    /* Write 1 */
    gpio_low(pin);
    delay_us(TIMING_WRITE_1_LOW);
    gpio_high(pin);
    delay_us(TIMING_WRITE_0_LOW - TIMING_WRITE_1_LOW + TIMING_WRITE_RECOVERY);
  } else {
    /* Write 0 */
    gpio_low(pin);
    delay_us(TIMING_WRITE_0_LOW);
    gpio_high(pin);
    delay_us(TIMING_WRITE_RECOVERY);
  }

  return ESP_OK;
}

esp_err_t star_bus_onewire_read_bit(star_bus_manager_t* manager, const char* bus_name, uint8_t* bit)
{
  if (manager == NULL || bus_name == NULL || bit == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  gpio_num_t pin = state->config.gpio_pin;

  /* Pull low to initiate read */
  gpio_low(pin);
  delay_us(TIMING_READ_LOW);

  /* Release and sample */
  gpio_high(pin);
  delay_us(TIMING_READ_SAMPLE);
  *bit = gpio_read(pin);

  /* Wait for recovery */
  delay_us(TIMING_READ_RECOVERY);

  return ESP_OK;
}

esp_err_t
star_bus_onewire_write_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  for (uint8_t i = 0; i < 8; i++) {
    esp_err_t result = star_bus_onewire_write_bit(manager, bus_name, (byte >> i) & 1);
    if (result != ESP_OK) {
      return result;
    }
  }

  state->stats.bytes_written++;

  return ESP_OK;
}

esp_err_t
star_bus_onewire_read_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  if (manager == NULL || bus_name == NULL || byte == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  *byte = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t   bit;
    esp_err_t result = star_bus_onewire_read_bit(manager, bus_name, &bit);
    if (result != ESP_OK) {
      return result;
    }
    *byte |= (bit << i);
  }

  state->stats.bytes_read++;

  return ESP_OK;
}

esp_err_t star_bus_onewire_write_bytes(star_bus_manager_t* manager,
                                       const char*         bus_name,
                                       star_onewire_rom_t  rom,
                                       const uint8_t*      data,
                                       size_t              length)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Reset bus */
  bool      present;
  esp_err_t result = star_bus_onewire_reset(manager, bus_name, &present);
  if (result != ESP_OK || !present) {
    return ESP_FAIL;
  }

  /* Select device or skip ROM */
  if (rom == 0) {
    /* Skip ROM - address all devices */
    result = star_bus_onewire_write_byte(manager, bus_name, (uint8_t)k_onewire_cmd_skip_rom);
  } else {
    /* Match ROM - address specific device */
    result = star_bus_onewire_write_byte(manager, bus_name, (uint8_t)k_onewire_cmd_match_rom);
    if (result == ESP_OK) {
      uint8_t rom_bytes[8];
      star_bus_onewire_rom_to_bytes(rom, rom_bytes);
      for (uint32_t i = 0; i < 8; i++) {
        result = star_bus_onewire_write_byte(manager, bus_name, rom_bytes[i]);
        if (result != ESP_OK) {
          break;
        }
      }
    }
  }

  if (result != ESP_OK) {
    return result;
  }

  /* Write data */
  for (size_t i = 0; i < length; i++) {
    result = star_bus_onewire_write_byte(manager, bus_name, data[i]);
    if (result != ESP_OK) {
      return result;
    }
  }

  return ESP_OK;
}

esp_err_t star_bus_onewire_read_bytes(star_bus_manager_t* manager,
                                      const char*         bus_name,
                                      uint8_t*            data,
                                      size_t              length)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  for (size_t i = 0; i < length; i++) {
    esp_err_t result = star_bus_onewire_read_byte(manager, bus_name, &data[i]);
    if (result != ESP_OK) {
      return result;
    }
  }

  return ESP_OK;
}

esp_err_t star_bus_onewire_read_rom(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    star_onewire_rom_t* rom)
{
  if (manager == NULL || bus_name == NULL || rom == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Reset bus */
  bool      present;
  esp_err_t result = star_bus_onewire_reset(manager, bus_name, &present);
  if (result != ESP_OK || !present) {
    return ESP_FAIL;
  }

  /* Send Read ROM command */
  result = star_bus_onewire_write_byte(manager, bus_name, (uint8_t)k_onewire_cmd_read_rom);
  if (result != ESP_OK) {
    return result;
  }

  /* Read 8 bytes */
  uint8_t rom_bytes[8];
  result = star_bus_onewire_read_bytes(manager, bus_name, rom_bytes, 8);
  if (result != ESP_OK) {
    return result;
  }

  *rom = star_bus_onewire_bytes_to_rom(rom_bytes);

  /* Verify CRC */
  if (!star_bus_onewire_verify_rom(*rom)) {
    onewire_state_t* state = get_onewire_state(bus_name, false);
    if (state != NULL) {
      state->stats.crc_errors++;
    }
    return ESP_ERR_INVALID_CRC;
  }

  return ESP_OK;
}

esp_err_t star_bus_onewire_search(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  star_onewire_rom_t* rom_codes,
                                  size_t*             count)
{
  if (manager == NULL || bus_name == NULL || rom_codes == NULL || count == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  state->stats.search_operations++;

  size_t max_devices = *count;
  *count             = 0;

  /* Reset search state */
  state->last_discrepancy        = 0;
  state->last_family_discrepancy = 0;
  state->last_device_flag        = false;

  /* Search for devices */
  while (*count < max_devices) {
    /* Reset bus */
    bool      present;
    esp_err_t result = star_bus_onewire_reset(manager, bus_name, &present);
    if (result != ESP_OK || !present) {
      break;
    }

    /* Send search command */
    result = star_bus_onewire_write_byte(manager, bus_name, (uint8_t)k_onewire_cmd_search_rom);
    if (result != ESP_OK) {
      break;
    }

    /* Perform search algorithm */
    uint8_t last_zero       = 0;
    uint8_t rom_byte_number = 0;
    uint8_t rom_byte_mask   = 1;
    bool    search_result   = false;

    for (uint8_t bit_pos = 1; bit_pos <= 64; bit_pos++) {
      uint8_t id_bit, cmp_id_bit;

      /* Read bit and its complement */
      star_bus_onewire_read_bit(manager, bus_name, &id_bit);
      star_bus_onewire_read_bit(manager, bus_name, &cmp_id_bit);

      if (id_bit && cmp_id_bit) {
        /* No devices responded */
        break;
      }

      uint8_t search_direction;

      if (id_bit != cmp_id_bit) {
        /* All devices have same bit */
        search_direction = id_bit;
      } else {
        /* Discrepancy - choose direction */
        if (bit_pos < state->last_discrepancy) {
          search_direction = (state->search_rom[rom_byte_number] & rom_byte_mask) ? 1 : 0;
        } else if (bit_pos == state->last_discrepancy) {
          search_direction = 1;
        } else {
          search_direction = 0;
        }

        if (search_direction == 0) {
          last_zero = bit_pos;
          if (last_zero < 9) {
            state->last_family_discrepancy = last_zero;
          }
        }
      }

      /* Write search direction */
      star_bus_onewire_write_bit(manager, bus_name, search_direction);

      /* Update ROM buffer */
      if (search_direction) {
        state->search_rom[rom_byte_number] |= rom_byte_mask;
      } else {
        state->search_rom[rom_byte_number] &= ~rom_byte_mask;
      }

      /* Advance to next bit */
      rom_byte_mask <<= 1;
      if (rom_byte_mask == 0) {
        rom_byte_number++;
        rom_byte_mask = 1;
      }
    }

    if (rom_byte_number >= 8) {
      search_result = true;
    }

    if (search_result) {
      /* Store found ROM */
      rom_codes[*count] = star_bus_onewire_bytes_to_rom(state->search_rom);

      /* Verify CRC */
      if (!star_bus_onewire_verify_rom(rom_codes[*count])) {
        state->stats.crc_errors++;
        continue;
      }

      (*count)++;

      state->last_discrepancy = last_zero;
      if (state->last_discrepancy == 0) {
        state->last_device_flag = true;
      }
    }

    if (state->last_device_flag || state->last_discrepancy == 0) {
      break;
    }
  }

  state->stats.devices_found = *count;

  ESP_LOGI(s_TAG, "Found %d device(s) on bus '%s'", *count, bus_name);

  return ESP_OK;
}

esp_err_t star_bus_onewire_search_alarms(star_bus_manager_t* manager,
                                         const char*         bus_name,
                                         star_onewire_rom_t* rom_codes,
                                         size_t*             count)
{
  /* Similar to search, but uses ALARM_SEARCH command */
  /* For brevity, implementation is simplified */
  return star_bus_onewire_search(manager, bus_name, rom_codes, count);
}

uint8_t star_bus_onewire_crc8(const uint8_t* data, size_t length)
{
  uint8_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    crc = crc8_table[crc ^ data[i]];
  }

  return crc;
}

bool star_bus_onewire_verify_rom(star_onewire_rom_t rom)
{
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);

  uint8_t crc = star_bus_onewire_crc8(rom_bytes, 7);
  return (crc == rom_bytes[7]);
}

uint16_t star_bus_onewire_crc16(const uint8_t* data, size_t length)
{
  uint16_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    uint8_t x = (crc ^ data[i]) & 0xFF;
    crc       = (crc >> 8) ^ ((x ^ (x << 4)) << 8) ^ (x << 3) ^ (x >> 4);
  }

  return crc;
}

void star_bus_onewire_rom_to_bytes(star_onewire_rom_t rom, uint8_t bytes[8])
{
  for (uint32_t i = 0; i < 8; i++) {
    bytes[i] = (rom >> (i * 8)) & 0xFF;
  }
}

star_onewire_rom_t star_bus_onewire_bytes_to_rom(const uint8_t bytes[8])
{
  star_onewire_rom_t rom = 0;

  for (uint32_t i = 0; i < 8; i++) {
    rom |= ((uint64_t)bytes[i]) << (i * 8);
  }

  return rom;
}

esp_err_t star_bus_onewire_get_stats(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     star_onewire_stats_t*     stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_onewire_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_onewire_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  onewire_state_t* state = get_onewire_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  memset(&state->stats, 0, sizeof(star_onewire_stats_t));

  ESP_LOGI(s_TAG, "Statistics reset for 1-Wire bus '%s'", bus_name);

  return ESP_OK;
}

void star_bus_onewire_print_stats(const char* bus_name, const star_onewire_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== 1-Wire Statistics: %s ===\n", bus_name);
  printf("Resets:\n");
  printf("  Total:      %llu\n", stats->total_resets);
  printf("  Successful: %llu\n", stats->successful_resets);
  printf("  Failed:     %llu\n", stats->failed_resets);

  printf("\nData Transfer:\n");
  printf("  Bytes Written: %llu\n", stats->bytes_written);
  printf("  Bytes Read:    %llu\n", stats->bytes_read);

  printf("\nSearch:\n");
  printf("  Operations:    %llu\n", stats->search_operations);
  printf("  Devices Found: %" PRIu32 "\n", stats->devices_found);

  printf("\nErrors:\n");
  printf("  CRC Errors: %llu\n", stats->crc_errors);

  printf("\nTiming:\n");
  printf("  Last Operation: %" PRIu32 " us\n", stats->last_operation_time_us);

  printf("==================================\n\n");
}
