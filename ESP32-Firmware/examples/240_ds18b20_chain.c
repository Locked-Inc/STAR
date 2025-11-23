/**
 * @file 240_ds18b20_chain.c
 * @brief DS18B20 1-Wire temperature sensor chain
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "DS18B20";

#define ONEWIRE_PIN (GPIO_NUM_4)
#define MAX_SENSORS (8)

static uint8_t s_sensor_addrs[MAX_SENSORS][8];
static int s_sensor_count = 0;

static bool priv_onewire_reset(void)
{
  gpio_set_direction(ONEWIRE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(ONEWIRE_PIN, 0);
  esp_rom_delay_us(480);
  gpio_set_direction(ONEWIRE_PIN, GPIO_MODE_INPUT);
  esp_rom_delay_us(70);
  bool presence = !gpio_get_level(ONEWIRE_PIN);
  esp_rom_delay_us(410);
  return presence;
}

static void priv_onewire_write_bit(bool bit)
{
  gpio_set_direction(ONEWIRE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(ONEWIRE_PIN, 0);
  esp_rom_delay_us(bit ? 6 : 60);
  gpio_set_level(ONEWIRE_PIN, 1);
  esp_rom_delay_us(bit ? 64 : 10);
}

static bool priv_onewire_read_bit(void)
{
  gpio_set_direction(ONEWIRE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(ONEWIRE_PIN, 0);
  esp_rom_delay_us(6);
  gpio_set_direction(ONEWIRE_PIN, GPIO_MODE_INPUT);
  esp_rom_delay_us(9);
  bool bit = gpio_get_level(ONEWIRE_PIN);
  esp_rom_delay_us(55);
  return bit;
}

static void priv_onewire_write_byte(uint8_t byte)
{
  for (int i = 0; i < 8; i++) {
    priv_onewire_write_bit(byte & (1 << i));
  }
}

static uint8_t priv_onewire_read_byte(void)
{
  uint8_t byte = 0;
  for (int i = 0; i < 8; i++) {
    if (priv_onewire_read_bit()) byte |= (1 << i);
  }
  return byte;
}

static float priv_ds18b20_read_temp(uint8_t *addr)
{
  priv_onewire_reset();
  priv_onewire_write_byte(0x55);  /* Match ROM */
  for (int i = 0; i < 8; i++) priv_onewire_write_byte(addr[i]);
  priv_onewire_write_byte(0x44);  /* Convert */

  vTaskDelay(pdMS_TO_TICKS(750));

  priv_onewire_reset();
  priv_onewire_write_byte(0x55);
  for (int i = 0; i < 8; i++) priv_onewire_write_byte(addr[i]);
  priv_onewire_write_byte(0xBE);  /* Read scratchpad */

  uint8_t lsb = priv_onewire_read_byte();
  uint8_t msb = priv_onewire_read_byte();

  int16_t raw = (msb << 8) | lsb;
  return raw / 16.0f;
}

void ds18b20_chain_example(void)
{
  ESP_LOGI(s_tag, "DS18B20 chain scanner started");

  /* Search for sensors */
  if (!priv_onewire_reset()) {
    ESP_LOGI(s_tag, "No 1-Wire devices found");
    return;
  }

  /* For demo, assume sensors found */
  s_sensor_count = 2;  /* Simulated */

  for (int i = 0; i < 100; i++) {
    ESP_LOGI(s_tag, "--- Reading %d sensors ---", s_sensor_count);

    for (int s = 0; s < s_sensor_count; s++) {
      float temp = priv_ds18b20_read_temp(s_sensor_addrs[s]);
      ESP_LOGI(s_tag, "Sensor %d: %.2f C", s, temp);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void app_main(void) { ds18b20_chain_example(); }
