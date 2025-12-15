/* src/main.c - STAR ESP32-S3 Quad Motor Control Application */

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h> /* For memcpy */

/* STAR Framework Core */
#include "star_bus_adc.h"
#include "star_bus_config.h"
#include "star_bus_gpio.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_onewire.h"
#include "star_bus_spi.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

/* Motor Control */
#include "star_encoder.h"
#include "star_motor.h"
#include "star_pid.h"

/* Sensors - DISABLED FOR INITIAL MOTOR CONTROL TESTING */
/* #include "star_bms_bq7850.h" */
/* #include "star_sensor_ds18b20.h" */

static const char* s_TAG = "STAR_MAIN";

/* ========================================================================= */
/*                    ESP32-S3 PIN CONFIGURATION                              */
/* ========================================================================= */

/* Motor Control Pins (4 Motors) */
/* Motor 1 */
static const gpio_num_t s_motor1_pwm_en_pin = GPIO_NUM_6;  /* Enable/Speed PWM */
static const gpio_num_t s_motor1_pwm_ph_pin = GPIO_NUM_7;  /* Phase/Direction */
static const adc_channel_t s_motor1_ipropi_adc = ADC_CHANNEL_0; /* GPIO1 current sense */

/* Motor 2 */
static const gpio_num_t s_motor2_pwm_en_pin = GPIO_NUM_14; /* Enable/Speed PWM */
static const gpio_num_t s_motor2_pwm_ph_pin = GPIO_NUM_15; /* Phase/Direction */
static const adc_channel_t s_motor2_ipropi_adc = ADC_CHANNEL_1; /* GPIO2 current sense */

/* Motor 3 */
static const gpio_num_t s_motor3_pwm_en_pin = GPIO_NUM_16; /* Enable/Speed PWM */
static const gpio_num_t s_motor3_pwm_ph_pin = GPIO_NUM_17; /* Phase/Direction */
static const adc_channel_t s_motor3_ipropi_adc = ADC_CHANNEL_3; /* GPIO4 current sense */

/* Motor 4 */
static const gpio_num_t s_motor4_pwm_en_pin = GPIO_NUM_18; /* Enable/Speed PWM */
static const gpio_num_t s_motor4_pwm_ph_pin = GPIO_NUM_21; /* Phase/Direction */
static const adc_channel_t s_motor4_ipropi_adc = ADC_CHANNEL_4; /* GPIO5 current sense */

/* Encoder Multiplexer Control */
static const gpio_num_t s_encoder_sel0_pin = GPIO_NUM_40; /* Address bit 0 (controls U3 & U4) */
static const gpio_num_t s_encoder_sel1_pin = GPIO_NUM_41; /* Address bit 1 (controls U3 & U4) */
static const gpio_num_t s_encoder_out0_pin = GPIO_NUM_42; /* Selected encoder A signal */
static const gpio_num_t s_encoder_out1_pin = GPIO_NUM_47; /* Selected encoder B signal */

/* RPi5 Communication (SPI3) - Reserved for future use */
__attribute__((unused)) static const gpio_num_t s_rpi_spi_cs_pin   = GPIO_NUM_10; /* Chip Select */
__attribute__((unused)) static const gpio_num_t s_rpi_spi_copi_pin = GPIO_NUM_11; /* MOSI (Controller Out, Peripheral In) */
__attribute__((unused)) static const gpio_num_t s_rpi_spi_sclk_pin = GPIO_NUM_12; /* CLK */
__attribute__((unused)) static const gpio_num_t s_rpi_spi_cipo_pin = GPIO_NUM_13; /* MISO (Controller In, Peripheral Out) */

/* I2C Sensors - Reserved for future use */
__attribute__((unused)) static const gpio_num_t s_i2c_sda_pin      = GPIO_NUM_8;  /* Fuel gauge SDA */
__attribute__((unused)) static const gpio_num_t s_i2c_scl_pin      = GPIO_NUM_9;  /* Fuel gauge SCL */
__attribute__((unused)) static const gpio_num_t s_temp_sensor_pin  = GPIO_NUM_48; /* DS18B20 temperature */

