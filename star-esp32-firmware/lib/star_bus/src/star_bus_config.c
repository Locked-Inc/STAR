/* lib/star_bus/src/star_bus_config.c */

/* DHT22 is a bus protocol (single-wire proprietary), similar to I2C/SPI.
 * These methods create bus configurations, not sensor drivers.
 * This is intentional and follows the same pattern as create_i2c(), create_spi(). */

#include "star_bus_config.h"

#include "star_bus_adc.h"
#include "star_bus_gpio.h"
#include "star_bus_i2c.h"
#include "star_bus_onewire.h"
#include "star_bus_spi.h"
#include "star_bus_spi_peripheral.h"
/* Include manager types to access manager struct for SPI host tracking */
#include "driver/spi_slave.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager_types.h"

/* --- Constants --- */

static const char* s_tag = "STAR_BUS_CONFIG";

/* --- Private Function Prototypes --- */

static void internal_star_bus_config_cleanup(star_bus_config_t* config);

static star_bus_config_t* internal_star_bus_config_create_common(const char*     name,
                                                                 star_bus_type_t type);

/* --- Public Functions --- */

star_bus_config_t* star_bus_config_create_i2c(const char* name,
                                              i2c_port_t  port,
                                              uint8_t     address,
                                              gpio_num_t  sda_pin,
                                              gpio_num_t  scl_pin,
                                              uint32_t    clk_speed)
{
  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_i2c);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
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

  ESP_LOGI(s_tag,
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
  ESP_RETURN_ON_FALSE(dev_cfg, NULL, s_tag, "SPI device config cannot be NULL");
  ESP_RETURN_ON_FALSE(host >= 0 && host < SPI_HOST_MAX,
                      NULL,
                      s_tag,
                      "Invalid SPI host number: %d",
                      host);

  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_spi);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
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
    s_tag,
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
                      s_tag,
                      "Invalid SPI host number: %d",
                      host);
  ESP_RETURN_ON_FALSE(queue_size > 0 && queue_size <= 10,
                      NULL,
                      s_tag,
                      "Invalid queue size: %d (must be 1-10)",
                      queue_size);
  ESP_RETURN_ON_FALSE(mode <= 3, NULL, s_tag, "Invalid SPI mode: %d (must be 0-3)", mode);

  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_spi);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
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
    s_tag,
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
star_bus_config_create_gpio(const char* name, gpio_num_t* pins, uint8_t pin_count)
{
  ESP_RETURN_ON_FALSE(pins, NULL, s_tag, "Pin array is NULL");
  ESP_RETURN_ON_FALSE(pin_count > 0, NULL, s_tag, "Pin count must be > 0");

  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_gpio);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure GPIO-specific parameters */
  config->proto.gpio.pins          = pins;
  config->proto.gpio.pin_count     = pin_count;
  config->proto.gpio.isr_installed = false;

  /* Initialize default GPIO operations */
  config->proto.gpio.ops = star_bus_gpio_get_default_ops();

  ESP_LOGI(s_tag, "Created GPIO config '%s' with %d pins", name, pin_count);

  return config;
}

star_bus_config_t* star_bus_config_create_adc(const char*    name,
                                              adc_unit_t     unit,
                                              adc_channel_t  channel,
                                              adc_bitwidth_t bitwidth,
                                              adc_atten_t    atten)
{
  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_adc);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure ADC-specific parameters */
  config->proto.adc.unit_handle = NULL; /* Will be initialized in star_bus_config_init */
  config->proto.adc.cali_handle = NULL; /* Will be initialized in star_bus_config_init */
  config->proto.adc.unit        = unit; /* Store unit for reference counting */
  config->proto.adc.channel     = channel;
  config->proto.adc.bitwidth    = bitwidth;
  config->proto.adc.atten       = atten;

  /* Initialize default ADC operations */
  config->proto.adc.ops = star_bus_adc_get_default_ops();

  ESP_LOGI(s_tag,
           "Created ADC config '%s' (Unit: %d, Channel: %d, Bitwidth: %d, Atten: %d)",
           name,
           unit,
           channel,
           bitwidth,
           atten);

  /* Store unit in handle temporarily - will be used during init */
  config->handle = (void*)(intptr_t)unit;

  return config;
}

