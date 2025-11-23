/**
 * @file 049_spi_multi_host.c
 * @brief Multiple SPI host controller usage
 *
 * Demonstrates:
 * - Using SPI2 and SPI3 simultaneously
 * - Independent SPI hosts on different pins
 * - Concurrent SPI operations
 * - Multi-host pin configuration
 * - Performance comparison
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SPI_MULTI_HOST";

/* SPI2 (HSPI) pins */
#define SPI2_COPI_PIN   (GPIO_NUM_23)
#define SPI2_CIPO_PIN   (GPIO_NUM_19)
#define SPI2_SCLK_PIN   (GPIO_NUM_18)
#define SPI2_CS_PIN     (GPIO_NUM_5)

/* SPI3 (VSPI) pins */
#define SPI3_COPI_PIN   (GPIO_NUM_13)
#define SPI3_CIPO_PIN   (GPIO_NUM_12)
#define SPI3_SCLK_PIN   (GPIO_NUM_14)
#define SPI3_CS_PIN     (GPIO_NUM_15)

#define SPI_CLK_SPEED   (10000000)  /* 10 MHz */

/**
 * @brief Test transfer on specific host
 */
static void priv_test_host_transfer(star_bus_manager_t *manager,
                                   const char *device_name,
                                   const char *host_name)
{
  ESP_LOGI(s_tag, "\n=== Testing %s ===\n", host_name);

  uint8_t tx_data[] = {0x9F, 0x00, 0x00, 0x00};  /* Read ID command */
  uint8_t rx_data[4] = {0};

  int64_t start = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transfer(manager, device_name,
                                        tx_data, rx_data,
                                        sizeof(tx_data), 1000);

  int64_t elapsed = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "%s transfer successful:", host_name);
    ESP_LOGI(s_tag, "  TX: %02X %02X %02X %02X",
             tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
    ESP_LOGI(s_tag, "  RX: %02X %02X %02X %02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    ESP_LOGI(s_tag, "  Time: %lld us\n", elapsed);
  } else {
    ESP_LOGE(s_tag, "%s transfer failed: %s",
             host_name, esp_err_to_name(ret));
  }
}

/**
 * @brief Test concurrent transfers on both hosts
 */
