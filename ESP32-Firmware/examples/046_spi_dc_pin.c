/**
 * @file 046_spi_dc_pin.c
 * @brief DC (Data/Command) pin control for display communication
 *
 * Demonstrates:
 * - DC pin configuration and control
 * - Command vs data differentiation
 * - Display initialization sequence
 * - Typical LCD/OLED communication pattern
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SPI_DC_PIN";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

/* Display control pins */
#define DC_PIN         (GPIO_NUM_2)   /* Data/Command select */
#define RESET_PIN      (GPIO_NUM_4)   /* Display reset (optional) */

#define SPI_CLK_SPEED  (40000000)  /* 40 MHz for display */

/* DC pin states */
#define DC_COMMAND     (0)  /* DC=0 for commands */
#define DC_DATA        (1)  /* DC=1 for data */

/* Example display commands (ILI9341/ST7789 style) */
#define CMD_NOP                (0x00)
#define CMD_SWRESET            (0x01)
#define CMD_SLPOUT             (0x11)
#define CMD_DISPOFF            (0x28)
#define CMD_DISPON             (0x29)
#define CMD_CASET      (0x2A)  /* Column address set */
#define CMD_RASET      (0x2B)  /* Row address set */
#define CMD_RAMWR      (0x2C)  /* Memory write */
#define CMD_MADCTL     (0x36)  /* Memory access control */
#define CMD_COLMOD     (0x3A)  /* Pixel format */

static star_bus_manager_t s_manager;

/**
 * @brief Initialize DC and Reset pins
 */
static esp_err_t priv_init_dc_pin(void)
{
  esp_err_t ret;

  /* Configure DC pin */
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << DC_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };

  ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to configure DC pin: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Configure Reset pin */
  io_conf.pin_bit_mask = (1ULL << RESET_PIN);
  ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to configure Reset pin: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Set initial states */
  gpio_set_level(DC_PIN, DC_DATA);      /* Default to data mode */
  gpio_set_level(RESET_PIN, 1);         /* Reset inactive (high) */

  ESP_LOGI(s_tag, "DC pin (GPIO %d) and Reset pin (GPIO %d) configured", DC_PIN, RESET_PIN);
  return ESP_OK;
}

/**
 * @brief Send command to display
 */
static esp_err_t priv_display_send_command(uint8_t cmd)
{
  /* Set DC pin LOW for command */
  gpio_set_level(DC_PIN, DC_COMMAND);

  /* Send command byte */
  esp_err_t ret = star_bus_spi_transmit(&s_manager, "display",
                                        &cmd, 1, 100);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to send command 0x%02X: %s", cmd, esp_err_to_name(ret));
  }

  return ret;
}

/**
 * @brief Send data to display
 */
static esp_err_t priv_display_send_data(const uint8_t *data, size_t len)
{
  /* Set DC pin HIGH for data */
  gpio_set_level(DC_PIN, DC_DATA);

  /* Send data bytes */
  esp_err_t ret = star_bus_spi_transmit(&s_manager, "display",
                                        data, len, 100);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to send %u bytes of data: %s", len, esp_err_to_name(ret));
  }

  return ret;
}

/**
 * @brief Send command with parameters
 */
static esp_err_t priv_display_send_command_with_params(uint8_t cmd,
                                                        const uint8_t *params,
                                                        size_t param_len)
{
  esp_err_t ret;

  /* Send command */
  ret = priv_display_send_command(cmd);
  if (ret != ESP_OK) return ret;

  /* Send parameters if any */
  if (params != NULL && param_len > 0) {
    ret = priv_display_send_data(params, param_len);
  }

  return ret;
}

/**
 * @brief Hardware reset display
 */
static void priv_display_hardware_reset(void)
{
  ESP_LOGI(s_tag, "Performing hardware reset...");

  gpio_set_level(RESET_PIN, 0);  /* Reset active (low) */
  vTaskDelay(pdMS_TO_TICKS(10));

  gpio_set_level(RESET_PIN, 1);  /* Reset inactive (high) */
  vTaskDelay(pdMS_TO_TICKS(120));

  ESP_LOGI(s_tag, "Hardware reset complete");
}

/**
 * @brief Initialize display with command sequence
 */
static esp_err_t priv_display_init_sequence(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "\n--- Display Initialization Sequence ---");

  /* 1. Hardware reset */
  priv_display_hardware_reset();

  /* 2. Software reset */
  ESP_LOGI(s_tag, "Sending software reset...");
  ret = priv_display_send_command(CMD_SWRESET);
  if (ret != ESP_OK) return ret;
  vTaskDelay(pdMS_TO_TICKS(150));

  /* 3. Sleep out */
  ESP_LOGI(s_tag, "Exiting sleep mode...");
  ret = priv_display_send_command(CMD_SLPOUT);
  if (ret != ESP_OK) return ret;
  vTaskDelay(pdMS_TO_TICKS(10));

  /* 4. Set pixel format (16-bit color) */
  ESP_LOGI(s_tag, "Setting pixel format...");
  uint8_t pixel_format = 0x55;  /* 16-bit/pixel */
  ret = priv_display_send_command_with_params(CMD_COLMOD, &pixel_format, 1);
  if (ret != ESP_OK) return ret;

  /* 5. Memory access control */
  ESP_LOGI(s_tag, "Configuring memory access...");
  uint8_t madctl = 0x48;  /* RGB order, etc. */
  ret = priv_display_send_command_with_params(CMD_MADCTL, &madctl, 1);
  if (ret != ESP_OK) return ret;

  /* 6. Display on */
  ESP_LOGI(s_tag, "Turning display on...");
  ret = priv_display_send_command(CMD_DISPON);
  if (ret != ESP_OK) return ret;
  vTaskDelay(pdMS_TO_TICKS(10));

  ESP_LOGI(s_tag, "Display initialization complete!\n");
  return ESP_OK;
}

