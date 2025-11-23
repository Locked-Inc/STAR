/**
 * @file 059_spi_quad_advanced.c
 * @brief Advanced quad SPI features and optimizations
 *
 * Demonstrates:
 * - Quad SPI performance optimization
 * - QPI mode (Quad Peripheral Interface)
 * - Dual I/O mode comparison
 * - Multi-line command/address phases
 * - Advanced flash operations
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_QUAD_ADV";

/* Standard SPI pins */
#define SPI_D0_PIN     (GPIO_NUM_23)  /* COPI / Data0 */
#define SPI_D1_PIN     (GPIO_NUM_19)  /* CIPO / Data1 */
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

/* Additional pins for Quad SPI */
#define SPI_D2_PIN     (GPIO_NUM_22)  /* Data2 / WP */
#define SPI_D3_PIN     (GPIO_NUM_21)  /* Data3 / HOLD */

#define SPI_CLK_SPEED  (40000000)  /* 40 MHz for performance */

/* Flash commands */
#define CMD_READ_JEDEC (0x9F)
#define CMD_FAST_READ         (0x0B)  /* Standard fast read */
#define CMD_DUAL_READ         (0x3B)  /* Dual output read */
#define CMD_DUAL_IO_READ      (0xBB)  /* Dual I/O read */
#define CMD_QUAD_READ         (0x6B)  /* Quad output read */
#define CMD_QUAD_IO_READ      (0xEB)  /* Quad I/O read */
#define CMD_ENABLE_QUAD       (0x38)  /* Enter quad mode */
#define CMD_READ_STATUS (0x05)

/**
 * @brief Compare SPI modes
 */
static void priv_compare_spi_modes(void)
{
  ESP_LOGI(s_tag, "\n=== SPI Mode Comparison ===\n");

  ESP_LOGI(s_tag, "Standard SPI (1-1-1):");
  ESP_LOGI(s_tag, "  Command:  1 line (COPI)");
  ESP_LOGI(s_tag, "  Address:  1 line (COPI)");
  ESP_LOGI(s_tag, "  Data:     1 line (CIPO)");
  ESP_LOGI(s_tag, "  Speed:    1x (baseline)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Dual Output (1-1-2):");
  ESP_LOGI(s_tag, "  Command:  1 line");
  ESP_LOGI(s_tag, "  Address:  1 line");
  ESP_LOGI(s_tag, "  Data:     2 lines (D0, D1)");
  ESP_LOGI(s_tag, "  Speed:    ~1.8x");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Dual I/O (1-2-2):");
  ESP_LOGI(s_tag, "  Command:  1 line");
  ESP_LOGI(s_tag, "  Address:  2 lines");
  ESP_LOGI(s_tag, "  Data:     2 lines");
  ESP_LOGI(s_tag, "  Speed:    ~1.9x");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Quad Output (1-1-4):");
  ESP_LOGI(s_tag, "  Command:  1 line");
  ESP_LOGI(s_tag, "  Address:  1 line");
  ESP_LOGI(s_tag, "  Data:     4 lines (D0-D3)");
  ESP_LOGI(s_tag, "  Speed:    ~3.5x");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Quad I/O (1-4-4):");
  ESP_LOGI(s_tag, "  Command:  1 line");
  ESP_LOGI(s_tag, "  Address:  4 lines");
  ESP_LOGI(s_tag, "  Data:     4 lines");
  ESP_LOGI(s_tag, "  Speed:    ~3.8x");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "QPI Mode (4-4-4):");
  ESP_LOGI(s_tag, "  Command:  4 lines");
  ESP_LOGI(s_tag, "  Address:  4 lines");
  ESP_LOGI(s_tag, "  Data:     4 lines");
  ESP_LOGI(s_tag, "  Speed:    ~4x (theoretical)");
  ESP_LOGI(s_tag, "  Note: Requires device to be in QPI mode");
}

/**
 * @brief Demonstrate read performance comparison
 */
static void priv_compare_read_performance(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Read Performance Comparison ===\n");

  const size_t read_size = 1024;
  uint8_t *read_buf = (uint8_t*)heap_caps_malloc(read_size, MALLOC_CAP_DMA);

  if (read_buf == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    return;
  }

  /* Test 1: Standard SPI read */
  ESP_LOGI(s_tag, "1. Standard SPI (1-1-1) read...");

  uint8_t std_cmd[5] = {CMD_FAST_READ, 0x00, 0x00, 0x00, 0xFF};  /* + dummy */

  int64_t start = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transmit(manager, "quad_flash",
                                        std_cmd, sizeof(std_cmd), 1000);
  if (ret == ESP_OK) {
    ret = star_bus_spi_receive(manager, "quad_flash",
                                read_buf, read_size, 5000);
  }

  int64_t std_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "   Time: %lld us", std_time);
    ESP_LOGI(s_tag, "   Speed: %.2f KB/s",
             (read_size / 1024.0) / (std_time / 1000000.0));
  }

  /* Test 2: Quad output read (conceptual - requires low-level API) */
  ESP_LOGI(s_tag, "\n2. Quad Output (1-1-4) read...");
  ESP_LOGI(s_tag, "   Note: Requires direct SPI transaction API");
  ESP_LOGI(s_tag, "   Expected: ~3.5x faster than standard");
  ESP_LOGI(s_tag, "   Theoretical time: ~%lld us", std_time / 3);

  /* Test 3: Quad I/O read (conceptual) */
  ESP_LOGI(s_tag, "\n3. Quad I/O (1-4-4) read...");
  ESP_LOGI(s_tag, "   Note: Requires direct SPI transaction API");
  ESP_LOGI(s_tag, "   Expected: ~3.8x faster than standard");
  ESP_LOGI(s_tag, "   Theoretical time: ~%lld us", std_time / 4);

  heap_caps_free(read_buf);
}

