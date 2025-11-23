/**
 * @file 229_rfid_reader.c
 * @brief RC522 RFID card reader
 */

#include <esp_log.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "RFID";

#define MFRC522_MOSI  (GPIO_NUM_23)
#define MFRC522_MISO  (GPIO_NUM_19)
#define MFRC522_SCK   (GPIO_NUM_18)
#define MFRC522_CS    (GPIO_NUM_5)
#define MFRC522_RST   (GPIO_NUM_22)

/* MFRC522 registers */
#define CommandReg (0x01)
#define ComIEnReg (0x02)
#define ComIrqReg (0x04)
#define FIFODataReg (0x09)
#define FIFOLevelReg (0x0A)
#define BitFramingReg (0x0D)
#define ModeReg (0x11)
#define TxControlReg (0x14)
#define TxASKReg (0x15)
#define TModeReg (0x2A)
#define TPrescalerReg (0x2B)
#define VersionReg (0x37)

static spi_device_handle_t s_spi;

static void priv_mfrc522_write(uint8_t reg, uint8_t value)
{
  uint8_t data[2] = {(reg << 1) & 0x7E, value};
  spi_transaction_t t = { .length = 16, .tx_buffer = data };
  spi_device_transmit(s_spi, &t);
}

static uint8_t priv_mfrc522_read(uint8_t reg)
{
  uint8_t tx[2] = {((reg << 1) & 0x7E) | 0x80, 0};
  uint8_t rx[2];
  spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
  spi_device_transmit(s_spi, &t);
  return rx[1];
}

static void priv_mfrc522_init(void)
{
  gpio_set_direction(MFRC522_RST, GPIO_MODE_OUTPUT);
  gpio_set_level(MFRC522_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(50));

  priv_mfrc522_write(CommandReg, 0x0F);  /* Soft reset */
  vTaskDelay(pdMS_TO_TICKS(50));

  priv_mfrc522_write(TModeReg, 0x8D);
  priv_mfrc522_write(TPrescalerReg, 0x3E);
  priv_mfrc522_write(TxASKReg, 0x40);
  priv_mfrc522_write(ModeReg, 0x3D);

  uint8_t val = priv_mfrc522_read(TxControlReg);
  priv_mfrc522_write(TxControlReg, val | 0x03);  /* Enable antenna */
}

static bool priv_mfrc522_is_card_present(void)
{
  priv_mfrc522_write(BitFramingReg, 0x07);

  uint8_t req_cmd[] = {0x26};
  priv_mfrc522_write(FIFOLevelReg, 0x80);  /* Clear FIFO */
  priv_mfrc522_write(FIFODataReg, req_cmd[0]);
  priv_mfrc522_write(CommandReg, 0x0C);  /* Transceive */

  vTaskDelay(pdMS_TO_TICKS(25));

  uint8_t irq = priv_mfrc522_read(ComIrqReg);
  return (irq & 0x30) != 0;
}

static bool priv_mfrc522_read_card_uid(uint8_t *uid)
{
  uint8_t anticoll[] = {0x93, 0x20};

  priv_mfrc522_write(BitFramingReg, 0x00);
  priv_mfrc522_write(FIFOLevelReg, 0x80);
  priv_mfrc522_write(FIFODataReg, anticoll[0]);
  priv_mfrc522_write(FIFODataReg, anticoll[1]);
  priv_mfrc522_write(CommandReg, 0x0C);

  vTaskDelay(pdMS_TO_TICKS(25));

  uint8_t fifo_level = priv_mfrc522_read(FIFOLevelReg);
  if (fifo_level >= 5) {
    for (int i = 0; i < 5; i++) {
      uid[i] = priv_mfrc522_read(FIFODataReg);
    }
    return true;
  }

  return false;
}

void rfid_reader_example(void)
{
  spi_bus_config_t buscfg = {
    .mosi_io_num = MFRC522_MOSI,
    .miso_io_num = MFRC522_MISO,
    .sclk_io_num = MFRC522_SCK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 5 * 1000 * 1000,
    .mode = 0,
    .spics_io_num = MFRC522_CS,
    .queue_size = 3,
  };

  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

  priv_mfrc522_init();

  uint8_t version = priv_mfrc522_read(VersionReg);
  ESP_LOGI(s_tag, "MFRC522 version: 0x%02X", version);

  uint8_t last_uid[5] = {0};

  for (int i = 0; i < 500; i++) {
    if (priv_mfrc522_is_card_present()) {
      uint8_t uid[5];
      if (priv_mfrc522_read_card_uid(uid)) {
        if (memcmp(uid, last_uid, 5) != 0) {
          ESP_LOGI(s_tag, "Card detected! UID: %02X:%02X:%02X:%02X:%02X",
                   uid[0], uid[1], uid[2], uid[3], uid[4]);
          memcpy(last_uid, uid, 5);
        }
      }
    } else {
      memset(last_uid, 0, 5);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  spi_bus_remove_device(s_spi);
}

void app_main(void) { rfid_reader_example(); }
