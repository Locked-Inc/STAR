/**
 * @file 044_spi_all_modes.c
 * @brief SPI modes 0, 1, 2, 3 (CPOL/CPHA combinations)
 *
 * Demonstrates:
 * - All 4 SPI modes (combinations of CPOL and CPHA)
 * - Clock polarity (CPOL) effects
 * - Clock phase (CPHA) effects
 * - Mode selection for different devices
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_MODES_EXAMPLE";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (1000000)  /* 1 MHz for clear observation */

/**
 * @brief Test SPI communication with a specific mode
 */
static void priv_test_spi_mode(star_bus_manager_t* manager, const char* device_name,
                          int mode, const char* mode_desc)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "\n=== Testing %s ===", mode_desc);

  /* Test data */
  uint8_t tx_data[] = {0xAA, 0x55, 0x12, 0x34};
  uint8_t rx_data[4] = {0};

  /* Perform transfer */
  ret = star_bus_spi_transfer(manager, device_name, tx_data, rx_data,
                               sizeof(tx_data), 100);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "TX: %02X %02X %02X %02X",
             tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
    ESP_LOGI(s_tag, "RX: %02X %02X %02X %02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    ESP_LOGI(s_tag, "Mode %d transfer: SUCCESS", mode);
  } else {
    ESP_LOGE(s_tag, "Mode %d transfer: FAILED (%s)", mode, esp_err_to_name(ret));
  }
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Modes (CPOL/CPHA) Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_modes_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- SPI Mode Theory --- */
  ESP_LOGI(s_tag, "--- SPI Mode Theory ---");
  ESP_LOGI(s_tag, "CPOL (Clock Polarity):");
  ESP_LOGI(s_tag, "  0 = Clock idle LOW");
  ESP_LOGI(s_tag, "  1 = Clock idle HIGH");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "CPHA (Clock Phase):");
  ESP_LOGI(s_tag, "  0 = Data sampled on leading edge, shifted on trailing");
  ESP_LOGI(s_tag, "  1 = Data shifted on leading edge, sampled on trailing");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Mode | CPOL | CPHA | Idle | Sample Edge");
  ESP_LOGI(s_tag, "-----|------|------|------|-------------");
  ESP_LOGI(s_tag, "  0  |  0   |  0   | LOW  | Rising");
  ESP_LOGI(s_tag, "  1  |  0   |  1   | LOW  | Falling");
  ESP_LOGI(s_tag, "  2  |  1   |  0   | HIGH | Falling");
  ESP_LOGI(s_tag, "  3  |  1   |  1   | HIGH | Rising");
  ESP_LOGI(s_tag, "");

  /* --- MODE 0: CPOL=0, CPHA=0 --- */
  ESP_LOGI(s_tag, "\n--- Creating MODE 0 Device ---");

  spi_device_interface_config_t mode0_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,  /* CPOL=0, CPHA=0 */
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
  };

  star_bus_config_t *spi_mode0 = star_bus_config_create_spi_device(
    "mode0_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &mode0_cfg
  );

  if (spi_mode0 != NULL) {
    star_bus_manager_add_bus(&manager, spi_mode0);
    ret = star_bus_config_init(spi_mode0, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "MODE 0 initialized");
      priv_test_spi_mode(&manager, "mode0_device", 0,
                    "MODE 0 (CPOL=0, CPHA=0) - Most Common");
    }
  }

  /* --- MODE 1: CPOL=0, CPHA=1 --- */
  ESP_LOGI(s_tag, "\n--- Creating MODE 1 Device ---");

  spi_device_interface_config_t mode1_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 1,  /* CPOL=0, CPHA=1 */
    .spics_io_num = GPIO_NUM_15,  /* Different CS */
    .queue_size = 7,
  };

  star_bus_config_t *spi_mode1 = star_bus_config_create_spi_device(
    "mode1_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &mode1_cfg
  );

  if (spi_mode1 != NULL) {
    star_bus_manager_add_bus(&manager, spi_mode1);
    ret = star_bus_config_init(spi_mode1, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "MODE 1 initialized");
      priv_test_spi_mode(&manager, "mode1_device", 1,
                    "MODE 1 (CPOL=0, CPHA=1)");
    }
  }

  /* --- MODE 2: CPOL=1, CPHA=0 --- */
  ESP_LOGI(s_tag, "\n--- Creating MODE 2 Device ---");

  spi_device_interface_config_t mode2_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 2,  /* CPOL=1, CPHA=0 */
    .spics_io_num = GPIO_NUM_16,  /* Different CS */
    .queue_size = 7,
  };

  star_bus_config_t *spi_mode2 = star_bus_config_create_spi_device(
    "mode2_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &mode2_cfg
  );

  if (spi_mode2 != NULL) {
    star_bus_manager_add_bus(&manager, spi_mode2);
    ret = star_bus_config_init(spi_mode2, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "MODE 2 initialized");
      priv_test_spi_mode(&manager, "mode2_device", 2,
                    "MODE 2 (CPOL=1, CPHA=0)");
    }
  }

  /* --- MODE 3: CPOL=1, CPHA=1 --- */
  ESP_LOGI(s_tag, "\n--- Creating MODE 3 Device ---");

  spi_device_interface_config_t mode3_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 3,  /* CPOL=1, CPHA=1 */
    .spics_io_num = GPIO_NUM_17,  /* Different CS */
    .queue_size = 7,
  };

  star_bus_config_t *spi_mode3 = star_bus_config_create_spi_device(
    "mode3_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &mode3_cfg
  );

  if (spi_mode3 != NULL) {
    star_bus_manager_add_bus(&manager, spi_mode3);
    ret = star_bus_config_init(spi_mode3, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "MODE 3 initialized");
      priv_test_spi_mode(&manager, "mode3_device", 3,
                    "MODE 3 (CPOL=1, CPHA=1)");
    }
  }

  /* --- Common Device Modes --- */
  ESP_LOGI(s_tag, "\n--- Common Device Mode Usage ---");
  ESP_LOGI(s_tag, "Mode 0 (CPOL=0, CPHA=0):");
  ESP_LOGI(s_tag, "  - SD Cards");
  ESP_LOGI(s_tag, "  - Most SPI Flash chips");
  ESP_LOGI(s_tag, "  - Many sensors (MPU6050, etc.)");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Mode 1 (CPOL=0, CPHA=1):");
  ESP_LOGI(s_tag, "  - Some accelerometers");
  ESP_LOGI(s_tag, "  - Less common");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Mode 2 (CPOL=1, CPHA=0):");
  ESP_LOGI(s_tag, "  - Less common");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Mode 3 (CPOL=1, CPHA=1):");
  ESP_LOGI(s_tag, "  - Some displays (ILI9341, ST7789)");
  ESP_LOGI(s_tag, "  - Some DACs/ADCs");
  ESP_LOGI(s_tag, "  - Real-time clocks");

  /* --- Troubleshooting --- */
  ESP_LOGI(s_tag, "\n--- Mode Selection Troubleshooting ---");
  ESP_LOGI(s_tag, "If communication fails:");
  ESP_LOGI(s_tag, "  1. Check device datasheet for required mode");
  ESP_LOGI(s_tag, "  2. Try all 4 modes systematically");
  ESP_LOGI(s_tag, "  3. Use logic analyzer to verify timing");
  ESP_LOGI(s_tag, "  4. Verify CS polarity (active low is standard)");
  ESP_LOGI(s_tag, "  5. Check clock speed (start slow, e.g., 1 MHz)");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Always check device datasheet first");
  ESP_LOGI(s_tag, "2. Mode 0 is most common - try it first");
  ESP_LOGI(s_tag, "3. Use logic analyzer for debugging");
  ESP_LOGI(s_tag, "4. Different devices on same bus can use different modes");
  ESP_LOGI(s_tag, "5. Mode is set per device, not per bus");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI modes example complete");
}
