/**
 * @file 057_spi_flags.c
 * @brief SPI transaction flags and options
 *
 * Demonstrates:
 * - SPI_DEVICE_* flags (HALFDUPLEX, 3WIRE, etc.)
 * - SPI_TRANS_* transaction flags
 * - Flag combinations and effects
 * - Common flag usage patterns
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_FLAGS";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/**
 * @brief Demonstrate device-level flags
 */
static void priv_demonstrate_device_flags(void)
{
  ESP_LOGI(s_tag, "\n=== Device-Level Flags (spi_device_interface_config_t) ===\n");

  ESP_LOGI(s_tag, "SPI_DEVICE_TXBIT_LSBFIRST:");
  ESP_LOGI(s_tag, "  - Transmit LSB first instead of MSB");
  ESP_LOGI(s_tag, "  - Rarely used");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_TXBIT_LSBFIRST");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_RXBIT_LSBFIRST:");
  ESP_LOGI(s_tag, "  - Receive LSB first instead of MSB");
  ESP_LOGI(s_tag, "  - Rarely used");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_RXBIT_LSBFIRST");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_HALFDUPLEX:");
  ESP_LOGI(s_tag, "  - Enable half-duplex mode");
  ESP_LOGI(s_tag, "  - TX and RX happen sequentially");
  ESP_LOGI(s_tag, "  - Saves memory for large transfers");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_HALFDUPLEX");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_3WIRE:");
  ESP_LOGI(s_tag, "  - Use 3-wire mode (no CIPO)");
  ESP_LOGI(s_tag, "  - COPI becomes bidirectional");
  ESP_LOGI(s_tag, "  - Different from omitting CIPO pin");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_3WIRE");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_POSITIVE_CS:");
  ESP_LOGI(s_tag, "  - CS is active high instead of low");
  ESP_LOGI(s_tag, "  - Unusual, device-specific");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_POSITIVE_CS");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_NO_DUMMY:");
  ESP_LOGI(s_tag, "  - Optimize by removing dummy phase");
  ESP_LOGI(s_tag, "  - May improve performance slightly");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_NO_DUMMY");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_DEVICE_DDRCLK:");
  ESP_LOGI(s_tag, "  - Use DDR clock (data on both edges)");
  ESP_LOGI(s_tag, "  - Doubles data rate");
  ESP_LOGI(s_tag, "  - Not all devices support this");
  ESP_LOGI(s_tag, "  - Example: .flags = SPI_DEVICE_DDRCLK");
}

/**
 * @brief Demonstrate HALFDUPLEX flag
 */
