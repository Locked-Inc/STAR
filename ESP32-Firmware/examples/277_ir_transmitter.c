/**
 * @file 277_ir_transmitter.c
 * @brief IR remote transmitter (NEC protocol)
 */

#include <esp_log.h>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "IR_TX";

#define IR_PIN (GPIO_NUM_18)
#define RMT_TX_CHANNEL (RMT_CHANNEL_1)

static rmt_item32_t nec_items[34];

static void priv_build_nec_frame(uint8_t addr, uint8_t cmd) {
  /* Leader */
  nec_items[0].duration0 = 9000;
  nec_items[0].level0 = 1;
  nec_items[0].duration1 = 4500;
  nec_items[0].level1 = 0;

  uint32_t data = addr | ((~addr & 0xFF) << 8) | (cmd << 16) | ((~cmd & 0xFF) << 24);

  for (int i = 0; i < 32; i++) {
    nec_items[i + 1].duration0 = 560;
    nec_items[i + 1].level0 = 1;
    nec_items[i + 1].duration1 = (data & (1 << i)) ? 1690 : 560;
    nec_items[i + 1].level1 = 0;
  }

  /* Stop bit */
  nec_items[33].duration0 = 560;
  nec_items[33].level0 = 1;
  nec_items[33].duration1 = 0;
  nec_items[33].level1 = 0;
}

void ir_transmitter_example(void)
{
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(IR_PIN, RMT_TX_CHANNEL);
  config.clk_div = 80;
  config.tx_config.carrier_en = true;
  config.tx_config.carrier_freq_hz = 38000;
  config.tx_config.carrier_duty_percent = 33;

  rmt_config(&config);
  rmt_driver_install(RMT_TX_CHANNEL, 0, 0);

  ESP_LOGI(s_tag, "IR transmitter started");

  uint8_t commands[] = {0x45, 0x46, 0x47, 0x44, 0x40, 0x43};
  int num_cmds = sizeof(commands);

  for (int i = 0; i < 20; i++) {
    uint8_t cmd = commands[i % num_cmds];
    priv_build_nec_frame(0x00, cmd);

    ESP_LOGI(s_tag, "Sending cmd: 0x%02X", cmd);
    rmt_write_items(RMT_TX_CHANNEL, nec_items, 34, true);

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  rmt_driver_uninstall(RMT_TX_CHANNEL);
}

void app_main(void) { ir_transmitter_example(); }