/* ========================================================================= */
/*                    SYSTEM CONFIGURATION CONSTANTS                          */
/* ========================================================================= */

/* System Configuration Constants - Array Sizes (must be macros for C array declarations) */
#define NUM_MOTORS   (4)
#define NUM_MUX_PINS (2)

/* Motor Control Timing */
static const float s_motor_control_dt_sec = 0.004f;
static const uint32_t s_motor_control_period_ms = 4;
static const uint32_t s_encoder_mux_settle_us = 1;

/* Encoder Configuration */
static const uint32_t s_encoder_edges_per_rev = 2000;
static const int32_t s_encoder_high_limit = 10000;
static const int32_t s_encoder_low_limit = -10000;
static const uint32_t s_encoder_filter_cycles = 1000;

/* ADC Configuration */
static const float s_adc_max_value = 4095.0f;
static const float s_adc_vref_volts = 3.3f;

/* DRV8243 Current Sensing */
static const float s_drv8243_ipropi_ratio = 525.0f;
static const float s_drv8243_ipropi_resistor_kohm = 1.5f;
static const float s_motor_overcurrent_ma = 2500.0f;

/* MCPWM Configuration */
static const uint32_t s_mcpwm_timer_resolution_hz = 10000000;
static const uint32_t s_mcpwm_pwm_freq_hz = 20000;
static const uint32_t s_mcpwm_dead_time_ns = 1000;

/* PID Controller Defaults */
static const float s_pid_kp_default = 1.0f;
static const float s_pid_ki_default = 0.5f;
static const float s_pid_kd_default = 0.1f;
static const float s_pid_output_min = -100.0f;
static const float s_pid_output_max = 100.0f;
static const float s_pid_integral_min = -50.0f;
static const float s_pid_integral_max = 50.0f;

/* FreeRTOS Task Configuration */
static const uint32_t s_motor_task_stack_size = 8192;
static const uint32_t s_telemetry_task_stack_size = 4096;
static const uint32_t s_comm_task_stack_size = 4096;
static const uint8_t s_motor_task_priority = 10;
static const uint8_t s_telemetry_task_priority = 5;
static const uint8_t s_comm_task_priority = 7;

/* Telemetry and Communication Timing */
static const uint32_t s_telemetry_period_ms = 100;
static const uint32_t s_comm_period_ms = 10;
static const uint32_t s_heartbeat_period_ms = 1000;
static const uint32_t s_state_mutex_timeout_ms = 100;

/* Error Handler Configuration */
static const uint8_t s_error_max_retries = 3;
static const uint32_t s_error_initial_delay_ms = 100;
static const uint32_t s_error_max_delay_ms = 5000;

/* Unit Conversion */
static const float s_seconds_per_minute = 60.0f;

/* ========================================================================= */
/*                    SYSTEM CONTEXT STRUCTURE                                */
/* ========================================================================= */

/**
 * @brief System context holding all hardware handles and state
 */
