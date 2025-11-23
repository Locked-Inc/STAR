/**
 * @file 216_canbus_sensor.c
 * @brief CAN bus automotive sensor network
 */

#include <esp_log.h>
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "CANBUS";

#define CAN_TX_PIN  (GPIO_NUM_21)
#define CAN_RX_PIN  (GPIO_NUM_22)

/* OBD-II PIDs */
#define OBD_MODE_CURRENT (0x01)
#define PID_ENGINE_RPM (0x0C)
#define PID_VEHICLE_SPEED (0x0D)
#define PID_COOLANT_TEMP (0x05)
#define PID_THROTTLE_POS (0x11)

static int priv_obd_query(uint8_t mode, uint8_t pid, uint8_t *response, size_t *len)
{
  twai_message_t tx_msg = {
    .identifier = 0x7DF,  /* OBD broadcast */
    .data_length_code = 8,
    .data = {0x02, mode, pid, 0, 0, 0, 0, 0}
  };

  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) != ESP_OK) return -1;

  twai_message_t rx_msg;
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(500)) != ESP_OK) return -1;

  if (rx_msg.identifier >= 0x7E8 && rx_msg.identifier <= 0x7EF) {
    *len = rx_msg.data[0] - 2;
    memcpy(response, &rx_msg.data[3], *len);
    return 0;
  }

  return -1;
}

void canbus_sensor_example(void)
{
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  ESP_LOGI(s_tag, "CAN bus started at 500kbps");

  for (int i = 0; i < 100; i++) {
    uint8_t response[4];
    size_t len;

    /* Query RPM */
    if (priv_obd_query(OBD_MODE_CURRENT, PID_ENGINE_RPM, response, &len) == 0 && len >= 2) {
      int rpm = ((response[0] << 8) | response[1]) / 4;
      ESP_LOGI(s_tag, "Engine RPM: %d", rpm);
    }

    /* Query Speed */
    if (priv_obd_query(OBD_MODE_CURRENT, PID_VEHICLE_SPEED, response, &len) == 0 && len >= 1) {
      int speed = response[0];
      ESP_LOGI(s_tag, "Speed: %d km/h", speed);
    }

    /* Query Coolant Temp */
    if (priv_obd_query(OBD_MODE_CURRENT, PID_COOLANT_TEMP, response, &len) == 0 && len >= 1) {
      int temp = response[0] - 40;
      ESP_LOGI(s_tag, "Coolant: %d C", temp);
    }

    /* Query Throttle */
    if (priv_obd_query(OBD_MODE_CURRENT, PID_THROTTLE_POS, response, &len) == 0 && len >= 1) {
      int throttle = response[0] * 100 / 255;
      ESP_LOGI(s_tag, "Throttle: %d%%", throttle);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  twai_stop();
  twai_driver_uninstall();
}

void app_main(void) { canbus_sensor_example(); }
