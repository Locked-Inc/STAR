/**
 * @file 055_spi_pre_post_cb.c
 * @brief Pre/post transaction callback hooks - Advanced usage
 *
 * Demonstrates:
 * - Advanced pre/post callback techniques
 * - DC pin control for displays
 * - Timing measurements with callbacks
 * - Resource management via callbacks
 * - Multi-device callback coordination
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_PRE_POST_CB";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_DC_PIN     (GPIO_NUM_2)   /* Data/Command pin for displays */
#define SPI_CLK_SPEED  (10000000)     /* 10 MHz */

/* Callback context data */
typedef struct {
  bool is_command;       /* true = command, false = data */
  int64_t start_time;    /* Transfer start timestamp */
  uint32_t transfer_count;
  uint32_t total_bytes;
} callback_context_t;

static callback_context_t s_ctx = {0};

/**
 * @brief Pre-callback: Set DC pin for display
 * This runs BEFORE CS is asserted
 */
static void priv_display_pre_callback(spi_transaction_t *trans)
{
  /* Record start time */
  s_ctx.start_time = esp_timer_get_time();

  /* Set DC pin based on command/data flag
   * DC=0 for commands, DC=1 for data
   */
  if (s_ctx.is_command) {
    gpio_set_level(SPI_DC_PIN, 0);  /* Command mode */
  } else {
    gpio_set_level(SPI_DC_PIN, 1);  /* Data mode */
  }

  /* Could also:
   * - Acquire a mutex
   * - Start DMA setup
   * - Toggle debug GPIO
   * - Log transfer details
   */

  (void)trans;  /* Unused in this example */
}

/**
 * @brief Post-callback: Cleanup after display transfer
 * This runs AFTER CS is deasserted
 */
static void priv_display_post_callback(spi_transaction_t *trans)
{
  /* Calculate elapsed time */
  int64_t elapsed = esp_timer_get_time() - s_ctx.start_time;

  /* Update statistics */
  s_ctx.transfer_count++;
  if (trans != NULL && trans->length > 0) {
    s_ctx.total_bytes += trans->length / 8;
  }

  /* Restore DC pin (optional) */
  gpio_set_level(SPI_DC_PIN, 0);

  /* Could also:
   * - Release a mutex
   * - Signal completion event
   * - Process received data
   * - Queue next transfer
   */

  (void)elapsed;  /* Would normally use this */
}

/**
 * @brief Initialize DC pin for display control
 */
