/**
 * @file 217_dmx512_lighting.c
 * @brief DMX512 lighting control
 */

#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "DMX512";

#define DMX_UART      (UART_NUM_2)
#define DMX_TX_PIN    (GPIO_NUM_17)
#define DMX_DE_PIN    (GPIO_NUM_4)

#define DMX_CHANNELS (512)

static uint8_t dmx_data[DMX_CHANNELS + 1];  /* +1 for start code */

static void priv_dmx_send(void)
{
  /* Send break (low for >88us) */
  gpio_set_level(DMX_DE_PIN, 1);
  uart_set_line_inverse(DMX_UART, UART_SIGNAL_TXD_INV);
  esp_rom_delay_us(100);
  uart_set_line_inverse(DMX_UART, 0);

  /* MAB (high for >8us) */
  esp_rom_delay_us(12);

  /* Send data */
  dmx_data[0] = 0;  /* Start code */
  uart_write_bytes(DMX_UART, dmx_data, DMX_CHANNELS + 1);
  uart_wait_tx_done(DMX_UART, pdMS_TO_TICKS(50));
}

static void priv_set_rgb_fixture(int start_addr, uint8_t r, uint8_t g, uint8_t b)
{
  if (start_addr >= 1 && start_addr <= DMX_CHANNELS - 2) {
    dmx_data[start_addr] = r;
    dmx_data[start_addr + 1] = g;
    dmx_data[start_addr + 2] = b;
  }
}

static void priv_set_dimmer(int addr, uint8_t level)
{
  if (addr >= 1 && addr <= DMX_CHANNELS) {
    dmx_data[addr] = level;
  }
}

void dmx512_example(void)
{
  uart_config_t uart_cfg = {
    .baud_rate = 250000,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_2,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  uart_driver_install(DMX_UART, 0, 1024, 0, NULL, 0);
  uart_param_config(DMX_UART, &uart_cfg);
  uart_set_pin(DMX_UART, DMX_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  gpio_set_direction(DMX_DE_PIN, GPIO_MODE_OUTPUT);

  memset(dmx_data, 0, sizeof(dmx_data));

  ESP_LOGI(s_tag, "DMX512 controller started");

  /* Demo: Color chase */
  for (int i = 0; i < 200; i++) {
    /* Clear all */
    memset(&dmx_data[1], 0, DMX_CHANNELS);

    /* RGB fixtures at addresses 1, 4, 7, 10 */
    int phase = i % 4;
    priv_set_rgb_fixture(1 + phase * 3, 255, 0, 0);
    priv_set_rgb_fixture(1 + ((phase + 1) % 4) * 3, 0, 255, 0);
    priv_set_rgb_fixture(1 + ((phase + 2) % 4) * 3, 0, 0, 255);
    priv_set_rgb_fixture(1 + ((phase + 3) % 4) * 3, 255, 255, 255);

    /* Dimmer channel at 13 - fade */
    priv_set_dimmer(13, (i * 5) % 256);

    priv_dmx_send();
    ESP_LOGI(s_tag, "DMX frame %d sent", i);

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  uart_driver_delete(DMX_UART);
}

void app_main(void) { dmx512_example(); }