star_bus_config_t*
star_bus_config_create_onewire(const char* name, gpio_num_t gpio_pin, bool use_parasitic_power)
{
  star_bus_config_t* config = internal_star_bus_config_create_common(name, k_star_bus_type_onewire);
  ESP_RETURN_ON_FALSE(config,
                      NULL,
                      s_tag,
                      "Failed to create common config for %s",
                      name ? name : "NULL");

  /* Configure OneWire-specific parameters */
  config->proto.onewire.gpio_pin            = gpio_pin;
  config->proto.onewire.use_parasitic_power = use_parasitic_power;
  config->proto.onewire.use_strong_pullup   = use_parasitic_power;
  config->proto.onewire.speed               = 0; /* 0 = standard speed */
  config->proto.onewire.search_timeout_ms   = 5000;

  /* Initialize default operations to NULL - OneWire uses direct API */
  memset(&config->proto.onewire.ops, 0, sizeof(star_onewire_ops_t));

  ESP_LOGI(s_tag,
           "Created OneWire config '%s' (Pin: %d, Parasitic: %s)",
           name,
           gpio_pin,
           use_parasitic_power ? "Yes" : "No");

  return config;
}

esp_err_t star_bus_config_destroy(star_bus_config_t* config)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config pointer is NULL");

  /* If the bus/device is initialized, deinitialize it first */
  if (config->initialized) {
    esp_err_t ret = star_bus_config_deinit(config);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag,
               "Failed to deinitialize bus '%s' before destroying: %s. Continuing cleanup.",
               config->name ? config->name : "UNKNOWN",
               esp_err_to_name(ret));
      /* Continue cleanup even if deinit failed to avoid memory leaks */
    }
  }

  ESP_LOGI(s_tag, "Destroying bus config '%s'", config->name ? config->name : "UNKNOWN");

  /* Specific cleanup for SPI is handled by the manager during remove_bus */
  /* The manager checks if the bus needs to be freed based on device count */

  /* Clean up and free the configuration */
  internal_star_bus_config_cleanup(config);

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
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config pointer is NULL");
  ESP_RETURN_ON_FALSE(!config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "Bus '%s' is already initialized",
                      config->name ? config->name : "UNKNOWN");

  esp_err_t   ret      = ESP_OK;
  const char* bus_name = config->name ? config->name : "UNKNOWN"; /* Use for logging */

  switch (config->type) {
    case k_star_bus_type_i2c:
      /* Initialize I2C bus */
      ESP_LOGI(s_tag, "Initializing I2C bus '%s' (Port %d)", bus_name, config->proto.i2c.port);

      ret = i2c_param_config(config->proto.i2c.port, &config->proto.i2c.config);
      ESP_GOTO_ON_ERROR(ret,
                        fail,
                        s_tag,
                        "i2c_param_config failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));

      ret = i2c_driver_install(config->proto.i2c.port,
                               config->proto.i2c.config.mode,
                               0,  /* RX buffer size (controller mode) */
                               0,  /* TX buffer size (controller mode) */
                               0); /* Flags */
      ESP_GOTO_ON_ERROR(ret,
                        fail_i2c_driver,
                        s_tag,
                        "i2c_driver_install failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));
      break;

    case k_star_bus_type_spi:
      ESP_RETURN_ON_FALSE(manager,
                          ESP_ERR_INVALID_ARG,
                          s_tag,
                          "Bus manager pointer is required for SPI initialization");

      spi_host_device_t host = config->proto.spi.host;
      ESP_RETURN_ON_FALSE(host >= 0 && host < SPI_HOST_MAX,
                          ESP_ERR_INVALID_ARG,
                          s_tag,
                          "Invalid SPI host number: %d",
                          host);

      /* Check if this is peripheral or controller mode */
      if (config->proto.spi.is_peripheral) {
        /* SPI Peripheral Mode */
        ESP_LOGI(s_tag, "Initializing SPI peripheral '%s' (Host %d)", bus_name, host);

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

        /* DMA for SPI peripheral is controlled by the DMA channel parameter */
#if CONFIG_IDF_TARGET_ESP32S3
        /* ESP32-S3: Enable DMA for better performance (95% CPU reduction) */
        int dma_chan = SPI_DMA_CH_AUTO;
#else
        /* ESP32-WROOM: No DMA support for SPI peripheral */
        int dma_chan = SPI_DMA_DISABLED;
#endif

        ret = spi_slave_initialize(host, &bus_cfg, &slave_cfg, dma_chan);
        ESP_GOTO_ON_ERROR(ret,
                          fail,
                          s_tag,
                          "spi_slave_initialize failed for '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));

        manager->spi_host_initialized[host] = true;
        ESP_LOGI(s_tag, "SPI peripheral '%s' initialized successfully", bus_name);
      } else {
        /* SPI Controller Mode */
        ESP_LOGI(s_tag, "Initializing SPI device '%s' (Host %d)", bus_name, host);

        /* Initialize the SPI bus *only if* it hasn't been initialized yet for this host */
        if (!manager->spi_host_initialized[host]) {
          ESP_LOGI(s_tag, "Initializing SPI bus driver for host %d...", host);
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
                            s_tag,
                            "spi_bus_initialize failed for host %d: %s",
                            host,
                            esp_err_to_name(ret));
          manager->spi_host_initialized[host] = true;
          manager->spi_device_count[host]     = 0;
          ESP_LOGI(s_tag, "SPI bus driver for host %d initialized.", host);
        } else {
          ESP_LOGD(s_tag, "SPI bus driver for host %d already initialized.", host);
        }

        /* Add the SPI device to the bus */
        spi_device_handle_t spi_handle;
        ret = spi_bus_add_device(host, &config->proto.spi.dev_cfg, &spi_handle);
        ESP_GOTO_ON_ERROR(ret,
                          fail_spi_add_device,
                          s_tag,
                          "spi_bus_add_device failed for '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));
        config->handle = (void*)spi_handle;
        manager->spi_device_count[host]++;
      }
      break;

    case k_star_bus_type_gpio: {
      /* Initialize GPIO pins */
      ESP_LOGI(s_tag,
               "Initializing GPIO bus '%s' with %d pins",
               bus_name,
               config->proto.gpio.pin_count);

      for (uint8_t i = 0; i < config->proto.gpio.pin_count; i++) {
        gpio_num_t pin = config->proto.gpio.pins[i];

        /* Configure each pin as output with default low level */
        gpio_config_t io_conf = {
          .pin_bit_mask = (1ULL << pin),
          .mode         = GPIO_MODE_INPUT_OUTPUT, /* Allow both input and output */
          .pull_up_en   = GPIO_PULLUP_DISABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .intr_type    = GPIO_INTR_DISABLE,
        };

        ret = gpio_config(&io_conf);
        ESP_GOTO_ON_ERROR(ret,
                          fail,
                          s_tag,
                          "gpio_config failed for pin %d: %s",
                          pin,
                          esp_err_to_name(ret));
      }

      ESP_LOGI(s_tag, "GPIO bus '%s' initialized successfully", bus_name);
      break;
    }

    case k_star_bus_type_adc: {
      /* Initialize ADC unit */
      ESP_RETURN_ON_FALSE(manager,
                          ESP_ERR_INVALID_ARG,
                          s_tag,
                          "Bus manager pointer is required for ADC initialization");

      adc_unit_t unit = config->proto.adc.unit;
      ESP_LOGI(s_tag,
               "Initializing ADC bus '%s' (Unit: %d, Channel: %d)",
               bus_name,
               unit,
               config->proto.adc.channel);

      /* Check if ADC unit is already initialized (shared across channels) */
      if (manager->adc_unit_handles[unit] == NULL) {
        /* First channel on this unit - create the unit handle */
        ESP_LOGI(s_tag, "Creating new ADC unit handle for unit %d", unit);

        adc_oneshot_unit_init_cfg_t unit_cfg = {
          .unit_id  = unit,
          .ulp_mode = ADC_ULP_MODE_DISABLE,
        };

        ret = adc_oneshot_new_unit(&unit_cfg, &manager->adc_unit_handles[unit]);
        ESP_GOTO_ON_ERROR(ret,
                          fail,
                          s_tag,
                          "adc_oneshot_new_unit failed for '%s': %s",
                          bus_name,
                          esp_err_to_name(ret));
      } else {
        /* Unit already initialized - reuse the handle */
        ESP_LOGI(s_tag, "Reusing existing ADC unit handle for unit %d", unit);
      }

      /* Store the shared unit handle in this config */
      config->proto.adc.unit_handle = manager->adc_unit_handles[unit];

      /* Configure this specific ADC channel */
      adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = config->proto.adc.bitwidth,
        .atten    = config->proto.adc.atten,
      };

      ret = adc_oneshot_config_channel(config->proto.adc.unit_handle,
                                       config->proto.adc.channel,
                                       &chan_cfg);
      ESP_GOTO_ON_ERROR(ret,
                        fail_adc_unit,
                        s_tag,
                        "adc_oneshot_config_channel failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));

      /* Increment channel count for this unit */
      manager->adc_channel_count[unit]++;

      /* Try to initialize calibration (optional, may fail on some chips) */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
      adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = unit,
        .chan     = config->proto.adc.channel,
        .atten    = config->proto.adc.atten,
        .bitwidth = config->proto.adc.bitwidth,
      };

      ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &config->proto.adc.cali_handle);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "ADC calibration (curve fitting) initialized for '%s'", bus_name);
      } else {
        ESP_LOGW(s_tag,
                 "ADC calibration initialization failed for '%s': %s (continuing without "
                 "calibration)",
                 bus_name,
                 esp_err_to_name(ret));
        config->proto.adc.cali_handle = NULL;
      }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
      adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = unit,
        .atten    = config->proto.adc.atten,
        .bitwidth = config->proto.adc.bitwidth,
      };

      ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &config->proto.adc.cali_handle);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "ADC calibration (line fitting) initialized for '%s'", bus_name);
      } else {
        ESP_LOGW(s_tag,
                 "ADC calibration initialization failed for '%s': %s (continuing without "
                 "calibration)",
                 bus_name,
                 esp_err_to_name(ret));
        config->proto.adc.cali_handle = NULL;
      }