static void priv_test_concurrent_transfers(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Testing Concurrent Transfers ===\n");

  ESP_LOGI(s_tag, "Note: Transfers are serialized by bus manager mutex");
  ESP_LOGI(s_tag, "Each host can operate independently\n");

  /* Alternate between hosts */
  for (int i = 0; i < 5; i++) {
    ESP_LOGI(s_tag, "Round %d:", i + 1);

    /* Transfer on SPI2 */
    uint8_t tx2[4] = {0x03, 0x00, 0x00, (uint8_t)i};
    uint8_t rx2[4] = {0};
    star_bus_spi_transfer(manager, "spi2_device", tx2, rx2, 4, 1000);
    ESP_LOGI(s_tag, "  SPI2: TX=%02X RX=%02X", tx2[0], rx2[0]);

    /* Transfer on SPI3 */
    uint8_t tx3[4] = {0x05, 0x00, 0x00, (uint8_t)i};
    uint8_t rx3[4] = {0};
    star_bus_spi_transfer(manager, "spi3_device", tx3, rx3, 4, 1000);
    ESP_LOGI(s_tag, "  SPI3: TX=%02X RX=%02X\n", tx3[0], rx3[0]);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * @brief Compare host performance
 */
static void priv_compare_host_performance(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Host Performance Comparison ===\n");

  const int num_transfers = 100;
  uint8_t tx_data[32];
  uint8_t rx_data[32];

  memset(tx_data, 0xAA, sizeof(tx_data));

  /* Test SPI2 */
  int64_t start2 = esp_timer_get_time();
  for (int i = 0; i < num_transfers; i++) {
    star_bus_spi_transfer(manager, "spi2_device",
                         tx_data, rx_data, sizeof(tx_data), 1000);
  }
  int64_t elapsed2 = esp_timer_get_time() - start2;

  /* Test SPI3 */
  int64_t start3 = esp_timer_get_time();
  for (int i = 0; i < num_transfers; i++) {
    star_bus_spi_transfer(manager, "spi3_device",
                         tx_data, rx_data, sizeof(tx_data), 1000);
  }
  int64_t elapsed3 = esp_timer_get_time() - start3;

  /* Print results */
  ESP_LOGI(s_tag, "SPI2 (HSPI):");
  ESP_LOGI(s_tag, "  %d transfers of %d bytes", num_transfers, sizeof(tx_data));
  ESP_LOGI(s_tag, "  Total time: %lld us", elapsed2);
  ESP_LOGI(s_tag, "  Per transfer: %lld us", elapsed2 / num_transfers);
  ESP_LOGI(s_tag, "  Throughput: %.2f KB/s\n",
           (num_transfers * sizeof(tx_data) * 1000000.0) / (elapsed2 * 1024.0));

  ESP_LOGI(s_tag, "SPI3 (VSPI):");
  ESP_LOGI(s_tag, "  %d transfers of %d bytes", num_transfers, sizeof(tx_data));
  ESP_LOGI(s_tag, "  Total time: %lld us", elapsed3);
  ESP_LOGI(s_tag, "  Per transfer: %lld us", elapsed3 / num_transfers);
  ESP_LOGI(s_tag, "  Throughput: %.2f KB/s\n",
           (num_transfers * sizeof(tx_data) * 1000000.0) / (elapsed3 * 1024.0));
}

/**
 * @brief Print multi-host configuration
 */
static void priv_print_multi_host_info(void)
{
  ESP_LOGI(s_tag, "\n=== Multi-Host SPI Configuration ===\n");

  ESP_LOGI(s_tag, "ESP32 has 3 SPI controllers:");
  ESP_LOGI(s_tag, "  SPI0: Reserved for flash memory");
  ESP_LOGI(s_tag, "  SPI1: Reserved for flash memory");
  ESP_LOGI(s_tag, "  SPI2: HSPI - Available for user");
  ESP_LOGI(s_tag, "  SPI3: VSPI - Available for user\n");

  ESP_LOGI(s_tag, "This example uses:");
  ESP_LOGI(s_tag, "  SPI2 (HSPI):");
  ESP_LOGI(s_tag, "    COPI: GPIO%d", SPI2_COPI_PIN);
  ESP_LOGI(s_tag, "    CIPO: GPIO%d", SPI2_CIPO_PIN);
  ESP_LOGI(s_tag, "    SCLK: GPIO%d", SPI2_SCLK_PIN);
  ESP_LOGI(s_tag, "    CS:   GPIO%d\n", SPI2_CS_PIN);

  ESP_LOGI(s_tag, "  SPI3 (VSPI):");
  ESP_LOGI(s_tag, "    COPI: GPIO%d", SPI3_COPI_PIN);
  ESP_LOGI(s_tag, "    CIPO: GPIO%d", SPI3_CIPO_PIN);
  ESP_LOGI(s_tag, "    SCLK: GPIO%d", SPI3_SCLK_PIN);
  ESP_LOGI(s_tag, "    CS:   GPIO%d\n", SPI3_CS_PIN);

  ESP_LOGI(s_tag, "Benefits of multiple hosts:");
  ESP_LOGI(s_tag, "  - Independent pin configurations");
  ESP_LOGI(s_tag, "  - Different clock speeds per host");
  ESP_LOGI(s_tag, "  - Separate DMA channels");
  ESP_LOGI(s_tag, "  - Can operate on different SPI modes");
  ESP_LOGI(s_tag, "  - Useful for different device types\n");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Multi-Host Example ===\n");

  priv_print_multi_host_info();

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "multi_host_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create SPI2 device */
  spi_device_interface_config_t dev2_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI2_CS_PIN,
    .queue_size = 7,
  };

  star_bus_config_t *spi2_config = star_bus_config_create_spi_device(
    "spi2_device",
    SPI2_HOST,
    SPI2_COPI_PIN,
    SPI2_CIPO_PIN,
    SPI2_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev2_cfg
  );

  if (spi2_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI2 config");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Create SPI3 device */
  spi_device_interface_config_t dev3_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI3_CS_PIN,
    .queue_size = 7,
  };

  star_bus_config_t *spi3_config = star_bus_config_create_spi_device(
    "spi3_device",
    SPI3_HOST,
    SPI3_COPI_PIN,
    SPI3_CIPO_PIN,
    SPI3_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev3_cfg
  );

  if (spi3_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI3 config");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Add and initialize both hosts */
  star_bus_manager_add_bus(&manager, spi2_config);
  ret = star_bus_config_init(spi2_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI2: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi3_config);
  ret = star_bus_config_init(spi3_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI3: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Both SPI hosts initialized\n");

  /* Test individual hosts */
  priv_test_host_transfer(&manager, "spi2_device", "SPI2 (HSPI)");
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_host_transfer(&manager, "spi3_device", "SPI3 (VSPI)");
  vTaskDelay(pdMS_TO_TICKS(200));

  /* Test concurrent usage */
  priv_test_concurrent_transfers(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  /* Compare performance */
  priv_compare_host_performance(&manager);

  /* Use case examples */
  ESP_LOGI(s_tag, "\n=== Multi-Host Use Cases ===\n");

  ESP_LOGI(s_tag, "1. Different device types:");
  ESP_LOGI(s_tag, "   SPI2: SD card (slow, mode 0)");
  ESP_LOGI(s_tag, "   SPI3: Display (fast, mode 3)\n");

  ESP_LOGI(s_tag, "2. Different speeds:");
  ESP_LOGI(s_tag, "   SPI2: Sensor @ 1 MHz");
  ESP_LOGI(s_tag, "   SPI3: Flash @ 80 MHz\n");

  ESP_LOGI(s_tag, "3. Pin availability:");
  ESP_LOGI(s_tag, "   SPI2: Using default pins");
  ESP_LOGI(s_tag, "   SPI3: Using alternate pins\n");

  ESP_LOGI(s_tag, "4. DMA isolation:");
  ESP_LOGI(s_tag, "   Each host has separate DMA");
  ESP_LOGI(s_tag, "   No DMA channel conflicts\n");

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Key Points:");
  ESP_LOGI(s_tag, "  - ESP32 has 2 user-accessible SPI hosts (SPI2, SPI3)");
  ESP_LOGI(s_tag, "  - Each host has independent pins and configuration");
  ESP_LOGI(s_tag, "  - Hosts can operate at different speeds/modes");
  ESP_LOGI(s_tag, "  - Star bus manager handles both hosts seamlessly");
  ESP_LOGI(s_tag, "  - Useful for complex systems with multiple SPI devices");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI multi-host example complete");
}
