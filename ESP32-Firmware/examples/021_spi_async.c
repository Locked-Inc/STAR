/**
 * @file 021_spi_async.c
 * @brief Asynchronous SPI operations example
 *
 * Demonstrates:
 * - Async SPI transmit
 * - Async SPI receive
 * - Async SPI transceive
 * - DMA configuration
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SPI_ASYNC_EXAMPLE";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (1000000)

static star_bus_manager_t s_manager;

static void priv_async_callback(star_async_handle_t handle,
                                star_async_status_t status,
                                esp_err_t result,
                                void *context)
{
  const char *op = (const char *)context;
  ESP_LOGI(s_tag, "Async %s: %s", op, esp_err_to_name(result));
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Async SPI Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&s_manager, "spi_async_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create SPI configuration */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
  };

  star_bus_config_t* spi_config = star_bus_config_create_spi_device(
    "async_flash",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    return;
  }

  star_bus_manager_add_bus(&s_manager, spi_config);
  star_bus_config_init(spi_config, &s_manager);

  ESP_LOGI(s_tag, "SPI initialized for async operations");

  /* --- Async Transmit --- */
  ESP_LOGI(s_tag, "\n--- Async Transmit ---");

  uint8_t tx_data[] = {0x9F, 0x00, 0x00, 0x00};  /* Read JEDEC ID */
  star_async_handle_t tx_handle;

  star_async_config_t tx_config = {
    .timeout_ms = 1000,
    .callback = priv_async_callback,
    .context = "transmit",
    .priority = 0
  };

  ret = star_bus_spi_transmit_async(
    &s_manager,
    "async_flash",
    tx_data,
    sizeof(tx_data),
    &tx_config,
    &tx_handle
  );

  if (ret == ESP_OK) {
    ret = star_async_wait(tx_handle, 1000);
    star_async_free_handle(tx_handle);
  }

  /* --- Async Receive --- */
  ESP_LOGI(s_tag, "\n--- Async Receive ---");

  uint8_t rx_buffer[8];
  star_async_handle_t rx_handle;

  star_async_config_t rx_config = {
    .timeout_ms = 1000,
    .callback = priv_async_callback,
    .context = "receive",
    .priority = 0
  };

  ret = star_bus_spi_receive_async(
    &s_manager,
    "async_flash",
    rx_buffer,
    sizeof(rx_buffer),
    &rx_config,
    &rx_handle
  );

  if (ret == ESP_OK) {
    ret = star_async_wait(rx_handle, 1000);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Received: %02X %02X %02X %02X",
               rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]);
    }
    star_async_free_handle(rx_handle);
  }

  /* --- Async Transceive --- */
  ESP_LOGI(s_tag, "\n--- Async Transceive ---");

  uint8_t cmd[] = {0x03, 0x00, 0x00, 0x00};  /* Read command + address */
  uint8_t read_data[8];
  star_async_handle_t xfer_handle;

  star_async_config_t xfer_config = {
    .timeout_ms = 1000,
    .callback = priv_async_callback,
    .context = "transceive",
    .priority = 0
  };

  ret = star_bus_spi_transceive_async(
    &s_manager,
    "async_flash",
    cmd,
    read_data,
    sizeof(cmd),
    &xfer_config,
    &xfer_handle
  );

  if (ret == ESP_OK) {
    star_async_status_t status;
    do {
      status = star_async_get_status(xfer_handle);
      vTaskDelay(pdMS_TO_TICKS(1));
    } while (status == STAR_ASYNC_STATUS_PENDING);

    if (status == STAR_ASYNC_STATUS_COMPLETE) {
      ESP_LOGI(s_tag, "Transceive complete");
    }
    star_async_free_handle(xfer_handle);
  }

  /* --- DMA Notes --- */
  ESP_LOGI(s_tag, "\n--- DMA Configuration ---");
  ESP_LOGI(s_tag, "DMA channels:");
  ESP_LOGI(s_tag, "  SPI_DMA_CH_AUTO - Auto select");
  ESP_LOGI(s_tag, "  SPI_DMA_CH1 - Channel 1");
  ESP_LOGI(s_tag, "  SPI_DMA_CH2 - Channel 2");
  ESP_LOGI(s_tag, "  SPI_DMA_DISABLED - No DMA");
  ESP_LOGI(s_tag, "DMA is recommended for large transfers");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nAsync SPI example complete");
}