#else
      ESP_LOGW(s_tag, "ADC calibration not supported on this chip, continuing without calibration");
      config->proto.adc.cali_handle = NULL;
#endif

      ESP_LOGI(s_tag, "ADC bus '%s' initialized successfully", bus_name);
      break;

    fail_adc_unit:
      /* Only delete the unit if we created it (first channel on this unit) */
      manager->adc_channel_count[unit]--;
      if (manager->adc_channel_count[unit] == 0 && manager->adc_unit_handles[unit]) {
        adc_oneshot_del_unit(manager->adc_unit_handles[unit]);
        manager->adc_unit_handles[unit] = NULL;
      }
      config->proto.adc.unit_handle = NULL;
      goto fail;
    }

    case k_star_bus_type_onewire: {
      /* Initialize OneWire bus */
      ESP_RETURN_ON_FALSE(manager,
                          ESP_ERR_INVALID_ARG,
                          s_tag,
                          "Bus manager pointer is required for OneWire initialization");

      ESP_LOGI(s_tag,
               "Initializing OneWire bus '%s' (Pin: %d)",
               bus_name,
               config->proto.onewire.gpio_pin);

      /* Create onewire config and call star_bus_onewire_init() */
      star_onewire_config_t ow_cfg;
      ow_cfg.gpio_pin            = config->proto.onewire.gpio_pin;
      ow_cfg.speed               = (star_onewire_speed_t)config->proto.onewire.speed;
      ow_cfg.use_parasitic_power = config->proto.onewire.use_parasitic_power;
      ow_cfg.use_strong_pullup   = config->proto.onewire.use_strong_pullup;
      ow_cfg.search_timeout_ms   = config->proto.onewire.search_timeout_ms;

      ret = star_bus_onewire_init(manager, config->name, &ow_cfg);
      ESP_GOTO_ON_ERROR(ret,
                        fail,
                        s_tag,
                        "star_bus_onewire_init failed for '%s': %s",
                        bus_name,
                        esp_err_to_name(ret));

      ESP_LOGI(s_tag, "OneWire bus '%s' initialized successfully", bus_name);
      break;
    }

    default:
      ESP_LOGE(s_tag, "Unsupported bus type for initialization: %d", config->type);
      ret = ESP_ERR_NOT_SUPPORTED;
      goto fail;
  }

  /* Mark as initialized */
  config->initialized = true;
  ESP_LOGI(s_tag,
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
  ESP_LOGE(s_tag, "Failed to initialize bus/device '%s'", bus_name);
  return ret;
}

