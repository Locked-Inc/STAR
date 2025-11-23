/**
 * @file 047_spi_speeds.c
 * @brief Various SPI clock speeds: 1MHz, 10MHz, 20MHz, 40MHz, 80MHz
 *
 * Demonstrates:
 * - Different clock speed configurations
 * - Speed vs reliability tradeoffs
 * - Performance measurement
 * - Speed limitations and recommendations
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_SPEEDS";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

#define TEST_BUFFER_SIZE (1024)  /* 1KB test buffer */

/**
 * @brief Create and test SPI device at specific speed
 */
static void priv_test_spi_speed(star_bus_manager_t *manager,
                                const char *device_name,
                           int speed_hz,
                           int cs_pin)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "\n=== Testing %d Hz (%.2f MHz) ===",
           speed_hz, speed_hz / 1000000.0);

  /* Create device configuration */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = speed_hz,
    .mode = 0,
    .spics_io_num = cs_pin,
    .queue_size = 7,
    .flags = 0,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    device_name,
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  star_bus_manager_add_bus(manager, spi_config);
  ret = star_bus_config_init(spi_config, manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* Allocate test buffers */
  uint8_t *tx_buffer = (uint8_t *)heap_caps_malloc(TEST_BUFFER_SIZE, MALLOC_CAP_DMA);
  uint8_t *rx_buffer = (uint8_t *)heap_caps_malloc(TEST_BUFFER_SIZE, MALLOC_CAP_DMA);

  if (tx_buffer == NULL || rx_buffer == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buffer) heap_caps_free(tx_buffer);
    if (rx_buffer) heap_caps_free(rx_buffer);
    return;
  }

  /* Fill TX buffer with test pattern */
  for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
    tx_buffer[i] = (uint8_t)(i & 0xFF);
  }

  /* --- Performance Test --- */
  ESP_LOGI(s_tag, "Starting performance test (%d bytes)...", TEST_BUFFER_SIZE);

  int64_t start_time = esp_timer_get_time();

  ret = star_bus_spi_transfer(manager, device_name,
                               tx_buffer, rx_buffer,
                               TEST_BUFFER_SIZE, 1000);

  int64_t end_time = esp_timer_get_time();
  int64_t elapsed_us = end_time - start_time;

  if (ret == ESP_OK) {
    /* Calculate throughput */
    double elapsed_sec = elapsed_us / 1000000.0;
    double throughput_kbps = (TEST_BUFFER_SIZE * 8) / elapsed_sec / 1000.0;
    double throughput_mbps = throughput_kbps / 1000.0;

    ESP_LOGI(s_tag, "Results:");
    ESP_LOGI(s_tag, "  Time:       %lld us", elapsed_us);
    ESP_LOGI(s_tag, "  Throughput: %.2f Kbps (%.3f Mbps)", throughput_kbps, throughput_mbps);
    ESP_LOGI(s_tag, "  Efficiency: %.1f%% of theoretical max",
             (throughput_mbps / (speed_hz / 1000000.0)) * 100.0);

    /* Verify first few bytes */
    ESP_LOGI(s_tag, "  RX verify:  %02X %02X %02X %02X ... %s",
             rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
             (rx_buffer[0] == 0x00 && rx_buffer[1] == 0x01) ? "OK" : "MISMATCH");
  } else {
    ESP_LOGE(s_tag, "Transfer FAILED: %s", esp_err_to_name(ret));
  }

  /* Cleanup */
  heap_caps_free(tx_buffer);
  heap_caps_free(rx_buffer);
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Speed Comparison Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_speed_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Speed Theory --- */
  ESP_LOGI(s_tag, "--- SPI Speed Information ---");
  ESP_LOGI(s_tag, "ESP32 SPI Clock Limits:");
  ESP_LOGI(s_tag, "  Max speed: 80 MHz (from 80 MHz APB clock)");
  ESP_LOGI(s_tag, "  Common speeds: 1, 5, 10, 20, 40, 80 MHz");
  ESP_LOGI(s_tag, "  Speed = APB_CLK / divider (divider must be even)");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Factors affecting achievable speed:");
  ESP_LOGI(s_tag, "  - Wire length (longer = slower max speed)");
  ESP_LOGI(s_tag, "  - PCB trace quality");
  ESP_LOGI(s_tag, "  - Device capabilities");
  ESP_LOGI(s_tag, "  - Power supply stability");
  ESP_LOGI(s_tag, "  - Electrical noise");
  ESP_LOGI(s_tag, "");

  /* --- Test 1: 1 MHz (slow but reliable) --- */
  priv_test_spi_speed(&manager, "spi_1mhz", 1000000, SPI_CS_PIN);

  /* --- Test 2: 5 MHz --- */
  priv_test_spi_speed(&manager, "spi_5mhz", 5000000, GPIO_NUM_15);

  /* --- Test 3: 10 MHz (common) --- */
  priv_test_spi_speed(&manager, "spi_10mhz", 10000000, GPIO_NUM_16);

  /* --- Test 4: 20 MHz (fast) --- */
  priv_test_spi_speed(&manager, "spi_20mhz", 20000000, GPIO_NUM_17);

  /* --- Test 5: 40 MHz (very fast) --- */
  priv_test_spi_speed(&manager, "spi_40mhz", 40000000, GPIO_NUM_21);

  /* --- Test 6: 80 MHz (maximum for ESP32) --- */
  ESP_LOGI(s_tag, "\n=== Testing 80 MHz (Maximum Speed) ===");
  ESP_LOGI(s_tag, "Note: 80 MHz may be unreliable with:");
  ESP_LOGI(s_tag, "  - Long wires (> 10cm)");
  ESP_LOGI(s_tag, "  - Breadboard connections");
  ESP_LOGI(s_tag, "  - Devices not rated for 80 MHz");
  priv_test_spi_speed(&manager, "spi_80mhz", 80000000, GPIO_NUM_22);

  /* --- Speed Recommendations --- */
  ESP_LOGI(s_tag, "\n\n--- Speed Selection Guidelines ---");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "1 MHz:");
  ESP_LOGI(s_tag, "  - Initial testing and debugging");
  ESP_LOGI(s_tag, "  - Long cables (> 1 meter)");
  ESP_LOGI(s_tag, "  - Noisy environments");
  ESP_LOGI(s_tag, "  - When reliability > speed");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "5-10 MHz:");
  ESP_LOGI(s_tag, "  - General purpose use");
  ESP_LOGI(s_tag, "  - SD cards (initial communication)");
  ESP_LOGI(s_tag, "  - Most sensors");
  ESP_LOGI(s_tag, "  - Good balance of speed/reliability");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "20-40 MHz:");
  ESP_LOGI(s_tag, "  - SPI Flash memory");
  ESP_LOGI(s_tag, "  - Fast displays");
  ESP_LOGI(s_tag, "  - Short PCB traces (< 10cm)");
  ESP_LOGI(s_tag, "  - High-speed data logging");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "80 MHz:");
  ESP_LOGI(s_tag, "  - On-board SPI Flash (ESP32 module)");
  ESP_LOGI(s_tag, "  - Very short traces (< 5cm)");
  ESP_LOGI(s_tag, "  - Devices specifically rated for it");
  ESP_LOGI(s_tag, "  - Requires good PCB design");

  /* --- Troubleshooting --- */
  ESP_LOGI(s_tag, "\n--- Speed-Related Issues ---");
  ESP_LOGI(s_tag, "Symptoms of too-high speed:");
  ESP_LOGI(s_tag, "  - Random data corruption");
  ESP_LOGI(s_tag, "  - Intermittent failures");
  ESP_LOGI(s_tag, "  - Wrong data read from device");
  ESP_LOGI(s_tag, "  - Works on desk, fails in enclosure");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Solutions:");
  ESP_LOGI(s_tag, "  1. Reduce clock speed");
  ESP_LOGI(s_tag, "  2. Shorten wire length");
  ESP_LOGI(s_tag, "  3. Add series termination resistors (22-100 ohm)");
  ESP_LOGI(s_tag, "  4. Improve power supply decoupling");
  ESP_LOGI(s_tag, "  5. Use twisted pair for clock/data");
  ESP_LOGI(s_tag, "  6. Check ground connections");

  /* --- Device-Specific Speeds --- */
  ESP_LOGI(s_tag, "\n--- Typical Device Speed Limits ---");
  ESP_LOGI(s_tag, "SD Card:         25 MHz (SD mode), 50 MHz (UHS mode)");
  ESP_LOGI(s_tag, "SPI Flash:       20-133 MHz (chip dependent)");
  ESP_LOGI(s_tag, "ILI9341 Display: 40 MHz");
  ESP_LOGI(s_tag, "ST7789 Display:  60 MHz");
  ESP_LOGI(s_tag, "MPU6050 Sensor:  1 MHz (SPI mode)");
  ESP_LOGI(s_tag, "MCP3008 ADC:     3.6 MHz (5V), 1.35 MHz (2.7V)");
  ESP_LOGI(s_tag, "NRF24L01 Radio:  10 MHz");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI speed comparison complete");
}
