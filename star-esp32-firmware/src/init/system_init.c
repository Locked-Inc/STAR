/* src/init/system_init.c - System initialization implementation */

#include "system_init.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

#include <string.h>

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_gpio.h"
#include "star_manufacturing.h"
#include "star_pin_validator.h"
#include "star_sensor_ds18b20.h"

#include "system_config.h"

extern const char* const s_TAG;

/**
 * @brief Emergency stop button ISR handler
 *
 * Triggers immediate motor stop and system shutdown when e-stop button pressed.
 * Must be placed in IRAM for fast execution.
 *
 * @param arg Pointer to system_context_t
 */
static void IRAM_ATTR estop_isr_handler(void* arg)
{
    system_context_t* ctx = (system_context_t*)arg;

    /* Disable system to trigger task shutdown */
    ctx->system_enabled = false;

    /* Emergency stop all motors from ISR */
    for (int i = 0; i < NUM_MOTORS; i++) {
        star_motor_stop(&ctx->motors[i], true);
    }
}

esp_err_t system_init(system_context_t* ctx)
{
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, s_TAG, "Context is NULL");

    /* Zero out the context structure */
    memset(ctx, 0, sizeof(system_context_t));

    /* ===================================================================== */
    /* STEP 0.5: Initialize Manufacturing Support                            */
    /* ===================================================================== */

    ESP_RETURN_ON_ERROR(star_mfg_init(), s_TAG, "Failed to initialize manufacturing info");

    /* ===================================================================== */
    /* STEP 1: Initialize Error Handler and Pin Validator                   */
    /* ===================================================================== */

    error_handler_init(&ctx->error_handler,
                       s_error_max_retries,
                       s_error_initial_delay_ms,
                       s_error_max_delay_ms,
                       NULL,
                       NULL);
    error_handler_get_interface(&ctx->error_iface, &ctx->error_handler);
    pin_validator_get_interface(&ctx->pin_iface);

    ESP_LOGI(s_TAG, "Error handler and pin validator initialized");

    /* ===================================================================== */
    /* STEP 1.5: Load Configuration from NVS                                 */
    /* ===================================================================== */

    ESP_RETURN_ON_ERROR(star_config_load(&ctx->config),
                        s_TAG,
                        "Failed to load configuration");

    ESP_LOGI(s_TAG,
             "Configuration loaded: Kp=%.2f Ki=%.2f Kd=%.2f, OC=%.0fmA, Timeout=%lums",
             ctx->config.motor_pid_kp[0],
             ctx->config.motor_pid_ki[0],
             ctx->config.motor_pid_kd[0],
             ctx->config.overcurrent_threshold_ma,
             ctx->config.communication_timeout_ms);

    /* ===================================================================== */
    /* STEP 2: Initialize Bus Manager                                        */
    /* ===================================================================== */

    ESP_RETURN_ON_ERROR(
        star_bus_manager_init(&ctx->bus_manager, "main", &ctx->error_iface, &ctx->pin_iface),
        s_TAG,
        "Failed to init bus manager");

    ESP_LOGI(s_TAG, "Bus manager initialized");

    /* ===================================================================== */
    /* STEP 3: Create and Add Buses                                          */
    /* ===================================================================== */

    /* GPIO Bus for encoder multiplexer control */
    gpio_num_t mux_pins[] = {s_encoder_sel0_pin, s_encoder_sel1_pin};
    star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus",
                                                                mux_pins,
                                                                NUM_MUX_PINS);
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, gpio_bus),
                        s_TAG,
                        "Failed to add GPIO bus");

    /* ADC Buses for 4-channel current sensing (one bus per channel) */
    star_bus_config_t* adc1_bus = star_bus_config_create_adc("adc1_motor1",
                                                               ADC_UNIT_1,
                                                               s_motor1_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, adc1_bus),
                        s_TAG,
                        "Failed to add ADC1 bus");

    star_bus_config_t* adc2_bus = star_bus_config_create_adc("adc2_motor2",
                                                               ADC_UNIT_1,
                                                               s_motor2_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, adc2_bus),
                        s_TAG,
                        "Failed to add ADC2 bus");

    star_bus_config_t* adc3_bus = star_bus_config_create_adc("adc3_motor3",
                                                               ADC_UNIT_1,
                                                               s_motor3_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, adc3_bus),
                        s_TAG,
                        "Failed to add ADC3 bus");

    star_bus_config_t* adc4_bus = star_bus_config_create_adc("adc4_motor4",
                                                               ADC_UNIT_1,
                                                               s_motor4_ipropi_adc,
                                                               ADC_BITWIDTH_12,
                                                               ADC_ATTEN_DB_12);
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, adc4_bus),
                        s_TAG,
                        "Failed to add ADC4 bus");

    /* I2C Bus for BMS communication (NEW - previously commented out) */
    star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
        "i2c_bus",
        I2C_NUM_0,
        BQ7850_DEFAULT_ADDR,
        s_i2c_sda_pin,
        s_i2c_scl_pin,
        100000); /* 100kHz SMBus */
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, i2c_bus),
                        s_TAG,
                        "Failed to add I2C bus");

    /* 1-Wire Bus for DS18B20 temperature sensor */
    star_bus_config_t* onewire_bus = star_bus_config_create_onewire(
        "temp_onewire",
        s_temp_sensor_pin,
        false /* No parasitic power (external VDD) */
    );
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, onewire_bus),
                        s_TAG,
                        "Failed to add OneWire bus");

    /* SPI3 Peripheral for RPi5 Communication */
    star_bus_config_t* rpi_spi = star_bus_config_create_spi_peripheral(
        "rpi_spi",           /* Bus name (matches communication_task.c) */
        SPI3_HOST,           /* ESP32-S3 SPI3 for peripheral mode */
        s_rpi_spi_copi_pin,  /* RPi5 MOSI → ESP32 input */
        s_rpi_spi_cipo_pin,  /* RPi5 MISO ← ESP32 output */
        s_rpi_spi_sclk_pin,  /* RPi5 SCLK */
        s_rpi_spi_cs_pin,    /* RPi5 CS */
        3,                   /* Queue size (up to 3 pending transactions) */
        0                    /* SPI mode 0 (CPOL=0, CPHA=0) */
    );
    ESP_RETURN_ON_ERROR(star_bus_manager_add_bus(&ctx->bus_manager, rpi_spi),
                        s_TAG,
                        "Failed to add RPi5 SPI peripheral");
    ESP_LOGI(s_TAG, "RPi5 SPI peripheral initialized (mode 0, 100 Hz)");

    ESP_LOGI(s_TAG, "All buses created and added to bus manager");

    /* ===================================================================== */
    /* STEP 4: Validate Pins                                                 */
    /* ===================================================================== */

    star_validate_pins();
    ESP_LOGI(s_TAG, "Pin validation passed");

    /* ===================================================================== */
    /* STEP 5: Initialize Multiplexer GPIO Control                           */
    /* ===================================================================== */

    /* Set initial multiplexer state to select motor 0 */
    ESP_RETURN_ON_ERROR(star_bus_gpio_write_digital(&ctx->bus_manager, "gpio_bus", 0, 0),
                        s_TAG,
                        "Failed to init GPIO SEL0");
    ESP_RETURN_ON_ERROR(star_bus_gpio_write_digital(&ctx->bus_manager, "gpio_bus", 1, 0),
                        s_TAG,
                        "Failed to init GPIO SEL1");
    ctx->selected_motor = 0;

    ESP_LOGI(s_TAG, "Encoder multiplexer GPIO initialized");

    /* ===================================================================== */
    /* STEP 5.5: Configure Emergency Stop Button Interrupt                   */
    /* ===================================================================== */

    /* Configure E-Stop GPIO with interrupt */
    gpio_config_t estop_cfg = {
        .pin_bit_mask = (1ULL << s_estop_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,  /* Trigger on button press (high→low) */
    };
    ESP_RETURN_ON_ERROR(gpio_config(&estop_cfg),
                        s_TAG,
                        "Failed to configure E-stop GPIO");

    /* Install ISR service (if not already installed) and add handler */
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(isr_ret, s_TAG, "Failed to install GPIO ISR service");
    }

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(s_estop_pin, estop_isr_handler, (void*)ctx),
                        s_TAG,
                        "Failed to add E-stop ISR handler");

    ESP_LOGI(s_TAG, "Emergency stop button configured on GPIO %d", s_estop_pin);

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
        ESP_RETURN_ON_ERROR(star_motor_init(&ctx->motors[i], &motor_cfg),
                            s_TAG,
                            "Failed to init motor %d",
                            i + 1);
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
    ESP_RETURN_ON_ERROR(star_encoder_init(&ctx->encoder, &enc_cfg),
                        s_TAG,
                        "Failed to init encoder");
    ESP_LOGI(s_TAG, "Shared encoder initialized (multiplexed)");

    /* ===================================================================== */
    /* STEP 8: Initialize 4 PID Controllers                                  */
    /* ===================================================================== */

    for (int i = 0; i < NUM_MOTORS; i++) {
        star_pid_config_t pid_cfg = {
            .kp           = ctx->config.motor_pid_kp[i], /* Load from NVS config */
            .ki           = ctx->config.motor_pid_ki[i],
            .kd           = ctx->config.motor_pid_kd[i],
            .output_min   = s_pid_output_min,   /* Hardware limits (not configurable) */
            .output_max   = s_pid_output_max,
            .integral_min = s_pid_integral_min,
            .integral_max = s_pid_integral_max,
        };
        ESP_RETURN_ON_ERROR(star_pid_init(&ctx->pid[i], &pid_cfg),
                            s_TAG,
                            "Failed to init PID %d",
                            i + 1);
        ctx->setpoint_rpm[i] = 0.0f; /* Initialize to stopped */
        ESP_LOGI(s_TAG,
                 "Motor %d PID initialized: Kp=%.2f Ki=%.2f Kd=%.2f",
                 i + 1,
                 ctx->config.motor_pid_kp[i],
                 ctx->config.motor_pid_ki[i],
                 ctx->config.motor_pid_kd[i]);
    }
    ESP_LOGI(s_TAG, "%d PID controllers initialized from config", NUM_MOTORS);

    /* ===================================================================== */
    /* STEP 9: Initialize Sensors                                            */
    /* ===================================================================== */

    /* DS18B20 Temperature Sensor */
    star_ds18b20_config_t temp_cfg = {
        .bus_manager = &ctx->bus_manager,
        .bus_name    = "temp_onewire",
        .resolution  = k_star_ds18b20_resolution_12_bit,
        .use_rom     = false, /* Single sensor mode */
    };
    ESP_RETURN_ON_ERROR(star_sensor_ds18b20_init(&ctx->temp_sensor, &temp_cfg),
                        s_TAG,
                        "Failed to init DS18B20");
    ESP_LOGI(s_TAG, "DS18B20 temperature sensor initialized");

    /* BQ7850 BMS */
    bq7850_config_t bms_cfg = {
        .num_cells        = 4,
        .num_temp         = 2,
        .smbus_addr       = BQ7850_DEFAULT_ADDR,
        .design_capacity  = 5000,  /* 5000 mAh */
        .design_voltage   = 14800, /* 14.8V nominal */
    };
    ESP_RETURN_ON_ERROR(star_bms_bq7850_init(&ctx->bms,
                                              &ctx->bus_manager,
                                              "i2c_bus",
                                              NULL,
                                              &bms_cfg),
                        s_TAG,
                        "Failed to init BQ7850");
    ESP_LOGI(s_TAG, "BQ7850 BMS initialized");

    /* ===================================================================== */
    /* STEP 10: Initialize System State                                      */
    /* ===================================================================== */

    /* Create mutex for protecting shared motor state */
    ctx->state_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(ctx->state_mutex, ESP_ERR_NO_MEM, s_TAG, "Failed to create mutex");

    /* Initialize system state */
    ctx->current_state = SYSTEM_STATE_ARMED;
    ctx->system_enabled = true;

    /* Initialize degradation tracking */
    ctx->encoder_failed = false;
    ctx->bms_failed = false;
    ctx->temp_sensor_failed = false;
    ctx->fault_count = 0;

    /* Initialize performance monitoring */
    ctx->motor_task_max_time_us = 0;
    ctx->telemetry_task_max_time_us = 0;
    ctx->comm_task_max_time_us = 0;

    /* Initialize all motors to stopped */
    for (int i = 0; i < NUM_MOTORS; i++) {
        ctx->setpoint_rpm[i]        = 0.0f;
        ctx->current_rpm[i]         = 0.0f;
        ctx->encoder_counts[i]      = 0;
        ctx->encoder_hw_baseline[i] = 0;
        ctx->motor_current_ma[i]    = 0.0f;
    }

    ESP_LOGI(s_TAG, "System initialized successfully - State: ARMED");

    return ESP_OK;
}

