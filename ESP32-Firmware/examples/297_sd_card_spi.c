/**
 * @file 297_sd_card_spi.c
 * @brief SD card via SPI interface
 */

#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <sdmmc_cmd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *s_tag = "SD_CARD";

#define MOUNT_POINT "/sdcard"
#define SD_CS   (GPIO_NUM_5)
#define SD_MOSI (GPIO_NUM_23)
#define SD_MISO (GPIO_NUM_19)
#define SD_CLK  (GPIO_NUM_18)

void sd_card_spi_example(void)
{
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SD_MOSI,
    .miso_io_num = SD_MISO,
    .sclk_io_num = SD_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
  };

  spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = SD_CS;
  slot_config.host_id = host.slot;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
  };

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "Failed to mount SD card");
    return;
  }

  ESP_LOGI(s_tag, "SD card mounted");
  sdmmc_card_print_info(stdout, card);

  /* Write test file */
  FILE* f = fopen(MOUNT_POINT "/test.txt", "w");
  if (f) {
    fprintf(f, "ESP32 STAR Firmware\nSD Card Test\n");
    for (int i = 0; i < 10; i++) {
      fprintf(f, "Line %d: %d\n", i, i * i);
    }
    fclose(f);
    ESP_LOGI(s_tag, "File written");
  }

  /* Read back */
  f = fopen(MOUNT_POINT "/test.txt", "r");
  if (f) {
    char line[64];
    while (fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\n")] = 0;
      ESP_LOGI(s_tag, "Read: %s", line);
    }
    fclose(f);
  }

  /* Get file info */
  struct stat st;
  if (stat(MOUNT_POINT "/test.txt", &st) == 0) {
    ESP_LOGI(s_tag, "File size: %ld bytes", st.st_size);
  }

  esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
  spi_bus_free(host.slot);
  ESP_LOGI(s_tag, "SD card unmounted");
}

void app_main(void) { sd_card_spi_example(); }
