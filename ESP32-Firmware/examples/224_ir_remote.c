/**
 * @file 224_ir_remote.c
 * @brief IR remote control receiver/transmitter
 */

#include <esp_log.h>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "IR_REMOTE";

#define IR_RX_PIN   (GPIO_NUM_19)
#define IR_TX_PIN   (GPIO_NUM_18)
#define RX_CHANNEL  (RMT_CHANNEL_0)
#define TX_CHANNEL  (RMT_CHANNEL_1)

/* NEC protocol timings (in 10us units) */
#define NEC_HDR_MARK (900)
#define NEC_HDR_SPACE (450)
#define NEC_BIT_MARK (56)
#define NEC_ONE_SPACE (169)
#define NEC_ZERO_SPACE (56)

static RingbufHandle_t s_rx_ring_buf;

static uint32_t priv_decode_nec(rmt_item32_t *items, size_t len)
{
  if (len < 34) return 0;

  /* Check header */
  if (items[0].duration0 < 800 || items[0].duration0 > 1000) return 0;
  if (items[0].duration1 < 400 || items[0].duration1 > 500) return 0;

  uint32_t code = 0;
  for (int i = 1; i <= 32; i++) {
    code <<= 1;
    if (items[i].duration1 > 100) {
      code |= 1;
    }
  }

  return code;
}

static void priv_send_nec(uint32_t code)
{
  rmt_item32_t items[34];

  /* Header */
  items[0].duration0 = NEC_HDR_MARK;
  items[0].level0 = 1;
  items[0].duration1 = NEC_HDR_SPACE;
  items[0].level1 = 0;

  /* Data bits */
  for (int i = 31; i >= 0; i--) {
    items[32 - i].duration0 = NEC_BIT_MARK;
    items[32 - i].level0 = 1;
    items[32 - i].duration1 = (code & (1 << i)) ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
    items[32 - i].level1 = 0;
  }

  /* Stop bit */
  items[33].duration0 = NEC_BIT_MARK;
  items[33].level0 = 1;
  items[33].duration1 = 0;
  items[33].level1 = 0;

  rmt_write_items(TX_CHANNEL, items, 34, true);
}

void ir_remote_example(void)
{
  /* RX config */
  rmt_config_t rx_cfg = {
    .rmt_mode = RMT_MODE_RX,
    .channel = RX_CHANNEL,
    .gpio_num = IR_RX_PIN,
    .clk_div = 80,  /* 1us resolution */
    .mem_block_num = 1,
    .rx_config = {
      .idle_threshold = 12000,
      .filter_ticks_thresh = 100,
      .filter_en = true
    }
  };

  rmt_config(&rx_cfg);
  rmt_driver_install(RX_CHANNEL, 1000, 0);
  rmt_get_ringbuf_handle(RX_CHANNEL, &s_rx_ring_buf);
  rmt_rx_start(RX_CHANNEL, true);

  /* TX config */
  rmt_config_t tx_cfg = {
    .rmt_mode = RMT_MODE_TX,
    .channel = TX_CHANNEL,
    .gpio_num = IR_TX_PIN,
    .clk_div = 80,
    .mem_block_num = 1,
    .tx_config = {
      .carrier_freq_hz = 38000,
      .carrier_duty_percent = 33,
      .carrier_en = true,
      .idle_output_en = true,
      .idle_level = RMT_IDLE_LEVEL_LOW
    }
  };

  rmt_config(&tx_cfg);
  rmt_driver_install(TX_CHANNEL, 0, 0);

  ESP_LOGI(s_tag, "IR remote started");

  /* Send test code */
  ESP_LOGI(s_tag, "Sending test code: 0x00FF00FF");
  priv_send_nec(0x00FF00FF);

  /* Receive codes */
  for (int i = 0; i < 100; i++) {
    size_t rx_size;
    rmt_item32_t *items = (rmt_item32_t*)xRingbufferReceive(s_rx_ring_buf, &rx_size, pdMS_TO_TICKS(1000));

    if (items) {
      size_t num_items = rx_size / sizeof(rmt_item32_t);
      uint32_t code = priv_decode_nec(items, num_items);

      if (code) {
        ESP_LOGI(s_tag, "Received NEC code: 0x%08lX", code);
      }

      vRingbufferReturnItem(s_rx_ring_buf, items);
    }
  }

  rmt_driver_uninstall(RX_CHANNEL);
  rmt_driver_uninstall(TX_CHANNEL);
}

void app_main(void) { ir_remote_example(); }
