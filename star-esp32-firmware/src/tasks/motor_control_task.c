/* src/tasks/motor_control_task.c - Motor control task implementation */

#include "motor_control_task.h"

#include "esp_log.h"
#include "esp_rom_sys.h"

#include "star_bus_adc.h"
#include "star_bus_gpio.h"

#include "system_config.h"

extern const char* const s_TAG;

/**
 * @brief Select encoder for a specific motor via multiplexer
 *
 * Sets SEL0 and SEL1 pins to select the desired motor's encoder.
 * Waits for multiplexer settling time before returning.
 *
 * Uses GPIO bus abstraction for centralized pin management and conflict detection.
 * Pin mapping: SEL0 = pin_index 0, SEL1 = pin_index 1 in "gpio_bus"
 *
 * @param ctx System context
 * @param motor_index Motor index (0-3)
 */
static void select_encoder_mux(system_context_t* ctx, uint8_t motor_index)
{
    /* Set address lines based on motor index */
    /* Motor 0: SEL1=0, SEL0=0 */
    /* Motor 1: SEL1=0, SEL0=1 */
    /* Motor 2: SEL1=1, SEL0=0 */
    /* Motor 3: SEL1=1, SEL0=1 */

    /* Write to GPIO bus: SEL0 is pin_index 0, SEL1 is pin_index 1 */
    star_bus_gpio_write_digital(&ctx->bus_manager, "gpio_bus", 0, motor_index & 0x01);
    star_bus_gpio_write_digital(&ctx->bus_manager, "gpio_bus", 1, (motor_index >> 1) & 0x01);

    /* Wait for multiplexer settling time (1us for typical 74HC157) */
    esp_rom_delay_us(s_encoder_mux_settle_us);

    ctx->selected_motor = motor_index;
}

