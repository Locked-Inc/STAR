/**
 * @file 035_i2c_register_dump.c
 * @brief Read and dump multiple consecutive registers
 *
 * This example demonstrates:
 * - Reading multiple consecutive registers
 * - Formatted register dump output
 * - Comparing register values over time
 * - Identifying changed registers
 * - Saving register snapshots
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "I2C_REG_DUMP";

/* Forward declarations */
static const char *priv_byte_to_binary(uint8_t byte);

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x48)

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/* Register dump configuration */
#define REG_START       (0x00)
#define REG_END         (0x3F)  /* 64 registers */
#define REG_COUNT       (REG_END - REG_START + 1)

/**
 * @brief Register snapshot
 */
typedef struct {
  uint8_t values[REG_COUNT];
  int64_t timestamp_us;
} reg_snapshot_t;

/**
 * @brief Read all registers
 */
esp_err_t read_all_registers(star_bus_manager_t *manager, const char *bus_name,
                             uint8_t *reg_values)
{
  size_t bytes_read;

  /* Try burst read first (if device supports auto-increment) */
  esp_err_t ret = star_bus_i2c_read(manager, bus_name, reg_values, REG_COUNT,
                                     REG_START, &bytes_read);

  if (ret == ESP_OK && bytes_read == REG_COUNT) {
    ESP_LOGI(s_tag, "Burst read successful - read %u registers", REG_COUNT);
    return ESP_OK;
  }

  /* Fall back to individual reads */
  ESP_LOGI(s_tag, "Burst read failed, using individual reads...");

  for (uint8_t reg = REG_START; reg <= REG_END; reg++) {
    ret = star_bus_i2c_read(manager, bus_name, &reg_values[reg - REG_START],
                            1, reg, &bytes_read);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to read register 0x%02X: %s",
               reg, esp_err_to_name(ret));
      reg_values[reg - REG_START] = 0xFF;  /* Mark as unreadable */
    }

    vTaskDelay(pdMS_TO_TICKS(2));  /* Small delay between reads */
  }

  return ESP_OK;
}

/**
 * @brief Print register dump in formatted table
 */
void print_register_dump(const uint8_t *reg_values, const char *title)
{
  ESP_LOGI(s_tag, "\n=== %s ===", title);
  ESP_LOGI(s_tag, "Register Dump (0x%02X - 0x%02X):", REG_START, REG_END);

  /* Print header */
  printf("\n      ");
  for (int col = 0; col < 16; col++) {
    printf(" %X ", col);
  }
  printf("\n");

  /* Print separator */
  printf("    ");
  for (int col = 0; col < 16; col++) {
    printf("---");
  }
  printf("\n");

  /* Print rows */
  for (int row = 0; row < ((REG_COUNT + 15) / 16); row++) {
    uint8_t base_addr = REG_START + (row * 16);
    printf("%02X: ", base_addr);

    for (int col = 0; col < 16; col++) {
      uint8_t addr = base_addr + col;

      if (addr > REG_END) {
        printf("   ");
      } else {
        uint8_t idx = addr - REG_START;
        printf(" %02X", reg_values[idx]);
      }
    }
    printf("\n");
  }
  printf("\n");
}

/**
 * @brief Print register dump with ASCII interpretation
 */
void print_register_dump_with_ascii(const uint8_t *reg_values)
{
  ESP_LOGI(s_tag, "\n=== Register Dump with ASCII ===");

  for (int row = 0; row < ((REG_COUNT + 15) / 16); row++) {
    uint8_t base_addr = REG_START + (row * 16);

    /* Print hex values */
    printf("%02X: ", base_addr);
    for (int col = 0; col < 16; col++) {
      uint8_t addr = base_addr + col;
      if (addr > REG_END) {
        printf("   ");
      } else {
        printf(" %02X", reg_values[addr - REG_START]);
      }
    }

    /* Print ASCII interpretation */
    printf("  | ");
    for (int col = 0; col < 16; col++) {
      uint8_t addr = base_addr + col;
      if (addr > REG_END) {
        printf(" ");
      } else {
        uint8_t val = reg_values[addr - REG_START];
        if (val >= 32 && val < 127) {
          printf("%c", val);
        } else {
          printf(".");
        }
      }
    }
    printf("\n");
  }
  printf("\n");
}

/**
 * @brief Compare two register snapshots
 */
