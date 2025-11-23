/**
 * @file 278_rfid_rc522.c
 * @brief MFRC522 RFID reader
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RFID";

#define MFRC522_CS (GPIO_NUM_5)

static star_bus_manager_t *g_manager;

static void priv_rc522_write(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {(reg << 1) & 0x7E, val};
  star_bus_spi_transmit(g_manager, "rc522", buf, 2, 0);
}

static uint8_t priv_rc522_read(uint8_t reg) {
  uint8_t tx = ((reg << 1) & 0x7E) | 0x80;
  uint8_t rx;
  star_bus_spi_transmit(g_manager, "rc522", &tx, 1, 0);
  star_bus_spi_receive(g_manager, "rc522", &rx, 1, 0);
  return rx;
}

static void priv_rc522_init(void) {
  priv_rc522_write(0x01, 0x0F);  /* Soft reset */
  vTaskDelay(pdMS_TO_TICKS(50));
  priv_rc522_write(0x2A, 0x8D);  /* Timer */
  priv_rc522_write(0x2B, 0x3E);
  priv_rc522_write(0x2D, 30);
  priv_rc522_write(0x2C, 0);
  priv_rc522_write(0x15, 0x40);
  priv_rc522_write(0x11, 0x3D);
  priv_rc522_write(0x14, priv_rc522_read(0x14) | 0x03);  /* Antenna on */
}

void rfid_rc522_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "RFID_Manager", NULL, NULL);
  g_manager = &manager;

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 5000000,
    .mode = 0,
    .spics_io_num = MFRC522_CS,
    .queue_size = 1,
  };
  star_bus_config_t *rfid_bus = star_bus_config_create_spi_device(
    "rc522", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg);
  star_bus_manager_add_bus(&manager, rfid_bus);
  star_bus_config_init(rfid_bus, &manager);

  priv_rc522_init();
  ESP_LOGI(s_tag, "MFRC522 RFID reader started");

  for (int i = 0; i < 200; i++) {
    /* Request card */
    priv_rc522_write(0x0D, 0x07);
    priv_rc522_write(0x01, 0x0C);
    priv_rc522_write(0x09, 0x26);

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t status = priv_rc522_read(0x06);
    if (status & 0x30) {
      ESP_LOGI(s_tag, "Card detected! Reading UID...");

      /* Anti-collision */
      priv_rc522_write(0x0D, 0x00);
      priv_rc522_write(0x09, 0x93);
      priv_rc522_write(0x09, 0x20);
      priv_rc522_write(0x01, 0x0C);
      vTaskDelay(pdMS_TO_TICKS(10));

      uint8_t uid[5];
      for (int j = 0; j < 5; j++) {
        uid[j] = priv_rc522_read(0x09);
      }
      ESP_LOGI(s_tag, "UID: %02X:%02X:%02X:%02X", uid[0], uid[1], uid[2], uid[3]);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { rfid_rc522_example(); }