typedef struct {
    /* Infrastructure */
    error_handler_t        error_handler;
    star_error_interface_t error_iface;
    star_pin_interface_t   pin_iface;
    star_bus_manager_t     bus_manager;

    /* Motor drivers (using low-level star_motor) */
    star_motor_handle_t motors[NUM_MOTORS];

    /* Single shared encoder handle (multiplexed) */
    star_encoder_handle_t encoder;

    /* PID controllers (one per motor) */
    star_pid_handle_t pid[NUM_MOTORS];

    /* Sensors - DISABLED FOR INITIAL TESTING */
    /* star_ds18b20_handle_t temp_sensor; */
    /* bq7850_handle_t       bms; */

    /* Task handles */
    TaskHandle_t motor_task_handle;
    TaskHandle_t telemetry_task_handle;
    TaskHandle_t comm_task_handle;

    /* Mutex for protecting shared state variables */
    SemaphoreHandle_t state_mutex; /* Protects motor state variables */

    /* Control variables (one per motor) */
    float    setpoint_rpm[NUM_MOTORS];
    float    current_rpm[NUM_MOTORS];
    int32_t  encoder_counts[NUM_MOTORS];       /* Accumulated encoder counts per motor */
    int32_t  encoder_hw_baseline[NUM_MOTORS];  /* Hardware count baseline for each motor */

    /* Motor current readings (mA) */
    float motor_current_ma[NUM_MOTORS];

    /* System state */
    bool    system_enabled;
    uint8_t selected_motor; /* Currently selected motor (0-3) */
} system_context_t;

/* Global system context */
static system_context_t g_sys_ctx;

/* ========================================================================= */
/*                    HELPER FUNCTIONS                                        */
/* ========================================================================= */

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

/* ========================================================================= */
/*                    FREERTOS TASKS                                          */
/* ========================================================================= */

/**
 * @brief Motor control task - Sequential control of 4 motors
 *
 * Processes all 4 motors sequentially within a 4ms cycle (250Hz update rate).
 * Each cycle: select encoder, read position, compute PID, apply PWM, read current
 * for all 4 motors, then wait for next 4ms period.
 *
 * @param pvParameters System context pointer
 */
static void motor_control_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;
    const float       dt  = s_motor_control_dt_sec;

    /* Previous encoder counts for velocity calculation */
    int32_t        prev_counts[NUM_MOTORS]     = {0};
    bool           first_iteration[NUM_MOTORS] = {true, true, true, true};
    TickType_t     last_wake_time = xTaskGetTickCount();
    const uint32_t delay_ticks    = pdMS_TO_TICKS(s_motor_control_period_ms);

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
                int32_t hw_delta = hw_count - ctx->encoder_hw_baseline[motor_idx];
                ctx->encoder_hw_baseline[motor_idx] = hw_count;

                /* Accumulate to motor's position */
                ctx->encoder_counts[motor_idx] += hw_delta;

                /* Calculate velocity from accumulated counts */
                int32_t delta_counts = ctx->encoder_counts[motor_idx] - prev_counts[motor_idx];
                prev_counts[motor_idx] = ctx->encoder_counts[motor_idx];

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

/**
 * @brief Telemetry task - Monitor BMS, temperature, and log motor telemetry
 *
 * Runs at 10Hz to monitor battery state, system temperature, and motor
 * telemetry. Implements thermal protection and battery monitoring.
 *
 * @param pvParameters System context pointer
 */
static void telemetry_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;

    ESP_LOGI(s_TAG, "Telemetry task started");

    while (ctx->system_enabled) {
        /* === Battery Monitoring - DISABLED FOR INITIAL TESTING === */
        /* TODO: Re-enable once BQ7850 BMS is initialized */

        /* === Temperature Monitoring - DISABLED FOR INITIAL TESTING === */
        /* TODO: Re-enable once DS18B20 is initialized */

        /* === Motor Telemetry (All Motors) === */
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
                     "  Motor %d: Setpoint=%.1f RPM, Actual=%.1f RPM, "
                     "Current=%.1f mA",
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

/**
 * @brief Communication task - SPI peripheral communication with RPi5
 *
 * Placeholder for SPI communication protocol. Will handle bidirectional
 * communication with Raspberry Pi 5 for setpoint commands and telemetry.
 *
 * @param pvParameters System context pointer
 */
static void communication_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;

    ESP_LOGI(s_TAG, "Communication task started");

    while (ctx->system_enabled) {
        /* TODO: Implement SPI peripheral protocol */
        /* This task will handle bidirectional communication with RPi5: */
        /* - Receive setpoint commands from RPi5 (4 motor setpoints) */
        /* - Send telemetry data to RPi5 (encoder counts, velocities,
         * currents) */
        /* - Handle emergency stop commands */

        ESP_LOGD(s_TAG, "Communication task running");

        vTaskDelay(pdMS_TO_TICKS(s_comm_period_ms));
    }

    ESP_LOGW(s_TAG, "Communication task exiting");
    vTaskDelete(NULL);
}

