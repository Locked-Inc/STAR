/**
 * @file 053_spi_optional_cipo.c
 * @brief Optional CIPO (MISO) pin configuration
 *
 * Demonstrates:
 * - SPI without CIPO pin (transmit-only devices)
 * - Write-only SPI peripherals (LEDs, DACs, displays)
 * - GPIO savings when RX not needed
 * - 3-wire SPI mode
 * - Performance optimization for TX-only
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_NO_CIPO";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/* Note: No CIPO_PIN defined - we're not using it! */

/**
 * @brief Demonstrate SPI without CIPO (write-only)
 */
static void priv_demonstrate_no_cipo(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== SPI Without CIPO Pin ===\n");

  ESP_LOGI(s_tag, "Transmitting data without CIPO connection...");

  uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

  /* Use transmit function - no receive buffer needed */
  esp_err_t ret = star_bus_spi_transmit(manager, "no_cipo_device",
                                        tx_data, sizeof(tx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transmit successful: %d bytes", sizeof(tx_data));
    ESP_LOGI(s_tag, "Data: %02X %02X %02X %02X %02X %02X %02X %02X",
             tx_data[0], tx_data[1], tx_data[2], tx_data[3],
             tx_data[4], tx_data[5], tx_data[6], tx_data[7]);
  } else {
    ESP_LOGE(s_tag, "Transmit failed: %s", esp_err_to_name(ret));
  }

  ESP_LOGI(s_tag, "\nNote: No CIPO pin connected - saves 1 GPIO!");
}

/**
 * @brief Demonstrate LED strip control (write-only SPI)
 */
static void priv_demonstrate_led_strip(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== LED Strip Control (Write-Only) ===\n");

  ESP_LOGI(s_tag, "Controlling WS2801/APA102 LED strip via SPI...\n");

  /* APA102 LED format: 4-byte start frame + 4 bytes per LED */
  const int num_leds = 8;
  const int frame_size = 4 + (num_leds * 4) + 4;  /* Start + LEDs + End */

  uint8_t led_data[frame_size];
  memset(led_data, 0, frame_size);

  /* Start frame: 32 zero bits */
  led_data[0] = 0x00;
  led_data[1] = 0x00;
  led_data[2] = 0x00;
  led_data[3] = 0x00;

  /* LED frames: 11100000 + brightness + B + G + R */
  for (int i = 0; i < num_leds; i++) {
    int offset = 4 + (i * 4);
    led_data[offset + 0] = 0xE0 | 0x1F;  /* Global + brightness */
    led_data[offset + 1] = 0xFF;         /* Blue */
    led_data[offset + 2] = 0x00;         /* Green */
    led_data[offset + 3] = (uint8_t)(i * 32);  /* Red gradient */
  }

  /* End frame: 32 one bits */
  int end_offset = 4 + (num_leds * 4);
  led_data[end_offset + 0] = 0xFF;
  led_data[end_offset + 1] = 0xFF;
  led_data[end_offset + 2] = 0xFF;
  led_data[end_offset + 3] = 0xFF;

  ESP_LOGI(s_tag, "Sending %d bytes to %d LEDs...", frame_size, num_leds);

  esp_err_t ret = star_bus_spi_transmit(manager, "no_cipo_device",
                                        led_data, frame_size, 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "LED strip updated successfully");
  } else {
    ESP_LOGE(s_tag, "LED strip update failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate DAC output (write-only)
 */
static void priv_demonstrate_dac_output(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== DAC Output (Write-Only) ===\n");

  ESP_LOGI(s_tag, "Sending waveform data to DAC...\n");

  /* MCP4921 DAC command format (16 bits):
   * [15:12] Config bits (0011 = write to DAC A, buffered, 1x gain, active)
   * [11:0]  12-bit data value
   */

  const int num_samples = 32;
  uint8_t dac_commands[num_samples * 2];  /* 2 bytes per sample */

  /* Generate sine wave approximation */
  for (int i = 0; i < num_samples; i++) {
    /* Simple triangle wave for demo */
    uint16_t value;
    if (i < num_samples / 2) {
      value = (i * 4095) / (num_samples / 2);
    } else {
      value = 4095 - ((i - num_samples / 2) * 4095) / (num_samples / 2);
    }

    /* Format DAC command */
    uint16_t dac_cmd = 0x3000 | (value & 0x0FFF);

    /* Store as big-endian */
    dac_commands[i * 2 + 0] = (uint8_t)(dac_cmd >> 8);
    dac_commands[i * 2 + 1] = (uint8_t)(dac_cmd & 0xFF);
  }

  ESP_LOGI(s_tag, "Sending %d samples to DAC...", num_samples);

  esp_err_t ret = star_bus_spi_transmit(manager, "no_cipo_device",
                                        dac_commands, sizeof(dac_commands),
                                        1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "DAC waveform sent successfully");
    ESP_LOGI(s_tag, "First sample: 0x%02X%02X",
             dac_commands[0], dac_commands[1]);
  } else {
    ESP_LOGE(s_tag, "DAC output failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate display commands (write-only)
 */
static void priv_demonstrate_display_commands(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Display Commands (Write-Only) ===\n");

  ESP_LOGI(s_tag, "Sending commands to SPI display (e.g., ST7789)...\n");

  /* Display initialization sequence example */
  const uint8_t init_commands[] = {
    0x01,             /* Software reset */
    0x11,             /* Sleep out */
    0x3A, 0x05,       /* Pixel format: 16-bit color */
    0x36, 0x00,       /* Memory access control */
    0x29,             /* Display on */
  };

  ESP_LOGI(s_tag, "Sending display init sequence (%d bytes)...",
           sizeof(init_commands));

  esp_err_t ret = star_bus_spi_transmit(manager, "no_cipo_device",
                                        (uint8_t*)init_commands,
                                        sizeof(init_commands), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Display initialized");
  }

  /* Send pixel data */
  ESP_LOGI(s_tag, "\nSending pixel data...");

  uint8_t pixel_data[64];
  for (int i = 0; i < 64; i++) {
    pixel_data[i] = (uint8_t)i;
  }

  ret = star_bus_spi_transmit(manager, "no_cipo_device",
                               pixel_data, sizeof(pixel_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pixel data sent: %d bytes", sizeof(pixel_data));
  }
}

/**
 * @brief Demonstrate large write-only transfer
 */
static void priv_demonstrate_large_write(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Large Write-Only Transfer ===\n");

  const size_t buffer_size = 4096;

  uint8_t *tx_buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);

  if (tx_buffer == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    return;
  }

  /* Fill with test pattern */
  for (size_t i = 0; i < buffer_size; i++) {
    tx_buffer[i] = (uint8_t)(i & 0xFF);
  }

  ESP_LOGI(s_tag, "Transmitting %zu bytes (DMA-enabled)...", buffer_size);

  int64_t start_time = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transmit(manager, "no_cipo_device",
                                        tx_buffer, buffer_size, 5000);

  int64_t elapsed_us = esp_timer_get_time() - start_time;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transfer complete:");
    ESP_LOGI(s_tag, "  Size:       %zu bytes", buffer_size);
    ESP_LOGI(s_tag, "  Time:       %lld us", elapsed_us);
    ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
             (buffer_size / 1024.0) / (elapsed_us / 1000000.0));

    ESP_LOGI(s_tag, "\nNote: No CIPO means no RX DMA overhead!");
  } else {
    ESP_LOGE(s_tag, "Transfer failed: %s", esp_err_to_name(ret));
  }

  heap_caps_free(tx_buffer);
}

/**
 * @brief Common write-only devices
 */
static void priv_list_common_devices(void)
{
  ESP_LOGI(s_tag, "\n=== Common Write-Only SPI Devices ===\n");

  ESP_LOGI(s_tag, "LEDs & Displays:");
  ESP_LOGI(s_tag, "  - WS2801/APA102 LED strips");
  ESP_LOGI(s_tag, "  - ST7789/ILI9341 TFT displays");
  ESP_LOGI(s_tag, "  - MAX7219 LED matrix");
  ESP_LOGI(s_tag, "  - SSD1306 OLED (some modes)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Digital-to-Analog:");
  ESP_LOGI(s_tag, "  - MCP4921/4922 DACs");
  ESP_LOGI(s_tag, "  - AD5623 DACs");
  ESP_LOGI(s_tag, "  - TLV5618 DACs");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Digital Potentiometers:");
  ESP_LOGI(s_tag, "  - MCP4131/4132");
  ESP_LOGI(s_tag, "  - AD5204/5206");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Other:");
  ESP_LOGI(s_tag, "  - Shift registers (74HC595)");
  ESP_LOGI(s_tag, "  - Some motor controllers");
  ESP_LOGI(s_tag, "  - Certain RF modules (TX mode)");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Optional CIPO Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_no_cipo_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create SPI device WITHOUT CIPO pin */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    /* flags can optionally include SPI_DEVICE_NO_DUMMY for optimization */
  };

  /* IMPORTANT: Pass -1 for CIPO pin to indicate no CIPO */
  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "no_cipo_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    -1,              /* No CIPO pin! */
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi_config);
  ret = star_bus_config_init(spi_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "SPI initialized WITHOUT CIPO pin\n");

  /* Demonstrate various use cases */
  priv_demonstrate_no_cipo(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_led_strip(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_dac_output(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_display_commands(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_large_write(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_list_common_devices();

  /* Benefits summary */
  ESP_LOGI(s_tag, "\n=== Benefits of Omitting CIPO ===");
  ESP_LOGI(s_tag, "1. Saves 1 GPIO pin");
  ESP_LOGI(s_tag, "2. Simplifies wiring (3-wire vs 4-wire)");
  ESP_LOGI(s_tag, "3. Reduces DMA overhead (no RX buffer)");
  ESP_LOGI(s_tag, "4. Lower memory usage (TX buffer only)");
  ESP_LOGI(s_tag, "5. Slightly faster for write-only operations");
  ESP_LOGI(s_tag, "");

  /* When to use */
  ESP_LOGI(s_tag, "=== When to Omit CIPO ===");
  ESP_LOGI(s_tag, "Use write-only (no CIPO) when:");
  ESP_LOGI(s_tag, "  - Device never sends data back");
  ESP_LOGI(s_tag, "  - GPIO pins are limited");
  ESP_LOGI(s_tag, "  - Only transmitting to device");
  ESP_LOGI(s_tag, "  - Using output-only peripherals");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Keep CIPO when:");
  ESP_LOGI(s_tag, "  - Need to read device status");
  ESP_LOGI(s_tag, "  - Reading sensor data");
  ESP_LOGI(s_tag, "  - Verifying writes");
  ESP_LOGI(s_tag, "  - Using full-duplex communication");

  /* Configuration notes */
  ESP_LOGI(s_tag, "\n=== Configuration Notes ===");
  ESP_LOGI(s_tag, "In star_bus_config_create_spi_device():");
  ESP_LOGI(s_tag, "  - Set cipo_gpio to -1 for no CIPO");
  ESP_LOGI(s_tag, "  - Use only transmit/transfer functions");
  ESP_LOGI(s_tag, "  - Receive will fail if attempted");
  ESP_LOGI(s_tag, "  - Can still use DMA for large writes");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Hardware:");
  ESP_LOGI(s_tag, "  - Leave MISO physically disconnected");
  ESP_LOGI(s_tag, "  - Or tie to VCC through pull-up");
  ESP_LOGI(s_tag, "  - Prevents floating input issues");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI optional CIPO example complete");
}