static void priv_init_dc_pin(void)
{
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << SPI_DC_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t ret = gpio_config(&io_conf);
  if (ret == ESP_OK) {
    gpio_set_level(SPI_DC_PIN, 0);
    ESP_LOGI(s_tag, "DC pin (GPIO%d) configured", SPI_DC_PIN);
  } else {
    ESP_LOGW(s_tag, "DC pin config failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Send display command
 */
static esp_err_t priv_send_display_command(star_bus_manager_t *manager,
                                       uint8_t cmd)
{
  s_ctx.is_command = true;

  esp_err_t ret = star_bus_spi_transmit(manager, "display_device",
                                        &cmd, 1, 1000);

  ESP_LOGI(s_tag, "Sent command: 0x%02X -> %s", cmd, esp_err_to_name(ret));

  return ret;
}

/**
 * @brief Send display data
 */
static esp_err_t priv_send_display_data(star_bus_manager_t *manager,
                                    const uint8_t *data,
                                    size_t length)
{
  s_ctx.is_command = false;

  esp_err_t ret = star_bus_spi_transmit(manager, "display_device",
                                        (uint8_t*)data, length, 1000);

  ESP_LOGI(s_tag, "Sent data: %zu bytes -> %s", length, esp_err_to_name(ret));

  return ret;
}

/**
 * @brief Demonstrate display initialization sequence
 */
static void priv_demonstrate_display_init(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Display Initialization Sequence ===\n");

  ESP_LOGI(s_tag, "Initializing display with command/data sequences...");

  /* Software reset */
  priv_send_display_command(manager, 0x01);
  vTaskDelay(pdMS_TO_TICKS(50));

  /* Sleep out */
  priv_send_display_command(manager, 0x11);
  vTaskDelay(pdMS_TO_TICKS(120));

  /* Color mode: 16-bit RGB565 */
  priv_send_display_command(manager, 0x3A);
  uint8_t color_data = 0x05;
  priv_send_display_data(manager, &color_data, 1);

  /* Memory access control */
  priv_send_display_command(manager, 0x36);
  uint8_t madctl_data = 0x00;
  priv_send_display_data(manager, &madctl_data, 1);

  /* Display on */
  priv_send_display_command(manager, 0x29);

  ESP_LOGI(s_tag, "Display initialization complete");
}

/**
 * @brief Demonstrate pixel data transfer
 */
static void priv_demonstrate_pixel_transfer(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Pixel Data Transfer ===\n");

  /* Set column address */
  priv_send_display_command(manager, 0x2A);
  uint8_t col_data[] = {0x00, 0x00, 0x00, 0xEF};  /* 0 to 239 */
  priv_send_display_data(manager, col_data, sizeof(col_data));

  /* Set row address */
  priv_send_display_command(manager, 0x2B);
  uint8_t row_data[] = {0x00, 0x00, 0x01, 0x3F};  /* 0 to 319 */
  priv_send_display_data(manager, row_data, sizeof(row_data));

  /* Memory write */
  priv_send_display_command(manager, 0x2C);

  /* Send pixel data (RGB565 format) */
  uint8_t pixel_data[128];
  for (int i = 0; i < 128; i += 2) {
    pixel_data[i] = 0xF8;      /* Red (5 bits) */
    pixel_data[i + 1] = 0x00;  /* Green/Blue */
  }

  priv_send_display_data(manager, pixel_data, sizeof(pixel_data));

  ESP_LOGI(s_tag, "Pixel data sent: %zu bytes", sizeof(pixel_data));
}

/**
 * @brief Timing measurement callback pair
 */
static int64_t s_timing_start = 0;

static void priv_timing_pre_callback(spi_transaction_t *trans)
{
  s_timing_start = esp_timer_get_time();
  (void)trans;
}

static void priv_timing_post_callback(spi_transaction_t *trans)
{
  int64_t elapsed = esp_timer_get_time() - s_timing_start;
  size_t bytes = (trans != NULL && trans->length > 0) ? (trans->length / 8) : 0;

  ESP_LOGI(s_tag, "[TIMING] Transfer: %zu bytes in %lld us (%.2f KB/s)",
           bytes, elapsed,
           bytes / (elapsed / 1000000.0) / 1024.0);
}

/**
 * @brief Demonstrate timing measurements
 */
static void priv_demonstrate_timing(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Timing Measurements ===\n");

  ESP_LOGI(s_tag, "Using callbacks to measure transfer time...\n");

  uint8_t test_data[512];
  for (int i = 0; i < 512; i++) {
    test_data[i] = (uint8_t)(i & 0xFF);
  }

  /* The timing callbacks will measure this automatically */
  esp_err_t ret = star_bus_spi_transmit(manager, "timing_device",
                                        test_data, sizeof(test_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transfer completed with timing");
  }
}

/**
 * @brief Resource management callbacks
 */
static SemaphoreHandle_t s_bus_mutex = NULL;

static void priv_mutex_pre_callback(spi_transaction_t *trans)
{
  /* Acquire mutex before transfer */
  if (s_bus_mutex != NULL) {
    xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
  }
  (void)trans;
}

static void priv_mutex_post_callback(spi_transaction_t *trans)
{
  /* Release mutex after transfer */
  if (s_bus_mutex != NULL) {
    xSemaphoreGive(s_bus_mutex);
  }
  (void)trans;
}

/**
 * @brief Demonstrate resource management
 */
static void priv_demonstrate_resource_mgmt(void)
{
  ESP_LOGI(s_tag, "\n=== Resource Management via Callbacks ===\n");

  ESP_LOGI(s_tag, "Use cases for resource management:");
  ESP_LOGI(s_tag, "  1. Mutex for shared bus access");
  ESP_LOGI(s_tag, "  2. Semaphore counting active transfers");
  ESP_LOGI(s_tag, "  3. Reference counting for power management");
  ESP_LOGI(s_tag, "  4. Event groups for synchronization");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Example: Mutex protection");
  ESP_LOGI(s_tag, "  pre_cb:  xSemaphoreTake(bus_mutex)");
  ESP_LOGI(s_tag, "  post_cb: xSemaphoreGive(bus_mutex)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Example: Power management");
  ESP_LOGI(s_tag, "  pre_cb:  esp_pm_lock_acquire(pm_lock)");
  ESP_LOGI(s_tag, "  post_cb: esp_pm_lock_release(pm_lock)");
}

/**
 * @brief Demonstrate callback statistics
 */
static void priv_print_callback_statistics(void)
{
  ESP_LOGI(s_tag, "\n=== Callback Statistics ===\n");

  ESP_LOGI(s_tag, "Total transfers: %lu", s_ctx.transfer_count);
  ESP_LOGI(s_tag, "Total bytes:     %lu", s_ctx.total_bytes);

  if (s_ctx.transfer_count > 0) {
    ESP_LOGI(s_tag, "Average bytes:   %lu",
             s_ctx.total_bytes / s_ctx.transfer_count);
  }
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Pre/Post Callback Example ===\n");

  /* Initialize DC pin */
  priv_init_dc_pin();

  /* Create mutex for resource management demo */
  s_bus_mutex = xSemaphoreCreateMutex();

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_callback_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Device 1: Display with DC pin control */
  spi_device_interface_config_t display_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    .pre_cb = priv_display_pre_callback,   /* Set DC pin */
    .post_cb = priv_display_post_callback, /* Restore DC pin */
  };

  star_bus_config_t *display_config = star_bus_config_create_spi_device(
    "display_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    -1,  /* No CIPO for display */
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &display_cfg
  );

  if (display_config != NULL) {
    star_bus_manager_add_bus(&manager, display_config);
    ret = star_bus_config_init(display_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Display device initialized\n");
    }
  }

  /* Device 2: Timing measurement device */
  spi_device_interface_config_t timing_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = GPIO_NUM_15,
    .queue_size     = 7,
    .pre_cb = priv_timing_pre_callback,
    .post_cb = priv_timing_post_callback,
  };

  star_bus_config_t *timing_config = star_bus_config_create_spi_device(
    "timing_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &timing_cfg
  );

  if (timing_config != NULL) {
    star_bus_manager_add_bus(&manager, timing_config);
    ret = star_bus_config_init(timing_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Timing device initialized\n");
    }
  }

  /* Demonstrate different callback use cases */
  priv_demonstrate_display_init(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_pixel_transfer(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_timing(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_resource_mgmt();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_print_callback_statistics();

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Callback Best Practices ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "DO:");
  ESP_LOGI(s_tag, "  - Keep callbacks VERY short (< 10 us)");
  ESP_LOGI(s_tag, "  - Use for timing-critical operations");
  ESP_LOGI(s_tag, "  - Set/clear GPIO pins");
  ESP_LOGI(s_tag, "  - Update volatile variables");
  ESP_LOGI(s_tag, "  - Acquire/release fast locks");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "DON'T:");
  ESP_LOGI(s_tag, "  - Call ESP_LOG* (too slow, ISR context)");
  ESP_LOGI(s_tag, "  - Use FreeRTOS delays");
  ESP_LOGI(s_tag, "  - Allocate memory");
  ESP_LOGI(s_tag, "  - Call other SPI functions");
  ESP_LOGI(s_tag, "  - Do complex processing");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Context:");
  ESP_LOGI(s_tag, "  - Callbacks run in ISR context");
  ESP_LOGI(s_tag, "  - Must be IRAM_ATTR on some chips");
  ESP_LOGI(s_tag, "  - Can't call non-ISR-safe functions");
  ESP_LOGI(s_tag, "  - Use for critical path only");

  /* Common patterns */
  ESP_LOGI(s_tag, "\n=== Common Callback Patterns ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "1. DC Pin Control (displays):");
  ESP_LOGI(s_tag, "   pre_cb:  gpio_set_level(DC, is_data ? 1 : 0)");
  ESP_LOGI(s_tag, "   post_cb: gpio_set_level(DC, 0)");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "2. Debug Timing (oscilloscope):");
  ESP_LOGI(s_tag, "   pre_cb:  gpio_set_level(DEBUG_PIN, 1)");
  ESP_LOGI(s_tag, "   post_cb: gpio_set_level(DEBUG_PIN, 0)");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "3. Transfer Counting:");
  ESP_LOGI(s_tag, "   pre_cb:  transfer_count++");
  ESP_LOGI(s_tag, "   post_cb: total_bytes += trans->length");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "4. Multi-device Arbitration:");
  ESP_LOGI(s_tag, "   pre_cb:  current_device = this_device");
  ESP_LOGI(s_tag, "   post_cb: current_device = NULL");

  /* Cleanup */
  if (s_bus_mutex != NULL) {
    vSemaphoreDelete(s_bus_mutex);
  }

  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI pre/post callback example complete");
}
