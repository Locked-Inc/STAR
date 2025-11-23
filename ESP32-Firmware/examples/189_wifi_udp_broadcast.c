/**
 * @file 189_wifi_udp_broadcast.c
 * @brief ESP32 broadcasts sensor data via UDP
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <string.h>

static const char *s_tag = "UDP_BROADCAST";

#define WIFI_SSID       "YourSSID"
#define WIFI_PASSWORD   "YourPassword"
#define BROADCAST_PORT (5000)

void wifi_udp_broadcast_example(void)
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);

  wifi_config_t sta_config = {
    .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
  };
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();
  esp_wifi_connect();

  vTaskDelay(pdMS_TO_TICKS(5000));  /* Wait for connection */

  /* Initialize sensor */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IMU_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  /* Create UDP socket */
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int broadcast = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

  struct sockaddr_in dest_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(BROADCAST_PORT),
    .sin_addr.s_addr = INADDR_BROADCAST,
  };

  ESP_LOGI(s_tag, "Broadcasting on UDP port %d", BROADCAST_PORT);

  for (int i = 0; i < 200; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    char msg[128];
    int len = snprintf(msg, sizeof(msg),
                      "{\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f}",
                      accel.x_g, accel.y_g, accel.z_g);

    sendto(sock, msg, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    ESP_LOGI(s_tag, "Broadcast: %s", msg);

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  close(sock);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { wifi_udp_broadcast_example(); }
