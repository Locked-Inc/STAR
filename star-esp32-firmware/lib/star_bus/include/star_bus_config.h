/* lib/star_bus/include/star_bus_config.h */

#ifndef STAR_COMPONENT_BUS_CONFIG_H
#define STAR_COMPONENT_BUS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h" /* Added for SPI */

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_config.h
 * @brief Bus configuration creation and lifecycle management
 *
 * This module provides factory functions for creating bus configurations for
 * different protocols (I2C, SPI, GPIO, DHT22). Each configuration represents
 * a communication channel that can be added to the bus manager.
 *
 * Key Features:
 * - Factory functions for I2C, SPI, GPIO, and DHT22 configurations
 * - Automatic driver initialization via star_bus_config_init()
 * - Resource cleanup via star_bus_config_destroy()
 * - Support for multiple devices on shared buses
 *
 * Example Usage:
 * @code
 * // === Creating I2C Bus Configuration ===
 *
 * #include "star_bus_config.h"
 * #include "star_bus_manager.h"
 *
 * // Create I2C config for MPU6050 IMU sensor
 * star_bus_config_t* imu_bus = star_bus_config_create_i2c(
 *     "imu_i2c",           // Unique bus name
 *     I2C_NUM_0,           // I2C port (0 or 1)
 *     0x68,                // MPU6050 I2C address
 *     GPIO_NUM_21,         // SDA pin
 *     GPIO_NUM_22,         // SCL pin
 *     400000               // 400kHz fast mode
 * );
 *
 * if (imu_bus == NULL) {
 *     ESP_LOGE(TAG, "Failed to create I2C config");
 *     return;
 * }
 *
 * // Add to bus manager (initializes the I2C driver)
 * star_bus_manager_add_bus(&bus_manager, imu_bus);
 *
 *
 * // === Creating Multiple I2C Devices on Same Port ===
 *
 * // Same I2C port, different device addresses
 * star_bus_config_t* accel_bus = star_bus_config_create_i2c(
 *     "accel_i2c", I2C_NUM_0, 0x1D, GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_config_t* mag_bus = star_bus_config_create_i2c(
 *     "mag_i2c", I2C_NUM_0, 0x1E, GPIO_NUM_21, GPIO_NUM_22, 400000);
 *
 * star_bus_manager_add_bus(&bus_manager, accel_bus);
 * star_bus_manager_add_bus(&bus_manager, mag_bus);
 *
 *
 * // === Creating SPI Device Configuration ===
 *
 * // Configure SPI device interface
 * spi_device_interface_config_t spi_dev_cfg = {
 *     .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz
 *     .mode = 0,                            // SPI mode 0
 *     .spics_io_num = GPIO_NUM_5,           // CS pin
 *     .queue_size = 7,                      // Transaction queue size
 *     .pre_cb = NULL,                       // Pre-transfer callback
 *     .post_cb = NULL,                      // Post-transfer callback
 * };
 *
 * star_bus_config_t* lcd_bus = star_bus_config_create_spi_device(
 *     "lcd_spi",           // Unique bus name
 *     SPI2_HOST,           // SPI host (SPI2 or SPI3)
 *     GPIO_NUM_23,         // COPI pin
 *     GPIO_NUM_19,         // CIPO pin (-1 if not used)
 *     GPIO_NUM_18,         // SCLK pin
 *     SPI_DMA_CH_AUTO,     // DMA channel (auto-select)
 *     &spi_dev_cfg         // Device configuration
 * );
 *
 * star_bus_manager_add_bus(&bus_manager, lcd_bus);
 *
 *
 * // === Creating SPI Peripheral Configuration ===
 *
 * // Configure ESP32 as SPI peripheral device
 * star_bus_config_t* slave_bus = star_bus_config_create_spi_peripheral(
 *     "spi_slave",         // Unique bus name
 *     SPI2_HOST,           // SPI host
 *     GPIO_NUM_23,         // COPI pin
 *     GPIO_NUM_19,         // CIPO pin
 *     GPIO_NUM_18,         // SCLK pin
 *     GPIO_NUM_5,          // CS pin
 *     3,                   // Transaction queue size
 *     0                    // SPI mode 0
 * );
 *
 * star_bus_manager_add_bus(&bus_manager, slave_bus);
 *
 *
 * // === Manual Initialization (Advanced) ===
 *
 * // If not using bus manager, you can manually init/deinit
 * star_bus_config_t* manual_bus = star_bus_config_create_i2c(
 *     "manual_i2c", I2C_NUM_1, 0x50, GPIO_NUM_25, GPIO_NUM_26, 100000);
 *
 * // Initialize the driver
 * esp_err_t ret = star_bus_config_init(manual_bus, &bus_manager);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init bus: %s", esp_err_to_name(ret));
 *     star_bus_config_destroy(manual_bus);
 *     return;
 * }
 *
 * // Use the bus...
 *
 * // Deinitialize driver (but keep config)
 * star_bus_config_deinit(manual_bus);
 *
 * // Destroy config and free memory
 * star_bus_config_destroy(manual_bus);
 *
 *
 * // === Cleanup ===
 *
 * // Remove bus from manager (deinitializes driver)
 * star_bus_manager_remove_bus(&bus_manager, "imu_i2c");
 *
 * // Destroy the configuration (frees memory)
 * star_bus_config_destroy(imu_bus);
 * @endcode
 */

