/* lib/star_bus/src/star_bus_smbus_peripheral.c */

#include "star_bus_smbus_peripheral.h"

#include <esp_log.h>
#include <string.h>

static const char* s_TAG = "STAR_SMBUS_PERIPH";

#define MAX_PERIPHERAL_BUSES (2)

typedef struct {
  char                           bus_name[32];
  star_smbus_peripheral_config_t config;
  star_smbus_peripheral_stats_t  stats;
  bool                           enabled;
} smbus_peripheral_state_t;

static smbus_peripheral_state_t g_peripheral_states[MAX_PERIPHERAL_BUSES] = {0};
static uint8_t                  g_num_peripheral_states                   = 0;

static smbus_peripheral_state_t* get_peripheral_state(const char* bus_name, bool create)
{
  for (uint8_t i = 0; i < g_num_peripheral_states; i++) {
    if (strcmp(g_peripheral_states[i].bus_name, bus_name) == 0) {
      return &g_peripheral_states[i];
    }
  }

  if (create && g_num_peripheral_states < MAX_PERIPHERAL_BUSES) {
    smbus_peripheral_state_t* state = &g_peripheral_states[g_num_peripheral_states];
    memset(state, 0, sizeof(smbus_peripheral_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';
    g_num_peripheral_states++;
    return state;
  }

  return NULL;
}

esp_err_t star_bus_smbus_peripheral_enable(star_bus_manager_t*                   manager,
                                           const char*                           bus_name,
                                           const star_smbus_peripheral_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, true);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memcpy(&state->config, config, sizeof(star_smbus_peripheral_config_t));
  memset(&state->stats, 0, sizeof(star_smbus_peripheral_stats_t));
  state->enabled = true;

  ESP_LOGI(s_TAG,
           "SMBus peripheral enabled on bus '%s' at address 0x%02X",
           bus_name,
           config->address);

  return ESP_OK;
}

esp_err_t star_bus_smbus_peripheral_disable(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  state->enabled = false;
  ESP_LOGI(s_TAG, "SMBus peripheral disabled on bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_smbus_peripheral_is_enabled(const star_bus_manager_t* manager,
                                               const char*               bus_name,
                                               bool*                     enabled)
{
  if (manager == NULL || bus_name == NULL || enabled == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, false);
  *enabled                        = (state != NULL && state->enabled);

  return ESP_OK;
}

esp_err_t star_bus_smbus_peripheral_send_alert(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  state->stats.alert_requests++;
  ESP_LOGI(s_TAG, "SMBus alert sent for bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_smbus_peripheral_get_stats(const star_bus_manager_t*      manager,
                                              const char*                    bus_name,
                                              star_smbus_peripheral_stats_t* stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_smbus_peripheral_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_smbus_peripheral_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  smbus_peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memset(&state->stats, 0, sizeof(star_smbus_peripheral_stats_t));
  ESP_LOGI(s_TAG, "Statistics reset for SMBus peripheral bus '%s'", bus_name);

  return ESP_OK;
}

void star_bus_smbus_peripheral_print_stats(const char*                          bus_name,
                                           const star_smbus_peripheral_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== SMBus Peripheral Statistics: %s ===\n", bus_name);
  printf("Commands:\n");
  printf("  Quick:        %llu\n", stats->quick_commands);
  printf("  Send Byte:    %llu\n", stats->send_byte_commands);
  printf("  Receive Byte: %llu\n", stats->receive_byte_commands);
  printf("  Read Byte:    %llu\n", stats->read_byte_commands);
  printf("  Write Byte:   %llu\n", stats->write_byte_commands);
  printf("  Read Word:    %llu\n", stats->read_word_commands);
  printf("  Write Word:   %llu\n", stats->write_word_commands);
  printf("  Read Block:   %llu\n", stats->read_block_commands);
  printf("  Write Block:  %llu\n", stats->write_block_commands);
  printf("  Process Call: %llu\n", stats->process_call_commands);
  printf("  Block Process:%llu\n", stats->block_process_commands);

  printf("\nErrors:\n");
  printf("  PEC Errors:      %llu\n", stats->pec_errors);
  printf("  Callback Errors: %llu\n", stats->callback_errors);

  printf("\nAlert:\n");
  printf("  Alert Requests: %llu\n", stats->alert_requests);

  printf("=========================================\n\n");
}
