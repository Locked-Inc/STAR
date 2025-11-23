/**
 * @file 150_qmc5883l_basic.c
 * @brief Basic QMC5883L 3-axis magnetometer example
 *
 * Demonstrates:
 * - Initializing QMC5883L via I2C
 * - Reading magnetic field data
 * - Calculating compass heading
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_qmc5883l.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "QMC5883L_BASIC";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define QMC5883L_ADDR (0x0D)

void qmc5883l_basic_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Compass_Manager", NULL, NULL);

  /* Create I2C config */
  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "qmc5883l", I2C_NUM_0, QMC5883L_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize QMC5883L */
  qmc5883l_handle_t sensor;
  qmc5883l_config_t cfg = {
    .mode = QMC5883L_MODE_CONTINUOUS_VAL,
    .odr = QMC5883L_ODR_200HZ_VAL,
    .range = QMC5883L_RNG_8G_VAL,
    .osr = QMC5883L_OSR_512_VAL
  };

  ret = star_sensor_qmc5883l_init(&sensor, &manager, "qmc5883l", &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init QMC5883L");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "QMC5883L initialized");

  /* Set magnetic declination for your location (in degrees) */
  float declination = 0.0f;  /* Adjust for your location */

  /* Main reading loop */
  for (int i = 0; i < 100; i++) {
    qmc5883l_mag_data_t data;
    ret = star_sensor_qmc5883l_read_mag(&sensor, &data);

    if (ret == ESP_OK) {
      /* Calculate heading from X and Y */
      float heading = atan2f(data.y_gauss, data.x_gauss) * 180.0f / M_PI;
      heading += declination;

      /* Normalize to 0-360 */
      if (heading < 0) heading += 360.0f;
      if (heading >= 360) heading -= 360.0f;

      const char *direction;
      if (heading < 22.5f || heading >= 337.5f) {
        direction = "N";
      } else if (heading < 67.5f) {
        direction = "NE";
      } else if (heading < 112.5f) {
        direction = "E";
      } else if (heading < 157.5f) {
        direction = "SE";
      } else if (heading < 202.5f) {
        direction = "S";
      } else if (heading < 247.5f) {
        direction = "SW";
      } else if (heading < 292.5f) {
        direction = "W";
      } else {
        direction = "NW";
      }

      ESP_LOGI(s_tag, "Mag: X=%.1f Y=%.1f Z=%.1f G",
               data.x_gauss, data.y_gauss, data.z_gauss);
      ESP_LOGI(s_tag, "Heading: %.1f deg (%s)", heading, direction);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* Cleanup */
  star_sensor_qmc5883l_deinit(&sensor);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void)
{
  qmc5883l_basic_example();
}
