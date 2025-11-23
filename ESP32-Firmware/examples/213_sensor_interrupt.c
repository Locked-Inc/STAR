/**
 * @file 213_sensor_interrupt.c
 * @brief Hardware interrupt-driven sensor reading
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static const char *s_tag = "SENSOR_INT";

#define MPU6050_INT_PIN (GPIO_NUM_19)

static QueueHandle_t gpio_evt_queue;
static mpu6050_handle_t imu;

static void IRAM_ATTR priv_gpio_isr_handler(void *arg)
{
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void priv_interrupt_task(void *arg)
{
  uint32_t io_num;

  while (1) {
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      mpu6050_accel_t accel;
      mpu6050_gyro_t gyro;
      star_sensor_mpu6050_read_accel(&imu, &accel);
      star_sensor_mpu6050_read_gyro(&imu, &gyro);

      ESP_LOGI(s_tag, "INT! A:%.2f,%.2f,%.2f G:%.1f,%.1f,%.1f",
               accel.x_g, accel.y_g, accel.z_g,
               gyro.x_dps, gyro.y_dps, gyro.z_dps);
    }
  }
}

void sensor_interrupt_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "INT_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_config_t cfg = {
    .i2c_addr = 0x68,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  /* NOTE: MPU6050 interrupt configuration not yet implemented in library */
  /* For now, we'll use a timer-based approach instead of hardware interrupts */

  /* Configure GPIO interrupt */
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << MPU6050_INT_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .intr_type = GPIO_INTR_NEGEDGE
  };
  gpio_config(&io_conf);

  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  xTaskCreate(priv_interrupt_task, "int_task", 4096, NULL, 10, NULL);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(MPU6050_INT_PIN, priv_gpio_isr_handler, (void*)MPU6050_INT_PIN);

  ESP_LOGI(s_tag, "Interrupt-driven sensor running");

  vTaskDelay(pdMS_TO_TICKS(30000));

  gpio_isr_handler_remove(MPU6050_INT_PIN);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { sensor_interrupt_example(); }
