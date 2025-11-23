/**
 * @file 006_i2c_async.c
 * @brief Asynchronous I2C operations example
 *
 * Demonstrates:
 * - Async I2C write and read
 * - Waiting for completion
 * - Checking operation status
 * - Canceling operations
 * - Event group integration
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

static const char *s_tag = "I2C_ASYNC_EXAMPLE";

#define I2C_SDA_PIN        (GPIO_NUM_21)
#define I2C_SCL_PIN        (GPIO_NUM_22)
#define I2C_CLK_SPEED      (100000)
#define SENSOR_ADDR        (0x48)

#define ASYNC_COMPLETE_BIT (BIT0)

static star_bus_manager_t s_manager;
static EventGroupHandle_t s_event_group;

/* Async completion callback */
static void priv_async_complete_callback(star_async_handle_t handle,
                                         star_async_status_t status,
                                         esp_err_t result, void *context)
{
  const char *op_name = (const char *)context;
  if (result == ESP_OK) {
    ESP_LOGI(s_tag, "Async %s completed successfully", op_name);
  } else {
    ESP_LOGE(s_tag, "Async %s failed: %s", op_name, esp_err_to_name(result));
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Async I2C Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "async_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create event group for async notifications */
  s_event_group = xEventGroupCreate();
  if (s_event_group == NULL) {
    ESP_LOGE(s_tag, "Failed to create event group");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "async_sensor",
    I2C_NUM_0,
    SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus");
    return;
  }

  ret = star_bus_config_init(i2c_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C");
    return;
  }

  ESP_LOGI(s_tag, "I2C initialized for async operations");

  /* --- Async Write Example --- */
  ESP_LOGI(s_tag, "\n--- Async Write ---");

  uint8_t             write_data[] = {0x01, 0x55};
  star_async_handle_t write_handle;

  star_async_config_t write_config = {
    .timeout_ms = 1000,
    .callback   = priv_async_complete_callback,
    .context    = "write",
    .priority   = 0
  };

  ret = star_bus_i2c_write_async(
    &s_manager,
    "async_sensor",
    write_data,
    sizeof(write_data),
    0x00,  /* register address */
    &write_config,
    &write_handle
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async write started");

    /* Wait for completion */
    ret = star_async_wait(write_handle, 1000);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Write completed");
    }

    star_async_free_handle(write_handle);
  }

  /* --- Async Read Example --- */
  ESP_LOGI(s_tag, "\n--- Async Read ---");

  uint8_t             read_buffer[4];
  star_async_handle_t read_handle;

  star_async_config_t read_config = {
    .timeout_ms = 1000,
    .callback   = priv_async_complete_callback,
    .context    = "read",
    .priority   = 0
  };

  ret = star_bus_i2c_read_async(
    &s_manager,
    "async_sensor",
    read_buffer,
    sizeof(read_buffer),
    0x00,  /* register address */
    &read_config,
    &read_handle
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async read started");

    /* Poll status instead of waiting */
    star_async_status_t status;
    do {
      status = star_async_get_status(read_handle);
      vTaskDelay(pdMS_TO_TICKS(10));
    } while (status == STAR_ASYNC_STATUS_PENDING);

    if (status == STAR_ASYNC_STATUS_COMPLETE) {
      ESP_LOGI(s_tag, "Read data: 0x%02X 0x%02X 0x%02X 0x%02X",
               read_buffer[0], read_buffer[1], read_buffer[2], read_buffer[3]);
    }

    star_async_free_handle(read_handle);
  }

  /* --- Event Group Integration --- */
  ESP_LOGI(s_tag, "\n--- Event Group Integration ---");

  star_async_handle_t event_handle;
  uint8_t             event_buffer[2];

  star_async_config_t event_config = {
    .timeout_ms = 1000,
    .callback   = NULL,
    .context    = NULL,
    .priority   = 0
  };

  ret = star_bus_i2c_read_async(
    &s_manager,
    "async_sensor",
    event_buffer,
    sizeof(event_buffer),
    0x00,
    &event_config,
    &event_handle
  );

  if (ret == ESP_OK) {
    /* Set event bits on completion */
    star_async_set_event_bits(event_handle, s_event_group, ASYNC_COMPLETE_BIT,
                              0);

    /* Wait on event group */
    EventBits_t bits = xEventGroupWaitBits(
      s_event_group,
      ASYNC_COMPLETE_BIT,
      pdTRUE,
      pdFALSE,
      pdMS_TO_TICKS(1000)
    );

    if (bits & ASYNC_COMPLETE_BIT) {
      ESP_LOGI(s_tag, "Event received - async operation complete");
    }

    star_async_free_handle(event_handle);
  }

  /* --- Cancel Operation Example --- */
  ESP_LOGI(s_tag, "\n--- Cancel Operation ---");

  star_async_handle_t cancel_handle;
  uint8_t             cancel_buffer[16];

  star_async_config_t cancel_config = {
    .timeout_ms = 1000,
    .callback   = NULL,
    .context    = NULL,
    .priority   = 0
  };

  ret = star_bus_i2c_read_async(
    &s_manager,
    "async_sensor",
    cancel_buffer,
    sizeof(cancel_buffer),
    0x00,
    &cancel_config,
    &cancel_handle
  );

  if (ret == ESP_OK) {
    /* Cancel immediately */
    ret = star_async_cancel(cancel_handle);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Operation cancelled");
    }

    star_async_status_t status = star_async_get_status(cancel_handle);
    ESP_LOGI(s_tag, "Status after cancel: %d", status);

    star_async_free_handle(cancel_handle);
  }

  /* --- Async Statistics --- */
  ESP_LOGI(s_tag, "\n--- Async Statistics ---");

  uint32_t pending_ops;
  uint64_t completed_ops;
  uint64_t failed_ops;
  uint64_t cancelled_ops;

  ret = star_async_get_stats(&s_manager, "async_sensor", &pending_ops,
                             &completed_ops, &failed_ops, &cancelled_ops);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pending operations: %lu", (unsigned long)pending_ops);
    ESP_LOGI(s_tag, "Completed: %llu", (unsigned long long)completed_ops);
    ESP_LOGI(s_tag, "Failed: %llu", (unsigned long long)failed_ops);
    ESP_LOGI(s_tag, "Cancelled: %llu", (unsigned long long)cancelled_ops);
  }

  /* Cleanup */
  vEventGroupDelete(s_event_group);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nAsync I2C example complete");
}
