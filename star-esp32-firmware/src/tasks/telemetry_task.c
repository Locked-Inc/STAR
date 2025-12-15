/* src/tasks/telemetry_task.c - Telemetry monitoring task implementation */

#include "telemetry_task.h"

#include "esp_log.h"

#include <string.h>

#include "star_bms_bq7850.h"
/* TODO: Re-enable once DS18B20 and 1-Wire are implemented */
/* #include "star_sensor_ds18b20.h" */

#include "system_config.h"

extern const char* const s_TAG;

/* Telemetry thresholds */
/* TODO: Re-enable thermal protection when DS18B20 is implemented */
/* static const float s_thermal_shutdown_temp_c = 85.0f; */
static const uint8_t s_low_battery_threshold_soc = 10;
static const uint16_t s_cell_imbalance_threshold_mv = 100;

void telemetry_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;

    ESP_LOGI(s_TAG, "Telemetry task started");

    while (ctx->system_enabled) {
        /* === Battery Monitoring (NEW - previously disabled) === */
        bq7850_battery_state_t bms_state;
        if (star_bms_bq7850_read_battery_state(&ctx->bms, &bms_state) == ESP_OK) {
            ESP_LOGI(s_TAG,
                     "Battery: SOC=%d%%, Voltage=%dmV, Current=%dmA, Temp=%.1fC",
                     bms_state.soc.relative_soc,
                     bms_state.current.voltage_mv,
                     bms_state.current.avg_current_ma,
                     bms_state.temps.avg_temp_c / 10.0f);

            /* Check for battery faults */
            if (bms_state.status.fault_active) {
                char status_str[256];
                star_bms_bq7850_status_to_string(&bms_state.status,
                                                  status_str,
                                                  sizeof(status_str));
                ESP_LOGW(s_TAG, "Battery fault: %s", status_str);
            }

            /* Check for critical low battery */
            if (bms_state.soc.relative_soc < s_low_battery_threshold_soc) {
                ESP_LOGW(s_TAG,
                         "Low battery warning: %d%% remaining",
                         bms_state.soc.relative_soc);
            }

            /* Cell imbalance detection */
            uint16_t min_mv = 65535;
            uint16_t max_mv = 0;
            for (int i = 0; i < bms_state.cells.valid_cells; i++) {
                if (bms_state.cells.cell_mv[i] < min_mv) {
                    min_mv = bms_state.cells.cell_mv[i];
                }
                if (bms_state.cells.cell_mv[i] > max_mv) {
                    max_mv = bms_state.cells.cell_mv[i];
                }
            }
            if ((max_mv - min_mv) > s_cell_imbalance_threshold_mv) {
                ESP_LOGW(s_TAG,
                         "Cell imbalance: %d mV (min=%d, max=%d)",
                         max_mv - min_mv,
                         min_mv,
                         max_mv);
            }
        }

        /* === Temperature Monitoring === */
        /* TODO: Re-enable once 1-Wire bus and DS18B20 are implemented */
        /* float temp_c; */
        /* if (star_sensor_ds18b20_read_temp(&ctx->temp_sensor, &temp_c) == ESP_OK) { */
        /*     ... thermal protection code ... */
        /* } */

        /* === Motor Telemetry (All 4 Motors) === */
        /* Copy shared state data under mutex protection */
        float setpoint_copy[NUM_MOTORS];
        float rpm_copy[NUM_MOTORS];
        float current_copy[NUM_MOTORS];

        if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(s_state_mutex_timeout_ms)) == pdTRUE) {
            memcpy(setpoint_copy, ctx->setpoint_rpm, sizeof(setpoint_copy));
            memcpy(rpm_copy, ctx->current_rpm, sizeof(rpm_copy));
            memcpy(current_copy, ctx->motor_current_ma, sizeof(current_copy));
            xSemaphoreGive(ctx->state_mutex);
        } else {
            ESP_LOGW(s_TAG, "Failed to take state mutex in telemetry_task");
            vTaskDelay(pdMS_TO_TICKS(s_telemetry_period_ms));
            continue;
        }

        /* Log telemetry using copied data (outside mutex) */
        ESP_LOGI(s_TAG, "Motor Status:");
        for (int i = 0; i < NUM_MOTORS; i++) {
            ESP_LOGI(s_TAG,
                     "  Motor %d: Setpoint=%.1f RPM, Actual=%.1f RPM, Current=%.1f mA",
                     i + 1,
                     setpoint_copy[i],
                     rpm_copy[i],
                     current_copy[i]);
        }

        vTaskDelay(pdMS_TO_TICKS(s_telemetry_period_ms));
    }

    ESP_LOGW(s_TAG, "Telemetry task exiting");
    vTaskDelete(NULL);
}