/**
 * @brief Quad SPI requirements
 */
static void priv_explain_quad_requirements(void)
{
  ESP_LOGI(s_tag, "\n=== Quad SPI Requirements ===\n");

  ESP_LOGI(s_tag, "Hardware Requirements:");
  ESP_LOGI(s_tag, "  1. Flash chip must support quad mode");
  ESP_LOGI(s_tag, "  2. Four data lines: D0, D1, D2, D3");
  ESP_LOGI(s_tag, "  3. All data lines must have same length");
  ESP_LOGI(s_tag, "  4. WP and HOLD pins become D2 and D3");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "GPIO Configuration:");
  ESP_LOGI(s_tag, "  D0 (MOSI):  Primary data out");
  ESP_LOGI(s_tag, "  D1 (MISO):  Primary data in");
  ESP_LOGI(s_tag, "  D2 (WP):    Write protect → Data2");
  ESP_LOGI(s_tag, "  D3 (HOLD):  Hold → Data3");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Flash Configuration:");
  ESP_LOGI(s_tag, "  1. Read status register");
  ESP_LOGI(s_tag, "  2. Enable quad mode (QE bit)");
  ESP_LOGI(s_tag, "  3. Use quad read commands");
  ESP_LOGI(s_tag, "  4. Verify quad mode active");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Signal Integrity:");
  ESP_LOGI(s_tag, "  - All traces same length (±5mm)");
  ESP_LOGI(s_tag, "  - Short traces (< 5cm ideal)");
  ESP_LOGI(s_tag, "  - Proper termination at high speeds");
  ESP_LOGI(s_tag, "  - Good ground plane");
}

/**
 * @brief QPI mode explanation
 */
static void priv_explain_qpi_mode(void)
{
  ESP_LOGI(s_tag, "\n=== QPI Mode (Quad Peripheral Interface) ===\n");

  ESP_LOGI(s_tag, "What is QPI?");
  ESP_LOGI(s_tag, "  - Full quad mode for ALL phases");
  ESP_LOGI(s_tag, "  - Command, address, and data on 4 lines");
  ESP_LOGI(s_tag, "  - Maximum possible speed");
  ESP_LOGI(s_tag, "  - Device must be switched to QPI mode");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Entering QPI Mode:");
  ESP_LOGI(s_tag, "  1. Send ENABLE_QPI command (0x38) in SPI mode");
  ESP_LOGI(s_tag, "  2. Device switches to QPI mode");
  ESP_LOGI(s_tag, "  3. All subsequent commands use 4 lines");
  ESP_LOGI(s_tag, "  4. Exit with EXIT_QPI command (0xFF)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "QPI Benefits:");
  ESP_LOGI(s_tag, "  - ~4x speed vs standard SPI");
  ESP_LOGI(s_tag, "  - Lower latency for small transfers");
  ESP_LOGI(s_tag, "  - Better bandwidth utilization");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "QPI Limitations:");
  ESP_LOGI(s_tag, "  - Not all flash chips support it");
  ESP_LOGI(s_tag, "  - More complex to implement");
  ESP_LOGI(s_tag, "  - Harder to debug");
  ESP_LOGI(s_tag, "  - Requires all 4 data lines working");
}

/**
 * @brief Dual vs Quad comparison
 */
static void priv_compare_dual_quad(void)
{
  ESP_LOGI(s_tag, "\n=== Dual vs Quad SPI ===\n");

  ESP_LOGI(s_tag, "Dual SPI (2 data lines):");
  ESP_LOGI(s_tag, "  Pins: D0, D1 (MOSI, MISO)");
  ESP_LOGI(s_tag, "  Speed: ~2x standard SPI");
  ESP_LOGI(s_tag, "  Cost: No extra GPIOs needed");
  ESP_LOGI(s_tag, "  Use: Moderate speed requirements");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Quad SPI (4 data lines):");
  ESP_LOGI(s_tag, "  Pins: D0, D1, D2, D3");
  ESP_LOGI(s_tag, "  Speed: ~4x standard SPI");
  ESP_LOGI(s_tag, "  Cost: Requires 2 extra GPIOs");
  ESP_LOGI(s_tag, "  Use: High-speed applications");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "When to use Quad:");
  ESP_LOGI(s_tag, "  - Large flash reads (firmware, assets)");
  ESP_LOGI(s_tag, "  - High-resolution displays");
  ESP_LOGI(s_tag, "  - Fast data logging");
  ESP_LOGI(s_tag, "  - GPIOs available for D2/D3");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "When to use Standard/Dual:");
  ESP_LOGI(s_tag, "  - GPIO constrained designs");
  ESP_LOGI(s_tag, "  - Low-speed requirements");
  ESP_LOGI(s_tag, "  - Simple firmware updates");
  ESP_LOGI(s_tag, "  - Cost-sensitive applications");
}

