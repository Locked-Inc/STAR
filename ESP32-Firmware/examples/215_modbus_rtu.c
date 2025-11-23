/**
 * @file 215_modbus_rtu.c
 * @brief Modbus RTU industrial sensor communication
 */

#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "MODBUS_RTU";

#define MODBUS_UART    (UART_NUM_2)
#define MODBUS_TX_PIN  (GPIO_NUM_17)
#define MODBUS_RX_PIN  (GPIO_NUM_16)
#define MODBUS_DE_PIN  GPIO_NUM_4  /* RS485 direction */

#define SLAVE_ADDR (0x01)

static uint16_t priv_crc16_modbus(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  return crc;
}

static int priv_modbus_read_registers(uint8_t slave, uint16_t start, uint16_t count, uint16_t *values)
{
  uint8_t request[8];
  request[0] = slave;
  request[1] = 0x03;  /* Read holding registers */
  request[2] = start >> 8;
  request[3] = start & 0xFF;
  request[4] = count >> 8;
  request[5] = count & 0xFF;
  uint16_t crc = priv_crc16_modbus(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  /* Enable TX */
  gpio_set_level(MODBUS_DE_PIN, 1);
  uart_write_bytes(MODBUS_UART, request, 8);
  uart_wait_tx_done(MODBUS_UART, pdMS_TO_TICKS(100));
  gpio_set_level(MODBUS_DE_PIN, 0);

  /* Read response */
  uint8_t response[256];
  int len = uart_read_bytes(MODBUS_UART, response, sizeof(response), pdMS_TO_TICKS(500));

  if (len < 5) return -1;
  if (response[0] != slave || response[1] != 0x03) return -1;

  /* Verify CRC */
  uint16_t rx_crc = response[len - 2] | (response[len - 1] << 8);
  if (rx_crc != priv_crc16_modbus(response, len - 2)) return -1;

  /* Extract values */
  int data_len = response[2];
  for (int i = 0; i < count && i * 2 < data_len; i++) {
    values[i] = (response[3 + i * 2] << 8) | response[4 + i * 2];
  }

  return count;
}

void modbus_rtu_example(void)
{
  /* Configure UART */
  uart_config_t uart_cfg = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  uart_driver_install(MODBUS_UART, 256, 256, 0, NULL, 0);
  uart_param_config(MODBUS_UART, &uart_cfg);
  uart_set_pin(MODBUS_UART, MODBUS_TX_PIN, MODBUS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  /* Configure RS485 direction pin */
  gpio_set_direction(MODBUS_DE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(MODBUS_DE_PIN, 0);

  ESP_LOGI(s_tag, "Modbus RTU master started");

  for (int i = 0; i < 50; i++) {
    uint16_t values[10];

    /* Read holding registers 0-9 */
    if (priv_modbus_read_registers(SLAVE_ADDR, 0, 10, values) > 0) {
      ESP_LOGI(s_tag, "Registers: %d %d %d %d %d",
               values[0], values[1], values[2], values[3], values[4]);
    } else {
      ESP_LOGI(s_tag, "Modbus read failed");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  uart_driver_delete(MODBUS_UART);
}

void app_main(void) { modbus_rtu_example(); }
