/* esp32-firmware/components/star_bus/star_bus_config.c */

#include "star_bus_config.h"

#include "star_bus_dht22_proprietary.h"
#include "star_bus_gpio.h"
#include "star_bus_i2c.h"
#include "star_bus_spi.h"
#include "star_bus_spi_peripheral.h"
/* Include manager types to access manager struct for SPI host tracking */
#include "driver/spi_slave.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager_types.h"

/* --- Constants --- */

static const char* s_TAG = "BusConfig";

/* --- Private Function Prototypes --- */

static void priv_star_bus_config_cleanup(star_bus_config_t* config);

static star_bus_config_t* priv_star_bus_config_create_common(const char*     name,
                                                             star_bus_type_t type);

/* --- Public Functions --- */

star_bus_config_t* star_bus_config_create_i2c(const char* name,
                                              i2c_port_t  port,
                                              uint8_t     address,
                                              gpio_num_t  sda_pin,
                                              gpio_num_t  scl_pin,
                                              uint32_t    clk_speed)
{
  star_bus_config_t* config = priv_star_bus_config_create_common(name, k_star_bus_type_i2c);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_TAG,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure I2C-specific parameters */
  config->proto.i2c.port    = port;
  config->proto.i2c.address = address;

  /* Initialize with default I2C configuration */
  memset(&config->proto.i2c.config, 0, sizeof(i2c_config_t));
  config->proto.i2c.config.mode             = I2C_MODE_MASTER;
  config->proto.i2c.config.sda_io_num       = sda_pin;
  config->proto.i2c.config.scl_io_num       = scl_pin;
  config->proto.i2c.config.sda_pullup_en    = GPIO_PULLUP_ENABLE; /* Common practice */
  config->proto.i2c.config.scl_pullup_en    = GPIO_PULLUP_ENABLE; /* Common practice */
  config->proto.i2c.config.master.clk_speed = clk_speed;

  /* Initialize default operations */
  star_bus_i2c_init_default_ops(&config->proto.i2c.ops);

  ESP_LOGI(s_TAG,
           "Created I2C config '%s' (Port: %d, Addr: 0x%02X, SDA: %d, SCL: %d, Speed: %" PRIu32
           " Hz)",
           name,
           port,
           address,
           sda_pin,
           scl_pin,
           clk_speed);
  return config;
}

star_bus_config_t* star_bus_config_create_spi_device(const char*                          name,
                                                     spi_host_device_t                    host,
                                                     gpio_num_t                           copi_pin,
                                                     gpio_num_t                           cipo_pin,
                                                     gpio_num_t                           sclk_pin,
                                                     int32_t                              dma_chan,
                                                     const spi_device_interface_config_t* dev_cfg)
{
  ESP_RETURN_ON_FALSE(dev_cfg, NULL, s_TAG, "SPI device config cannot be NULL");
  ESP_RETURN_ON_FALSE(host >= 0 && host < SPI_HOST_MAX,
                      NULL,
                      s_TAG,
                      "Invalid SPI host number: %d",
                      host);

  star_bus_config_t* config = priv_star_bus_config_create_common(name, k_star_bus_type_spi);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_TAG,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure SPI-specific parameters */
  config->proto.spi.host = host;

  /* Initialize SPI bus configuration structure within the config */
  /* We copy the relevant pin numbers to our internal structure */
  memset(&config->proto.spi.bus_cfg, 0, sizeof(spi_bus_config_t));
  config->proto.spi.copi_io_num   = copi_pin;
  config->proto.spi.cipo_io_num   = cipo_pin;
  config->proto.spi.sclk_io_num   = sclk_pin;
  config->proto.spi.quadwp_io_num = -1; /* Not used for standard SPI, only QuadSPI */
  config->proto.spi.quadhd_io_num = -1;
  config->proto.spi.max_transfer_sz =
    0; /* 0 means use default (usually 4092 bytes if DMA enabled, 64 otherwise) */

  /* Copy the provided device configuration */
  memcpy(&config->proto.spi.dev_cfg, dev_cfg, sizeof(spi_device_interface_config_t));

  /* Store DMA channel request */
  config->proto.spi.bus_cfg.flags = (dma_chan > 0) ? SPICOMMON_BUSFLAG_MASTER : 0;
  /* The actual DMA channel assignment happens in spi_bus_initialize */

  /* Mark as controller mode (not peripheral) */
  config->proto.spi.is_peripheral = false;

  /* Initialize default operations */
  star_bus_spi_init_default_ops(&config->proto.spi.ops);

  ESP_LOGI(
    s_TAG,
    "Created SPI config '%s' (Host: %d, COPI: %d, CIPO: %d, SCLK: %d, CS: %d, Mode: %d, Speed: %" PRId32
    " Hz, DMA: %s)",
    name,
    host,
    copi_pin,
    cipo_pin,
    sclk_pin,
    dev_cfg->spics_io_num,
    dev_cfg->mode,
    (long)dev_cfg->clock_speed_hz,
    (dma_chan > 0) ? "Enabled" : "Disabled");

  return config;
}

