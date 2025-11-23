/**
 * @file 168_imx219_stereo_camera.c
 * @brief IMX219-83 stereo camera module example
 *
 * Demonstrates:
 * - Dual camera initialization
 * - Synchronized stereo capture
 * - Exposure and gain control
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_camera_imx219.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "IMX219_STEREO";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define IMX219_LEFT_ADDR (0x10)
#define IMX219_RIGHT_ADDR (0x36)

void imx219_stereo_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Camera_Manager", NULL, NULL);

  /* Create I2C configs for both cameras */
  star_bus_config_t* left_bus = star_bus_config_create_i2c(
    "cam_left", I2C_NUM_0, IMX219_LEFT_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_config_t* right_bus = star_bus_config_create_i2c(
    "cam_right", I2C_NUM_0, IMX219_RIGHT_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  star_bus_manager_add_bus(&manager, left_bus);
  star_bus_manager_add_bus(&manager, right_bus);
  star_bus_config_init(left_bus, &manager);
  star_bus_config_init(right_bus, &manager);

  imx219_handle_t stereo;
  imx219_config_t cfg = {
    .manager = &manager,
    .bus_name = "cam_left",
    .left_addr = IMX219_LEFT_ADDR,
    .right_addr = IMX219_RIGHT_ADDR,
    .mode = IMX219_MODE_1920x1080_30FPS,
    .enable_stereo = true
  };

  if (star_camera_imx219_init(&stereo, &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init stereo camera");
    return;
  }

  ESP_LOGI(s_tag, "IMX219 stereo camera initialized (1920x1080 @ 30fps)");

  /* Configure exposure and gain for both cameras */
  star_camera_imx219_set_exposure(&stereo, 10000);   /* 10ms */
  star_camera_imx219_set_gain(&stereo, 128);         /* Mid-range gain */

  /* Start streaming */
  star_camera_imx219_start_streaming(&stereo);

  /* Capture synchronized frames */
  for (int frame = 0; frame < 100; frame++) {
    imx219_frame_t frame_left;
    imx219_frame_t frame_right;

    if (star_camera_imx219_capture_stereo(&stereo, &frame_left, &frame_right) == ESP_OK) {
      int64_t timestamp_diff = (int64_t)frame_right.timestamp_us - (int64_t)frame_left.timestamp_us;

      ESP_LOGI(s_tag, "Frame %d: Left %ux%u (%zu bytes), Right %ux%u (%zu bytes), Delta=%lld us",
               frame,
               frame_left.width, frame_left.height, frame_left.buffer_size,
               frame_right.width, frame_right.height, frame_right.buffer_size,
               timestamp_diff);

      /* Process frames for depth estimation here */
      /* frame_left.buffer, frame_right.buffer */

      /* Note: Frames are managed by the driver, no explicit release needed */
    }

    vTaskDelay(pdMS_TO_TICKS(33));  /* ~30 fps */
  }

  star_camera_imx219_stop_streaming(&stereo);
  star_camera_imx219_deinit(&stereo);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { imx219_stereo_example(); }
