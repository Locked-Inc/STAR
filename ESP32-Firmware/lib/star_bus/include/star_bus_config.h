/* esp32-firmware/components/star_bus/include/star_bus_config.h */

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
 *                        Modern replacement for MOSI (Master Out, Slave In).
 * @param[in] cipo_pin    GPIO number for CIPO (Controller In, Peripheral Out) line (-1 if not used).
 *                        Modern replacement for MISO (Master In, Slave Out).
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

/**
 * @brief Create a new GPIO bus configuration.
 *
 * Configures parameters for GPIO pin control as a logical "bus".
 * Useful for sensors that use direct GPIO control (e.g., HC-SR04 ultrasonic sensor).
 * The GPIO pins will be configured when star_bus_config_init is called.
 *
 * @param[in] name      Unique name for this GPIO bus instance (e.g., "HCSR04_GPIO"). Must be non-NULL.
 * @param[in] pins      Array of GPIO pin numbers to manage. Must be non-NULL.
 * @param[in] pin_count Number of pins in the array (max 8).
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure. Must be destroyed via star_bus_config_destroy.
 */
star_bus_config_t*
star_bus_config_create_gpio(const char* name, const gpio_num_t pins[], uint8_t pin_count);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_CONFIG_H */