void motor_control_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;
    const float       dt  = s_motor_control_dt_sec;

    /* Previous encoder counts for velocity calculation */
    int32_t        prev_counts[NUM_MOTORS]     = {0};
    bool           first_iteration[NUM_MOTORS] = {true, true, true, true};
    TickType_t     last_wake_time              = xTaskGetTickCount();
    const uint32_t delay_ticks                 = pdMS_TO_TICKS(s_motor_control_period_ms);

    ESP_LOGI(s_TAG, "Motor control task started");

    while (ctx->system_enabled) {
        /* Cycle through all motors */
        for (int motor_idx = 0; motor_idx < NUM_MOTORS; motor_idx++) {
            /* === STEP 1: Select encoder via multiplexer === */
            select_encoder_mux(ctx, motor_idx);

            /* === STEP 2: Read encoder position === */
            int32_t hw_count;
            if (star_encoder_get_count(&ctx->encoder, &hw_count) == ESP_OK) {
                /* On first read for this motor, initialize baseline */
                if (first_iteration[motor_idx]) {
                    ctx->encoder_hw_baseline[motor_idx] = hw_count;
                    prev_counts[motor_idx]              = 0;
                    first_iteration[motor_idx]          = false;
                    continue; /* Skip first iteration */
                }

                /* Calculate hardware delta since last time this motor was selected */
                int32_t hw_delta                       = hw_count - ctx->encoder_hw_baseline[motor_idx];
                ctx->encoder_hw_baseline[motor_idx]    = hw_count;

                /* Accumulate to motor's position */
                ctx->encoder_counts[motor_idx] += hw_delta;

                /* Calculate velocity from accumulated counts */
                int32_t delta_counts      = ctx->encoder_counts[motor_idx] - prev_counts[motor_idx];
                prev_counts[motor_idx]    = ctx->encoder_counts[motor_idx];

                /* Convert counts/sample to RPM */
                /* Assuming 500 PPR encoder: 2000 edges per revolution */
                /* RPM = (delta_counts / edges_per_rev) * (1 / dt) * 60 */
                float velocity_rpm = (delta_counts / (float)s_encoder_edges_per_rev) * (1.0f / dt) * s_seconds_per_minute;

                /* Update shared state (protected by mutex) */
                if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                    ctx->current_rpm[motor_idx] = velocity_rpm;
                    xSemaphoreGive(ctx->state_mutex);
                } else {
                    ESP_LOGW(s_TAG, "Failed to take state mutex in motor_control_task");
                }

                /* === STEP 3: Run PID controller === */
                /* Read setpoint from shared state */
                float setpoint;
                if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                    setpoint = ctx->setpoint_rpm[motor_idx];
                    xSemaphoreGive(ctx->state_mutex);
                } else {
                    ESP_LOGW(s_TAG, "Failed to take state mutex for setpoint read");
                    continue;
                }

                float pid_output;
                if (star_pid_compute(&ctx->pid[motor_idx],
                                     setpoint,
                                     velocity_rpm,
                                     dt,
                                     &pid_output)
                    == ESP_OK) {

                    /* === STEP 4: Apply PWM to motor === */
                    star_motor_set_duty(&ctx->motors[motor_idx], pid_output);
                }
            } else {
                /* Encoder read failed */
                ESP_LOGE(s_TAG, "Motor %d: Encoder read failed", motor_idx + 1);

                /* Stop motor for safety */
                star_motor_stop(&ctx->motors[motor_idx], true);

                /* Reset velocity to zero */
                if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                    ctx->current_rpm[motor_idx] = 0.0f;
                    xSemaphoreGive(ctx->state_mutex);
                } else {
                    ESP_LOGW(s_TAG,
                             "Failed to take state mutex for velocity reset on "
                             "encoder error");
                }

                /* Continue to next motor */
                continue;
            }

            /* === STEP 5: Read motor current (ADC) === */
            /* ADC bus names: one per motor */
            const char* adc_bus_names[NUM_MOTORS] = {
                "adc1_motor1", "adc2_motor2", "adc3_motor3", "adc4_motor4"
            };

            int adc_raw;
            if (star_bus_adc_read_raw(&ctx->bus_manager,
                                      adc_bus_names[motor_idx],
                                      &adc_raw)
                == ESP_OK) {
                /* Convert ADC reading to current */
                /* Assuming IPROPI resistor = 1.5k ohms, DRV8243 ratio = 525 */
                /* I_LOAD (mA) = (ADC_voltage * ratio) / (Ripropi) */
                /* ADC_voltage = (adc_raw / adc_max) * vref */
                float adc_voltage = (adc_raw / s_adc_max_value) * s_adc_vref_volts;
                float current_ma  = (adc_voltage * s_drv8243_ipropi_ratio) / s_drv8243_ipropi_resistor_kohm;

                /* Update shared state */
                if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                    ctx->motor_current_ma[motor_idx] = current_ma;
                    xSemaphoreGive(ctx->state_mutex);
                } else {
                    ESP_LOGW(s_TAG, "Failed to take state mutex for current update");
                }

                /* Check for overcurrent */
                if (current_ma > s_motor_overcurrent_ma) {
                    ESP_LOGW(s_TAG,
                             "Motor %d overcurrent: %.1fmA",
                             motor_idx + 1,
                             current_ma);
                    star_motor_stop(&ctx->motors[motor_idx], true);
                    star_pid_reset(&ctx->pid[motor_idx]);

                    /* Update setpoint to stopped */
                    if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                        ctx->setpoint_rpm[motor_idx] = 0.0f;
                        xSemaphoreGive(ctx->state_mutex);
                    } else {
                        ESP_LOGW(s_TAG, "Failed to take state mutex for setpoint reset");
                    }
                }
            }
        }

        /* Wait 4ms before next cycle (gives 250Hz update rate for all motors) */
        vTaskDelayUntil(&last_wake_time, delay_ticks);
    }

    /* Emergency stop all motors on exit */
    ESP_LOGW(s_TAG, "Motor control task exiting - stopping all motors");
    for (int i = 0; i < NUM_MOTORS; i++) {
        star_motor_stop(&ctx->motors[i], true);
    }
    vTaskDelete(NULL);
}