/* --- Forward declaration needed for the function signature --- */
struct star_bus_manager;

/* --- Public Functions --- */

/**
 * @brief Create a new I2C bus/device configuration.
 *
 * Configures parameters for a specific I2C device communication.
 * The underlying I2C driver/port will be initialized when star_bus_config_init is called.
 *
 * @param[in] name    Unique name for this bus/device instance (e.g., "I2C_SensorA"). Must be non-NULL.
 * @param[in] port    I2C port number (I2C_NUM_0 or I2C_NUM_1).
 * @param[in] address 7-bit I2C device address.
 * @param[in] sda_pin GPIO number for SDA line.
 * @param[in] scl_pin GPIO number for SCL line.
 * @param[in] clk_speed Clock speed in Hz (e.g., 100000 for 100kHz).
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t* star_bus_config_create_i2c(const char* name,
                                              i2c_port_t  port,
                                              uint8_t     address,
                                              gpio_num_t  sda_pin,
                                              gpio_num_t  scl_pin,
                                              uint32_t    clk_speed);

/**
 * @brief Create a new SPI device configuration.
 *
 * Configures parameters for a specific SPI device communication.
 * The underlying SPI bus driver will be initialized when star_bus_config_init is called
 * for the first device on a given SPI host.
 *
 * @param[in] name        Unique name for this SPI device instance (e.g., "SPI_LCD"). Must be non-NULL.
 * @param[in] host        SPI host device (e.g., SPI2_HOST, SPI3_HOST).
 * @param[in] copi_pin    GPIO number for COPI (Controller Out, Peripheral In) line.
 * @param[in] cipo_pin    GPIO number for CIPO (Controller In, Peripheral Out) line (-1 if not used).
 * @param[in] sclk_pin    GPIO number for SCLK line.
 * @param[in] dma_chan    SPI DMA channel to use (SPI_DMA_CH_AUTO, SPI_DMA_CH1, SPI_DMA_CH2, or 0 if DMA disabled).
 * @param[in] dev_cfg     Pointer to the ESP-IDF SPI device interface configuration structure.
 *                        This structure contains CS pin, clock speed, mode, queue size, callbacks, etc.
 *                        The bus manager does NOT take ownership of this pointer or its contents;
 *                        it copies the relevant data. The caller must ensure dev_cfg is valid.
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t* star_bus_config_create_spi_device(const char*                          name,
                                                     spi_host_device_t                    host,
                                                     gpio_num_t                           copi_pin,
                                                     gpio_num_t                           cipo_pin,
                                                     gpio_num_t                           sclk_pin,
                                                     int32_t                              dma_chan,
                                                     const spi_device_interface_config_t* dev_cfg);

/**
 * @brief Destroy a bus configuration and free all associated resources.
 *
 * This function will first attempt to deinitialize the bus/device driver if it was initialized
 * by this configuration (e.g., i2c_driver_delete, spi_bus_remove_device).
 * For SPI, it will also attempt to free the underlying SPI bus if this was the last device on it.
 * Then, it frees the memory allocated for the configuration structure itself.
 *
 * @param[in] config Pointer to the bus configuration to destroy. This pointer will be invalid after the call.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if config is NULL, or an error code from the underlying deinitialization process.
 */
esp_err_t star_bus_config_destroy(star_bus_config_t* config);

/**
 * @brief Initialize the bus/device associated with this configuration.
 *
 * For I2C: Performs i2c_param_config, i2c_driver_install.
 * For SPI: Performs spi_bus_initialize (if not already done for this host) and spi_bus_add_device.
 *          Requires the bus manager to track host initialization status.
 *
 * @param[in] config Pointer to the bus configuration to initialize.
 * @param[in] manager Pointer to the bus manager (needed for SPI host tracking). Must not be NULL.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if config or manager is NULL, ESP_ERR_INVALID_STATE if already initialized, or an error code from the underlying driver initialization.
 */
