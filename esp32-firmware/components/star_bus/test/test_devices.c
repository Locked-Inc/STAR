/* esp32-firmware/components/star_bus/test/test_devices.c */

#include <driver/i2c.h>
#include <esp_log.h>
#include <string.h>

#include "star_bus_config.h"
#include "star_bus_devices.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_test.h"

static const char* TAG = "test_devices";

/* Test fixtures */
static star_bus_manager_t test_manager;

/* Setup/teardown helpers */
static void setup_test_manager(void)
{
  esp_err_t ret = star_bus_manager_init(&test_manager, "test_devices");
  STAR_ASSERT_EQUAL(ESP_OK, ret);
}

static void teardown_test_manager(void)
{
  esp_err_t ret = star_bus_manager_deinit(&test_manager);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
}

/* --- BMP280 Tests --- */

STAR_TEST_CASE(devices_bmp280, init_null_manager)
{
  bmp280_config_t config;
  esp_err_t       ret = star_bus_bmp280_init(NULL, "bmp280", &config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_bmp280, init_null_bus_name)
{
  setup_test_manager();
  bmp280_config_t config;
  esp_err_t       ret = star_bus_bmp280_init(&test_manager, NULL, &config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(devices_bmp280, init_null_config)
{
  setup_test_manager();
  esp_err_t ret = star_bus_bmp280_init(&test_manager, "bmp280", NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(devices_bmp280, configure_null_manager)
{
  bmp280_config_t config = {0};
  esp_err_t       ret    = star_bus_bmp280_configure(NULL, "bmp280", &config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_bmp280, read_temperature_null_manager)
{
  bmp280_config_t config;
  float           temp;
  esp_err_t       ret = star_bus_bmp280_read_temperature(NULL, "bmp280", &config, &temp);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_bmp280, read_pressure_null_manager)
{
  bmp280_config_t config;
  float           pressure;
  esp_err_t       ret = star_bus_bmp280_read_pressure(NULL, "bmp280", &config, &pressure);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- MPU6050 Tests --- */

STAR_TEST_CASE(devices_mpu6050, init_null_manager)
{
  esp_err_t ret = star_bus_mpu6050_init(NULL, "mpu6050");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_mpu6050, init_null_bus_name)
{
  setup_test_manager();
  esp_err_t ret = star_bus_mpu6050_init(&test_manager, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(devices_mpu6050, configure_null_manager)
{
  mpu6050_config_t config = {0};
  esp_err_t        ret    = star_bus_mpu6050_configure(NULL, "mpu6050", &config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_mpu6050, read_all_null_manager)
{
  mpu6050_data_t data;
  esp_err_t      ret = star_bus_mpu6050_read_all(NULL, "mpu6050", &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- SSD1306 Tests --- */

STAR_TEST_CASE(devices_ssd1306, init_null_manager)
{
  esp_err_t ret = star_bus_ssd1306_init(NULL, "ssd1306", 128, 64);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ssd1306, init_null_bus_name)
{
  setup_test_manager();
  esp_err_t ret = star_bus_ssd1306_init(&test_manager, NULL, 128, 64);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(devices_ssd1306, clear_null_manager)
{
  esp_err_t ret = star_bus_ssd1306_clear(NULL, "ssd1306");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ssd1306, update_null_buffer)
{
  setup_test_manager();
  esp_err_t ret = star_bus_ssd1306_update(&test_manager, "ssd1306", NULL, 1024);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

/* --- ADS1115 Tests --- */

STAR_TEST_CASE(devices_ads1115, init_null_manager)
{
  esp_err_t ret = star_bus_ads1115_init(NULL, "ads1115");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ads1115, configure_null_manager)
{
  ads1115_config_t config = {0};
  esp_err_t        ret    = star_bus_ads1115_configure(NULL, "ads1115", &config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ads1115, read_null_manager)
{
  int16_t   value;
  esp_err_t ret = star_bus_ads1115_read(NULL, "ads1115", &value);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- PCF8574 Tests --- */

STAR_TEST_CASE(devices_pcf8574, write_null_manager)
{
  esp_err_t ret = star_bus_pcf8574_write(NULL, "pcf8574", 0xFF);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_pcf8574, read_null_manager)
{
  uint8_t   value;
  esp_err_t ret = star_bus_pcf8574_read(NULL, "pcf8574", &value);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- AT24C256 Tests --- */

STAR_TEST_CASE(devices_at24c256, write_null_manager)
{
  uint8_t   data[] = {0x01, 0x02, 0x03};
  esp_err_t ret    = star_bus_at24c256_write(NULL, "at24c256", 0, data, sizeof(data));
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_at24c256, write_null_data)
{
  setup_test_manager();
  esp_err_t ret = star_bus_at24c256_write(&test_manager, "at24c256", 0, NULL, 10);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(devices_at24c256, read_null_manager)
{
  uint8_t   data[10];
  esp_err_t ret = star_bus_at24c256_read(NULL, "at24c256", 0, data, sizeof(data));
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- DS3231 Tests --- */

STAR_TEST_CASE(devices_ds3231, init_null_manager)
{
  esp_err_t ret = star_bus_ds3231_init(NULL, "ds3231");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ds3231, set_time_null_manager)
{
  ds3231_time_t time = {0};
  esp_err_t     ret  = star_bus_ds3231_set_time(NULL, "ds3231", &time);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ds3231, get_time_null_manager)
{
  ds3231_time_t time;
  esp_err_t     ret = star_bus_ds3231_get_time(NULL, "ds3231", &time);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(devices_ds3231, get_temperature_null_manager)
{
  float     temp;
  esp_err_t ret = star_bus_ds3231_get_temperature(NULL, "ds3231", &temp);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* Test registration */
STAR_TEST_LIST_BEGIN()
STAR_TEST_REF(devices_bmp280, init_null_manager)
STAR_TEST_REF(devices_bmp280, init_null_bus_name)
STAR_TEST_REF(devices_bmp280, init_null_config)
STAR_TEST_REF(devices_bmp280, configure_null_manager)
STAR_TEST_REF(devices_bmp280, read_temperature_null_manager)
STAR_TEST_REF(devices_bmp280, read_pressure_null_manager)
STAR_TEST_REF(devices_mpu6050, init_null_manager)
STAR_TEST_REF(devices_mpu6050, init_null_bus_name)
STAR_TEST_REF(devices_mpu6050, configure_null_manager)
STAR_TEST_REF(devices_mpu6050, read_all_null_manager)
STAR_TEST_REF(devices_ssd1306, init_null_manager)
STAR_TEST_REF(devices_ssd1306, init_null_bus_name)
STAR_TEST_REF(devices_ssd1306, clear_null_manager)
STAR_TEST_REF(devices_ssd1306, update_null_buffer)
STAR_TEST_REF(devices_ads1115, init_null_manager)
STAR_TEST_REF(devices_ads1115, configure_null_manager)
STAR_TEST_REF(devices_ads1115, read_null_manager)
STAR_TEST_REF(devices_pcf8574, write_null_manager)
STAR_TEST_REF(devices_pcf8574, read_null_manager)
STAR_TEST_REF(devices_at24c256, write_null_manager)
STAR_TEST_REF(devices_at24c256, write_null_data)
STAR_TEST_REF(devices_at24c256, read_null_manager)
STAR_TEST_REF(devices_ds3231, init_null_manager)
STAR_TEST_REF(devices_ds3231, set_time_null_manager)
STAR_TEST_REF(devices_ds3231, get_time_null_manager)
STAR_TEST_REF(devices_ds3231, get_temperature_null_manager)
STAR_TEST_LIST_END()
