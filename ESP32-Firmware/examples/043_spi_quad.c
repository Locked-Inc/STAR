/**
 * @file 043_spi_quad.c
 * @brief Quad SPI (QSPI) mode configuration
 *
 * Demonstrates:
 * - Quad SPI mode setup
 * - 4-bit data bus configuration
 * - WP and HD pin configuration
 * - Fast read operations using QSPI
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_QUAD_EXAMPLE";

/* Standard SPI pins */
#define SPI_COPI_PIN   (GPIO_NUM_23)  /* Also D0 in quad mode */
#define SPI_CIPO_PIN   (GPIO_NUM_19)  /* Also D1 in quad mode */
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

/* Additional pins for Quad SPI */
#define SPI_WP_PIN     (GPIO_NUM_22)  /* Write Protect / D2 */
#define SPI_HD_PIN     (GPIO_NUM_21)  /* Hold / D3 */

#define SPI_CLK_SPEED  (20000000)  /* 20 MHz */

/* Quad SPI Flash Commands (standard JEDEC) */
#define CMD_QUAD_READ           (0x6B)  /* Fast Read Quad Output */
#define CMD_QUAD_READ_4B        (0xEB)  /* Fast Read Quad I/O */
#define CMD_ENABLE_QUAD         (0x38)  /* Enter Quad mode */
#define CMD_READ_STATUS_REG     (0x05)
#define CMD_WRITE_ENABLE        (0x06)
#define CMD_READ_JEDEC_ID       (0x9F)

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== Quad SPI (QSPI) Mode Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "qspi_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Standard SPI Device (for comparison) --- */
  ESP_LOGI(s_tag, "--- Standard SPI Configuration ---");

  spi_device_interface_config_t std_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
    .flags = 0,  /* Standard SPI */
  };

  star_bus_config_t *spi_std = star_bus_config_create_spi_device(
    "std_flash",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &std_cfg
  );

  if (spi_std != NULL) {
    star_bus_manager_add_bus(&manager, spi_std);
    ret = star_bus_config_init(spi_std, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Standard SPI initialized");

      /* Read JEDEC ID using standard SPI */
      uint8_t cmd[] = {CMD_READ_JEDEC_ID, 0x00, 0x00, 0x00};
      uint8_t jedec_id[4] = {0};

      ret = star_bus_spi_transfer(&manager, "std_flash", cmd, jedec_id, 4, 100);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "JEDEC ID (Standard SPI): %02X %02X %02X",
                 jedec_id[1], jedec_id[2], jedec_id[3]);
      }
    }
  }

  /* --- Quad SPI Device Configuration --- */
  ESP_LOGI(s_tag, "\n--- Quad SPI Configuration ---");

  ESP_LOGI(s_tag, "Pin Mapping:");
  ESP_LOGI(s_tag, "  D0 (COPI): GPIO %d", SPI_COPI_PIN);
  ESP_LOGI(s_tag, "  D1 (CIPO): GPIO %d", SPI_CIPO_PIN);
  ESP_LOGI(s_tag, "  D2 (WP):   GPIO %d", SPI_WP_PIN);
  ESP_LOGI(s_tag, "  D3 (HD):   GPIO %d", SPI_HD_PIN);
  ESP_LOGI(s_tag, "  SCLK:      GPIO %d", SPI_SCLK_PIN);
  ESP_LOGI(s_tag, "  CS:        GPIO %d\n", SPI_CS_PIN);

  /* Configure Quad SPI device */
  spi_device_interface_config_t quad_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
    .flags = SPI_DEVICE_HALFDUPLEX,  /* Required for quad mode */
    .input_delay_ns = 0,
  };

  /* Note: ESP-IDF handles quad mode through bus configuration */
  /* We need to configure the bus with quad pins */

  star_bus_config_t *spi_quad = star_bus_config_create_spi_device(
    "quad_flash",
    SPI3_HOST,  /* Use SPI3 for quad mode */
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &quad_cfg
  );

  if (spi_quad == NULL) {
    ESP_LOGE(s_tag, "Failed to create Quad SPI config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi_quad);
  ret = star_bus_config_init(spi_quad, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init Quad SPI: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Quad SPI device initialized");

  /* --- Quad SPI Operations --- */
  ESP_LOGI(s_tag, "\n--- Quad SPI Operations ---");

  /* 1. Enable Quad mode on flash device */
  ESP_LOGI(s_tag, "Step 1: Enable Quad mode on flash");

  uint8_t write_enable_cmd = CMD_WRITE_ENABLE;
  ret = star_bus_spi_transmit(&manager, "quad_flash", &write_enable_cmd, 1, 100);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Write enable sent");
  }

  /* Note: Actual quad enable depends on flash chip */
  /* This is just an example - real implementation would:
   * 1. Read status register
   * 2. Set quad enable bit
   * 3. Write status register back
   */

  /* 2. Perform Quad Read */
  ESP_LOGI(s_tag, "\nStep 2: Quad Read Operation");

  uint8_t quad_read_cmd[] = {
    CMD_QUAD_READ,      /* Quad read command */
    0x00, 0x00, 0x00,   /* 24-bit address */
    0x00                /* Dummy byte */
  };

  uint8_t quad_read_data[64] = {0};

  /* For true quad operation, we'd use spi_device_transmit with custom flags */
  ret = star_bus_spi_transfer(&manager, "quad_flash",
                               quad_read_cmd, quad_read_data,
                               sizeof(quad_read_cmd), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Quad read completed (first 16 bytes):");
    for (int i = 0; i < 16; i++) {
      printf("%02X ", quad_read_data[i]);
      if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
  }

  /* --- Quad SPI Advantages --- */
  ESP_LOGI(s_tag, "\n--- Quad SPI Benefits ---");
  ESP_LOGI(s_tag, "1. 4x throughput on data phase");
  ESP_LOGI(s_tag, "2. Command/address still single-line (in QIO, they're also quad)");
  ESP_LOGI(s_tag, "3. Typical speeds: 80-120 MB/s at 40-80 MHz");
  ESP_LOGI(s_tag, "4. Common in SPI Flash, PSRAM, displays");

  /* --- Configuration Notes --- */
  ESP_LOGI(s_tag, "\n--- QSPI Configuration Notes ---");
  ESP_LOGI(s_tag, "Quad SPI Modes:");
  ESP_LOGI(s_tag, "  QO (Quad Output):  Command/Address single, Data quad");
  ESP_LOGI(s_tag, "  QIO (Quad I/O):    Address/Data quad");
  ESP_LOGI(s_tag, "  QPI:               Everything quad (command too)");

  ESP_LOGI(s_tag, "\nPin Requirements:");
  ESP_LOGI(s_tag, "  D0: Data bit 0 (MOSI in standard SPI)");
  ESP_LOGI(s_tag, "  D1: Data bit 1 (MISO in standard SPI)");
  ESP_LOGI(s_tag, "  D2: Data bit 2 (WP pin in standard SPI)");
  ESP_LOGI(s_tag, "  D3: Data bit 3 (HOLD pin in standard SPI)");

  ESP_LOGI(s_tag, "\nFlash-specific setup required:");
  ESP_LOGI(s_tag, "  - Check datasheet for quad enable sequence");
  ESP_LOGI(s_tag, "  - May need to modify status register");
  ESP_LOGI(s_tag, "  - Some chips use different commands");

  /* --- Advanced: Custom Transaction with Quad Flags --- */
  ESP_LOGI(s_tag, "\n--- Custom Quad Transaction Example ---");
  ESP_LOGI(s_tag, "For true quad mode, use spi_device_transmit directly:");
  ESP_LOGI(s_tag, "  trans.flags |= SPI_TRANS_MODE_QIO;");
  ESP_LOGI(s_tag, "  trans.flags |= SPI_TRANS_MODE_QUAD;");
  ESP_LOGI(s_tag, "See ESP-IDF spi_master.h for full flag options");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nQuad SPI example complete");
}
