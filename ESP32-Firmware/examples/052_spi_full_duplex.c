/**
 * @file 052_spi_full_duplex.c
 * @brief Full-duplex SPI communication patterns
 *
 * Demonstrates:
 * - Full-duplex simultaneous transmit and receive
 * - Half-duplex mode configuration
 * - Command-response patterns
 * - Bidirectional data flow
 * - Full vs half-duplex performance comparison
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_FULL_DUPLEX";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/**
 * @brief Demonstrate basic full-duplex transfer
 */
static void priv_demonstrate_full_duplex(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Full-Duplex Transfer ===\n");

  ESP_LOGI(s_tag, "Full-duplex: TX and RX happen simultaneously");
  ESP_LOGI(s_tag, "Each clock cycle sends one bit and receives one bit\n");

  /* Prepare TX data */
  uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  uint8_t rx_data[8] = {0};

  ESP_LOGI(s_tag, "TX data: %02X %02X %02X %02X %02X %02X %02X %02X",
           tx_data[0], tx_data[1], tx_data[2], tx_data[3],
           tx_data[4], tx_data[5], tx_data[6], tx_data[7]);

  /* Perform full-duplex transfer */
  esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                        tx_data, rx_data,
                                        sizeof(tx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "RX data: %02X %02X %02X %02X %02X %02X %02X %02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3],
             rx_data[4], rx_data[5], rx_data[6], rx_data[7]);
    ESP_LOGI(s_tag, "Transfer successful");
  } else {
    ESP_LOGE(s_tag, "Transfer failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate command-response pattern
 */
static void priv_demonstrate_command_response(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Command-Response Pattern ===\n");

  ESP_LOGI(s_tag, "Common pattern for SPI peripherals:");
  ESP_LOGI(s_tag, "1. Send command while receiving dummy bytes");
  ESP_LOGI(s_tag, "2. Receive response while sending dummy bytes\n");

  /* Step 1: Send command (e.g., READ STATUS) */
  uint8_t cmd_tx[4] = {0x05, 0x00, 0x00, 0x00};  /* Command + dummy */
  uint8_t cmd_rx[4] = {0};

  ESP_LOGI(s_tag, "Sending command: 0x%02X", cmd_tx[0]);

  esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                        cmd_tx, cmd_rx,
                                        sizeof(cmd_tx), 1000);

  if (ret == ESP_OK) {
    /* Response typically starts after command byte */
    ESP_LOGI(s_tag, "Response: %02X %02X %02X %02X",
             cmd_rx[0], cmd_rx[1], cmd_rx[2], cmd_rx[3]);
    ESP_LOGI(s_tag, "Status: 0x%02X", cmd_rx[1]);  /* Status in second byte */
  } else {
    ESP_LOGE(s_tag, "Command failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate JEDEC ID read (full-duplex)
 */
static void priv_demonstrate_jedec_id_read(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== JEDEC ID Read (Full-Duplex) ===\n");

  ESP_LOGI(s_tag, "Reading flash memory JEDEC ID...");

  /* JEDEC ID command: 0x9F followed by 3 dummy bytes */
  uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t rx_buf[4] = {0};

  esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                        tx_buf, rx_buf,
                                        sizeof(tx_buf), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "JEDEC ID response:");
    ESP_LOGI(s_tag, "  Manufacturer: 0x%02X", rx_buf[1]);
    ESP_LOGI(s_tag, "  Memory Type:  0x%02X", rx_buf[2]);
    ESP_LOGI(s_tag, "  Capacity:     0x%02X", rx_buf[3]);

    /* Note: First byte is echo of command, actual data starts at byte 1 */
  } else {
    ESP_LOGE(s_tag, "JEDEC ID read failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate register read/write (full-duplex)
 */
static void priv_demonstrate_register_access(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Register Access (Full-Duplex) ===\n");

  /* Write to register */
  ESP_LOGI(s_tag, "Writing to register 0x02...");

  uint8_t write_cmd[3] = {0x02, 0x10, 0xAB};  /* WRITE, address, data */
  uint8_t write_rx[3] = {0};

  esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                        write_cmd, write_rx,
                                        sizeof(write_cmd), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Write successful");
  }

  /* Read from register */
  ESP_LOGI(s_tag, "Reading from register 0x10...");

  uint8_t read_cmd[3] = {0x03, 0x10, 0x00};  /* READ, address, dummy */
  uint8_t read_rx[3] = {0};

  ret = star_bus_spi_transfer(manager, "fullduplex_device",
                               read_cmd, read_rx,
                               sizeof(read_cmd), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read data: 0x%02X", read_rx[2]);
  }
}

/**
 * @brief Demonstrate half-duplex mode
 */
static void priv_demonstrate_half_duplex(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Half-Duplex Mode ===\n");

  ESP_LOGI(s_tag, "Half-duplex: TX and RX happen sequentially");
  ESP_LOGI(s_tag, "Used for large transfers to save memory\n");

  /* Transmit phase only */
  uint8_t tx_data[16];
  for (int i = 0; i < 16; i++) {
    tx_data[i] = (uint8_t)i;
  }

  ESP_LOGI(s_tag, "Transmit phase: sending 16 bytes");
  esp_err_t ret = star_bus_spi_transmit(manager, "halfduplex_device",
                                        tx_data, sizeof(tx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transmit complete");
  }

  /* Receive phase only */
  uint8_t rx_data[16];

  ESP_LOGI(s_tag, "Receive phase: reading 16 bytes");
  ret = star_bus_spi_receive(manager, "halfduplex_device",
                              rx_data, sizeof(rx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Receive complete");
    ESP_LOGI(s_tag, "First 4 bytes: %02X %02X %02X %02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
  }

  ESP_LOGI(s_tag, "\nHalf-duplex benefits:");
  ESP_LOGI(s_tag, "- Only need one buffer (TX or RX)");
  ESP_LOGI(s_tag, "- Saves memory for large transfers");
  ESP_LOGI(s_tag, "- Good for display updates, flash writes");
}

/**
 * @brief Compare full-duplex vs half-duplex performance
 */
static void priv_compare_performance(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Performance Comparison ===\n");

  const size_t transfer_size = 512;
  uint8_t *tx_buf = (uint8_t *)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);
  uint8_t *rx_buf = (uint8_t *)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);

  if (tx_buf == NULL || rx_buf == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buf) heap_caps_free(tx_buf);
    if (rx_buf) heap_caps_free(rx_buf);
    return;
  }

  /* Initialize TX buffer */
  for (size_t i = 0; i < transfer_size; i++) {
    tx_buf[i] = (uint8_t)(i & 0xFF);
  }

  /* Test 1: Full-duplex */
  ESP_LOGI(s_tag, "Testing full-duplex (%zu bytes)...", transfer_size);

  int64_t start = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                        tx_buf, rx_buf,
                                        transfer_size, 5000);

  int64_t full_duplex_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Full-duplex: %lld us", full_duplex_time);
  }

  /* Test 2: Half-duplex (separate TX and RX) */
  ESP_LOGI(s_tag, "Testing half-duplex (%zu bytes)...", transfer_size);

  start = esp_timer_get_time();

  ret = star_bus_spi_transmit(manager, "halfduplex_device",
                               tx_buf, transfer_size, 5000);
  if (ret == ESP_OK) {
    ret = star_bus_spi_receive(manager, "halfduplex_device",
                                rx_buf, transfer_size, 5000);
  }

  int64_t half_duplex_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Half-duplex: %lld us", half_duplex_time);
  }

  /* Analysis */
  ESP_LOGI(s_tag, "\nAnalysis:");
  ESP_LOGI(s_tag, "  Full-duplex: %lld us (1x)", full_duplex_time);
  ESP_LOGI(s_tag, "  Half-duplex: %lld us (%.1fx)",
           half_duplex_time,
           (float)half_duplex_time / full_duplex_time);

  ESP_LOGI(s_tag, "\nNote: Half-duplex takes ~2x time but uses ~1/2 memory");

  heap_caps_free(tx_buf);
  heap_caps_free(rx_buf);
}

/**
 * @brief Demonstrate bidirectional streaming
 */
static void priv_demonstrate_bidirectional_stream(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Bidirectional Streaming ===\n");

  ESP_LOGI(s_tag, "Streaming data in both directions simultaneously\n");

  const int num_chunks = 5;
  const int chunk_size = 32;

  uint8_t tx_chunk[32];
  uint8_t rx_chunk[32];

  for (int i = 0; i < num_chunks; i++) {
    /* Prepare TX chunk */
    for (int j = 0; j < chunk_size; j++) {
      tx_chunk[j] = (uint8_t)((i * chunk_size + j) & 0xFF);
    }

    /* Transfer chunk */
    esp_err_t ret = star_bus_spi_transfer(manager, "fullduplex_device",
                                          tx_chunk, rx_chunk,
                                          chunk_size, 1000);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Chunk %d: TX[0]=0x%02X, RX[0]=0x%02X",
               i, tx_chunk[0], rx_chunk[0]);
    } else {
      ESP_LOGE(s_tag, "Chunk %d failed: %s", i, esp_err_to_name(ret));
      break;
    }
  }

  ESP_LOGI(s_tag, "\nBidirectional streaming complete");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Full-Duplex Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_fullduplex_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create full-duplex SPI device (default mode) */
  spi_device_interface_config_t full_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    /* Note: Full-duplex is the default mode */
  };

  star_bus_config_t *full_config = star_bus_config_create_spi_device(
    "fullduplex_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &full_cfg
  );

  if (full_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create full-duplex config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, full_config);
  ret = star_bus_config_init(full_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init full-duplex: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Create half-duplex SPI device */
  spi_device_interface_config_t half_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = GPIO_NUM_15,  /* Different CS */
    .queue_size     = 7,
    .flags          = SPI_DEVICE_HALFDUPLEX,  /* Enable half-duplex mode */
  };

  star_bus_config_t *half_config = star_bus_config_create_spi_device(
    "halfduplex_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &half_cfg
  );

  if (half_config != NULL) {
    star_bus_manager_add_bus(&manager, half_config);
    ret = star_bus_config_init(half_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Half-duplex device initialized\n");
    }
  }

  ESP_LOGI(s_tag, "SPI devices initialized\n");

  /* Demonstrate different patterns */
  priv_demonstrate_full_duplex(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_command_response(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_jedec_id_read(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_register_access(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_half_duplex(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_compare_performance(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_bidirectional_stream(&manager);

  /* Mode comparison */
  ESP_LOGI(s_tag, "\n=== Mode Comparison ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Full-Duplex:");
  ESP_LOGI(s_tag, "  - Simultaneous TX and RX");
  ESP_LOGI(s_tag, "  - Requires 2 buffers (TX and RX)");
  ESP_LOGI(s_tag, "  - Faster for command-response");
  ESP_LOGI(s_tag, "  - Good for: registers, status, bidirectional data");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Half-Duplex:");
  ESP_LOGI(s_tag, "  - Sequential TX then RX");
  ESP_LOGI(s_tag, "  - Requires 1 buffer");
  ESP_LOGI(s_tag, "  - Memory efficient");
  ESP_LOGI(s_tag, "  - Good for: displays, flash, large unidirectional");

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Best Practices ===");
  ESP_LOGI(s_tag, "1. Use full-duplex for typical SPI peripherals");
  ESP_LOGI(s_tag, "2. Use half-duplex for memory-constrained large transfers");
  ESP_LOGI(s_tag, "3. Account for dummy bytes in command protocols");
  ESP_LOGI(s_tag, "4. Verify response timing in device datasheet");
  ESP_LOGI(s_tag, "5. Consider DMA for transfers > 32 bytes");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI full-duplex example complete");
}