void compare_snapshots(const reg_snapshot_t *snap1, const reg_snapshot_t *snap2)
{
  ESP_LOGI(s_tag, "\n=== Register Comparison ===");
  ESP_LOGI(s_tag, "Snapshot 1: %lld us", snap1->timestamp_us);
  ESP_LOGI(s_tag, "Snapshot 2: %lld us", snap2->timestamp_us);
  ESP_LOGI(s_tag, "Time delta: %lld us\n",
           snap2->timestamp_us - snap1->timestamp_us);

  int changed_count = 0;

  /* Find changed registers */
  for (int i = 0; i < REG_COUNT; i++) {
    if (snap1->values[i] != snap2->values[i]) {
      uint8_t reg_addr = REG_START + i;
      ESP_LOGI(s_tag, "Reg 0x%02X: 0x%02X -> 0x%02X (changed by %d)",
               reg_addr,
               snap1->values[i],
               snap2->values[i],
               (int)snap2->values[i] - (int)snap1->values[i]);
      changed_count++;
    }
  }

  if (changed_count == 0) {
    ESP_LOGI(s_tag, "No registers changed");
  } else {
    ESP_LOGI(s_tag, "\nTotal changed: %d/%d registers", changed_count,
             REG_COUNT);
  }
}

/**
 * @brief Print specific register range
 */
void print_register_range(const uint8_t *reg_values, uint8_t start, uint8_t end,
                          const char *description)
{
  ESP_LOGI(s_tag, "\n=== %s (0x%02X - 0x%02X) ===", description, start, end);

  for (uint8_t reg = start; reg <= end; reg++) {
    if (reg >= REG_START && reg <= REG_END) {
      uint8_t idx = reg - REG_START;
      ESP_LOGI(s_tag, "  Reg 0x%02X: 0x%02X  (binary: %s)",
               reg, reg_values[idx],
               priv_byte_to_binary(reg_values[idx]));
    }
  }
}

/**
 * @brief Convert byte to binary string
 */
static const char *priv_byte_to_binary(uint8_t byte)
{
  static char binary[9];
  for (int i = 7; i >= 0; i--) {
    binary[7 - i] = (byte & (1 << i)) ? '1' : '0';
  }
  binary[8] = '\0';
  return binary;
}

/**
 * @brief Save snapshot to "file" (simulated with static buffer)
 */
void save_snapshot(const reg_snapshot_t *snapshot, const char *name)
{
  ESP_LOGI(s_tag, "Snapshot '%s' saved at %lld us", name, snapshot->timestamp_us);
  /* In real application, could save to NVS, SD card, etc. */
}

void i2c_register_dump_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_RegDump", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");

  /* Take first snapshot */
  reg_snapshot_t snapshot1 = {0};
  snapshot1.timestamp_us = esp_timer_get_time();

  ESP_LOGI(s_tag, "\nReading all registers (snapshot 1)...");
  ret = read_all_registers(&manager, "device", snapshot1.values);

  if (ret == ESP_OK) {
    print_register_dump(snapshot1.values, "Snapshot 1");
    print_register_dump_with_ascii(snapshot1.values);
    save_snapshot(&snapshot1, "Initial");
  }

  /* Write to some registers */
  vTaskDelay(pdMS_TO_TICKS(500));
  ESP_LOGI(s_tag, "\nModifying some registers...");

  size_t  bytes_written;
  uint8_t write_val = 0xAA;
  star_bus_i2c_write(&manager, "device", &write_val, 1, 0x10, &bytes_written);

  write_val = 0x55;
  star_bus_i2c_write(&manager, "device", &write_val, 1, 0x11, &bytes_written);

  /* Take second snapshot */
  vTaskDelay(pdMS_TO_TICKS(100));

  reg_snapshot_t snapshot2 = {0};
  snapshot2.timestamp_us = esp_timer_get_time();

  ESP_LOGI(s_tag, "\nReading all registers (snapshot 2)...");
  ret = read_all_registers(&manager, "device", snapshot2.values);

  if (ret == ESP_OK) {
    print_register_dump(snapshot2.values, "Snapshot 2");
    save_snapshot(&snapshot2, "After_Write");

    /* Compare snapshots */
    compare_snapshots(&snapshot1, &snapshot2);
  }

  /* Print specific register ranges */
  print_register_range(snapshot2.values, 0x00, 0x0F, "Control Registers");
  print_register_range(snapshot2.values, 0x10, 0x1F, "Data Registers");

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Register Dump Use Cases ===");
  ESP_LOGI(s_tag, "1. Device configuration verification");
  ESP_LOGI(s_tag, "2. Debugging device behavior");
  ESP_LOGI(s_tag, "3. Tracking register changes");
  ESP_LOGI(s_tag, "4. Reverse engineering device protocol");
  ESP_LOGI(s_tag, "5. Creating device snapshots for restore");
  ESP_LOGI(s_tag, "6. Comparing devices (golden vs. test)");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nRegister dump example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Register Dump Example ===\n");
  i2c_register_dump_example();
}
