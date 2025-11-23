/**
 * @file 045_spi_multi_device.c
 * @brief Multiple devices on SPI2_HOST with different CS pins
 *
 * Demonstrates:
 * - Multiple SPI devices on same bus
 * - Different chip select (CS) pins
 * - Different clock speeds per device
 * - Device-specific configurations
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_MULTI_DEVICE";

/* Shared SPI bus pins */
#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)

/* Individual CS pins for each device */
#define CS_FLASH       (GPIO_NUM_5)
#define CS_SENSOR      (GPIO_NUM_15)
#define CS_DISPLAY     (GPIO_NUM_16)
#define CS_ADC         (GPIO_NUM_17)

/**
 * @brief Configure and add a device to the manager
 */
static esp_err_t priv_add_spi_device(star_bus_manager_t *manager,
                                     const char *name,
                                int cs_pin,
                                int clock_speed,
                                int mode)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "Adding device: %s", name);
  ESP_LOGI(s_tag, "  CS Pin:  GPIO %d", cs_pin);
  ESP_LOGI(s_tag, "  Speed:   %d Hz", clock_speed);
  ESP_LOGI(s_tag, "  Mode:    %d", mode);

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = clock_speed,
    .mode = mode,
    .spics_io_num = cs_pin,
    .queue_size = 7,
    .flags = 0,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    name,
    SPI2_HOST,  /* All devices share SPI2_HOST */
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config for %s", name);
    return ESP_FAIL;
  }

  ret = star_bus_manager_add_bus(manager, spi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add %s to manager: %s", name, esp_err_to_name(ret));
    star_bus_config_destroy(spi_config);
    return ret;
  }

  ret = star_bus_config_init(spi_config, manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init %s: %s", name, esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_tag, "Device %s initialized successfully\n", name);
  return ESP_OK;
}

/**
 * @brief Communicate with flash device
 */
