/* src/tasks/telemetry_task.c - Telemetry monitoring task implementation */

#include "telemetry_task.h"

#include "esp_log.h"

#include <string.h>

#include "star_bms_bq7850.h"
#include "star_motor.h"
#include "star_sensor_ds18b20.h"

#include "system_config.h"

extern const char* const s_TAG;

/* Telemetry thresholds */
static const float s_thermal_shutdown_temp_c = 85.0f;
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

                /* Automatically enable balancing if conditions are safe */
                bool can_balance = true;

                /* Safety check 1: Not under heavy load (current < 1A) */
                if (abs(bms_state.current.avg_current_ma) > 1000) {
                    can_balance = false;
                    ESP_LOGD(s_TAG,
                             "Balancing deferred - high current: %d mA",
                             bms_state.current.avg_current_ma);
                }

                /* Safety check 2: Temperature within safe range (10C to 45C) */
                /* Temperature in 0.1C units: 100 = 10.0C, 450 = 45.0C */
                if (bms_state.temps.avg_temp_c < 100 || bms_state.temps.avg_temp_c > 450) {
                    can_balance = false;
                    ESP_LOGD(s_TAG,
                             "Balancing deferred - temperature out of range: %.1fC",
                             bms_state.temps.avg_temp_c / 10.0f);
                }

                /* Safety check 3: No active faults */
                if (bms_state.status.fault_active) {
                    can_balance = false;
                    ESP_LOGD(s_TAG, "Balancing deferred - BMS fault active");
                }

                /* Enable balancing on all cells if safe */
                if (can_balance) {
                    uint16_t  cell_mask = (1 << bms_state.cells.valid_cells) - 1;
                    esp_err_t bal_ret   = star_bms_bq7850_enable_cell_balancing(&ctx->bms,
                                                                                 cell_mask);
                    if (bal_ret == ESP_OK) {
                        ESP_LOGI(s_TAG, "Cell balancing enabled (mask: 0x%04X)", cell_mask);
                    } else {
                        ESP_LOGW(s_TAG,
                                 "Failed to enable balancing: %s",
                                 esp_err_to_name(bal_ret));
                    }
                }
            } else {
                /* Cells balanced - disable balancing to save power */
                uint16_t active_mask = 0;
                if (star_bms_bq7850_get_balancing_status(&ctx->bms, &active_mask) == ESP_OK) {
                    if (active_mask != 0) {
                        star_bms_bq7850_disable_cell_balancing(&ctx->bms);
                        ESP_LOGI(s_TAG, "Cell balancing disabled - cells balanced");
                    }
                }
            }
        }

        /* === Temperature Monitoring === */
        float temp_c = 0.0f;
        esp_err_t temp_ret = star_sensor_ds18b20_read_temp(&ctx->temp_sensor, &temp_c);
        if (temp_ret == ESP_OK) {
            ESP_LOGI(s_TAG, "Temperature: %.2fC", temp_c);

            /* Thermal protection - emergency stop if temperature critical */
            if (temp_c > s_thermal_shutdown_temp_c) {
                ESP_LOGE(s_TAG,
                         "CRITICAL TEMPERATURE: %.2fC - EMERGENCY STOP",
                         temp_c);
                /* Stop all motors immediately with emergency brake */
                for (int i = 0; i < NUM_MOTORS; i++) {
                    star_motor_stop(&ctx->motors[i], true);
                }
                /* Update shared state to indicate motors stopped */
                if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    for (int i = 0; i < NUM_MOTORS; i++) {
                        ctx->setpoint_rpm[i] = 0.0f;
                    }
                    xSemaphoreGive(ctx->state_mutex);
                }
            }
        } else {
            ESP_LOGW(s_TAG, "Failed to read temperature: %s", esp_err_to_name(temp_ret));
        }

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
