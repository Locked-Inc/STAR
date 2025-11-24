/* esp32-firmware/components/star_bus/star_bus_helpers.c */

#include "star_bus_helpers.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include "star_bus_manager.h"

static const char* s_TAG = "STAR_HELPERS";

/* --- I2C Presets --- */

star_bus_config_t* star_bus_config_i2c_standard(const char* name,
                                                uint8_t     address,
                                                gpio_num_t  sda_pin,
                                                gpio_num_t  scl_pin)
{
  return star_bus_config_create_i2c(name,
                                    I2C_NUM_0,
                                    address,
                                    sda_pin,
                                    scl_pin,
                                    STAR_BUS_I2C_FREQ_STANDARD);
}

star_bus_config_t*
star_bus_config_i2c_fast(const char* name, uint8_t address, gpio_num_t sda_pin, gpio_num_t scl_pin)
{
  return star_bus_config_create_i2c(name,
                                    I2C_NUM_0,
                                    address,
                                    sda_pin,
                                    scl_pin,
                                    STAR_BUS_I2C_FREQ_FAST);
}

star_bus_config_t* star_bus_config_i2c_fast_plus(const char* name,
                                                 uint8_t     address,
                                                 gpio_num_t  sda_pin,
                                                 gpio_num_t  scl_pin)
{
  return star_bus_config_create_i2c(name,
                                    I2C_NUM_0,
                                    address,
                                    sda_pin,
                                    scl_pin,
                                    STAR_BUS_I2C_FREQ_FAST_PLUS);
}

/* --- Validation Helpers --- */

bool star_bus_validate_i2c_address(uint8_t address)
{
  /* Reserved addresses: 0x00-0x07, 0x78-0x7F */
  return (address > 0x07 && address < 0x78);
}

bool star_bus_validate_i2c_frequency(uint32_t frequency)
{
  /* ESP32 supports up to 1 MHz */
  return (frequency >= 10000 && frequency <= 1000000);
}

bool star_bus_validate_spi_frequency(uint32_t frequency)
{
  /* ESP32 supports up to 80 MHz, but 20 MHz is practical max */
  return (frequency >= 100000 && frequency <= 20000000);
}

bool star_bus_validate_uart_baudrate(uint32_t baudrate)
{
  /* Common range: 300 - 921600 */
  return (baudrate >= 300 && baudrate <= 921600);
}

bool star_bus_validate_gpio_i2c(gpio_num_t pin)
{
  /* ESP32: Most GPIO pins support I2C except strapping pins */
  if (pin < 0 || pin >= GPIO_NUM_MAX) {
    return false;
  }
  /* Avoid GPIO 6-11 (connected to flash) and GPIO 0, 2 (strapping) */
  if ((pin >= 6 && pin <= 11) || pin == 0 || pin == 2) {
    return false;
  }
  return true;
}

bool star_bus_validate_gpio_spi(gpio_num_t pin)
{
  /* Same restrictions as I2C */
  return star_bus_validate_gpio_i2c(pin);
}

/* --- Quick Setup Helpers --- */

esp_err_t star_bus_quick_setup_i2c(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             address,
                                   gpio_num_t          sda_pin,
                                   gpio_num_t          scl_pin)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_config_t* config = star_bus_config_i2c_fast(bus_name, address, sda_pin, scl_pin);
  if (config == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = star_bus_manager_add_bus(manager, config);

  if (result == ESP_OK) {
    ESP_LOGI(s_TAG,
             "Quick I2C setup: '%s' addr=0x%02X on SDA=%d, SCL=%d",
             bus_name,
             address,
             sda_pin,
             scl_pin);
  } else {
    star_bus_config_destroy(config);
  }

  return result;
}