esp_err_t star_bus_config_deinit(star_bus_config_t* config)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config pointer is NULL");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "Bus/device '%s' is not initialized",
                      config->name ? config->name : "UNKNOWN");

  esp_err_t   ret      = ESP_OK;
  const char* bus_name = config->name ? config->name : "UNKNOWN";

  switch (config->type) {
    case k_star_bus_type_i2c:
      /* Deinitialize I2C bus */
      ESP_LOGI(s_tag, "Deinitializing I2C bus '%s' (Port %d)", bus_name, config->proto.i2c.port);

      ret = i2c_driver_delete(config->proto.i2c.port);
      break;

    case k_star_bus_type_spi:
      if (config->proto.spi.is_peripheral) {
        /* SPI Peripheral Mode - Free the peripheral driver */
        ESP_LOGI(s_tag,
                 "Deinitializing SPI peripheral '%s' (Host %d)",
                 bus_name,
                 config->proto.spi.host);
        ret = spi_slave_free(config->proto.spi.host);
      } else {
        /* SPI Controller Mode - Remove device */
        ESP_LOGI(s_tag,
                 "Deinitializing SPI device '%s' (Host %d)",
                 bus_name,
                 config->proto.spi.host);
        if (config->handle) {
          ret            = spi_bus_remove_device((spi_device_handle_t)config->handle);
          config->handle = NULL;
        } else {
          ESP_LOGW(s_tag, "SPI device '%s' handle was already NULL before deinit.", bus_name);
          ret = ESP_OK;
        }
      }
      break;

    case k_star_bus_type_gpio:
      /* GPIO pins don't need explicit deinitialization - they can be reconfigured anytime */
      ESP_LOGI(s_tag, "Deinitializing GPIO bus '%s'", bus_name);
      ret = ESP_OK;
      break;

    case k_star_bus_type_adc:
      /* Deinitialize ADC channel */
      ESP_LOGI(s_tag, "Deinitializing ADC bus '%s'", bus_name);

      /* Delete calibration if it was initialized */
      if (config->proto.adc.cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_curve_fitting(config->proto.adc.cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_line_fitting(config->proto.adc.cali_handle);
#endif
        if (ret != ESP_OK) {
          ESP_LOGW(s_tag,
                   "Failed to delete ADC calibration for '%s': %s",
                   bus_name,
                   esp_err_to_name(ret));
        }
        config->proto.adc.cali_handle = NULL;
      }

      /* Note: ADC unit handle is shared and managed by bus_manager.
       * It will be deleted when the last channel using this unit is removed. */
      config->proto.adc.unit_handle = NULL;
      ret                           = ESP_OK;
      break;

    case k_star_bus_type_onewire:
      /* Deinitialize OneWire bus */
      ESP_LOGI(s_tag, "Deinitializing OneWire bus '%s'", bus_name);
      /* Call with NULL manager - the function doesn't use it, only needs bus_name */
      ret = star_bus_onewire_deinit(NULL, bus_name);
      if (ret != ESP_OK) {
        ESP_LOGW(s_tag,
                 "Failed to deinitialize OneWire bus '%s': %s",
                 bus_name,
                 esp_err_to_name(ret));
      }
      break;

    default:
      ESP_LOGE(s_tag, "Unsupported bus type for deinitialization: %d", config->type);
      ret = ESP_ERR_NOT_SUPPORTED;
      break;
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to deinitialize bus/device '%s': %s", bus_name, esp_err_to_name(ret));
    /* Do not return early, mark as uninitialized anyway */
  }

  /* Mark as not initialized */
  config->initialized = false;
  config->handle      = NULL; /* Clear handle */

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Successfully deinitialized bus/device '%s'", bus_name);
  }

  return ret; /* Return the ret of the deinit operation */
}