star_bus_config_t* star_bus_config_create_spi_peripheral(const char*       name,
                                                         spi_host_device_t host,
                                                         gpio_num_t        copi_pin,
                                                         gpio_num_t        cipo_pin,
                                                         gpio_num_t        sclk_pin,
                                                         gpio_num_t        cs_pin,
                                                         int32_t           queue_size,
                                                         uint8_t           mode)
{
  ESP_RETURN_ON_FALSE(host >= 0 && host < SPI_HOST_MAX,
                      NULL,
                      s_TAG,
                      "Invalid SPI host number: %d",
                      host);
  ESP_RETURN_ON_FALSE(queue_size > 0 && queue_size <= 10,
                      NULL,
                      s_TAG,
                      "Invalid queue size: %d (must be 1-10)",
                      queue_size);
  ESP_RETURN_ON_FALSE(mode <= 3, NULL, s_TAG, "Invalid SPI mode: %d (must be 0-3)", mode);

  star_bus_config_t* config = priv_star_bus_config_create_common(name, k_star_bus_type_spi);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_TAG,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure SPI peripheral-specific parameters */
  config->proto.spi.host          = host;
  config->proto.spi.is_peripheral = true; /* Mark as peripheral mode */

  /* Store pin numbers */
  config->proto.spi.copi_io_num   = copi_pin;
  config->proto.spi.cipo_io_num   = cipo_pin;
  config->proto.spi.sclk_io_num   = sclk_pin;
  config->proto.spi.cs_io_num     = cs_pin;
  config->proto.spi.quadwp_io_num = -1;
  config->proto.spi.quadhd_io_num = -1;

  /* Store peripheral-specific parameters */
  config->proto.spi.queue_size = queue_size;
  config->proto.spi.mode       = mode;

  /* Initialize default peripheral operations */
  star_bus_spi_peripheral_init_default_ops(&config->proto.spi.ops);

  ESP_LOGI(
    s_TAG,
    "Created SPI Peripheral config '%s' (Host: %d, COPI: %d, CIPO: %d, SCLK: %d, CS: %d, Mode: %d, Queue: %d)",
    name,
    host,
    copi_pin,
    cipo_pin,
    sclk_pin,
    cs_pin,
    mode,
    queue_size);

  return config;
}

star_bus_config_t*
star_bus_config_create_gpio(const char* name, const gpio_num_t pins[], uint8_t pin_count)
{
  ESP_RETURN_ON_FALSE(pins, NULL, s_TAG, "Pins array is NULL");
  ESP_RETURN_ON_FALSE(pin_count > 0, NULL, s_TAG, "Pin count must be at least 1");

  star_bus_config_t* config = priv_star_bus_config_create_common(name, k_star_bus_type_gpio);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_TAG,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Allocate memory for pins array */
  gpio_num_t* pins_copy = (gpio_num_t*)malloc(pin_count * sizeof(gpio_num_t));
  if (pins_copy == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate memory for GPIO pins array");
    priv_star_bus_config_cleanup(config);
    return NULL;
  }

  /* Copy pin numbers */
  memcpy(pins_copy, pins, pin_count * sizeof(gpio_num_t));

  /* Configure GPIO-specific parameters */
  config->proto.gpio.pins          = pins_copy;
  config->proto.gpio.pin_count     = pin_count;
  config->proto.gpio.isr_installed = false;

  /* Initialize callbacks to NULL */
  memset(&config->proto.gpio.callbacks, 0, sizeof(star_gpio_callbacks_t));

  /* Initialize default operations */
  star_bus_gpio_init_default_ops(&config->proto.gpio.ops);

  ESP_LOGI(s_TAG, "Created GPIO bus config '%s' with %d pins", name, pin_count);
  for (uint8_t i = 0; i < pin_count; ++i) {
    ESP_LOGI(s_TAG, "  Pin[%d]: GPIO%d", i, pins[i]);
  }

  return config;
}

