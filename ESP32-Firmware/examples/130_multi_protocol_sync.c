/**
 * @file 130_multi_protocol_sync.c
 * @brief Protocol synchronization across multiple buses
 *
 * Demonstrates:
 * - Synchronize operations across protocols
 * - Barrier patterns
 * - Ordered execution
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char* TAG = "MULTI_PROTO_SYNC";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS_PIN       (GPIO_NUM_5)

#define SENSOR_ADDR      (0x48)

/* Synchronization bits */
#define I2C_READY_BIT    (BIT0)
#define SPI_READY_BIT    (BIT1)
#define ALL_READY_BITS   (I2C_READY_BIT | SPI_READY_BIT)

#define I2C_DONE_BIT     (BIT4)
#define SPI_DONE_BIT     (BIT5)
#define ALL_DONE_BITS    (I2C_DONE_BIT | SPI_DONE_BIT)

static star_bus_manager_t g_manager;
static EventGroupHandle_t g_sync_events;
static SemaphoreHandle_t g_sequence_mutex;

/* Sequence counter for ordered operations */
static volatile int g_sequence = 0;

/* Barrier synchronization structure */
typedef struct {
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t barrier;
    int count;
    int threshold;
} barrier_t;

static barrier_t g_barrier;

static void barrier_init(barrier_t* b, int threshold)
{
    b->mutex = xSemaphoreCreateMutex();
    b->barrier = xSemaphoreCreateBinary();
    b->count = 0;
    b->threshold = threshold;
}

static void barrier_wait(barrier_t* b)
{
    xSemaphoreTake(b->mutex, portMAX_DELAY);
    b->count++;
    if (b->count >= b->threshold) {
        b->count = 0;
        /* Release all waiting threads */
        for (int i = 0; i < b->threshold - 1; i++) {
            xSemaphoreGive(b->barrier);
        }
        xSemaphoreGive(b->mutex);
    } else {
        xSemaphoreGive(b->mutex);
        xSemaphoreTake(b->barrier, portMAX_DELAY);
    }
}

static void barrier_deinit(barrier_t* b)
{
    vSemaphoreDelete(b->mutex);
    vSemaphoreDelete(b->barrier);
}

/* I2C synchronized task */
static void i2c_sync_task(void* arg)
{
    uint8_t buffer[4];
    size_t bytes_read;

    ESP_LOGI(TAG, "I2C task: waiting at barrier...");

    /* Signal ready */
    xEventGroupSetBits(g_sync_events, I2C_READY_BIT);

    /* Wait for all protocols ready */
    xEventGroupWaitBits(g_sync_events, ALL_READY_BITS, pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "I2C task: starting synchronized operation");

    /* Perform I2C operation */
    esp_err_t ret = star_bus_i2c_read(&g_manager, "i2c_sensor", buffer, 4, 0x00, &bytes_read);

    /* Record sequence */
    xSemaphoreTake(g_sequence_mutex, portMAX_DELAY);
    int seq = g_sequence++;
    xSemaphoreGive(g_sequence_mutex);

    ESP_LOGI(TAG, "I2C task: completed (seq=%d, result=%s)", seq, esp_err_to_name(ret));

    /* Signal done */
    xEventGroupSetBits(g_sync_events, I2C_DONE_BIT);

    vTaskDelete(NULL);
}

