/**
 * @file 02_spi_basic.c
 * @brief Basic SPI operations example
 *
 * This example demonstrates:
 * - Creating an SPI device configuration
 * - Adding it to the bus manager
 * - Initializing the SPI bus
 * - Performing transmit, receive, and transfer operations
 * - Proper cleanup
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_EXAMPLE";

/* Pin definitions - adjust for your hardware */
#define SPI_COPI_PIN  (GPIO_NUM_23)  /* Controller Out, Peripheral In (MOSI) */
#define SPI_CIPO_PIN  (GPIO_NUM_19)  /* Controller In, Peripheral Out (MISO) */
#define SPI_SCLK_PIN  (GPIO_NUM_18)  /* Clock */
#define SPI_CS_PIN    (GPIO_NUM_5)   /* Chip Select */

/* SPI configuration */
#define SPI_CLK_SPEED (1000000)      /* 1 MHz */

void spi_basic_example(void)
{
  esp_err_t ret;

  /* Step 1: Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "SPI_Example", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Step 2: Create SPI device interface config */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,              /* SPI mode 0: CPOL=0, CPHA=0 */
    .spics_io_num   = SPI_CS_PIN,     /* CS pin */
    .queue_size     = 7,              /* Transaction queue size */
    .pre_cb         = NULL,           /* Pre-transfer callback */
    .post_cb        = NULL,           /* Post-transfer callback */
  };

  /* Step 3: Create SPI bus configuration */
  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "flash_device",      /* Unique name for this device */
    SPI2_HOST,           /* SPI host (SPI2_HOST or SPI3_HOST) */
    SPI_COPI_PIN,        /* COPI (MOSI) GPIO */
    SPI_CIPO_PIN,        /* CIPO (MISO) GPIO */
    SPI_SCLK_PIN,        /* SCLK GPIO */
    SPI_DMA_CH_AUTO,     /* DMA channel (auto-select) */
    &dev_cfg             /* Device interface config */
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "SPI config created");

  /* Step 4: Add config to manager */
  ret = star_bus_manager_add_bus(&manager, spi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus: %s", esp_err_to_name(ret));
    star_bus_config_destroy(spi_config);
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "SPI device added to manager");

  /* Step 5: Initialize the SPI driver */
  ret = star_bus_config_init(spi_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "SPI driver initialized");

  /* Step 6: Transmit data (write only) */
  uint8_t tx_data[] = {0x9F, 0x00, 0x00, 0x00};  /* Example: JEDEC ID command */
  ret = star_bus_spi_transmit(&manager, "flash_device", tx_data, sizeof(tx_data),
                              0);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transmitted %u bytes", (unsigned)sizeof(tx_data));
  } else {
    ESP_LOGW(s_tag, "Transmit failed: %s", esp_err_to_name(ret));
  }

  /* Step 7: Receive data (read only) */
  uint8_t rx_data[4] = {0};
  ret = star_bus_spi_receive(&manager, "flash_device", rx_data, sizeof(rx_data),
                             0);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Received: 0x%02X 0x%02X 0x%02X 0x%02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
  } else {
    ESP_LOGW(s_tag, "Receive failed: %s", esp_err_to_name(ret));
  }

  /* Step 8: Full-duplex transfer (simultaneous read/write) */
  uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};  /* Send command */
  uint8_t rx_buf[4] = {0};                       /* Receive response */

  ret = star_bus_spi_transfer(&manager, "flash_device", tx_buf, rx_buf,
                              sizeof(tx_buf), 0);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Full-duplex transfer:");
    ESP_LOGI(s_tag, "  TX: 0x%02X 0x%02X 0x%02X 0x%02X",
             tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3]);
    ESP_LOGI(s_tag, "  RX: 0x%02X 0x%02X 0x%02X 0x%02X",
             rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
  } else {
    ESP_LOGW(s_tag, "Transfer failed: %s", esp_err_to_name(ret));
  }

  /* Step 9: Multiple devices on same bus */
  ESP_LOGI(s_tag, "\n--- Adding second SPI device ---");

  spi_device_interface_config_t dev2_cfg = {
    .clock_speed_hz = 500000,        /* Different clock speed */
    .mode           = 0,
    .spics_io_num   = GPIO_NUM_15,   /* Different CS pin */
    .queue_size     = 7,
  };

  star_bus_config_t *spi_config2 = star_bus_config_create_spi_device(
    "sensor_device",
    SPI2_HOST,           /* Same SPI host */
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev2_cfg
  );

  if (spi_config2 != NULL) {
    ret = star_bus_manager_add_bus(&manager, spi_config2);
    if (ret == ESP_OK) {
      ret = star_bus_config_init(spi_config2, &manager);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Second SPI device initialized");

        /* Use the second device */
        uint8_t sensor_cmd[] = {0x01, 0x02};
        ret = star_bus_spi_transmit(&manager, "sensor_device", sensor_cmd, 2, 0);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "Sent command to sensor device");
        }
      }
    }
  }

  /* Step 10: Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "SPI example complete - all resources freed");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== SPI Basic Example ===\n");
  spi_basic_example();
}