/* --- Private Functions --- */

/**
 * @brief Clean up and free memory for a bus configuration structure.
 *        Does NOT deinitialize the hardware/driver.
 * @param[in] config Pointer to the bus configuration to clean up.
 */
static void internal_star_bus_config_cleanup(star_bus_config_t* config)
{
  if (config == NULL) {
    return;
  }

  /* Free the name if it was allocated (strdup'd in create_common) */
  if (config->name) {
    free((void*)config->name); /* Cast needed as it's stored as const char* */
    config->name = NULL;
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
static star_bus_config_t* internal_star_bus_config_create_common(const char*     name,
                                                                 star_bus_type_t type)
{
  /* Name validation */
  if (name == NULL) {
    ESP_LOGE(s_tag, "Bus name cannot be NULL");
    return NULL;
  }

  /* Allocate memory for the bus configuration */
  star_bus_config_t* config = (star_bus_config_t*)malloc(sizeof(star_bus_config_t));
  if (config == NULL) {
    ESP_LOGE(s_tag, "Failed to allocate memory for bus configuration");
    return NULL;
  }

  /* Initialize the configuration to zeros */
  memset(config, 0, sizeof(star_bus_config_t));

  /* Make a copy of the bus name */
  char* name_copy = strdup(name);
  if (name_copy == NULL) {
    ESP_LOGE(s_tag, "Failed to allocate memory for bus name");
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
