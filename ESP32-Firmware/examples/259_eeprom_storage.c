/**
 * @file 259_eeprom_storage.c
 * @brief AT24C32 I2C EEPROM storage
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "EEPROM";

#define AT24C32_ADDR (0x50)

static star_bus_manager_t *g_manager;

static void priv_eeprom_write_byte(uint16_t addr, uint8_t data) {
  uint8_t buf[3] = {addr >> 8, addr & 0xFF, data};
  size_t bytes_written;
  star_bus_i2c_write(g_manager, "eeprom", buf, 3, 0x00, &bytes_written);
  vTaskDelay(pdMS_TO_TICKS(5));  /* Write cycle */
}

static uint8_t priv_eeprom_read_byte(uint16_t addr) {
  uint8_t buf[2] = {addr >> 8, addr & 0xFF};
  uint8_t data;
  size_t bytes_written, bytes_read;
  star_bus_i2c_write(g_manager, "eeprom", buf, 2, 0x00, &bytes_written);
  star_bus_i2c_read(g_manager, "eeprom", &data, 1, 0x00, &bytes_read);
  return data;
}

static void priv_eeprom_write_string(uint16_t addr, const char *str) {
  while (*str) priv_eeprom_write_byte(addr++, *str++);
  priv_eeprom_write_byte(addr, 0);
}

static void priv_eeprom_read_string(uint16_t addr, char *str, int max) {
  for (int i = 0; i < max - 1; i++) {
    str[i] = priv_eeprom_read_byte(addr + i);
    if (str[i] == 0) break;
  }
  str[max - 1] = 0;
}

void eeprom_storage_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "EEPROM_Manager", NULL, NULL);
  g_manager = &manager;

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "eeprom", I2C_NUM_0, AT24C32_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "AT24C32 EEPROM (4KB) initialized");

  /* Write data */
  const char *test_str = "Hello EEPROM!";
  ESP_LOGI(s_tag, "Writing: %s", test_str);
  priv_eeprom_write_string(0x0000, test_str);

  /* Write boot count */
  uint8_t boot_count = priv_eeprom_read_byte(0x0100);
  boot_count++;
  priv_eeprom_write_byte(0x0100, boot_count);
  ESP_LOGI(s_tag, "Boot count: %d", boot_count);

  /* Read back */
  char read_str[32];
  priv_eeprom_read_string(0x0000, read_str, sizeof(read_str));
  ESP_LOGI(s_tag, "Read back: %s", read_str);

  star_bus_manager_deinit(&manager);
}

void app_main(void) { eeprom_storage_example(); }