static void priv_test_flash_device(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "=== Testing Flash Device ===");

  /* Read JEDEC ID command */
  uint8_t cmd[] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t id[4] = {0};

  esp_err_t ret = star_bus_spi_transfer(manager, "flash_device",
                                        cmd, id, sizeof(cmd), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Flash JEDEC ID: %02X %02X %02X", id[1], id[2], id[3]);
  } else {
    ESP_LOGW(s_tag, "Flash read failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Communicate with sensor device
 */
static void priv_test_sensor_device(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "=== Testing Sensor Device ===");

  /* Example: Read WHO_AM_I register (common in sensors) */
  uint8_t read_reg[] = {0x75 | 0x80, 0x00};  /* Read reg 0x75 */
  uint8_t response[2] = {0};

  esp_err_t ret = star_bus_spi_transfer(manager, "sensor_device",
                                        read_reg, response, sizeof(read_reg), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Sensor WHO_AM_I: 0x%02X", response[1]);
  } else {
    ESP_LOGW(s_tag, "Sensor read failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Communicate with display device
 */
static void priv_test_display_device(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "=== Testing Display Device ===");

  /* Send display initialization command */
  uint8_t init_cmd[] = {0x01};  /* Software reset */

  esp_err_t ret = star_bus_spi_transmit(manager, "display_device",
                                        init_cmd, sizeof(init_cmd), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Display command sent successfully");
  } else {
    ESP_LOGW(s_tag, "Display command failed: %s", esp_err_to_name(ret));
  }

  /* Send display data */
  uint8_t pixel_data[] = {0xFF, 0x00, 0xFF, 0x00};
  ret = star_bus_spi_transmit(manager, "display_device",
                              pixel_data, sizeof(pixel_data), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Display data sent successfully");
  }
}

/**
 * @brief Communicate with ADC device
 */
static void priv_test_adc_device(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "=== Testing ADC Device ===");

  /* Start conversion and read result */
  uint8_t start_conversion = 0x01;
  uint8_t adc_data[2] = {0};

  /* Send start conversion command */
  esp_err_t ret = star_bus_spi_transmit(manager, "adc_device",
                                        &start_conversion, 1, 100);

  if (ret == ESP_OK) {
    /* Read ADC result */
    ret = star_bus_spi_receive(manager, "adc_device", adc_data, 2, 100);
    if (ret == ESP_OK) {
      uint16_t value = (adc_data[0] << 8) | adc_data[1];
      ESP_LOGI(s_tag, "ADC Value: 0x%04X (%d)", value, value);
    }
  } else {
    ESP_LOGW(s_tag, "ADC read failed: %s", esp_err_to_name(ret));
  }
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== Multiple SPI Devices Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "multi_spi_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "--- Bus Configuration ---");
  ESP_LOGI(s_tag, "Shared pins on SPI2_HOST:");
  ESP_LOGI(s_tag, "  COPI (MOSI): GPIO %d", SPI_COPI_PIN);
  ESP_LOGI(s_tag, "  CIPO (MISO): GPIO %d", SPI_CIPO_PIN);
  ESP_LOGI(s_tag, "  SCLK:        GPIO %d\n", SPI_SCLK_PIN);

  /* --- Add Device 1: Flash (fast, mode 0) --- */
  ESP_LOGI(s_tag, "\n--- Device 1: Flash Memory ---");
  ret = priv_add_spi_device(&manager, "flash_device", CS_FLASH,
                       20000000,  /* 20 MHz */
                       0);        /* Mode 0 */
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add flash device");
  }

  /* --- Add Device 2: Sensor (medium speed, mode 0) --- */
  ESP_LOGI(s_tag, "\n--- Device 2: Sensor (Accelerometer) ---");
  ret = priv_add_spi_device(&manager, "sensor_device", CS_SENSOR,
                       5000000,   /* 5 MHz */
                       0);        /* Mode 0 */
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add sensor device");
  }

  /* --- Add Device 3: Display (fast, mode 3) --- */
  ESP_LOGI(s_tag, "\n--- Device 3: LCD Display ---");
  ret = priv_add_spi_device(&manager, "display_device", CS_DISPLAY,
                       40000000,  /* 40 MHz */
                       3);        /* Mode 3 */
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add display device");
  }

  /* --- Add Device 4: ADC (slow, mode 0) --- */
  ESP_LOGI(s_tag, "\n--- Device 4: ADC Converter ---");
  ret = priv_add_spi_device(&manager, "adc_device", CS_ADC,
                       1000000,   /* 1 MHz */
                       0);        /* Mode 0 */
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add ADC device");
  }

  /* --- Test each device --- */
  ESP_LOGI(s_tag, "\n\n--- Testing All Devices ---\n");

  priv_test_flash_device(&manager);
  ESP_LOGI(s_tag, "");

  priv_test_sensor_device(&manager);
  ESP_LOGI(s_tag, "");

  priv_test_display_device(&manager);
  ESP_LOGI(s_tag, "");

  priv_test_adc_device(&manager);
  ESP_LOGI(s_tag, "");

  /* --- Demonstrate sequential access --- */
  ESP_LOGI(s_tag, "\n--- Sequential Device Access ---");
  ESP_LOGI(s_tag, "Accessing devices in sequence...\n");

  for (int i = 0; i < 3; i++) {
    ESP_LOGI(s_tag, "Round %d:", i + 1);

    uint8_t dummy_tx = 0xAA;
    uint8_t dummy_rx;

    /* Flash */
    star_bus_spi_transfer(&manager, "flash_device", &dummy_tx, &dummy_rx, 1, 50);
    ESP_LOGI(s_tag, "  Flash accessed");

    /* Sensor */
    star_bus_spi_transfer(&manager, "sensor_device", &dummy_tx, &dummy_rx, 1, 50);
    ESP_LOGI(s_tag, "  Sensor accessed");

    /* Display */
    star_bus_spi_transmit(&manager, "display_device", &dummy_tx, 1, 50);
    ESP_LOGI(s_tag, "  Display accessed");

    /* ADC */
    star_bus_spi_receive(&manager, "adc_device", &dummy_rx, 1, 50);
    ESP_LOGI(s_tag, "  ADC accessed\n");
  }

  /* --- Key Points --- */
  ESP_LOGI(s_tag, "\n--- Multi-Device SPI Notes ---");
  ESP_LOGI(s_tag, "Bus Sharing:");
  ESP_LOGI(s_tag, "  - COPI, CIPO, SCLK shared by all devices");
  ESP_LOGI(s_tag, "  - Each device has unique CS pin");
  ESP_LOGI(s_tag, "  - Only one CS active at a time");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Device-Specific Settings:");
  ESP_LOGI(s_tag, "  - Each device can have different clock speed");
  ESP_LOGI(s_tag, "  - Each device can have different SPI mode");
  ESP_LOGI(s_tag, "  - Settings auto-switch when accessing different devices");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Benefits:");
  ESP_LOGI(s_tag, "  - Saves GPIO pins (only 3 + N CS pins for N devices)");
  ESP_LOGI(s_tag, "  - Simplified wiring");
  ESP_LOGI(s_tag, "  - Automatic arbitration by driver");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Limitations:");
  ESP_LOGI(s_tag, "  - Sequential access only (not simultaneous)");
  ESP_LOGI(s_tag, "  - Maximum devices limited by available GPIOs for CS");
  ESP_LOGI(s_tag, "  - Use different SPI hosts (SPI2/SPI3) for true parallel access");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nMulti-device SPI example complete");
}
