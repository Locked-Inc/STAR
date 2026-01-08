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
#include "motor_config.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
    k_uart_baudrate = 115200, /**< Debug UART baud rate */
} uart_config_t;

typedef enum {
    k_i2c_frequency_hz = 400000, /**< I2C clock frequency (400 kHz) */
    k_riic_channel     = 0,      /**< RIIC channel for BQ4050 */
} i2c_config_t;

typedef enum {
    k_rspi_channel        = 0, /**< RSPI channel for motor drivers */
    k_rspi_mode           = 0, /**< SPI mode 0 (CPOL=0, CPHA=0) */
    k_rspi_use_16bit      = 0, /**< Use 8-bit frames (false) */
} spi_config_t;

typedef enum {
    k_adc_unit       = 0,  /**< ADC unit 0 */
    k_adc_resolution = 12, /**< 12-bit ADC resolution */
} adc_config_t;

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
     * TODO: Call rx_mtu3a_init() for each encoder channel
     */
    rx_log_info(s_tag, "MTU encoders: TODO - implement in Phase 2");

    /* -------------------------------------------------------------------------
     * 6. GPTW PWM (Motor Drivers)
     * -------------------------------------------------------------------------
     * Initialize GPTW channels for 20 kHz PWM generation
     * TODO: Call rx_gptw_init() for each motor channel
     */
    rx_log_info(s_tag, "GPTW PWM: TODO - implement in Phase 2");

    /* -------------------------------------------------------------------------
     * 7. ADC (Current Sensing)
     * -------------------------------------------------------------------------
     * Initialize ADC for motor current sensing (12-bit resolution)
     * TODO: Call adc_init() for each motor current sense channel
     */
    rx_log_info(s_tag, "ADC: TODO - implement in Phase 2");

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
     * TODO: Bus manager init for 1-Wire GPIO bit-bang
     */
    rx_log_info(s_tag, "1-Wire: TODO - implement in Phase 4");

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
     * Initialization Complete
     * -------------------------------------------------------------------------
     */
    rx_log_info(s_tag, "Hardware initialization complete");

    return k_rx_ok;
}
