/**
 * @file 034_i2c_transaction_log.c
 * @brief Log all I2C transactions with circular buffer
 *
 * This example demonstrates:
 * - Logging all I2C transactions
 * - Circular buffer implementation for transaction history
 * - Transaction timestamps and durations
 * - Filtering and querying logged transactions
 * - Debug and diagnostic capabilities
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>

static const char *s_tag = "I2C_TX_LOG";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x48)

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/* Transaction log configuration */
#define LOG_BUFFER_SIZE (32)  /* Number of transactions to log */
#define MAX_DATA_LOG    (16)  /* Max data bytes to log per transaction */

/**
 * @brief Transaction type
 */
typedef enum {
  TX_TYPE_READ,
  TX_TYPE_WRITE,
  TX_TYPE_COMMAND,
  TX_TYPE_READ_RAW,
} tx_type_t;

/**
 * @brief Transaction log entry
 */
typedef struct {
  uint32_t  index;                    /* Transaction index */
  int64_t   timestamp_us;             /* Timestamp in microseconds */
  uint32_t  duration_us;              /* Transaction duration */
  tx_type_t type;                     /* Transaction type */
  uint8_t   device_addr;              /* Device address */
  uint8_t   reg_addr;                 /* Register address */
  uint8_t   data[MAX_DATA_LOG];       /* Data transferred */
  size_t    data_len;                 /* Actual data length */
  esp_err_t result;                   /* Transaction result */
  char      bus_name[16];             /* Bus name */
} tx_log_entry_t;

/**
 * @brief Circular transaction log
 */
typedef struct {
  tx_log_entry_t    entries[LOG_BUFFER_SIZE];
  uint32_t          write_index;
  uint32_t          total_count;
  SemaphoreHandle_t mutex;
} tx_log_t;

static tx_log_t s_tx_log = {0};

/**
 * @brief Initialize transaction log
 */
void tx_log_init(void)
{
  memset(&s_tx_log, 0, sizeof(tx_log_t));
  s_tx_log.mutex = xSemaphoreCreateMutex();
  ESP_LOGI(s_tag, "Transaction log initialized (buffer size: %d)",
           LOG_BUFFER_SIZE);
}

/**
 * @brief Add entry to transaction log
 */
