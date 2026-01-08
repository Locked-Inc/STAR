/* src/hardware_init.c */

/**
 * @file hardware_init.c
 * @brief Hardware initialization implementation
 * @details
 * Centralized hardware initialization for STAR RX72N motor control firmware.
 * All peripherals are initialized in a specific order to ensure proper
 * dependencies.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "hardware_init.h"
#include "hardware.h"
#include "hardware_pinout.h"
#include "motor_config.h"
#include "rx_bus_manager.h"
#include "rx_bus_onewire.h"
#include "rx_gptw.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_mtu_encoder.h"
#include "rx_register_guard.h"
#include "rx_usb.h"

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static const char* s_tag = "hardware_init";

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t hardware_init_all(void)
{
    rx_err_t ret = k_rx_ok;

    /* -------------------------------------------------------------------------
     * 1. System Clocks
     * -------------------------------------------------------------------------
     * Initialize PLL to 240 MHz, PCLKA=120MHz, PCLKB=60MHz
     */
    ret = system_init();
    if (ret != k_rx_ok) {
        /* Cannot log yet - UART not initialized */
        return ret;
    }

    /* -------------------------------------------------------------------------
     * 2. Debug UART
     * -------------------------------------------------------------------------
     * Initialize SCI9 for debug output (115200 baud)
     */
    ret = uart_init();
    if (ret != k_rx_ok) {
        return ret;
    }

    rx_log_info(s_tag, "STAR RX72N Firmware Initialization");
    rx_log_info(s_tag, "System clock: 240 MHz");

    /* -------------------------------------------------------------------------
     * 3. GPIO Ports
     * -------------------------------------------------------------------------
     * Configure all GPIO pins for motor control, sensors, communication
     * Pin configuration is done by individual peripheral init functions
     */
    rx_log_info(s_tag, "GPIO ports initialized");

    /* -------------------------------------------------------------------------
     * 4. Watchdog Timer (IWDT)
     * -------------------------------------------------------------------------
     * Initialize independent watchdog with 1000ms timeout
     * Must be fed every 4ms by Motor_Controller thread (250 Hz)
     */
    ret = rx_iwdt_init(k_iwdt_timeout_ms);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "IWDT init failed");
        return ret;
    }
    rx_log_info(s_tag, "Watchdog initialized (1000ms timeout)");

    /* -------------------------------------------------------------------------
     * 5. MTU Encoders (Quadrature Counting)
     * -------------------------------------------------------------------------
     * Initialize MTU channels 1-4 for encoder quadrature counting
     * 341 PPR with 4x decoding = 1364 counts per revolution
     */
    const rx_encoder_config_t encoder_configs[k_motor_count] = {
        {.channel = k_mtu_channel_1, .counts_per_rev = k_encoder_cpr, .invert_direction = false},
        {.channel = k_mtu_channel_2, .counts_per_rev = k_encoder_cpr, .invert_direction = false},
        {.channel = k_mtu_channel_3, .counts_per_rev = k_encoder_cpr, .invert_direction = false},
        {.channel = k_mtu_channel_4, .counts_per_rev = k_encoder_cpr, .invert_direction = false},
    };

    for (uint8_t i = 0; i < k_motor_count; i++) {
        ret = rx_encoder_init(&encoder_configs[i]);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "MTU encoder init failed");
            return ret;
        }
    }
    rx_log_info(s_tag, "MTU encoders initialized (4 channels, 1364 CPR)");

    /* -------------------------------------------------------------------------
     * 6. GPTW PWM (Motor Drivers)
     * -------------------------------------------------------------------------
     * Initialize GPTW channels for 20 kHz PWM generation
     * 120 MHz / 20 kHz = 6000 discrete duty cycle steps
     */
    const rx_gptw_config_t pwm_config = {
        .frequency_hz         = k_pwm_frequency_hz,
        .deadtime_ns          = 1000,
        .enable_complementary = true,
        .invert_polarity      = false,
    };

    for (uint8_t i = 0; i < k_motor_count; i++) {
        ret = rx_gptw_init_pwm((rx_gptw_channel_t)i, &pwm_config);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "GPTW PWM init failed");
            return ret;
        }
    }
    rx_log_info(s_tag, "GPTW PWM initialized (4 channels, 20 kHz)");

    /* -------------------------------------------------------------------------
     * 7. ADC (Current Sensing)
     * -------------------------------------------------------------------------
     * Initialize ADC for motor current sensing (12-bit resolution)
     * AN000-AN003 for 4 motor drivers (DRV8243S IPROPI outputs)
     */
    const uint8_t adc_channels[k_motor_count] = {
        k_adc_motor0_current,
        k_adc_motor1_current,
        k_adc_motor2_current,
        k_adc_motor3_current,
    };

    for (uint8_t i = 0; i < k_motor_count; i++) {
        ret = adc_init(k_adc_unit, adc_channels[i], k_adc_resolution);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "ADC init failed");
            return ret;
        }
    }
    rx_log_info(s_tag, "ADC initialized (4 channels, 12-bit)");

    /* -------------------------------------------------------------------------
     * 8. SPI (Motor Drivers + RPi5)
     * -------------------------------------------------------------------------
     * Initialize RSPI in peripheral mode for RPi5 communication
     * Also used for DRV8243S motor driver configuration
     */
    ret = rspi_init_peripheral(k_rspi_channel, k_rspi_mode, k_rspi_use_16bit);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "RSPI init failed");
        return ret;
    }
    rx_log_info(s_tag, "SPI initialized (peripheral mode)");

    /* -------------------------------------------------------------------------
     * 9. I2C (Battery Management)
     * -------------------------------------------------------------------------
     * Initialize RIIC for BQ4050 fuel gauge communication
     */
    ret = riic_init(k_riic_channel, k_i2c_frequency_hz);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "RIIC init failed");
        return ret;
    }
    rx_log_info(s_tag, "I2C initialized (400 kHz)");

    /* -------------------------------------------------------------------------
     * 10. 1-Wire (Temperature Sensor)
     * -------------------------------------------------------------------------
     * Initialize 1-Wire bus for DS18B20 temperature monitoring
     * P05 (pin 100) with 4.7kΩ pull-up to 3.3V
     */
    rx_bus_config_t onewire_config = {
        .type = k_rx_bus_onewire,
        .onewire =
            {
                .gpio_pin = k_pin_temp_sensor,
            },
    };

    ret = rx_bus_init(&onewire_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "1-Wire init failed");
        return ret;
    }
    rx_log_info(s_tag, "1-Wire initialized (DS18B20 on P05)");

    /* -------------------------------------------------------------------------
     * 11. USB CDC (Primary Communication)
     * -------------------------------------------------------------------------
     * Initialize USB CDC-ACM for RPi5 communication (primary mode)
     */
    rx_usb_config_t usb_config = {0};
    ret                        = rx_usb_init(&usb_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "USB CDC init failed");
        return ret;
    }
    rx_log_info(s_tag, "USB CDC initialized (primary communication)");

    /* -------------------------------------------------------------------------
     * 12. Register Guard
     * -------------------------------------------------------------------------
     * Capture golden register values for ESD/EMI protection
     * IMPORTANT: Must be called AFTER all peripheral initialization complete
     */
    ret = rx_register_guard_init();
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Register guard init failed");
        return ret;
    }
    rx_log_info(s_tag, "Register guard initialized (ESD/EMI protection)");

    /* -------------------------------------------------------------------------
     * Initialization Complete
     * -------------------------------------------------------------------------
     */
    rx_log_info(s_tag, "Hardware initialization complete");

    return k_rx_ok;
}