/**
 * @brief Common quad flash chips
 */
static void priv_list_quad_flash_chips(void)
{
  ESP_LOGI(s_tag, "\n=== Common Quad SPI Flash Chips ===\n");

  ESP_LOGI(s_tag, "Winbond:");
  ESP_LOGI(s_tag, "  W25Q32, W25Q64, W25Q128");
  ESP_LOGI(s_tag, "  - Popular, good availability");
  ESP_LOGI(s_tag, "  - Full quad support");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Macronix:");
  ESP_LOGI(s_tag, "  MX25L3233F, MX25L6433F");
  ESP_LOGI(s_tag, "  - Industrial grade");
  ESP_LOGI(s_tag, "  - QPI mode support");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "GigaDevice:");
  ESP_LOGI(s_tag, "  GD25Q32, GD25Q64");
  ESP_LOGI(s_tag, "  - Cost-effective");
  ESP_LOGI(s_tag, "  - Standard quad commands");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ISSI:");
  ESP_LOGI(s_tag, "  IS25LP064, IS25WP128");
  ESP_LOGI(s_tag, "  - Low power variants");
  ESP_LOGI(s_tag, "  - Quad support");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Check datasheet for:");
  ESP_LOGI(s_tag, "  - Quad enable procedure");
  ESP_LOGI(s_tag, "  - Supported commands");
  ESP_LOGI(s_tag, "  - Maximum clock speed");
  ESP_LOGI(s_tag, "  - QPI mode availability");
}

/**
 * @brief Optimization tips for quad SPI
 */
static void priv_print_optimization_tips(void)
{
  ESP_LOGI(s_tag, "\n=== Quad SPI Optimization Tips ===\n");

  ESP_LOGI(s_tag, "1. Use Quad for Large Reads:");
  ESP_LOGI(s_tag, "   - > 256 bytes benefits most");
  ESP_LOGI(s_tag, "   - Overhead amortized over data");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Standard SPI for Commands:");
  ESP_LOGI(s_tag, "   - Small commands don't benefit");
  ESP_LOGI(s_tag, "   - Simpler to implement");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Match Clock Speed to Lines:");
  ESP_LOGI(s_tag, "   - Standard: up to 50 MHz");
  ESP_LOGI(s_tag, "   - Dual: up to 80 MHz");
  ESP_LOGI(s_tag, "   - Quad: up to 104 MHz (device dependent)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. PCB Design Matters:");
  ESP_LOGI(s_tag, "   - Length match all data lines");
  ESP_LOGI(s_tag, "   - Short traces minimize reflections");
  ESP_LOGI(s_tag, "   - Test signal integrity");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. Consider XIP (Execute in Place):");
  ESP_LOGI(s_tag, "   - ESP32 can run code from flash");
  ESP_LOGI(s_tag, "   - Quad mode speeds up execution");
  ESP_LOGI(s_tag, "   - Reduces RAM requirements");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== Advanced Quad SPI Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "quad_adv_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create quad-capable SPI device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    /* Note: Quad mode requires additional configuration
     * beyond what star_bus currently exposes.
     * This example is educational about quad concepts.
     */
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "quad_flash",
    SPI2_HOST,
    SPI_D0_PIN,
    SPI_D1_PIN,
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

  ESP_LOGI(s_tag, "SPI initialized (standard mode)\n");

  /* Educational demonstrations */
  priv_compare_spi_modes();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_compare_read_performance(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_explain_quad_requirements();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_explain_qpi_mode();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_compare_dual_quad();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_list_quad_flash_chips();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_print_optimization_tips();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Quad SPI Benefits:");
  ESP_LOGI(s_tag, "  - 3-4x speed increase");
  ESP_LOGI(s_tag, "  - Better bandwidth utilization");
  ESP_LOGI(s_tag, "  - Enables faster applications");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Implementation:");
  ESP_LOGI(s_tag, "  - Requires 4 GPIO pins");
  ESP_LOGI(s_tag, "  - More complex initialization");
  ESP_LOGI(s_tag, "  - PCB design considerations");
  ESP_LOGI(s_tag, "  - Device-specific commands");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Note:");
  ESP_LOGI(s_tag, "  Full quad mode implementation requires");
  ESP_LOGI(s_tag, "  direct ESP-IDF SPI driver API access.");
  ESP_LOGI(s_tag, "  See ESP-IDF examples for complete code.");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nAdvanced quad SPI example complete");
}