/* SPI synchronized task */
static void spi_sync_task(void* arg)
{
    uint8_t tx_buf[4] = {0x9F, 0, 0, 0};
    uint8_t rx_buf[4];

    ESP_LOGI(TAG, "SPI task: waiting at barrier...");

    /* Signal ready */
    xEventGroupSetBits(g_sync_events, SPI_READY_BIT);

    /* Wait for all protocols ready */
    xEventGroupWaitBits(g_sync_events, ALL_READY_BITS, pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "SPI task: starting synchronized operation");

    /* Perform SPI operation */
    esp_err_t ret = star_bus_spi_transfer(&g_manager, "spi_device", tx_buf, rx_buf, 4, 0);

    /* Record sequence */
    xSemaphoreTake(g_sequence_mutex, portMAX_DELAY);
    int seq = g_sequence++;
    xSemaphoreGive(g_sequence_mutex);

    ESP_LOGI(TAG, "SPI task: completed (seq=%d, result=%s)", seq, esp_err_to_name(ret));

    /* Signal done */
    xEventGroupSetBits(g_sync_events, SPI_DONE_BIT);

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Multi-Protocol Synchronization Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "sync_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create synchronization primitives */
    g_sync_events = xEventGroupCreate();
    g_sequence_mutex = xSemaphoreCreateMutex();
    barrier_init(&g_barrier, 2);

    /* Create I2C bus */
    star_bus_config_t* i2c_config = star_bus_config_create_i2c(
        "i2c_sensor", I2C_NUM_0, SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (i2c_config) {
        star_bus_manager_add_bus(&g_manager, i2c_config);
        star_bus_config_init(i2c_config, &g_manager);
    }

    /* Create SPI bus */
    spi_device_interface_config_t spi_dev_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* spi_config = star_bus_config_create_spi_device(
        "spi_device", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_dev_cfg);

    if (spi_config) {
        star_bus_manager_add_bus(&g_manager, spi_config);
        star_bus_config_init(spi_config, &g_manager);
    }

    /* --- Barrier Synchronization --- */
    ESP_LOGI(TAG, "\n--- Barrier Synchronization ---");

    xEventGroupClearBits(g_sync_events, 0xFFFFFF);
    g_sequence = 0;

    /* Create synchronized tasks */
    xTaskCreate(i2c_sync_task, "i2c_sync", 4096, NULL, 5, NULL);
    xTaskCreate(spi_sync_task, "spi_sync", 4096, NULL, 5, NULL);

    /* Wait for all operations to complete */
    xEventGroupWaitBits(g_sync_events, ALL_DONE_BITS, pdTRUE, pdTRUE, pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "All synchronized operations complete");

    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Ordered Execution --- */
    ESP_LOGI(TAG, "\n--- Ordered Execution (I2C then SPI) ---");

    uint8_t i2c_buf[4], spi_tx[4] = {0x9F}, spi_rx[4];
    size_t bytes;

    int64_t start = esp_timer_get_time();

    /* Step 1: I2C read */
    ESP_LOGI(TAG, "Step 1: I2C read...");
    ret = star_bus_i2c_read(&g_manager, "i2c_sensor", i2c_buf, 4, 0x00, &bytes);
    ESP_LOGI(TAG, "  I2C result: %s", esp_err_to_name(ret));

    /* Step 2: SPI transfer (depends on I2C result) */
    ESP_LOGI(TAG, "Step 2: SPI transfer...");
    spi_tx[1] = i2c_buf[0];  /* Use I2C data in SPI command */
    ret = star_bus_spi_transfer(&g_manager, "spi_device", spi_tx, spi_rx, 4, 0);
    ESP_LOGI(TAG, "  SPI result: %s", esp_err_to_name(ret));

    int64_t elapsed = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "Ordered execution took %lld us", elapsed);

    /* --- Pipeline Pattern --- */
    ESP_LOGI(TAG, "\n--- Pipeline Pattern ---");

    ESP_LOGI(TAG, "Stage 1: Read config from I2C EEPROM");
    ESP_LOGI(TAG, "Stage 2: Configure SPI device with config");
    ESP_LOGI(TAG, "Stage 3: Perform SPI data transfer");

    /* Stage 1 */
    uint8_t config_data[4];
    star_bus_i2c_read(&g_manager, "i2c_sensor", config_data, 4, 0x00, &bytes);
    ESP_LOGI(TAG, "  Config read: 0x%02X%02X%02X%02X",
             config_data[0], config_data[1], config_data[2], config_data[3]);

    /* Stage 2 - Use config for SPI */
    uint8_t spi_cmd[2] = {0x01, config_data[0]};
    star_bus_spi_transmit(&g_manager, "spi_device", spi_cmd, 2, 0);
    ESP_LOGI(TAG, "  SPI configured with 0x%02X", config_data[0]);

    /* Stage 3 - SPI transfer */
    star_bus_spi_transfer(&g_manager, "spi_device", spi_tx, spi_rx, 4, 0);
    ESP_LOGI(TAG, "  SPI transfer complete");

    /* --- Async Synchronization --- */
    ESP_LOGI(TAG, "\n--- Async with Event Synchronization ---");

    star_async_handle_t i2c_handle, spi_handle;

    star_async_config_t async_cfg = {
        .timeout_ms = 1000,
        .callback = NULL,
        .context = NULL,
        .priority = 5
    };

    xEventGroupClearBits(g_sync_events, ALL_DONE_BITS);

    /* Start both async operations */
    ret = star_bus_i2c_read_async(&g_manager, "i2c_sensor", i2c_buf, 4, 0x00,
                                   &async_cfg, &i2c_handle);
    if (ret == ESP_OK) {
        star_async_set_event_bits(i2c_handle, g_sync_events, I2C_DONE_BIT, 0);
    }

    ret = star_bus_spi_transceive_async(&g_manager, "spi_device", spi_tx, spi_rx, 4,
                                         &async_cfg, &spi_handle);
    if (ret == ESP_OK) {
        star_async_set_event_bits(spi_handle, g_sync_events, SPI_DONE_BIT, 0);
    }

    /* Wait for both */
    EventBits_t bits = xEventGroupWaitBits(g_sync_events, ALL_DONE_BITS,
                                            pdTRUE, pdTRUE, pdMS_TO_TICKS(2000));

    if ((bits & ALL_DONE_BITS) == ALL_DONE_BITS) {
        ESP_LOGI(TAG, "Both async operations synchronized");
    }

    star_async_free_handle(i2c_handle);
    star_async_free_handle(spi_handle);

    /* Cleanup */
    barrier_deinit(&g_barrier);
    vSemaphoreDelete(g_sequence_mutex);
    vEventGroupDelete(g_sync_events);
    star_bus_manager_deinit(&g_manager);

    ESP_LOGI(TAG, "\nMulti-protocol sync example complete");
}
