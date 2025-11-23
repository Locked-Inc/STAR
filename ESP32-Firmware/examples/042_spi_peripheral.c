/**
 * @file 042_spi_peripheral.c
 * @brief SPI peripheral (slave) mode with callbacks
 *
 * Demonstrates:
 * - SPI peripheral (slave) mode configuration
 * - Pre and post transfer callbacks
 * - Handling data from SPI controller
 * - Transaction completion notifications
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <driver/spi_slave.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SPI_PERIPHERAL_EXAMPLE";

#define SPI_COPI_PIN   (GPIO_NUM_23)  /* Controller -> Peripheral */
#define SPI_CIPO_PIN   (GPIO_NUM_19)  /* Peripheral -> Controller */
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

#define BUFFER_SIZE (128)

/* Global buffers for peripheral mode */
static uint8_t s_rx_buffer[BUFFER_SIZE];
static uint8_t s_tx_buffer[BUFFER_SIZE];
static volatile int s_transaction_count = 0;

/**
 * @brief Pre-transfer callback (called before CS goes low)
 */
static void IRAM_ATTR priv_spi_pre_transfer_callback(spi_slave_transaction_t *trans)
{
  /* This runs in ISR context - keep it minimal! */
  /* Can be used to prepare data or set GPIO states */
}

/**
 * @brief Post-transfer callback (called after CS goes high)
 */
static void IRAM_ATTR priv_spi_post_transfer_callback(spi_slave_transaction_t *trans)
{
  /* This runs in ISR context - keep it minimal! */
  s_transaction_count++;
}

/**
 * @brief Initialize SPI peripheral (slave) mode
 */
static esp_err_t priv_init_spi_peripheral(void)
{
  esp_err_t ret;

  /* Configure SPI peripheral */
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SPI_COPI_PIN,
    .miso_io_num = SPI_CIPO_PIN,
    .sclk_io_num = SPI_SCLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = BUFFER_SIZE,
  };

  spi_slave_interface_config_t slave_cfg = {
    .mode = 0,                    /* SPI mode 0 */
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 3,              /* Transaction queue size */
    .flags = 0,
    .post_setup_cb = priv_spi_pre_transfer_callback,
    .post_trans_cb = priv_spi_post_transfer_callback,
  };

  /* Initialize SPI peripheral (slave) */
  ret = spi_slave_initialize(SPI2_HOST, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to initialize SPI peripheral: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_tag, "SPI peripheral initialized successfully");
  return ESP_OK;
}

/**
 * @brief Peripheral task - handles incoming SPI transactions
 */
static void priv_spi_peripheral_task(void *arg)
{
  esp_err_t ret;
  spi_slave_transaction_t trans;
  int transaction_num = 0;

  /* Prepare TX buffer with response data */
  for (int i = 0; i < BUFFER_SIZE; i++) {
    s_tx_buffer[i] = 0xA0 + (i % 16);  /* Response pattern */
  }

  ESP_LOGI(s_tag, "Peripheral task started - waiting for SPI transactions...");

  while (1) {
    /* Clear buffers */
    memset(s_rx_buffer, 0, BUFFER_SIZE);
    memset(&trans, 0, sizeof(trans));

    /* Setup transaction */
    trans.length = BUFFER_SIZE * 8;  /* Length in bits */
    trans.tx_buffer = s_tx_buffer;
    trans.rx_buffer = s_rx_buffer;

    /* Queue transaction - this will block until controller initiates transfer */
    ret = spi_slave_queue_trans(SPI2_HOST, &trans, portMAX_DELAY);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to queue transaction: %s", esp_err_to_name(ret));
      continue;
    }

    /* Wait for transaction to complete */
    spi_slave_transaction_t *ret_trans;
    ret = spi_slave_get_trans_result(SPI2_HOST, &ret_trans, portMAX_DELAY);
    if (ret == ESP_OK) {
      transaction_num++;

      ESP_LOGI(s_tag, "\n--- Transaction %d Complete ---", transaction_num);
      ESP_LOGI(s_tag, "Received %d bits (%d bytes)",
               ret_trans->trans_len, ret_trans->trans_len / 8);

      /* Display received data */
      int bytes_received = ret_trans->trans_len / 8;
      if (bytes_received > 0) {
        ESP_LOGI(s_tag, "RX data (first 16 bytes):");
        for (int i = 0; i < 16 && i < bytes_received; i++) {
          printf("%02X ", s_rx_buffer[i]);
          if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
      }

      ESP_LOGI(s_tag, "Total transactions processed: %d", s_transaction_count);
    } else {
      ESP_LOGE(s_tag, "Transaction failed: %s", esp_err_to_name(ret));
    }

    /* Small delay before next transaction */
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SPI Peripheral (Slave) Mode Example ===\n");

  ESP_LOGI(s_tag, "Pin Configuration:");
  ESP_LOGI(s_tag, "  COPI (MOSI): GPIO %d (Controller -> Peripheral)", SPI_COPI_PIN);
  ESP_LOGI(s_tag, "  CIPO (MISO): GPIO %d (Peripheral -> Controller)", SPI_CIPO_PIN);
  ESP_LOGI(s_tag, "  SCLK:        GPIO %d", SPI_SCLK_PIN);
  ESP_LOGI(s_tag, "  CS:          GPIO %d\n", SPI_CS_PIN);

  /* Initialize SPI peripheral */
  ret = priv_init_spi_peripheral();
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Peripheral initialization failed");
    return;
  }

  /* --- Callback Information --- */
  ESP_LOGI(s_tag, "\n--- Callback Functions ---");
  ESP_LOGI(s_tag, "Pre-transfer callback:");
  ESP_LOGI(s_tag, "  - Called before CS goes low");
  ESP_LOGI(s_tag, "  - Use to prepare data or set GPIOs");
  ESP_LOGI(s_tag, "  - Runs in ISR context (keep minimal!)");

  ESP_LOGI(s_tag, "\nPost-transfer callback:");
  ESP_LOGI(s_tag, "  - Called after CS goes high");
  ESP_LOGI(s_tag, "  - Use for notifications or counters");
  ESP_LOGI(s_tag, "  - Runs in ISR context (keep minimal!)");

  /* --- Start peripheral task --- */
  ESP_LOGI(s_tag, "\n--- Starting Peripheral Task ---");

  xTaskCreate(priv_spi_peripheral_task, "spi_peripheral", 4096, NULL, 5, NULL);

  /* Main task - monitor status */
  int last_count = 0;
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    if (s_transaction_count != last_count) {
      ESP_LOGI(s_tag, "[Status] Total transactions: %d", s_transaction_count);
      last_count = s_transaction_count;
    } else {
      ESP_LOGI(s_tag, "[Status] Waiting for SPI controller... (no new transactions)");
    }
  }

  /* Note: In a real application, implement proper cleanup */
  /* spi_slave_free(SPI2_HOST); */
}