esp_err_t system_deinit(system_context_t* ctx)
{
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, s_TAG, "Context is NULL");

    /* Cleanup in reverse order of initialization */

    if (ctx->state_mutex) {
        vSemaphoreDelete(ctx->state_mutex);
    }

    star_bms_bq7850_deinit(&ctx->bms);
    star_sensor_ds18b20_deinit(&ctx->temp_sensor);

    for (int i = 0; i < NUM_MOTORS; i++) {
        star_pid_deinit(&ctx->pid[i]);
    }

    star_encoder_deinit(&ctx->encoder);

    for (int i = 0; i < NUM_MOTORS; i++) {
        star_motor_deinit(&ctx->motors[i]);
    }

    /* Remove buses in reverse order of initialization */
    star_bus_manager_remove_bus(&ctx->bus_manager, "rpi_spi");
    star_bus_manager_remove_bus(&ctx->bus_manager, "temp_onewire");
    star_bus_manager_remove_bus(&ctx->bus_manager, "i2c_bus");
    star_bus_manager_remove_bus(&ctx->bus_manager, "adc4_motor4");
    star_bus_manager_remove_bus(&ctx->bus_manager, "adc3_motor3");
    star_bus_manager_remove_bus(&ctx->bus_manager, "adc2_motor2");
    star_bus_manager_remove_bus(&ctx->bus_manager, "adc1_motor1");
    star_bus_manager_remove_bus(&ctx->bus_manager, "gpio_bus");

    star_bus_manager_deinit(&ctx->bus_manager);

    return ESP_OK;
}
