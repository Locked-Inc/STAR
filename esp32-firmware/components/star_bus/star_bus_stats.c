/* esp32-firmware/components/star_bus/star_bus_stats.c */

#include "star_bus_stats.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* --- Constants --- */

static const char* TAG = "STAR_STATS";

/* --- Types --- */

/**
 * @brief Statistics state for a bus
 */
typedef struct {
  star_bus_stats_t    stats;                 /**< Statistics data */
  star_stats_config_t config;                /**< Statistics configuration */
  bool                enabled;               /**< Whether stats collection is enabled */
  char                bus_name[32];          /**< Bus name */
  uint64_t            operation_time_sum_us; /**< Sum for average calculation */
} stats_state_t;

/* --- Static Storage --- */

#define MAX_STATS_BUSES (8)
static stats_state_t g_stats_states[MAX_STATS_BUSES] = {0};
static uint8_t       g_num_stats_states              = 0;

/* --- Helper Functions --- */

/**
 * @brief Get or create statistics state for a bus
 */
static stats_state_t* get_stats_state(const char* bus_name, bool create)
{
  /* Search for existing state */
  for (uint8_t i = 0; i < g_num_stats_states; i++) {
    if (strcmp(g_stats_states[i].bus_name, bus_name) == 0) {
      return &g_stats_states[i];
    }
  }

  /* Create new state if requested and space available */
  if (create && g_num_stats_states < MAX_STATS_BUSES) {
    stats_state_t* state = &g_stats_states[g_num_stats_states];
    memset(state, 0, sizeof(stats_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->stats.min_operation_time_us = UINT32_MAX;
    state->stats.stats_start_time_ms   = pdTICKS_TO_MS(xTaskGetTickCount());
    g_num_stats_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Update timing statistics
 */
static void update_timing_stats(stats_state_t* state, uint32_t operation_time_us)
{
  if (!state->config.collect_timing) {
    return;
  }

  state->stats.last_operation_time_us = operation_time_us;

  if (operation_time_us < state->stats.min_operation_time_us) {
    state->stats.min_operation_time_us = operation_time_us;
  }

  if (operation_time_us > state->stats.max_operation_time_us) {
    state->stats.max_operation_time_us = operation_time_us;
  }

  state->operation_time_sum_us += operation_time_us;

  if (state->stats.total_operations > 0) {
    state->stats.avg_operation_time_us =
      (uint32_t)(state->operation_time_sum_us / state->stats.total_operations);
  }
}

/* --- Public Functions --- */

esp_err_t star_bus_stats_enable(star_bus_manager_t*        manager,
                                const char*                bus_name,
                                const star_stats_config_t* config)
{
  if (manager == NULL || bus_name == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(TAG, "Failed to create statistics state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  /* Apply configuration */
  if (config != NULL) {
    memcpy(&state->config, config, sizeof(star_stats_config_t));
  } else {
    /* Use defaults */
    star_stats_config_t default_config = STAR_STATS_CONFIG_DEFAULT();
    memcpy(&state->config, &default_config, sizeof(star_stats_config_t));
  }

  state->enabled                   = true;
  state->stats.stats_start_time_ms = pdTICKS_TO_MS(xTaskGetTickCount());

  ESP_LOGI(TAG, "Statistics enabled for bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_stats_disable(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  state->enabled = false;

  ESP_LOGI(TAG, "Statistics disabled for bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t
star_bus_stats_is_enabled(const star_bus_manager_t* manager, const char* bus_name, bool* enabled)
{
  if (manager == NULL || bus_name == NULL || enabled == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, false);
  if (state == NULL) {
    *enabled = false;
    return ESP_OK;
  }

  *enabled = state->enabled;
  return ESP_OK;
}

esp_err_t
star_bus_stats_get(const star_bus_manager_t* manager, const char* bus_name, star_bus_stats_t* stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_bus_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_stats_reset(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Preserve configuration and enabled status */
  star_stats_config_t config  = state->config;
  bool                enabled = state->enabled;
  char                name[32];
  strncpy(name, state->bus_name, sizeof(name));

  /* Reset statistics */
  memset(state, 0, sizeof(stats_state_t));
  state->config  = config;
  state->enabled = enabled;
  strncpy(state->bus_name, name, sizeof(state->bus_name));
  state->stats.min_operation_time_us = UINT32_MAX;
  state->stats.stats_start_time_ms   = pdTICKS_TO_MS(xTaskGetTickCount());
  state->stats.last_reset_time_ms    = pdTICKS_TO_MS(xTaskGetTickCount());

  ESP_LOGI(TAG, "Statistics reset for bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_stats_reset_all(star_bus_manager_t* manager)
{
  if (manager == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  for (uint8_t i = 0; i < g_num_stats_states; i++) {
    star_bus_stats_reset(manager, g_stats_states[i].bus_name);
  }

  ESP_LOGI(TAG, "Statistics reset for all buses");

  return ESP_OK;
}

esp_err_t star_bus_stats_snapshot(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_stats_snapshot_t*    snapshot)
{
  if (manager == NULL || bus_name == NULL || snapshot == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats_state_t* state = get_stats_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(&snapshot->stats, &state->stats, sizeof(star_bus_stats_t));
  snapshot->timestamp_ms = pdTICKS_TO_MS(xTaskGetTickCount());
  strncpy(snapshot->bus_name, bus_name, sizeof(snapshot->bus_name) - 1);

  return ESP_OK;
}

esp_err_t star_bus_stats_diff(const star_bus_manager_t*    manager,
                              const char*                  bus_name,
                              const star_stats_snapshot_t* snapshot,
                              star_bus_stats_t*            diff)
{
  if (manager == NULL || bus_name == NULL || snapshot == NULL || diff == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_stats_t current;
  esp_err_t        result = star_bus_stats_get(manager, bus_name, &current);
  if (result != ESP_OK) {
    return result;
  }

  /* Calculate differences */
  diff->total_operations  = current.total_operations - snapshot->stats.total_operations;
  diff->read_operations   = current.read_operations - snapshot->stats.read_operations;
  diff->write_operations  = current.write_operations - snapshot->stats.write_operations;
  diff->failed_operations = current.failed_operations - snapshot->stats.failed_operations;
  diff->bytes_read        = current.bytes_read - snapshot->stats.bytes_read;
  diff->bytes_written     = current.bytes_written - snapshot->stats.bytes_written;

  /* Timing stats - use current values (not differences) */
  diff->min_operation_time_us = current.min_operation_time_us;
  diff->max_operation_time_us = current.max_operation_time_us;
  diff->avg_operation_time_us = current.avg_operation_time_us;

  /* Error stats */
  diff->timeout_errors = current.timeout_errors - snapshot->stats.timeout_errors;
  diff->nack_errors    = current.nack_errors - snapshot->stats.nack_errors;
  diff->bus_errors     = current.bus_errors - snapshot->stats.bus_errors;
  diff->other_errors   = current.other_errors - snapshot->stats.other_errors;

  /* Utilization */
  diff->total_bus_time_ms = current.total_bus_time_ms - snapshot->stats.total_bus_time_ms;
  diff->idle_time_ms      = current.idle_time_ms - snapshot->stats.idle_time_ms;

  /* Last operation info */
  diff->last_error             = current.last_error;
  diff->last_operation_time_us = current.last_operation_time_us;

  /* Time period */
  uint32_t current_time     = pdTICKS_TO_MS(xTaskGetTickCount());
  diff->stats_start_time_ms = current_time - snapshot->timestamp_ms;

  return ESP_OK;
}

float star_bus_stats_error_rate(const star_bus_stats_t* stats)
{
  if (stats == NULL || stats->total_operations == 0) {
    return 0.0f;
  }

  return (float)stats->failed_operations / (float)stats->total_operations * 100.0f;
}

float star_bus_stats_utilization(const star_bus_stats_t* stats)
{
  if (stats == NULL) {
    return 0.0f;
  }

  uint32_t total_time = stats->total_bus_time_ms + stats->idle_time_ms;
  if (total_time == 0) {
    return 0.0f;
  }

  return (float)stats->total_bus_time_ms / (float)total_time * 100.0f;
}

uint32_t star_bus_stats_throughput(const star_bus_stats_t* stats)
{
  if (stats == NULL || stats->total_bus_time_ms == 0) {
    return 0;
  }

  uint64_t total_bytes  = stats->bytes_read + stats->bytes_written;
  uint32_t time_seconds = stats->total_bus_time_ms / 1000;

  if (time_seconds == 0) {
    return 0;
  }

  return (uint32_t)(total_bytes / time_seconds);
}

uint32_t star_bus_stats_ops_per_second(const star_bus_stats_t* stats)
{
  if (stats == NULL || stats->total_bus_time_ms == 0) {
    return 0;
  }

  uint32_t time_seconds = stats->total_bus_time_ms / 1000;

  if (time_seconds == 0) {
    return 0;
  }

  return (uint32_t)(stats->total_operations / time_seconds);
}

void star_bus_stats_print(const char* bus_name, const star_bus_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== Bus Statistics: %s ===\n", bus_name);
  printf("Operations:\n");
  printf("  Total:  %llu\n", stats->total_operations);
  printf("  Reads:  %llu\n", stats->read_operations);
  printf("  Writes: %llu\n", stats->write_operations);
  printf("  Failed: %llu (%.2f%%)\n", stats->failed_operations, star_bus_stats_error_rate(stats));

  printf("\nData Transfer:\n");
  printf("  Bytes Read:    %llu\n", stats->bytes_read);
  printf("  Bytes Written: %llu\n", stats->bytes_written);
  printf("  Throughput:    %lu bytes/sec\n", star_bus_stats_throughput(stats));

  printf("\nTiming (microseconds):\n");
  printf("  Min: %lu\n", stats->min_operation_time_us);
  printf("  Max: %lu\n", stats->max_operation_time_us);
  printf("  Avg: %lu\n", stats->avg_operation_time_us);

  printf("\nErrors:\n");
  printf("  Timeouts: %lu\n", stats->timeout_errors);
  printf("  NACKs:    %lu\n", stats->nack_errors);
  printf("  Bus:      %lu\n", stats->bus_errors);
  printf("  Other:    %lu\n", stats->other_errors);

  printf("\nUtilization:\n");
  printf("  Active Time: %lu ms\n", stats->total_bus_time_ms);
  printf("  Idle Time:   %lu ms\n", stats->idle_time_ms);
  printf("  Utilization: %.2f%%\n", star_bus_stats_utilization(stats));

  printf("\nLast Operation:\n");
  printf("  Error: %s\n", esp_err_to_name(stats->last_error));
  printf("  Time:  %lu us\n", stats->last_operation_time_us);

  printf("\nStats Collection:\n");
  printf("  Started: %lu ms ago\n",
         pdTICKS_TO_MS(xTaskGetTickCount()) - stats->stats_start_time_ms);
  if (stats->last_reset_time_ms > 0) {
    printf("  Last Reset: %lu ms ago\n",
           pdTICKS_TO_MS(xTaskGetTickCount()) - stats->last_reset_time_ms);
  }
  printf("================================\n\n");
}

void star_bus_stats_print_all(const star_bus_manager_t* manager)
{
  if (manager == NULL) {
    return;
  }

  printf("\n");
  printf("================================================================================\n");
  printf("                          ALL BUS STATISTICS                                    \n");
  printf("================================================================================\n");

  for (uint8_t i = 0; i < g_num_stats_states; i++) {
    if (g_stats_states[i].enabled) {
      star_bus_stats_print(g_stats_states[i].bus_name, &g_stats_states[i].stats);
    }
  }

  printf("================================================================================\n\n");
}

esp_err_t star_bus_stats_to_json(const star_bus_stats_t* stats, char* buffer, size_t buffer_size)
{
  if (stats == NULL || buffer == NULL || buffer_size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  int written = snprintf(buffer,
                         buffer_size,
                         "{"
                         "\"total_operations\":%llu,"
                         "\"read_operations\":%llu,"
                         "\"write_operations\":%llu,"
                         "\"failed_operations\":%llu,"
                         "\"bytes_read\":%llu,"
                         "\"bytes_written\":%llu,"
                         "\"min_time_us\":%lu,"
                         "\"max_time_us\":%lu,"
                         "\"avg_time_us\":%lu,"
                         "\"timeout_errors\":%lu,"
                         "\"nack_errors\":%lu,"
                         "\"bus_errors\":%lu,"
                         "\"other_errors\":%lu,"
                         "\"active_time_ms\":%lu,"
                         "\"idle_time_ms\":%lu,"
                         "\"error_rate\":%.2f,"
                         "\"utilization\":%.2f,"
                         "\"throughput\":%lu,"
                         "\"ops_per_sec\":%lu"
                         "}",
                         stats->total_operations,
                         stats->read_operations,
                         stats->write_operations,
                         stats->failed_operations,
                         stats->bytes_read,
                         stats->bytes_written,
                         stats->min_operation_time_us,
                         stats->max_operation_time_us,
                         stats->avg_operation_time_us,
                         stats->timeout_errors,
                         stats->nack_errors,
                         stats->bus_errors,
                         stats->other_errors,
                         stats->total_bus_time_ms,
                         stats->idle_time_ms,
                         star_bus_stats_error_rate(stats),
                         star_bus_stats_utilization(stats),
                         star_bus_stats_throughput(stats),
                         star_bus_stats_ops_per_second(stats));

  if (written < 0 || (size_t)written >= buffer_size) {
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}
