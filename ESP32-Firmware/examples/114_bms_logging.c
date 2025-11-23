/**
 * @file 114_bms_logging.c
 * @brief Continuous data logging and statistics
 *
 * Demonstrates:
 * - Periodic data capture
 * - Log to console with timestamps
 * - Statistics aggregation
 * - Data export format
 * - Long-term monitoring
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

static const char* TAG = "BMS_LOGGING";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Logging configuration */
#define LOG_INTERVAL_MS    (1000)  /* Log every 1 second */
#define LOG_DURATION_S     (30)    /* Log for 30 seconds */

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/* Statistics structure */
typedef struct {
  /* Voltage statistics */
  uint32_t voltage_min;
  uint32_t voltage_max;
  uint64_t voltage_sum;

  /* Current statistics */
  int32_t current_min;
  int32_t current_max;
  int64_t current_sum;

  /* Temperature statistics */
  int16_t temp_min;
  int16_t temp_max;
  int64_t temp_sum;

  /* SOC statistics */
  uint8_t soc_min;
  uint8_t soc_max;
  uint64_t soc_sum;

  /* Sample count */
  uint32_t sample_count;
} bms_statistics_t;

static bms_statistics_t stats = {0};

/**
 * @brief Get current timestamp string
 */
static void get_timestamp(char* buffer, size_t size)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  time_t now = tv.tv_sec;
  struct tm* timeinfo = localtime(&now);

  snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           timeinfo->tm_hour,
           timeinfo->tm_min,
           timeinfo->tm_sec,
           tv.tv_usec / 1000);
}

/**
 * @brief Update statistics with new sample
 */
static void update_statistics(const bq7850_battery_state_t* state)
{
  uint16_t pack_mv = state->cells.pack_mv;
  int32_t current_ma = state->current.current_ma;
  int16_t temp_c = (int16_t)(state->temps.avg_temp_c * 10);
  uint8_t soc = state->soc.relative_soc;

  if (stats.sample_count == 0) {
    /* First sample - initialize min/max */
    stats.voltage_min = pack_mv;
    stats.voltage_max = pack_mv;
    stats.current_min = current_ma;
    stats.current_max = current_ma;
    stats.temp_min = temp_c;
    stats.temp_max = temp_c;
    stats.soc_min = soc;
    stats.soc_max = soc;
  } else {
    /* Update min/max */
    if (pack_mv < stats.voltage_min) stats.voltage_min = pack_mv;
    if (pack_mv > stats.voltage_max) stats.voltage_max = pack_mv;

    if (current_ma < stats.current_min) stats.current_min = current_ma;
    if (current_ma > stats.current_max) stats.current_max = current_ma;

    if (temp_c < stats.temp_min) stats.temp_min = temp_c;
    if (temp_c > stats.temp_max) stats.temp_max = temp_c;

    if (soc < stats.soc_min) stats.soc_min = soc;
    if (soc > stats.soc_max) stats.soc_max = soc;
  }

  /* Update sums */
  stats.voltage_sum += pack_mv;
  stats.current_sum += current_ma;
  stats.temp_sum += temp_c;
  stats.soc_sum += soc;

  stats.sample_count++;
}

/**
 * @brief Print statistics summary
 */
