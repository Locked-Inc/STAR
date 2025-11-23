/**
 * @file 230_fingerprint_sensor.c
 * @brief R307 fingerprint sensor
 */

#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "FINGERPRINT";

#define FP_UART     (UART_NUM_2)
#define FP_TX_PIN   (GPIO_NUM_17)
#define FP_RX_PIN   (GPIO_NUM_16)

static uint8_t s_fp_packet[64];

static uint16_t priv_calc_checksum(uint8_t *data, int len)
{
  uint16_t sum = 0;
  for (int i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

static int priv_fp_send_cmd(uint8_t cmd, uint8_t *params, int param_len, uint8_t *response)
{
  int pkt_len = 3 + param_len;

  s_fp_packet[0] = 0xEF; s_fp_packet[1] = 0x01;  /* Header */
  s_fp_packet[2] = 0xFF; s_fp_packet[3] = 0xFF;
  s_fp_packet[4] = 0xFF; s_fp_packet[5] = 0xFF;  /* Address */
  s_fp_packet[6] = 0x01;  /* Command packet */
  s_fp_packet[7] = (pkt_len >> 8) & 0xFF;
  s_fp_packet[8] = pkt_len & 0xFF;
  s_fp_packet[9] = cmd;

  if (params && param_len > 0) {
    memcpy(&s_fp_packet[10], params, param_len);
  }

  uint16_t chk = priv_calc_checksum(&s_fp_packet[6], 4 + param_len);
  s_fp_packet[10 + param_len] = (chk >> 8) & 0xFF;
  s_fp_packet[11 + param_len] = chk & 0xFF;

  uart_write_bytes(FP_UART, s_fp_packet, 12 + param_len);

  int len = uart_read_bytes(FP_UART, response, 64, pdMS_TO_TICKS(2000));
  return len;
}

static uint8_t priv_fp_get_image(void)
{
  uint8_t resp[64];
  int len = priv_fp_send_cmd(0x01, NULL, 0, resp);
  if (len >= 12) return resp[9];
  return 0xFF;
}

static uint8_t priv_fp_img_to_char(uint8_t buffer_id)
{
  uint8_t params[1] = {buffer_id};
  uint8_t resp[64];
  int len = priv_fp_send_cmd(0x02, params, 1, resp);
  if (len >= 12) return resp[9];
  return 0xFF;
}

static uint8_t priv_fp_search(uint16_t *finger_id, uint16_t *score)
{
  uint8_t params[5] = {0x01, 0x00, 0x00, 0x00, 0xA3};  /* Buffer 1, start 0, count 163 */
  uint8_t resp[64];
  int len = priv_fp_send_cmd(0x04, params, 5, resp);
  if (len >= 16 && resp[9] == 0x00) {
    *finger_id = (resp[10] << 8) | resp[11];
    *score = (resp[12] << 8) | resp[13];
    return 0x00;
  }
  return 0xFF;
}

void fingerprint_example(void)
{
  uart_config_t uart_cfg = {
    .baud_rate = 57600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  uart_driver_install(FP_UART, 256, 256, 0, NULL, 0);
  uart_param_config(FP_UART, &uart_cfg);
  uart_set_pin(FP_UART, FP_TX_PIN, FP_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  ESP_LOGI(s_tag, "Fingerprint sensor initialized");

  for (int i = 0; i < 100; i++) {
    ESP_LOGI(s_tag, "Place finger on sensor...");

    uint8_t result = priv_fp_get_image();
    if (result == 0x00) {
      ESP_LOGI(s_tag, "Image captured, processing...");

      result = priv_fp_img_to_char(1);
      if (result == 0x00) {
        uint16_t finger_id, score;
        result = priv_fp_search(&finger_id, &score);

        if (result == 0x00) {
          ESP_LOGI(s_tag, "Fingerprint matched! ID: %d, Score: %d", finger_id, score);
        } else {
          ESP_LOGI(s_tag, "Fingerprint not found");
        }
      }
    } else if (result == 0x02) {
      ESP_LOGI(s_tag, "No finger detected");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  uart_driver_delete(FP_UART);
}

void app_main(void) { fingerprint_example(); }