star_bus_config_t* star_bus_config_create_dht22(const char*        name,
                                                gpio_num_t         gpio_pin,
                                                star_dht22_model_t model,
                                                uint32_t           start_low_ms,
                                                uint32_t           read_timeout_ms)
{
  ESP_RETURN_ON_FALSE(gpio_pin >= 0, NULL, s_TAG, "Invalid GPIO pin");

  star_bus_config_t* config = priv_star_bus_config_create_common(name, k_star_bus_type_dht22);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_TAG,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure DHT22-specific parameters */
  config->proto.dht22.gpio_pin        = gpio_pin;
  config->proto.dht22.model           = model;
  config->proto.dht22.start_low_ms    = start_low_ms > 0 ? start_low_ms : 18;
  config->proto.dht22.read_timeout_ms = read_timeout_ms > 0 ? read_timeout_ms : 100;

  /* Initialize callbacks to NULL */
  memset(&config->proto.dht22.callbacks, 0, sizeof(star_dht22_callbacks_t));

  /* Initialize default operations */
  star_bus_dht22_init_default_ops(&config->proto.dht22.ops);

  const char* model_str = (model == k_star_dht22_model_dht11)    ? "DHT11"
                          : (model == k_star_dht22_model_am2301) ? "AM2301"
                                                                 : "DHT22";
  ESP_LOGI(s_TAG,
           "Created DHT22 bus config '%s' (GPIO: %d, Model: %s, Start: %" PRIu32
           " ms, Timeout: %" PRIu32 " ms)",
           name,
           gpio_pin,
           model_str,
           config->proto.dht22.start_low_ms,
           config->proto.dht22.read_timeout_ms);

  return config;
}

esp_err_t star_bus_config_destroy(star_bus_config_t* config)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config pointer is NULL");

  /* If the bus/device is initialized, deinitialize it first */
  if (config->initialized) {
    esp_err_t ret = star_bus_config_deinit(config);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG,
               "Failed to deinitialize bus '%s' before destroying: %s. Continuing cleanup.",
               config->name ? config->name : "UNKNOWN",
               esp_err_to_name(ret));
      /* Continue cleanup even if deinit failed to avoid memory leaks */
    }
  }

  ESP_LOGI(s_TAG, "Destroying bus config '%s'", config->name ? config->name : "UNKNOWN");

  /* Specific cleanup for SPI is handled by the manager during remove_bus */
  /* The manager checks if the bus needs to be freed based on device count */

  /* Clean up and free the configuration */
  priv_star_bus_config_cleanup(config);

  /* IMPORTANT: After this call, the config pointer is invalid. */
  return ESP_OK;
}

/**
 * @brief Initialize the bus/device associated with this configuration.
 *
 * For I2C: Performs i2c_param_config, i2c_driver_install.
 * For SPI: Performs spi_bus_initialize (if not already done for this host) and spi_bus_add_device.
 *          Requires the bus manager to track host initialization status.
 *
 * @param[in] config Pointer to the bus configuration to initialize.
 * @param[in] manager Pointer to the bus manager (needed for SPI host tracking).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if config or manager is NULL, ESP_ERR_INVALID_STATE if already initialized, or an error code from the underlying driver initialization.
 */