esp_err_t star_bus_config_init(star_bus_config_t* config, struct star_bus_manager* manager);

/**
 * @brief Create a new SPI peripheral (slave) configuration.
 *
 * Configures parameters for operating as an SPI peripheral device.
 * The peripheral will respond to transactions initiated by an external SPI controller.
 * The underlying SPI slave driver will be initialized when star_bus_config_init is called.
 *
 * @param[in] name        Unique name for this SPI peripheral instance (e.g., "SPI_Peripheral"). Must be non-NULL.
 * @param[in] host        SPI host device (e.g., SPI2_HOST, SPI3_HOST).
 * @param[in] copi_pin    GPIO number for COPI (Controller Out, Peripheral In) line.
 * @param[in] cipo_pin    GPIO number for CIPO (Controller In, Peripheral Out) line.
 * @param[in] sclk_pin    GPIO number for SCLK line.
 * @param[in] cs_pin      GPIO number for CS (Chip Select) line.
 * @param[in] queue_size  Transaction queue size (typically 1-3).
 * @param[in] mode        SPI mode (0-3), determines clock polarity and phase.
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t* star_bus_config_create_spi_peripheral(const char*       name,
                                                         spi_host_device_t host,
                                                         gpio_num_t        copi_pin,
                                                         gpio_num_t        cipo_pin,
                                                         gpio_num_t        sclk_pin,
                                                         gpio_num_t        cs_pin,
                                                         int32_t           queue_size,
                                                         uint8_t           mode);

/**
 * @brief Create a GPIO bus configuration.
 *
 * Creates a configuration for a GPIO bus to control multiple digital I/O pins.
 * GPIO bus is used for simple digital control such as multiplexer select lines.
 *
 * @param[in] name      Unique name for this GPIO bus (e.g., "mux_gpio"). Must be non-NULL.
 * @param[in] pins      Array of GPIO pins to control. Must be non-NULL.
 * @param[in] pin_count Number of pins in the array. Must be > 0.
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t*
star_bus_config_create_gpio(const char* name, gpio_num_t* pins, uint8_t pin_count);

/**
 * @brief Create an ADC bus configuration.
 *
 * Creates a configuration for an ADC bus to read analog voltages.
 * ADC bus is used for reading analog sensor values such as motor current sense lines.
 *
 * @param[in] name      Unique name for this ADC bus (e.g., "motor_current"). Must be non-NULL.
 * @param[in] unit      ADC unit (ADC_UNIT_1 or ADC_UNIT_2).
 * @param[in] channel   ADC channel to read from.
 * @param[in] bitwidth  ADC resolution (e.g., ADC_BITWIDTH_12 for 12-bit).
 * @param[in] atten     ADC attenuation for voltage range (e.g., ADC_ATTEN_DB_11 for 0-3.3V).
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t* star_bus_config_create_adc(const char*    name,
                                              adc_unit_t     unit,
                                              adc_channel_t  channel,
                                              adc_bitwidth_t bitwidth,
                                              adc_atten_t    atten);

/**
 * @brief Create a OneWire bus configuration.
 *
 * Creates a configuration for a 1-Wire bus for communicating with Dallas/Maxim devices
 * such as DS18B20 temperature sensors. The 1-Wire protocol uses a single GPIO pin for
 * bidirectional communication.
 *
 * @param[in] name      Unique name for this 1-Wire bus (e.g., "temp_onewire"). Must be non-NULL.
 * @param[in] gpio_pin  GPIO pin for 1-Wire data line (requires external 4.7kΩ pullup resistor).
 * @param[in] use_parasitic_power Enable parasite power mode (devices powered through data line).
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t*
star_bus_config_create_onewire(const char* name, gpio_num_t gpio_pin, bool use_parasitic_power);

/**
 * @brief Deinitialize the bus/device associated with this configuration.
 *
 * For I2C: Performs i2c_driver_delete.
 * For SPI: Performs spi_bus_remove_device. Does NOT automatically free the SPI bus;
 *          that happens during star_bus_config_destroy if it's the last device.
 *
 * @param[in] config Pointer to the bus configuration to deinitialize.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if config is NULL, ESP_ERR_INVALID_STATE if not initialized, or an error code from the underlying driver deinitialization.
 */
esp_err_t star_bus_config_deinit(star_bus_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_CONFIG_H */
