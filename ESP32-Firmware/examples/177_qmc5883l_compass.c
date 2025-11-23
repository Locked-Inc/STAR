/**
 * @file 177_qmc5883l_compass.c
 * @brief QMC5883L digital compass with heading
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_qmc5883l.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "COMPASS";

static const char *heading_to_direction(float heading)
{
  if (heading < 22.5 || heading >= 337.5) return "N";
  if (heading < 67.5) return "NE";
  if (heading < 112.5) return "E";
  if (heading < 157.5) return "SE";
  if (heading < 202.5) return "S";
  if (heading < 247.5) return "SW";
  if (heading < 292.5) return "W";
  return "NW";
}

void compass_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Compass_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "qmc5883l", I2C_NUM_0, 0x0D, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  qmc5883l_handle_t sensor;
  qmc5883l_config_t cfg = {
    .mode = QMC5883L_MODE_CONTINUOUS_VAL,
    .odr = QMC5883L_ODR_10HZ_VAL,
    .range = QMC5883L_RNG_2G_VAL,
    .osr = QMC5883L_OSR_512_VAL
  };
  star_sensor_qmc5883l_init(&sensor, &manager, "qmc5883l", &cfg);

  for (int i = 0; i < 100; i++) {
    qmc5883l_mag_data_t data;
    if (star_sensor_qmc5883l_read_mag(&sensor, &data) == ESP_OK) {
      float heading = atan2f(data.y_gauss, data.x_gauss) * 180.0f / M_PI;
      if (heading < 0) heading += 360.0f;

      ESP_LOGI(s_tag, "Heading: %.1f° %s", heading, heading_to_direction(heading));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_qmc5883l_deinit(&sensor);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { compass_example(); }
