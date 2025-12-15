/* lib/star_bus/src/star_bus_onewire_peripheral.c */

#include "star_bus_onewire_peripheral.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_onewire.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_OW_PERIPH";

/**
 * @brief Maximum response buffer size for OneWire peripheral
 */
#define STAR_ONEWIRE_PERIPH_RESPONSE_BUFFER_SIZE (256)

#define MAX_PERIPHERAL_BUSES (2)

/* --- Types --- */

/**
 * @brief 1-Wire peripheral state
 */
typedef struct {
  char                             bus_name[STAR_BUS_NAME_MAX_LEN]; /* XXX: As stated in other files, this is still hard coded */
  star_onewire_peripheral_config_t config;
  star_onewire_peripheral_stats_t  stats;
  bool                             enabled;
  gpio_num_t                       gpio_pin;
  bool                             alarm_active;

  /* Response buffer for read operations */
  uint8_t response_buffer[STAR_ONEWIRE_PERIPH_RESPONSE_BUFFER_SIZE];
  size_t  response_length;
  uint8_t last_function_cmd;
} peripheral_state_t;

/* --- Static Storage --- */

static peripheral_state_t g_peripheral_states[MAX_PERIPHERAL_BUSES] = {0};
static uint8_t            g_num_peripheral_states                   = 0;

/* --- Helper Functions --- */

/**
 * @brief Get peripheral state for a bus
 */
static peripheral_state_t* get_peripheral_state(const char* bus_name, bool create)
{
  for (uint8_t i = 0; i < g_num_peripheral_states; i++) {
    if (strcmp(g_peripheral_states[i].bus_name, bus_name) == 0) {
      return &g_peripheral_states[i];
    }
  }

  if (create && g_num_peripheral_states < MAX_PERIPHERAL_BUSES) {
    peripheral_state_t* state = &g_peripheral_states[g_num_peripheral_states];
    memset(state, 0, sizeof(peripheral_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';
    g_num_peripheral_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Calculate and set ROM CRC
 */
static star_onewire_rom_t calculate_rom_crc(star_onewire_rom_t rom)
{
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);

  /* Calculate CRC for first 7 bytes */
  uint8_t crc = star_bus_onewire_crc8(rom_bytes, 7);

  /* Set CRC as last byte */
  rom_bytes[7] = crc;

  return star_bus_onewire_bytes_to_rom(rom_bytes);
}

/* --- Public Functions --- */

esp_err_t star_bus_onewire_peripheral_enable(star_bus_manager_t*                     manager,
                                             const char*                             bus_name,
                                             const star_onewire_peripheral_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->on_read == NULL || config->on_write == NULL) {
    ESP_LOGE(s_TAG, "Read and write callbacks are required");
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(s_TAG, "Failed to create peripheral state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  /* Store configuration */
  memcpy(&state->config, config, sizeof(star_onewire_peripheral_config_t));
  memset(&state->stats, 0, sizeof(star_onewire_peripheral_stats_t));

  /* Calculate CRC if requested */
  if (config->auto_calculate_crc) {
    state->config.rom_code = calculate_rom_crc(config->rom_code);
  }

  /* Verify ROM CRC */
  if (!star_bus_onewire_verify_rom(state->config.rom_code)) {
    ESP_LOGW(s_TAG, "ROM code has invalid CRC: 0x%016llX", state->config.rom_code);
  }

  state->enabled      = true;
  state->alarm_active = false;

  ESP_LOGI(s_TAG,
           "1-Wire peripheral enabled on bus '%s' with ROM 0x%016llX",
           bus_name,
           state->config.rom_code);

  ESP_LOGW(s_TAG,
           "Note: 1-Wire peripheral mode requires precise timing and "
           "is best suited for slower bus speeds. Hardware limitations "
           "may affect reliability at standard 1-Wire speeds.");

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_disable(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  state->enabled = false;

  ESP_LOGI(s_TAG, "1-Wire peripheral disabled on bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_is_enabled(const star_bus_manager_t* manager,
                                                 const char*               bus_name,
                                                 bool*                     enabled)
{
  if (manager == NULL || bus_name == NULL || enabled == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL) {
    *enabled = false;
    return ESP_OK;
  }

  *enabled = state->enabled;
  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_set_rom(star_bus_manager_t* manager,
                                              const char*         bus_name,
                                              star_onewire_rom_t  rom_code,
                                              bool                auto_calculate_crc)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  if (auto_calculate_crc) {
    state->config.rom_code = calculate_rom_crc(rom_code);
  } else {
    state->config.rom_code = rom_code;
  }

  ESP_LOGI(s_TAG, "ROM code updated to 0x%016llX", state->config.rom_code);

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_get_rom(const star_bus_manager_t* manager,
                                              const char*               bus_name,
                                              star_onewire_rom_t*       rom_code)
{
  if (manager == NULL || bus_name == NULL || rom_code == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  *rom_code = state->config.rom_code;

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_set_alarm(star_bus_manager_t* manager,
                                                const char*         bus_name,
                                                bool                alarm_active)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  state->alarm_active = alarm_active;

  ESP_LOGI(s_TAG, "Alarm %s for bus '%s'", alarm_active ? "activated" : "deactivated", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_get_stats(const star_bus_manager_t*        manager,
                                                const char*                      bus_name,
                                                star_onewire_peripheral_stats_t* stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_onewire_peripheral_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_onewire_peripheral_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memset(&state->stats, 0, sizeof(star_onewire_peripheral_stats_t));

  ESP_LOGI(s_TAG, "Statistics reset for peripheral bus '%s'", bus_name);

  return ESP_OK;
}