esp_err_t star_bus_config_init(star_bus_config_t* config, star_bus_manager_t* manager)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config pointer is NULL");
  ESP_RETURN_ON_FALSE(!config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus '%s' is already initialized",
                      config->name ? config->name : "UNKNOWN");

  esp_err_t   ret      = ESP_OK;
  const char* bus_name = config->name ? config->name : "UNKNOWN"; /* Use for logging */

  switch (config->type) {
    case k_star_bus_type_i2c:
      /* Initialize I2C bus */
      ESP_LOGI(s_TAG, "Initializing I2C bus '%s' (Port %d)", bus_name, config->proto.i2c.port);

      ret = i2c_param_config(config->proto.i2c.port, &config->proto.i2c.config);
      ESP_GOTO_ON_ERROR(ret,
                        fail,
                        s_TAG,
                        "i2c_param_config failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));

      ret = i2c_driver_install(config->proto.i2c.port,
                               config->proto.i2c.config.mode,
                               0,  /* RX buffer size (master mode) */
                               0,  /* TX buffer size (master mode) */
                               0); /* Flags */
      ESP_GOTO_ON_ERROR(ret,
                        fail_i2c_driver,
                        s_TAG,
                        "i2c_driver_install failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));
      break;

    case k_star_bus_type_spi:
      ESP_RETURN_ON_FALSE(manager,
                          ESP_ERR_INVALID_ARG,
                          s_TAG,
                          "Bus manager pointer is required for SPI initialization");

      spi_host_device_t host = config->proto.spi.host;
      ESP_RETURN_ON_FALSE(host >= 0 && host < SPI_HOST_MAX,
                          ESP_ERR_INVALID_ARG,
                          s_TAG,
                          "Invalid SPI host number: %d",
                          host);

      /* Check if this is peripheral or controller mode */
      if (config->proto.spi.is_peripheral) {
        /* SPI Peripheral (Slave) Mode */
        ESP_LOGI(s_TAG, "Initializing SPI peripheral '%s' (Host %d)", bus_name, host);

        spi_slave_interface_config_t slave_cfg = {
          .spics_io_num  = config->proto.spi.cs_io_num,
          .flags         = 0,
          .queue_size    = config->proto.spi.queue_size,
          .mode          = config->proto.spi.mode,
          .post_setup_cb = NULL,
          .post_trans_cb = NULL,
        };

        /* Map COPI/CIPO to ESP-IDF's MOSI/MISO */
        spi_bus_config_t bus_cfg = {
          .mosi_io_num     = config->proto.spi.copi_io_num,
          .miso_io_num     = config->proto.spi.cipo_io_num,
          .sclk_io_num     = config->proto.spi.sclk_io_num,
          .quadwp_io_num   = -1,
          .quadhd_io_num   = -1,
          .max_transfer_sz = 0,
        };

        ret = spi_slave_initialize(host, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
        ESP_GOTO_ON_ERROR(ret,
                          fail,
                          s_TAG,
                          "spi_slave_initialize failed for '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));

        manager->spi_host_initialized[host] = true;
        ESP_LOGI(s_TAG, "SPI peripheral '%s' initialized successfully", bus_name);
      } else {
        /* SPI Controller (Master) Mode */
        ESP_LOGI(s_TAG, "Initializing SPI device '%s' (Host %d)", bus_name, host);

        /* Initialize the SPI bus *only if* it hasn't been initialized yet for this host */
        if (!manager->spi_host_initialized[host]) {
          ESP_LOGI(s_TAG, "Initializing SPI bus driver for host %d...", host);
          /* Map COPI/CIPO to ESP-IDF's MOSI/MISO */
          spi_bus_config_t esp_idf_bus_cfg = {
            .mosi_io_num     = config->proto.spi.copi_io_num,
            .miso_io_num     = config->proto.spi.cipo_io_num,
            .sclk_io_num     = config->proto.spi.sclk_io_num,
            .quadwp_io_num   = config->proto.spi.quadwp_io_num,
            .quadhd_io_num   = config->proto.spi.quadhd_io_num,
            .max_transfer_sz = config->proto.spi.max_transfer_sz,
            .flags           = config->proto.spi.bus_cfg.flags,
          };
          int32_t dma_chan = (config->proto.spi.bus_cfg.flags & SPICOMMON_BUSFLAG_MASTER)
                               ? SPI_DMA_CH_AUTO
                               : SPI_DMA_DISABLED;
          ret              = spi_bus_initialize(host, &esp_idf_bus_cfg, dma_chan);
          ESP_GOTO_ON_ERROR(ret,
                            fail,
                            s_TAG,
                            "spi_bus_initialize failed for host %d: %s",
                            host,
                            esp_err_to_name(ret));
          manager->spi_host_initialized[host] = true;
          manager->spi_device_count[host]     = 0;
          ESP_LOGI(s_TAG, "SPI bus driver for host %d initialized.", host);
        } else {
          ESP_LOGD(s_TAG, "SPI bus driver for host %d already initialized.", host);
        }

        /* Add the SPI device to the bus */
        spi_device_handle_t spi_handle;
        ret = spi_bus_add_device(host, &config->proto.spi.dev_cfg, &spi_handle);
        ESP_GOTO_ON_ERROR(ret,
                          fail_spi_add_device,
                          s_TAG,
                          "spi_bus_add_device failed for '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));
        config->handle = (void*)spi_handle;
        manager->spi_device_count[host]++;
      }
      break;

    case k_star_bus_type_gpio:
      /* Initialize GPIO bus - pins are configured on-demand via gpio_configure */
      ESP_LOGI(s_TAG,
               "Initializing GPIO bus '%s' with %d pins",
               bus_name,
               config->proto.gpio.pin_count);

      /* GPIO initialization is minimal - actual pin configuration happens on first use */
      /* No driver installation needed for GPIO unlike I2C/SPI */

      ESP_LOGI(s_TAG, "GPIO bus '%s' ready (pins will be configured on-demand)", bus_name);
      break;

    case k_star_bus_type_dht22:
      /* Initialize DHT22 bus */
      ESP_LOGI(s_TAG,
               "Initializing DHT22 bus '%s' on GPIO %d",
               bus_name,
               config->proto.dht22.gpio_pin);

      /* Configure GPIO as open-drain with pull-up for DHT22 protocol */
      {
        gpio_config_t io_conf = {
          .pin_bit_mask = (1ULL << config->proto.dht22.gpio_pin),
          .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
          .pull_up_en   = GPIO_PULLUP_ENABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .intr_type    = GPIO_INTR_DISABLE,
        };

        ret = gpio_config(&io_conf);
        ESP_GOTO_ON_ERROR(ret,
                          fail,
                          s_TAG,
                          "Failed to configure GPIO for DHT22 bus '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));

        /* Release bus (high) */
        gpio_set_level(config->proto.dht22.gpio_pin, 1);
      }

      ESP_LOGI(s_TAG,
               "DHT22 bus '%s' initialized on GPIO %d",
               bus_name,
               config->proto.dht22.gpio_pin);
      break;

    default:
      ESP_LOGE(s_TAG, "Unsupported bus type for initialization: %d", config->type);
      ret = ESP_ERR_NOT_SUPPORTED;
      goto fail;
  }

  /* Mark as initialized */
  config->initialized = true;
  ESP_LOGI(s_TAG,
           "Successfully initialized bus/device '%s' (%s)",
           bus_name,
           star_bus_type_to_string(config->type));
  return ESP_OK;

/* Failure points with specific cleanup needs */
fail_i2c_driver:
  /* If driver install failed after param_config, no specific cleanup needed for param_config */
  goto fail;

fail_spi_add_device:
  /* If add_device failed, we might need to free the bus if we just initialized it */
  /* This logic is now handled by the manager during remove_bus */
  goto fail;

/* Generic failure point */
fail:
  ESP_LOGE(s_TAG, "Failed to initialize bus/device '%s'", bus_name);
  return ret;
}

esp_err_t star_bus_config_deinit(star_bus_config_t* config)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config pointer is NULL");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus/device '%s' is not initialized",
                      config->name ? config->name : "UNKNOWN");

  esp_err_t   ret      = ESP_OK;
  const char* bus_name = config->name ? config->name : "UNKNOWN";

  switch (config->type) {
    case k_star_bus_type_i2c:
      /* Deinitialize I2C bus */
      ESP_LOGI(s_TAG, "Deinitializing I2C bus '%s' (Port %d)", bus_name, config->proto.i2c.port);

      ret = i2c_driver_delete(config->proto.i2c.port);
      break;

    case k_star_bus_type_spi:
      if (config->proto.spi.is_peripheral) {
        /* SPI Peripheral Mode - Free the slave driver */
        ESP_LOGI(s_TAG,
                 "Deinitializing SPI peripheral '%s' (Host %d)",
                 bus_name,
                 config->proto.spi.host);
        ret = spi_slave_free(config->proto.spi.host);
      } else {
        /* SPI Controller Mode - Remove device */
        ESP_LOGI(s_TAG,
                 "Deinitializing SPI device '%s' (Host %d)",
                 bus_name,
                 config->proto.spi.host);
        if (config->handle) {
          ret            = spi_bus_remove_device((spi_device_handle_t)config->handle);
          config->handle = NULL;
        } else {
          ESP_LOGW(s_TAG, "SPI device '%s' handle was already NULL before deinit.", bus_name);
          ret = ESP_OK;
        }
      }
      break;

    case k_star_bus_type_gpio:
      /* Deinitialize GPIO bus */
      ESP_LOGI(s_TAG, "Deinitializing GPIO bus '%s'", bus_name);

      /* Remove ISR handlers for all pins if ISR service was installed */
      if (config->proto.gpio.isr_installed) {
        for (uint8_t i = 0; i < config->proto.gpio.pin_count; ++i) {
          gpio_num_t pin = config->proto.gpio.pins[i];
          if (pin != GPIO_NUM_NC) {
            /* Try to remove ISR handler, ignore errors if not set */
            gpio_isr_handler_remove(pin);
          }
        }
        /* Note: We don't uninstall ISR service as it might be shared with other GPIO buses */
      }

      /* Reset all pins to input with pull-down (safe default state) */
      for (uint8_t i = 0; i < config->proto.gpio.pin_count; ++i) {
        gpio_num_t pin = config->proto.gpio.pins[i];
        if (pin != GPIO_NUM_NC) {
          gpio_reset_pin(pin);
        }
      }

      ret = ESP_OK;
      break;

    case k_star_bus_type_dht22:
      /* Deinitialize DHT22 bus */
      ESP_LOGI(s_TAG,
               "Deinitializing DHT22 bus '%s' (GPIO %d)",
               bus_name,
               config->proto.dht22.gpio_pin);

      /* Reset GPIO to default state */
      if (config->proto.dht22.gpio_pin != GPIO_NUM_NC) {
        gpio_reset_pin(config->proto.dht22.gpio_pin);
      }

      ret = ESP_OK;
      break;

    default:
      ESP_LOGE(s_TAG, "Unsupported bus type for deinitialization: %d", config->type);
      ret = ESP_ERR_NOT_SUPPORTED;
      break;
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to deinitialize bus/device '%s': %s", bus_name, esp_err_to_name(ret));
    /* Do not return early, mark as uninitialized anyway */
  }

  /* Mark as not initialized */
  config->initialized = false;
  config->handle      = NULL; /* Clear handle */

  if (ret == ESP_OK) {
    ESP_LOGI(s_TAG, "Successfully deinitialized bus/device '%s'", bus_name);
  }

  return ret; /* Return the ret of the deinit operation */
}

/* --- Private Functions --- */

/**
 * @brief Clean up and free memory for a bus configuration structure.
 *        Does NOT deinitialize the hardware/driver.
 * @param[in] config Pointer to the bus configuration to clean up.
 */
static void priv_star_bus_config_cleanup(star_bus_config_t* config)
{
  if (config == NULL) {
    return;
  }

  /* Free the name if it was allocated (strdup'd in create_common) */
  if (config->name) {
    free((void*)config->name); /* Cast needed as it's stored as const char* */
    config->name = NULL;
  }

  /* Free GPIO pins array if this is a GPIO config */
  if (config->type == k_star_bus_type_gpio && config->proto.gpio.pins != NULL) {
    free(config->proto.gpio.pins);
    config->proto.gpio.pins      = NULL;
    config->proto.gpio.pin_count = 0;
  }

  /* Clear other pointers */
  config->handle   = NULL;
  config->user_ctx = NULL;
  config->next     = NULL;

  /* Free the bus configuration structure itself */
  free(config);
}

/**
 * @brief Create a generic bus configuration structure and allocate memory.
 *
 * @param[in] name Name of the bus (will be duplicated). Must not be NULL.
 * @param[in] type Type of the bus.
 * @return star_bus_config_t* Pointer to the created configuration, or NULL on failure.
 */
static star_bus_config_t* priv_star_bus_config_create_common(const char* name, star_bus_type_t type)
{
  /* Name validation */
  if (name == NULL) {
    ESP_LOGE(s_TAG, "Bus name cannot be NULL");
    return NULL;
  }

  /* Allocate memory for the bus configuration */
  star_bus_config_t* config = (star_bus_config_t*)malloc(sizeof(star_bus_config_t));
  if (config == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate memory for bus configuration");
    return NULL;
  }

  /* Initialize the configuration to zeros */
  memset(config, 0, sizeof(star_bus_config_t));

  /* Make a copy of the bus name */
  char* name_copy = strdup(name);
  if (name_copy == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate memory for bus name");
    free(config);
    return NULL;
  }
  config->name = name_copy; /* Store the copy */

  /* Set common parameters */
  config->type        = type;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Initialize callback structs to NULL */
  memset(&config->proto.i2c.callbacks, 0, sizeof(config->proto.i2c.callbacks));
  memset(&config->proto.spi.callbacks, 0, sizeof(config->proto.spi.callbacks));

  return config;
}
