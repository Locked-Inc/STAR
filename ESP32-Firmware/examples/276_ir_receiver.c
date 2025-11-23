/**
 * @file 276_ir_receiver.c
 * @brief IR remote receiver (NEC protocol)
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "IR_RX";

#define IR_PIN (GPIO_NUM_15)
#define RMT_RX_CHANNEL (RMT_CHANNEL_0)

void ir_receiver_example(void)
{
  rmt_config_t config = RMT_DEFAULT_CONFIG_RX(IR_PIN, RMT_RX_CHANNEL);
  config.clk_div = 80;
  config.rx_config.filter_en = true;
  config.rx_config.filter_ticks_thresh = 100;
  config.rx_config.idle_threshold = 12000;

  rmt_config(&config);
  rmt_driver_install(RMT_RX_CHANNEL, 1000, 0);

  RingbufHandle_t rb;
  rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);
  rmt_rx_start(RMT_RX_CHANNEL, true);

  ESP_LOGI(s_tag, "IR receiver started");

  for (int i = 0; i < 100; i++) {
    size_t rx_size;
    rmt_item32_t *items = (rmt_item32_t*)xRingbufferReceive(rb, &rx_size, pdMS_TO_TICKS(1000));

    if (items) {
      int num_items = rx_size / sizeof(rmt_item32_t);
      if (num_items >= 32) {
        uint32_t code = 0;
        for (int j = 0; j < 32; j++) {
          if (items[j + 1].duration1 > 1000) {
            code |= (1 << j);
          }
        }
        uint8_t addr = code & 0xFF;
        uint8_t cmd = (code >> 16) & 0xFF;
        ESP_LOGI(s_tag, "IR: addr=0x%02X cmd=0x%02X", addr, cmd);
      }
      vRingbufferReturnItem(rb, items);
    }
  }

  rmt_driver_uninstall(RMT_RX_CHANNEL);
}

void app_main(void) { ir_receiver_example(); }