static void print_statistics(void)
{
  if (stats.sample_count == 0) {
    ESP_LOGW(TAG, "No statistics available");
    return;
  }

  ESP_LOGI(TAG, "\n+------------------------------------------+");
  ESP_LOGI(TAG, "|      BMS Logging Statistics              |");
  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| Samples: %-30lu |", (unsigned long)stats.sample_count);
  ESP_LOGI(TAG, "+------------------------------------------+");

  uint32_t voltage_avg = stats.voltage_sum / stats.sample_count;
  ESP_LOGI(TAG, "| Voltage (mV):                            |");
  ESP_LOGI(TAG, "|   Min: %-32lu |", (unsigned long)stats.voltage_min);
  ESP_LOGI(TAG, "|   Avg: %-32lu |", (unsigned long)voltage_avg);
  ESP_LOGI(TAG, "|   Max: %-32lu |", (unsigned long)stats.voltage_max);

  int32_t current_avg = stats.current_sum / stats.sample_count;
  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| Current (mA):                            |");
  ESP_LOGI(TAG, "|   Min: %-32ld |", (long)stats.current_min);
  ESP_LOGI(TAG, "|   Avg: %-32ld |", (long)current_avg);
  ESP_LOGI(TAG, "|   Max: %-32ld |", (long)stats.current_max);

  int16_t temp_avg = stats.temp_sum / stats.sample_count;
  float temp_min_c = stats.temp_min / 10.0f;
  float temp_avg_c = temp_avg / 10.0f;
  float temp_max_c = stats.temp_max / 10.0f;

  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| Temperature (C):                         |");
  ESP_LOGI(TAG, "|   Min: %-32.1f |", temp_min_c);
  ESP_LOGI(TAG, "|   Avg: %-32.1f |", temp_avg_c);
  ESP_LOGI(TAG, "|   Max: %-32.1f |", temp_max_c);

  uint8_t soc_avg = stats.soc_sum / stats.sample_count;
  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| SOC (%%):                                 |");
  ESP_LOGI(TAG, "|   Min: %-32d |", stats.soc_min);
  ESP_LOGI(TAG, "|   Avg: %-32d |", soc_avg);
  ESP_LOGI(TAG, "|   Max: %-32d |", stats.soc_max);
  ESP_LOGI(TAG, "+------------------------------------------+");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Data Logging Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "logging_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus manager");
    return;
  }

  /* Create and configure I2C/SMBus bus first */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "bms_bus",
    I2C_NUM_0,
    BQ7850_DEFAULT_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  ret = star_bus_manager_add_bus(&g_manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add bus to manager");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ret = star_bus_config_init(i2c_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  /* Initialize BMS */
  bq7850_config_t bms_config = {
    .num_cells = 16,
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&g_bms, &g_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init BMS: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ESP_LOGI(TAG, "BMS initialized successfully\n");

  /* ===== LOGGING CONFIGURATION ===== */
  ESP_LOGI(TAG, "===== Logging Configuration =====");
  ESP_LOGI(TAG, "Log interval: %d ms", LOG_INTERVAL_MS);
  ESP_LOGI(TAG, "Log duration: %d seconds", LOG_DURATION_S);
  ESP_LOGI(TAG, "Expected samples: %d\n", (LOG_DURATION_S * 1000) / LOG_INTERVAL_MS);

  /* ===== CSV HEADER ===== */
  ESP_LOGI(TAG, "===== Data Log (CSV Format) =====\n");
  ESP_LOGI(TAG, "Timestamp,Voltage_mV,Current_mA,Temp_C,SOC_%%,Cell1_mV,Cell2_mV,Remaining_mAh");

  /* Reset statistics */
  memset(&stats, 0, sizeof(stats));

  /* ===== DATA LOGGING LOOP ===== */
  int sample_count = (LOG_DURATION_S * 1000) / LOG_INTERVAL_MS;

  for (int i = 0; i < sample_count; i++) {
    char timestamp[48];
    get_timestamp(timestamp, sizeof(timestamp));

    /* Read complete battery state */
    bq7850_battery_state_t state;
    ret = star_bms_bq7850_read_battery_state(&g_bms, &state);

    if (ret == ESP_OK) {
      /* Read additional cell data */
      bq7850_cell_data_t cells;
      star_bms_bq7850_read_cells(&g_bms, &cells);

      /* Log in CSV format */
      ESP_LOGI(TAG, "%s,%u,%d,%.1f,%d,%d,%d,%d",
               timestamp,
               state.cells.pack_mv,
               state.current.current_ma,
               state.temps.avg_temp_c,
               state.soc.relative_soc,
               cells.valid_cells > 0 ? cells.cell_mv[0] : 0,
               cells.valid_cells > 1 ? cells.cell_mv[1] : 0,
               state.soc.remaining_capacity_mah);

      /* Update statistics */
      update_statistics(&state);

    } else {
      ESP_LOGW(TAG, "%s,ERROR,ERROR,ERROR,ERROR", timestamp);
    }

    vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
  }

  /* ===== STATISTICS SUMMARY ===== */
  ESP_LOGI(TAG, "\n===== Logging Complete =====");
  print_statistics();

  /* ===== DATA EXPORT FORMATS ===== */
  ESP_LOGI(TAG, "\n===== Data Export Formats =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. CSV Format (demonstrated above):");
  ESP_LOGI(TAG, "   - Easy to import into Excel/Sheets");
  ESP_LOGI(TAG, "   - Human-readable");
  ESP_LOGI(TAG, "   - Compatible with most tools");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. JSON Format:");
  ESP_LOGI(TAG, "   {");
  ESP_LOGI(TAG, "     \"timestamp\": \"2024-01-01 12:00:00\",");
  ESP_LOGI(TAG, "     \"voltage_mv\": 12600,");
  ESP_LOGI(TAG, "     \"current_ma\": -1500,");
  ESP_LOGI(TAG, "     \"temperature_c\": 25.3,");
  ESP_LOGI(TAG, "     \"soc_percent\": 75");
  ESP_LOGI(TAG, "   }");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Binary Format:");
  ESP_LOGI(TAG, "   - Most compact");
  ESP_LOGI(TAG, "   - Fastest to write");
  ESP_LOGI(TAG, "   - Requires decoder");

  /* ===== STORAGE OPTIONS ===== */
  ESP_LOGI(TAG, "\n===== Data Storage Options =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. SD Card:");
  ESP_LOGI(TAG, "   - Large capacity");
  ESP_LOGI(TAG, "   - Removable for analysis");
  ESP_LOGI(TAG, "   - Requires SPI/SDIO interface");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. SPIFFS/LittleFS:");
  ESP_LOGI(TAG, "   - Built-in flash storage");
  ESP_LOGI(TAG, "   - Limited capacity (2-4MB typical)");
  ESP_LOGI(TAG, "   - No external hardware");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Cloud Upload:");
  ESP_LOGI(TAG, "   - WiFi/cellular required");
  ESP_LOGI(TAG, "   - Unlimited storage");
  ESP_LOGI(TAG, "   - Real-time access");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. UART/USB:");
  ESP_LOGI(TAG, "   - Stream to PC");
  ESP_LOGI(TAG, "   - Real-time monitoring");
  ESP_LOGI(TAG, "   - Requires connected host");

  /* ===== ANALYSIS RECOMMENDATIONS ===== */
  ESP_LOGI(TAG, "\n===== Data Analysis Recommendations =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Key metrics to track:");
  ESP_LOGI(TAG, "1. Voltage trends");
  ESP_LOGI(TAG, "   - Detect capacity fade");
  ESP_LOGI(TAG, "   - Identify weak cells");
  ESP_LOGI(TAG, "   - Monitor cell balance");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Current patterns");
  ESP_LOGI(TAG, "   - Usage profiles");
  ESP_LOGI(TAG, "   - Peak demand analysis");
  ESP_LOGI(TAG, "   - Efficiency calculations");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Temperature correlation");
  ESP_LOGI(TAG, "   - Thermal management effectiveness");
  ESP_LOGI(TAG, "   - Ambient vs internal temp");
  ESP_LOGI(TAG, "   - Cooling system performance");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. SOC accuracy");
  ESP_LOGI(TAG, "   - Compare to actual capacity");
  ESP_LOGI(TAG, "   - Calibration needs");
  ESP_LOGI(TAG, "   - Algorithm validation");

  /* ===== LOGGING BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Logging Best Practices =====");
  ESP_LOGI(TAG, "1. Choose appropriate sample rate");
  ESP_LOGI(TAG, "   - Fast events: 10-100 Hz");
  ESP_LOGI(TAG, "   - Normal monitoring: 0.1-1 Hz");
  ESP_LOGI(TAG, "   - Long-term trends: 0.01-0.1 Hz");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Implement circular buffering");
  ESP_LOGI(TAG, "   - Prevents storage overflow");
  ESP_LOGI(TAG, "   - Keeps most recent data");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Add event triggers");
  ESP_LOGI(TAG, "   - Increase rate on faults");
  ESP_LOGI(TAG, "   - Capture transients");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Timestamp all samples");
  ESP_LOGI(TAG, "   - Essential for analysis");
  ESP_LOGI(TAG, "   - Use RTC for accuracy");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "5. Include metadata");
  ESP_LOGI(TAG, "   - Firmware version");
  ESP_LOGI(TAG, "   - Device serial number");
  ESP_LOGI(TAG, "   - Configuration");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "6. Rotate log files");
  ESP_LOGI(TAG, "   - Daily/weekly rotation");
  ESP_LOGI(TAG, "   - Prevent single file too large");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "7. Compress old data");
  ESP_LOGI(TAG, "   - Save storage space");
  ESP_LOGI(TAG, "   - Archive long-term");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "8. Test recovery from incomplete writes");
  ESP_LOGI(TAG, "   - Power loss during logging");
  ESP_LOGI(TAG, "   - Storage full conditions");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nData logging example complete");
}