/**
 * @brief Set drawing window
 */
static esp_err_t priv_display_set_window(uint16_t x0,
                                          uint16_t y0,
                                          uint16_t x1,
                                          uint16_t y1)
{
  esp_err_t ret;

  /* Column address set */
  uint8_t caset_params[] = {
    (x0 >> 8) & 0xFF, x0 & 0xFF,
    (x1 >> 8) & 0xFF, x1 & 0xFF
  };
  ret = priv_display_send_command_with_params(CMD_CASET, caset_params, 4);
  if (ret != ESP_OK) return ret;

  /* Row address set */
  uint8_t raset_params[] = {
    (y0 >> 8) & 0xFF, y0 & 0xFF,
    (y1 >> 8) & 0xFF, y1 & 0xFF
  };
  ret = priv_display_send_command_with_params(CMD_RASET, raset_params, 4);
  if (ret != ESP_OK) return ret;

  /* Memory write command */
  ret = priv_display_send_command(CMD_RAMWR);

  return ret;
}

/**
 * @brief Draw colored rectangle
 */
static void priv_display_draw_rectangle(uint16_t x,
                                         uint16_t y,
                                         uint16_t width,
                                         uint16_t height,
                                         uint16_t color)
{
  ESP_LOGI(s_tag, "Drawing rectangle at (%d,%d) size %dx%d, color 0x%04X",
           x, y, width, height, color);

  /* Set drawing window */
  priv_display_set_window(x, y, x + width - 1, y + height - 1);

  /* Prepare pixel data (RGB565 format) */
  uint8_t pixel_bytes[2] = {
    (color >> 8) & 0xFF,
    color & 0xFF
  };

  /* Send pixel data for entire rectangle */
  int total_pixels = width * height;
  for (int i = 0; i < total_pixels; i++) {
    priv_display_send_data(pixel_bytes, 2);
  }

  ESP_LOGI(s_tag, "Rectangle drawn (%d pixels)", total_pixels);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SPI Display with DC Pin Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&s_manager, "spi_dc_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Initialize DC and Reset pins */
  ret = priv_init_dc_pin();
  if (ret != ESP_OK) {
    star_bus_manager_deinit(&s_manager);
    return;
  }

  /* Configure SPI device for display */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 3,  /* Mode 3 common for displays */
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
    .flags = 0,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "display",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    star_bus_manager_deinit(&s_manager);
    return;
  }

  star_bus_manager_add_bus(&s_manager, spi_config);
  ret = star_bus_config_init(spi_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&s_manager);
    return;
  }

  ESP_LOGI(s_tag, "SPI initialized for display communication\n");

  /* Initialize display */
  ret = priv_display_init_sequence();
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Display initialization failed");
    star_bus_manager_deinit(&s_manager);
    return;
  }

  /* --- Draw some graphics --- */
  ESP_LOGI(s_tag, "\n--- Drawing Graphics ---\n");

  /* Red rectangle */
  priv_display_draw_rectangle(10, 10, 50, 30, 0xF800);  /* Red in RGB565 */

  /* Green rectangle */
  priv_display_draw_rectangle(70, 10, 50, 30, 0x07E0);  /* Green in RGB565 */

  /* Blue rectangle */
  priv_display_draw_rectangle(130, 10, 50, 30, 0x001F); /* Blue in RGB565 */

  /* --- DC Pin Explanation --- */
  ESP_LOGI(s_tag, "\n--- DC Pin Usage ---");
  ESP_LOGI(s_tag, "DC (Data/Command) Pin:");
  ESP_LOGI(s_tag, "  DC=0 (LOW):  Command mode");
  ESP_LOGI(s_tag, "               - Register addresses");
  ESP_LOGI(s_tag, "               - Control commands");
  ESP_LOGI(s_tag, "               - Configuration settings");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "  DC=1 (HIGH): Data mode");
  ESP_LOGI(s_tag, "               - Command parameters");
  ESP_LOGI(s_tag, "               - Pixel data");
  ESP_LOGI(s_tag, "               - Configuration values");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Typical sequence:");
  ESP_LOGI(s_tag, "  1. Set DC=0, send command byte");
  ESP_LOGI(s_tag, "  2. Set DC=1, send parameter/data bytes");
  ESP_LOGI(s_tag, "  3. Repeat as needed");

  ESP_LOGI(s_tag, "\n--- Common Display Types Using DC Pin ---");
  ESP_LOGI(s_tag, "- ILI9341 (320x240 TFT)");
  ESP_LOGI(s_tag, "- ST7789 (240x240/320x240 TFT)");
  ESP_LOGI(s_tag, "- ST7735 (128x160 TFT)");
  ESP_LOGI(s_tag, "- SSD1306 (128x64 OLED)");
  ESP_LOGI(s_tag, "- SSD1351 (128x128 OLED)");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSPI DC pin example complete");
}