static void priv_test_halfduplex_flag(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== SPI_DEVICE_HALFDUPLEX Flag ===\n");

  uint8_t tx_data[64];
  uint8_t rx_data[64];

  /* Fill TX buffer */
  for (int i = 0; i < 64; i++) {
    tx_data[i] = (uint8_t)i;
  }

  ESP_LOGI(s_tag, "Testing half-duplex device (separate TX/RX)...");

  /* Transmit phase */
  esp_err_t ret = star_bus_spi_transmit(manager, "halfduplex_dev",
                                        tx_data, sizeof(tx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transmit: %d bytes sent", sizeof(tx_data));
  } else {
    ESP_LOGE(s_tag, "Transmit failed: %s", esp_err_to_name(ret));
  }

  /* Receive phase */
  ret = star_bus_spi_receive(manager, "halfduplex_dev",
                              rx_data, sizeof(rx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Receive: %d bytes read", sizeof(rx_data));
    ESP_LOGI(s_tag, "First 4 bytes: %02X %02X %02X %02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
  } else {
    ESP_LOGE(s_tag, "Receive failed: %s", esp_err_to_name(ret));
  }

  ESP_LOGI(s_tag, "\nHalf-duplex benefits:");
  ESP_LOGI(s_tag, "  - Memory: only need 1 buffer at a time");
  ESP_LOGI(s_tag, "  - Use case: large display updates, flash writes");
}

/**
 * @brief Demonstrate NO_DUMMY flag
 */
static void priv_test_no_dummy_flag(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== SPI_DEVICE_NO_DUMMY Flag ===\n");

  ESP_LOGI(s_tag, "NO_DUMMY flag optimizations:");
  ESP_LOGI(s_tag, "  - Removes dummy bit phase");
  ESP_LOGI(s_tag, "  - Slightly faster transfers");
  ESP_LOGI(s_tag, "  - Use when device doesn't need setup time");
  ESP_LOGI(s_tag, "");

  uint8_t data[16] = {0x01, 0x02, 0x03, 0x04};

  esp_err_t ret = star_bus_spi_transmit(manager, "no_dummy_dev",
                                        data, sizeof(data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transfer with NO_DUMMY successful");
  } else {
    ESP_LOGE(s_tag, "Transfer failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate flag combinations
 */
static void priv_demonstrate_flag_combinations(void)
{
  ESP_LOGI(s_tag, "\n=== Flag Combinations ===\n");

  ESP_LOGI(s_tag, "Combining multiple flags:");
  ESP_LOGI(s_tag, "  .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY");
  ESP_LOGI(s_tag, "  Effect: Half-duplex mode without dummy phase");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Display with 3-wire:");
  ESP_LOGI(s_tag, "  .flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX");
  ESP_LOGI(s_tag, "  Effect: Bidirectional data on single line");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "High-speed DDR:");
  ESP_LOGI(s_tag, "  .flags = SPI_DEVICE_DDRCLK");
  ESP_LOGI(s_tag, "  Effect: Data on both clock edges (2x speed)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Active-high CS:");
  ESP_LOGI(s_tag, "  .flags = SPI_DEVICE_POSITIVE_CS");
  ESP_LOGI(s_tag, "  Effect: CS high during transfer (unusual)");
}

/**
 * @brief Common device configurations
 */
static void priv_demonstrate_common_configs(void)
{
  ESP_LOGI(s_tag, "\n=== Common Device Configurations ===\n");

  ESP_LOGI(s_tag, "1. Standard SPI Flash:");
  ESP_LOGI(s_tag, "   .flags = 0  // No special flags");
  ESP_LOGI(s_tag, "   .mode = 0");
  ESP_LOGI(s_tag, "   Full-duplex, MSB first");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. TFT Display (ST7789/ILI9341):");
  ESP_LOGI(s_tag, "   .flags = SPI_DEVICE_NO_DUMMY");
  ESP_LOGI(s_tag, "   .mode = 0");
  ESP_LOGI(s_tag, "   Write-only, no CIPO pin");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. SD Card:");
  ESP_LOGI(s_tag, "   .flags = 0");
  ESP_LOGI(s_tag, "   .mode = 0");
  ESP_LOGI(s_tag, "   Full-duplex, standard timing");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. E-Paper Display:");
  ESP_LOGI(s_tag, "   .flags = SPI_DEVICE_HALFDUPLEX");
  ESP_LOGI(s_tag, "   .mode = 0");
  ESP_LOGI(s_tag, "   Large image transfers");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. OLED Display (SSD1306 SPI):");
  ESP_LOGI(s_tag, "   .flags = SPI_DEVICE_NO_DUMMY");
  ESP_LOGI(s_tag, "   .mode = 0");
  ESP_LOGI(s_tag, "   Write-only with DC pin");
}

/**
 * @brief Transaction-level flags (informational)
 */
static void priv_demonstrate_transaction_flags(void)
{
  ESP_LOGI(s_tag, "\n=== Transaction-Level Flags (Advanced) ===\n");

  ESP_LOGI(s_tag, "Note: These are set in spi_transaction_t for direct driver use");
  ESP_LOGI(s_tag, "star_bus API handles these internally\n");

  ESP_LOGI(s_tag, "SPI_TRANS_MODE_DIO:");
  ESP_LOGI(s_tag, "  - Dual I/O mode");
  ESP_LOGI(s_tag, "  - 2 data lines");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_TRANS_MODE_QIO:");
  ESP_LOGI(s_tag, "  - Quad I/O mode");
  ESP_LOGI(s_tag, "  - 4 data lines");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_TRANS_USE_RXDATA:");
  ESP_LOGI(s_tag, "  - Store RX in transaction structure");
  ESP_LOGI(s_tag, "  - For small transfers (< 32 bits)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_TRANS_USE_TXDATA:");
  ESP_LOGI(s_tag, "  - Send from transaction structure");
  ESP_LOGI(s_tag, "  - For small transfers (< 32 bits)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_TRANS_VARIABLE_CMD:");
  ESP_LOGI(s_tag, "  - Variable command length");
  ESP_LOGI(s_tag, "  - Advanced use cases");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "SPI_TRANS_VARIABLE_ADDR:");
  ESP_LOGI(s_tag, "  - Variable address length");
  ESP_LOGI(s_tag, "  - Advanced use cases");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Flags and Options Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_flags_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Device 1: Half-duplex */
  spi_device_interface_config_t halfduplex_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    .flags          = SPI_DEVICE_HALFDUPLEX,  /* Half-duplex flag */
  };

  star_bus_config_t *halfduplex_config = star_bus_config_create_spi_device(
    "halfduplex_dev",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &halfduplex_cfg
  );

  if (halfduplex_config != NULL) {
    star_bus_manager_add_bus(&manager, halfduplex_config);
    ret = star_bus_config_init(halfduplex_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Half-duplex device initialized\n");
    }
  }

  /* Device 2: No dummy */
  spi_device_interface_config_t no_dummy_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = GPIO_NUM_15,
    .queue_size     = 7,
    .flags          = SPI_DEVICE_NO_DUMMY,  /* No dummy flag */
  };

  star_bus_config_t *no_dummy_config = star_bus_config_create_spi_device(
    "no_dummy_dev",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &no_dummy_cfg
  );

  if (no_dummy_config != NULL) {
    star_bus_manager_add_bus(&manager, no_dummy_config);
    ret = star_bus_config_init(no_dummy_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "No-dummy device initialized\n");
    }
  }

  /* Demonstrate flags */
  priv_demonstrate_device_flags();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_halfduplex_flag(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_no_dummy_flag(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_flag_combinations();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_common_configs();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_transaction_flags();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Flags Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Most Common Flags:");
  ESP_LOGI(s_tag, "  1. SPI_DEVICE_HALFDUPLEX - Large transfers");
  ESP_LOGI(s_tag, "  2. SPI_DEVICE_NO_DUMMY - Performance optimization");
  ESP_LOGI(s_tag, "  3. 0 (no flags) - Standard full-duplex");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "When to Use:");
  ESP_LOGI(s_tag, "  HALFDUPLEX: Displays, flash writes, memory-limited");
  ESP_LOGI(s_tag, "  NO_DUMMY: Simple devices, performance critical");
  ESP_LOGI(s_tag, "  3WIRE: Specific devices requiring bidirectional");
  ESP_LOGI(s_tag, "  POSITIVE_CS: Rare, device-specific requirement");
  ESP_LOGI(s_tag, "  DDRCLK: High-speed transfers, if supported");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Default (flags = 0):");
  ESP_LOGI(s_tag, "  - Full-duplex");
  ESP_LOGI(s_tag, "  - MSB first");
  ESP_LOGI(s_tag, "  - Active-low CS");
  ESP_LOGI(s_tag, "  - Standard timing");
  ESP_LOGI(s_tag, "  - Works for most devices");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI flags example complete");
}