/* ========================================================================= */
/*                    MAIN APPLICATION ENTRY POINT                            */
/* ========================================================================= */

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(s_TAG, "=== STAR ESP32-S3 Quad Motor Firmware Starting ===");
    ESP_LOGI(s_TAG, "Version: %s", STAR_FIRMWARE_VERSION);
    ESP_LOGI(s_TAG, "Build: %s %s", __DATE__, __TIME__);

    /* ===================================================================== */
    /* STEP 1: Initialize Error Handler and Pin Validator                   */
    /* ===================================================================== */

    error_handler_init(&g_sys_ctx.error_handler,
                       s_error_max_retries,
                       s_error_initial_delay_ms,
                       s_error_max_delay_ms,
                       NULL,
                       NULL);
    error_handler_get_interface(&g_sys_ctx.error_iface,
                                 &g_sys_ctx.error_handler);
    pin_validator_get_interface(&g_sys_ctx.pin_iface);

    ESP_LOGI(s_TAG, "Error handler and pin validator initialized");

    /* ===================================================================== */
    /* STEP 2: Initialize Bus Manager                                        */
    /* ===================================================================== */

    ret = star_bus_manager_init(&g_sys_ctx.bus_manager,
                                 "main",
                                 &g_sys_ctx.error_iface,
                                 &g_sys_ctx.pin_iface);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(s_TAG, "Bus manager initialized");

    /* ===================================================================== */
    /* STEP 3: Create and Add Buses                                          */
    /* ===================================================================== */

    /* GPIO Bus for encoder multiplexer control */
    gpio_num_t mux_pins[] = {s_encoder_sel0_pin, s_encoder_sel1_pin};
    star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus",
                                                                mux_pins,
                                                                NUM_MUX_PINS);
    ESP_ERROR_CHECK(star_bus_manager_add_bus(&g_sys_ctx.bus_manager, gpio_bus));

    /* ADC Buses for 4-channel current sensing (one bus per channel) */
    star_bus_config_t* adc1_bus = star_bus_config_create_adc("adc1_motor1",
                                                               ADC_UNIT_1,
                                                               s_motor1_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_ERROR_CHECK(star_bus_manager_add_bus(&g_sys_ctx.bus_manager, adc1_bus));

    star_bus_config_t* adc2_bus = star_bus_config_create_adc("adc2_motor2",
                                                               ADC_UNIT_1,
                                                               s_motor2_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_ERROR_CHECK(star_bus_manager_add_bus(&g_sys_ctx.bus_manager, adc2_bus));

    star_bus_config_t* adc3_bus = star_bus_config_create_adc("adc3_motor3",
                                                               ADC_UNIT_1,
                                                               s_motor3_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_ERROR_CHECK(star_bus_manager_add_bus(&g_sys_ctx.bus_manager, adc3_bus));

    star_bus_config_t* adc4_bus = star_bus_config_create_adc("adc4_motor4",
                                                               ADC_UNIT_1,
                                                               s_motor4_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_ERROR_CHECK(star_bus_manager_add_bus(&g_sys_ctx.bus_manager, adc4_bus));

    /* I2C Bus for BMS communication - DISABLED FOR INITIAL TESTING */
    /* TODO: Re-enable once BMS is initialized */

    /* 1-Wire Bus for DS18B20 - DISABLED FOR INITIAL TESTING */
    /* TODO: Re-enable once DS18B20 driver is initialized */

    ESP_LOGI(s_TAG, "All buses created and added to bus manager");

    /* ===================================================================== */
    /* STEP 4: Validate Pins                                                 */
    /* ===================================================================== */

    star_validate_pins();
    ESP_LOGI(s_TAG, "Pin validation passed");

    /* ===================================================================== */
    /* STEP 5: Initialize Multiplexer GPIO Control                           */
    /* ===================================================================== */

    /* GPIO bus handles initialization automatically when added to bus manager */
    /* Set initial multiplexer state to select motor 0 */
    ESP_ERROR_CHECK(star_bus_gpio_write_digital(&g_sys_ctx.bus_manager, "gpio_bus", 0, 0));
    ESP_ERROR_CHECK(star_bus_gpio_write_digital(&g_sys_ctx.bus_manager, "gpio_bus", 1, 0));
    g_sys_ctx.selected_motor = 0;

    ESP_LOGI(s_TAG, "Encoder multiplexer GPIO initialized via bus abstraction");

    /* ===================================================================== */
    /* STEP 6: Initialize 4 Motor Drivers (star_motor)                       */
    /* ===================================================================== */

    const struct {
        gpio_num_t pwm_en;
        gpio_num_t pwm_ph;
        int        mcpwm_group;
    } motor_pins[NUM_MOTORS] = {
        {s_motor1_pwm_en_pin, s_motor1_pwm_ph_pin, 0}, /* Motor 1 - MCPWM Group 0 */
        {s_motor2_pwm_en_pin, s_motor2_pwm_ph_pin, 0}, /* Motor 2 - MCPWM Group 0 */
        {s_motor3_pwm_en_pin, s_motor3_pwm_ph_pin, 1}, /* Motor 3 - MCPWM Group 1 */
        {s_motor4_pwm_en_pin, s_motor4_pwm_ph_pin, 1}, /* Motor 4 - MCPWM Group 1 */
    };

    for (int i = 0; i < NUM_MOTORS; i++) {
        star_motor_config_t motor_cfg = {
            .group_id            = motor_pins[i].mcpwm_group,
            .timer_resolution_hz = s_mcpwm_timer_resolution_hz,
            .pwm_freq_hz         = s_mcpwm_pwm_freq_hz,
            .pin_pwm_a           = motor_pins[i].pwm_en,
            .pin_pwm_b           = motor_pins[i].pwm_ph,
            .dead_time_ns        = s_mcpwm_dead_time_ns,
            .fault_pin           = -1, /* No fault pin (use ADC monitoring) */
            .invert_pwm          = false,
        };
        ESP_ERROR_CHECK(star_motor_init(&g_sys_ctx.motors[i], &motor_cfg));
        ESP_LOGI(s_TAG,
                 "Motor %d initialized (EN=%d, PH=%d, Group=%d)",
                 i + 1,
                 motor_pins[i].pwm_en,
                 motor_pins[i].pwm_ph,
                 motor_pins[i].mcpwm_group);
    }

    /* ===================================================================== */
    /* STEP 7: Initialize Shared Encoder (Multiplexed)                       */
    /* ===================================================================== */

    star_encoder_config_t enc_cfg = {
        .pin_a        = s_encoder_out0_pin, /* GPIO42 - Multiplexed A signal */
        .pin_b        = s_encoder_out1_pin, /* GPIO47 - Multiplexed B signal */
        .filter_value = s_encoder_filter_cycles, /* Glitch filter (APB cycles) */
        .high_limit   = s_encoder_high_limit,
        .low_limit    = s_encoder_low_limit,
    };
    ESP_ERROR_CHECK(star_encoder_init(&g_sys_ctx.encoder, &enc_cfg));
    ESP_LOGI(s_TAG, "Shared encoder initialized (multiplexed)");

    /* ===================================================================== */
    /* STEP 8: Initialize 4 PID Controllers                                  */
    /* ===================================================================== */

    for (int i = 0; i < NUM_MOTORS; i++) {
        star_pid_config_t pid_cfg = {
            .kp           = s_pid_kp_default,
            .ki           = s_pid_ki_default,
            .kd           = s_pid_kd_default,
            .output_min   = s_pid_output_min,
            .output_max   = s_pid_output_max,
            .integral_min = s_pid_integral_min,
            .integral_max = s_pid_integral_max,
        };
        ESP_ERROR_CHECK(star_pid_init(&g_sys_ctx.pid[i], &pid_cfg));
        g_sys_ctx.setpoint_rpm[i] = 0.0f; /* Initialize to stopped */
    }
    ESP_LOGI(s_TAG, "%d PID controllers initialized", NUM_MOTORS);

    /* ===================================================================== */
    /* STEP 9: Initialize Sensors                                            */
    /* ===================================================================== */

    /* DS18B20 Temperature Sensor - DISABLED FOR INITIAL TESTING */
    /* TODO: Re-enable once OneWire bus configuration is verified */
    ESP_LOGI(s_TAG, "DS18B20 temperature sensor disabled for initial testing");

    /* BQ7850 BMS - DISABLED FOR INITIAL TESTING */
    /* TODO: Re-enable once I2C/SMBus communication is verified */
    ESP_LOGI(s_TAG, "BQ7850 BMS disabled for initial testing");

    /* ===================================================================== */
    /* STEP 10: Initialize System State                                      */
    /* ===================================================================== */

    /* Create mutex for protecting shared motor state */
    g_sys_ctx.state_mutex = xSemaphoreCreateMutex();
    if (g_sys_ctx.state_mutex == NULL) {
        ESP_LOGE(s_TAG, "Failed to create state mutex");
        return;
    }

    g_sys_ctx.system_enabled = true;
    g_sys_ctx.selected_motor = 0;

    /* Initialize all motors to stopped */
    for (int i = 0; i < NUM_MOTORS; i++) {
        g_sys_ctx.setpoint_rpm[i]         = 0.0f;
        g_sys_ctx.current_rpm[i]          = 0.0f;
        g_sys_ctx.encoder_counts[i]       = 0;
        g_sys_ctx.encoder_hw_baseline[i]  = 0;
        g_sys_ctx.motor_current_ma[i]     = 0.0f;
    }

    ESP_LOGI(s_TAG, "System state initialized");

    /* ===================================================================== */
    /* STEP 11: Create FreeRTOS Tasks                                        */
    /* ===================================================================== */

    /* Create motor control task (highest priority) */
    BaseType_t task_ret = xTaskCreate(motor_control_task,
                                      "motor_ctrl",
                                      s_motor_task_stack_size,
                                      &g_sys_ctx,
                                      s_motor_task_priority,
                                      &g_sys_ctx.motor_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create motor control task");
        return;
    }

    /* Create telemetry task (medium priority) */
    task_ret = xTaskCreate(telemetry_task,
                           "telemetry",
                           s_telemetry_task_stack_size,
                           &g_sys_ctx,
                           s_telemetry_task_priority,
                           &g_sys_ctx.telemetry_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create telemetry task");
        return;
    }

    /* Create communication task (high priority for responsive commands) */
    task_ret = xTaskCreate(communication_task,
                           "comm",
                           s_comm_task_stack_size,
                           &g_sys_ctx,
                           s_comm_task_priority,
                           &g_sys_ctx.comm_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create communication task");
        return;
    }

    ESP_LOGI(s_TAG, "All tasks created successfully");

    /* ===================================================================== */
    /* STEP 12: Main Loop                                                    */
    /* ===================================================================== */

    ESP_LOGI(s_TAG, "=== System initialization complete ===");
    ESP_LOGI(s_TAG, "Entering main loop...");

    /* Main loop - heartbeat */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(s_heartbeat_period_ms));
        ESP_LOGI(s_TAG, "System running - Heartbeat");
    }
}