void tx_log_add(const char *bus_name, tx_type_t type, uint8_t device_addr,
                uint8_t reg_addr, const uint8_t *data, size_t data_len,
                esp_err_t result, int64_t start_time)
{
  if (xSemaphoreTake(s_tx_log.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    tx_log_entry_t *entry = &s_tx_log.entries[s_tx_log.write_index];

    entry->index        = s_tx_log.total_count++;
    entry->timestamp_us = start_time;
    entry->duration_us  = (uint32_t)(esp_timer_get_time() - start_time);
    entry->type         = type;
    entry->device_addr  = device_addr;
    entry->reg_addr     = reg_addr;
    entry->result       = result;

    strncpy(entry->bus_name, bus_name, sizeof(entry->bus_name) - 1);
    entry->bus_name[sizeof(entry->bus_name) - 1] = '\0';

    /* Copy data (up to max) */
    entry->data_len = (data_len > MAX_DATA_LOG) ? MAX_DATA_LOG : data_len;
    if (data && entry->data_len > 0) {
      memcpy(entry->data, data, entry->data_len);
    }

    /* Advance write index (circular) */
    s_tx_log.write_index = (s_tx_log.write_index + 1) % LOG_BUFFER_SIZE;

    xSemaphoreGive(s_tx_log.mutex);
  }
}

/**
 * @brief Get type name string
 */
static const char *priv_tx_type_name(tx_type_t type)
{
  switch (type) {
    case TX_TYPE_READ:
      return "READ";
    case TX_TYPE_WRITE:
      return "WRITE";
    case TX_TYPE_COMMAND:
      return "CMD";
    case TX_TYPE_READ_RAW:
      return "RAW_RD";
    default:
      return "UNKNOWN";
  }
}

/**
 * @brief Print single transaction entry
 */
void tx_log_print_entry(const tx_log_entry_t *entry)
{
  ESP_LOGI(s_tag, "[%04u] %lld us | %s | 0x%02X | Reg:0x%02X | %u bytes | "
           "%u us | %s",
           entry->index,
           entry->timestamp_us,
           priv_tx_type_name(entry->type),
           entry->device_addr,
           entry->reg_addr,
           (unsigned)entry->data_len,
           entry->duration_us,
           esp_err_to_name(entry->result));

  /* Print data if present */
  if (entry->data_len > 0) {
    printf("       Data: ");
    for (size_t i = 0; i < entry->data_len; i++) {
      printf("%02X ", entry->data[i]);
    }
    printf("\n");
  }
}

/**
 * @brief Print all logged transactions
 */
void tx_log_print_all(void)
{
  ESP_LOGI(s_tag, "\n=== Transaction Log ===");
  ESP_LOGI(s_tag, "Total transactions: %u", s_tx_log.total_count);

  if (xSemaphoreTake(s_tx_log.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint32_t count = (s_tx_log.total_count < LOG_BUFFER_SIZE)
                       ? s_tx_log.total_count : LOG_BUFFER_SIZE;

    for (uint32_t i = 0; i < count; i++) {
      /* Calculate index (oldest first) */
      uint32_t idx = (s_tx_log.write_index + i) % LOG_BUFFER_SIZE;
      tx_log_print_entry(&s_tx_log.entries[idx]);
    }

    xSemaphoreGive(s_tx_log.mutex);
  }
}

/**
 * @brief Print transactions filtered by type
 */
void tx_log_print_by_type(tx_type_t type)
{
  ESP_LOGI(s_tag, "\n=== Transactions of type: %s ===", priv_tx_type_name(type));

  if (xSemaphoreTake(s_tx_log.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint32_t count = (s_tx_log.total_count < LOG_BUFFER_SIZE)
                       ? s_tx_log.total_count : LOG_BUFFER_SIZE;

    for (uint32_t i = 0; i < count; i++) {
      uint32_t idx = (s_tx_log.write_index + i) % LOG_BUFFER_SIZE;
      if (s_tx_log.entries[idx].type == type) {
        tx_log_print_entry(&s_tx_log.entries[idx]);
      }
    }

    xSemaphoreGive(s_tx_log.mutex);
  }
}

/**
 * @brief Get transaction statistics
 */
void tx_log_print_stats(void)
{
  ESP_LOGI(s_tag, "\n=== Transaction Statistics ===");

  if (xSemaphoreTake(s_tx_log.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint32_t count = (s_tx_log.total_count < LOG_BUFFER_SIZE)
                       ? s_tx_log.total_count : LOG_BUFFER_SIZE;

    uint32_t read_count     = 0;
    uint32_t write_count    = 0;
    uint32_t cmd_count      = 0;
    uint32_t success_count  = 0;
    uint32_t fail_count     = 0;
    uint32_t total_duration = 0;
    uint32_t min_duration   = UINT32_MAX;
    uint32_t max_duration   = 0;

    for (uint32_t i = 0; i < count; i++) {
      uint32_t idx = (s_tx_log.write_index + i) % LOG_BUFFER_SIZE;
      tx_log_entry_t *entry = &s_tx_log.entries[idx];

      /* Count by type */
      switch (entry->type) {
        case TX_TYPE_READ:
        case TX_TYPE_READ_RAW:
          read_count++;
          break;
        case TX_TYPE_WRITE:
          write_count++;
          break;
        case TX_TYPE_COMMAND:
          cmd_count++;
          break;
      }

      /* Count results */
      if (entry->result == ESP_OK) {
        success_count++;
      } else {
        fail_count++;
      }

      /* Duration stats */
      total_duration += entry->duration_us;
      if (entry->duration_us < min_duration) {
        min_duration = entry->duration_us;
      }
      if (entry->duration_us > max_duration) {
        max_duration = entry->duration_us;
      }
    }

    ESP_LOGI(s_tag, "Total logged: %u transactions", count);
    ESP_LOGI(s_tag, "Reads: %u | Writes: %u | Commands: %u",
             read_count, write_count, cmd_count);
    ESP_LOGI(s_tag, "Success: %u | Failed: %u", success_count, fail_count);

    if (count > 0) {
      ESP_LOGI(s_tag, "Duration: min=%u us, avg=%u us, max=%u us",
               min_duration, total_duration / count, max_duration);
    }

    xSemaphoreGive(s_tx_log.mutex);
  }
}

/**
 * @brief Logged I2C read
 */
esp_err_t logged_i2c_read(star_bus_manager_t *manager, const char *bus_name,
                          uint8_t *data, size_t len, uint8_t reg,
                          size_t *bytes_read)
{
  int64_t start   = esp_timer_get_time();
  esp_err_t ret   = star_bus_i2c_read(manager, bus_name, data, len,
                                      reg, bytes_read);
  tx_log_add(bus_name, TX_TYPE_READ, DEVICE_ADDR, reg, data, *bytes_read,
             ret, start);
  return ret;
}

/**
 * @brief Logged I2C write
 */
esp_err_t logged_i2c_write(star_bus_manager_t *manager, const char *bus_name,
                           const uint8_t *data, size_t len, uint8_t reg,
                           size_t *bytes_written)
{
  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_write(manager, bus_name, data, len,
                                     reg, bytes_written);
  tx_log_add(bus_name, TX_TYPE_WRITE, DEVICE_ADDR, reg, data, len, ret, start);
  return ret;
}

void perform_test_transactions(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Performing Test Transactions ===");

  size_t bytes_read, bytes_written;

  /* Read operations */
  for (int i = 0; i < 5; i++) {
    uint8_t data[4];
    logged_i2c_read(manager, "device", data, 4, i, &bytes_read);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  /* Write operations */
  for (int i = 0; i < 3; i++) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    logged_i2c_write(manager, "device", data, 3, i, &bytes_written);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  /* Command operations */
  for (int i = 0; i < 2; i++) {
    int64_t start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_write_command(manager, "device", 0xF0 + i);
    tx_log_add("device", TX_TYPE_COMMAND, DEVICE_ADDR, 0xF0 + i, NULL, 0,
               ret, start);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Test transactions complete");
}

void i2c_transaction_log_example(void)
{
  esp_err_t ret;

  /* Initialize transaction log */
  tx_log_init();

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_TxLog", NULL, NULL);
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

  /* Perform test transactions */
  perform_test_transactions(&manager);

  /* Display logs in various ways */
  vTaskDelay(pdMS_TO_TICKS(100));
  tx_log_print_all();

  vTaskDelay(pdMS_TO_TICKS(100));
  tx_log_print_by_type(TX_TYPE_READ);

  vTaskDelay(pdMS_TO_TICKS(100));
  tx_log_print_by_type(TX_TYPE_WRITE);

  vTaskDelay(pdMS_TO_TICKS(100));
  tx_log_print_stats();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Transaction Logging Benefits ===");
  ESP_LOGI(s_tag, "1. Debug communication issues");
  ESP_LOGI(s_tag, "2. Performance analysis");
  ESP_LOGI(s_tag, "3. Error pattern detection");
  ESP_LOGI(s_tag, "4. Transaction replay for testing");
  ESP_LOGI(s_tag, "5. Compliance and audit trails");

  /* Cleanup */
  if (s_tx_log.mutex) {
    vSemaphoreDelete(s_tx_log.mutex);
  }
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nTransaction log example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Transaction Logger Example ===\n");
  i2c_transaction_log_example();
}
