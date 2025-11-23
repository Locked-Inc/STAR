/* esp32-firmware/components/star_bus/star_bus_devices.c */

#include "star_bus_devices.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "star_bus_devices";

/* --- Helper Functions --- */

/**
 * @brief Convert BCD to decimal
 */
static uint8_t priv_bcd_to_dec(uint8_t bcd)
{
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/**
 * @brief Convert decimal to BCD
 */
static uint8_t priv_dec_to_bcd(uint8_t dec)
{
  return ((dec / 10) << 4) | (dec % 10);
}

/* --- BMP280 Implementation --- */

#define BMP280_REG_CHIP_ID (0xD0)
#define BMP280_REG_RESET (0xE0)
#define BMP280_REG_STATUS (0xF3)
#define BMP280_REG_CTRL_MEAS (0xF4)
#define BMP280_REG_CONFIG (0xF5)
#define BMP280_REG_PRESS_MSB (0xF7)
#define BMP280_REG_TEMP_MSB (0xFA)
#define BMP280_REG_CALIB_START (0x88)

#define BMP280_CHIP_ID (0x58)

esp_err_t star_bus_bmp280_init(const star_bus_manager_t* manager,
                               const char*               bus_name,
                               bmp280_config_t*          config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read chip ID
  uint8_t   chip_id;
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, &chip_id, 1, BMP280_REG_CHIP_ID, &bytes_read);
  if (ret != ESP_OK || chip_id != BMP280_CHIP_ID) {
    ESP_LOGE(s_TAG, "BMP280 init failed: chip_id=0x%02X (expected 0x%02X)", chip_id, BMP280_CHIP_ID);
    return ESP_ERR_NOT_FOUND;
  }

  // Read calibration data (24 bytes starting at 0x88)
  uint8_t calib[24];
  ret = star_bus_i2c_read(manager, bus_name, calib, 24, BMP280_REG_CALIB_START, &bytes_read);
  if (ret != ESP_OK || bytes_read != 24) {
    ESP_LOGE(s_TAG, "BMP280 calibration read failed");
    return ret;
  }

  // Parse calibration coefficients
  config->dig_t1 = (uint16_t)(calib[1] << 8 | calib[0]);
  config->dig_t2 = (int16_t)(calib[3] << 8 | calib[2]);
  config->dig_t3 = (int16_t)(calib[5] << 8 | calib[4]);
  config->dig_p1 = (uint16_t)(calib[7] << 8 | calib[6]);
  config->dig_p2 = (int16_t)(calib[9] << 8 | calib[8]);
  config->dig_p3 = (int16_t)(calib[11] << 8 | calib[10]);
  config->dig_p4 = (int16_t)(calib[13] << 8 | calib[12]);
  config->dig_p5 = (int16_t)(calib[15] << 8 | calib[14]);
  config->dig_p6 = (int16_t)(calib[17] << 8 | calib[16]);
  config->dig_p7 = (int16_t)(calib[19] << 8 | calib[18]);
  config->dig_p8 = (int16_t)(calib[21] << 8 | calib[20]);
  config->dig_p9 = (int16_t)(calib[23] << 8 | calib[22]);

  // Set default config
  config->oversampling_temp = 2; // 2x oversampling
  config->oversampling_pres = 5; // 16x oversampling
  config->mode              = 3; // Normal mode
  config->standby_time      = 5; // 1000ms
  config->filter            = 0; // Filter off
  config->t_fine            = 0;

  ESP_LOGI(s_TAG, "BMP280 initialized on bus '%s'", bus_name);
  return ESP_OK;
}

esp_err_t star_bus_bmp280_configure(const star_bus_manager_t* manager,
                                    const char*               bus_name,
                                    const bmp280_config_t*    config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Write ctrl_meas register
  uint8_t ctrl_meas =
    (config->oversampling_temp << 5) | (config->oversampling_pres << 2) | config->mode;
  size_t    bytes_written;
  esp_err_t ret =
    star_bus_i2c_write(manager, bus_name, &ctrl_meas, 1, BMP280_REG_CTRL_MEAS, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "BMP280 configure ctrl_meas failed");
    return ret;
  }

  // Write config register
  uint8_t config_reg = (config->standby_time << 5) | (config->filter << 2);
  ret = star_bus_i2c_write(manager, bus_name, &config_reg, 1, BMP280_REG_CONFIG, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "BMP280 configure config register failed");
    return ret;
  }

  ESP_LOGI(s_TAG,
           "BMP280 configured: temp_os=%d pres_os=%d mode=%d",
           config->oversampling_temp,
           config->oversampling_pres,
           config->mode);
  return ESP_OK;
}

esp_err_t star_bus_bmp280_read_temperature(const star_bus_manager_t* manager,
                                           const char*               bus_name,
                                           bmp280_config_t*          config,
                                           float*                    temp_celsius)
{
  if (manager == NULL || bus_name == NULL || config == NULL || temp_celsius == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read raw temperature (3 bytes)
  uint8_t   temp_raw[3];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, temp_raw, 3, BMP280_REG_TEMP_MSB, &bytes_read);
  if (ret != ESP_OK || bytes_read != 3) {
    ESP_LOGE(s_TAG, "BMP280 temperature read failed");
    return ret;
  }

  int32_t adc_T = (int32_t)((temp_raw[0] << 12) | (temp_raw[1] << 4) | (temp_raw[2] >> 4));

  // Compensate temperature (from datasheet)
  int32_t var1 =
    ((((adc_T >> 3) - ((int32_t)config->dig_t1 << 1))) * ((int32_t)config->dig_t2)) >> 11;
  int32_t var2 =
    (((((adc_T >> 4) - ((int32_t)config->dig_t1)) * ((adc_T >> 4) - ((int32_t)config->dig_t1))) >>
      12) *
     ((int32_t)config->dig_t3)) >>
    14;
  config->t_fine = var1 + var2;
  int32_t T      = (config->t_fine * 5 + 128) >> 8;
  *temp_celsius  = (float)T / 100.0f;

  return ESP_OK;
}

esp_err_t star_bus_bmp280_read_pressure(const star_bus_manager_t* manager,
                                        const char*               bus_name,
                                        bmp280_config_t*          config,
                                        float*                    pressure_pa)
{
  if (manager == NULL || bus_name == NULL || config == NULL || pressure_pa == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read raw pressure (3 bytes)
  uint8_t   press_raw[3];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, press_raw, 3, BMP280_REG_PRESS_MSB, &bytes_read);
  if (ret != ESP_OK || bytes_read != 3) {
    ESP_LOGE(s_TAG, "BMP280 pressure read failed");
    return ret;
  }

  int32_t adc_P = (int32_t)((press_raw[0] << 12) | (press_raw[1] << 4) | (press_raw[2] >> 4));

  // Compensate pressure (from datasheet)
  int64_t var1 = ((int64_t)config->t_fine) - 128000;
  int64_t var2 = var1 * var1 * (int64_t)config->dig_p6;
  var2         = var2 + ((var1 * (int64_t)config->dig_p5) << 17);
  var2         = var2 + (((int64_t)config->dig_p4) << 35);
  var1 = ((var1 * var1 * (int64_t)config->dig_p3) >> 8) + ((var1 * (int64_t)config->dig_p2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)config->dig_p1) >> 33;

  if (var1 == 0) {
    return ESP_ERR_INVALID_STATE; // Avoid division by zero
  }

  int64_t p = 1048576 - adc_P;
  p         = (((p << 31) - var2) * 3125) / var1;
  var1      = (((int64_t)config->dig_p9) * (p >> 13) * (p >> 13)) >> 25;
  var2      = (((int64_t)config->dig_p8) * p) >> 19;
  p         = ((p + var1 + var2) >> 8) + (((int64_t)config->dig_p7) << 4);

  *pressure_pa = (float)p / 256.0f;

  return ESP_OK;
}

/* --- MPU6050 Implementation --- */

#define MPU6050_REG_PWR_MGMT_1 (0x6B)
#define MPU6050_REG_GYRO_CONFIG (0x1B)
#define MPU6050_REG_ACCEL_CONFIG (0x1C)
#define MPU6050_REG_ACCEL_XOUT_H (0x3B)
#define MPU6050_REG_WHO_AM_I (0x75)
#define MPU6050_REG_SMPLRT_DIV (0x19)
#define MPU6050_REG_CONFIG (0x1A)

#define MPU6050_WHO_AM_I (0x68)

esp_err_t star_bus_mpu6050_init(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read WHO_AM_I
  uint8_t   who_am_i;
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, &who_am_i, 1, MPU6050_REG_WHO_AM_I, &bytes_read);
  if (ret != ESP_OK || who_am_i != MPU6050_WHO_AM_I) {
    ESP_LOGE(s_TAG,
             "MPU6050 init failed: who_am_i=0x%02X (expected 0x%02X)",
             who_am_i,
             MPU6050_WHO_AM_I);
    return ESP_ERR_NOT_FOUND;
  }

  // Wake up device (clear sleep bit)
  uint8_t pwr_mgmt = 0x00;
  size_t  bytes_written;
  ret = star_bus_i2c_write(manager, bus_name, &pwr_mgmt, 1, MPU6050_REG_PWR_MGMT_1, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "MPU6050 wake up failed");
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for device to wake up

  ESP_LOGI(s_TAG, "MPU6050 initialized on bus '%s'", bus_name);
  return ESP_OK;
}

esp_err_t star_bus_mpu6050_configure(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     const mpu6050_config_t*   config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  size_t    bytes_written;
  esp_err_t ret;

  // Set gyro range
  uint8_t gyro_config = config->gyro_range << 3;
  ret =
    star_bus_i2c_write(manager, bus_name, &gyro_config, 1, MPU6050_REG_GYRO_CONFIG, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "MPU6050 gyro config failed");
    return ret;
  }

  // Set accel range
  uint8_t accel_config = config->accel_range << 3;
  ret                  = star_bus_i2c_write(manager,
                           bus_name,
                           &accel_config,
                           1,
                           MPU6050_REG_ACCEL_CONFIG,
                           &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "MPU6050 accel config failed");
    return ret;
  }

  // Set DLPF
  ret = star_bus_i2c_write(manager,
                           bus_name,
                           &config->dlpf_mode,
                           1,
                           MPU6050_REG_CONFIG,
                           &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "MPU6050 DLPF config failed");
    return ret;
  }

  // Set sample rate divider
  ret = star_bus_i2c_write(manager,
                           bus_name,
                           &config->sample_rate_div,
                           1,
                           MPU6050_REG_SMPLRT_DIV,
                           &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "MPU6050 sample rate config failed");
    return ret;
  }

  ESP_LOGI(s_TAG,
           "MPU6050 configured: accel_range=%d gyro_range=%d dlpf=%d",
           config->accel_range,
           config->gyro_range,
           config->dlpf_mode);
  return ESP_OK;
}

esp_err_t star_bus_mpu6050_read_all(const star_bus_manager_t* manager,
                                    const char*               bus_name,
                                    mpu6050_data_t*           data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read all 14 bytes (accel + temp + gyro)
  uint8_t   raw[14];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, raw, 14, MPU6050_REG_ACCEL_XOUT_H, &bytes_read);
  if (ret != ESP_OK || bytes_read != 14) {
    ESP_LOGE(s_TAG, "MPU6050 read all failed");
    return ret;
  }

  // Parse data
  data->accel_x = (int16_t)(raw[0] << 8 | raw[1]);
  data->accel_y = (int16_t)(raw[2] << 8 | raw[3]);
  data->accel_z = (int16_t)(raw[4] << 8 | raw[5]);
  data->temp    = (int16_t)(raw[6] << 8 | raw[7]);
  data->gyro_x  = (int16_t)(raw[8] << 8 | raw[9]);
  data->gyro_y  = (int16_t)(raw[10] << 8 | raw[11]);
  data->gyro_z  = (int16_t)(raw[12] << 8 | raw[13]);

  return ESP_OK;
}

/* --- SSD1306 Implementation --- */

#define SSD1306_CMD_DISPLAY_OFF (0xAE)
#define SSD1306_CMD_DISPLAY_ON (0xAF)
#define SSD1306_CMD_SET_DISPLAY_CLK (0xD5)
#define SSD1306_CMD_SET_MULTIPLEX (0xA8)
#define SSD1306_CMD_SET_DISPLAY_OFFSET (0xD3)
#define SSD1306_CMD_SET_START_LINE (0x40)
#define SSD1306_CMD_CHARGE_PUMP (0x8D)
#define SSD1306_CMD_MEMORY_MODE (0x20)
#define SSD1306_CMD_SEG_REMAP (0xA1)
#define SSD1306_CMD_COM_SCAN_DEC (0xC8)
#define SSD1306_CMD_SET_COM_PINS (0xDA)
#define SSD1306_CMD_SET_CONTRAST (0x81)
#define SSD1306_CMD_SET_PRECHARGE (0xD9)
#define SSD1306_CMD_SET_VCOM_DETECT (0xDB)
#define SSD1306_CMD_DISPLAY_ALL_ON (0xA4)
#define SSD1306_CMD_NORMAL_DISPLAY (0xA6)

esp_err_t star_bus_ssd1306_init(const star_bus_manager_t* manager,
                                const char*               bus_name,
                                uint8_t                   width,
                                uint8_t                   height)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Initialization sequence
  uint8_t init_cmds[] = {
    SSD1306_CMD_DISPLAY_OFF,
    SSD1306_CMD_SET_DISPLAY_CLK,
    0x80,
    SSD1306_CMD_SET_MULTIPLEX,
    (uint8_t)(height - 1),
    SSD1306_CMD_SET_DISPLAY_OFFSET,
    0x00,
    SSD1306_CMD_SET_START_LINE | 0x00,
    SSD1306_CMD_CHARGE_PUMP,
    0x14, // Enable charge pump
    SSD1306_CMD_MEMORY_MODE,
    0x00, // Horizontal addressing mode
    SSD1306_CMD_SEG_REMAP | 0x01,
    SSD1306_CMD_COM_SCAN_DEC,
    SSD1306_CMD_SET_COM_PINS,
    (uint8_t)(height == 64 ? 0x12 : 0x02),
    SSD1306_CMD_SET_CONTRAST,
    0x8F,
    SSD1306_CMD_SET_PRECHARGE,
    0xF1,
    SSD1306_CMD_SET_VCOM_DETECT,
    0x40,
    SSD1306_CMD_DISPLAY_ALL_ON,
    SSD1306_CMD_NORMAL_DISPLAY,
    SSD1306_CMD_DISPLAY_ON,
  };

  // Send initialization commands (control byte 0x00 for command)
  for (size_t i = 0; i < sizeof(init_cmds); i++) {
    esp_err_t ret = star_bus_i2c_write_command(manager, bus_name, init_cmds[i]);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "SSD1306 init command %zu failed", i);
      return ret;
    }
  }

  ESP_LOGI(s_TAG, "SSD1306 initialized on bus '%s' (%dx%d)", bus_name, width, height);
  return ESP_OK;
}

esp_err_t star_bus_ssd1306_clear(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Clear display by writing zeros
  uint8_t zeros[128] = {0};
  for (uint32_t page = 0; page < 8; page++) {
    // Set page address
    uint8_t   page_cmd = 0xB0 | page;
    esp_err_t ret      = star_bus_i2c_write_command(manager, bus_name, page_cmd);
    if (ret != ESP_OK) {
      return ret;
    }

    // Set column address to 0
    ret = star_bus_i2c_write_command(manager, bus_name, 0x00);
    if (ret != ESP_OK) {
      return ret;
    }
    ret = star_bus_i2c_write_command(manager, bus_name, 0x10);
    if (ret != ESP_OK) {
      return ret;
    }

    // Write zeros (data byte with control byte 0x40)
    size_t bytes_written;
    ret = star_bus_i2c_write(manager, bus_name, zeros, 128, 0x40, &bytes_written);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "SSD1306 clear page %d failed", page);
      return ret;
    }
  }

  return ESP_OK;
}

esp_err_t star_bus_ssd1306_update(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  const uint8_t*            buffer,
                                  size_t                    size)
{
  if (manager == NULL || bus_name == NULL || buffer == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Write framebuffer (simplified - assumes horizontal addressing mode)
  size_t    bytes_written;
  esp_err_t ret = star_bus_i2c_write(manager, bus_name, buffer, size, 0x40, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "SSD1306 update failed");
    return ret;
  }

  return ESP_OK;
}

/* --- ADS1115 Implementation --- */

#define ADS1115_REG_CONVERSION (0x00)
#define ADS1115_REG_CONFIG (0x01)

esp_err_t star_bus_ads1115_init(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read config register to verify device presence
  uint8_t   config[2];
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, bus_name, config, 2, ADS1115_REG_CONFIG, &bytes_read);
  if (ret != ESP_OK || bytes_read != 2) {
    ESP_LOGE(s_TAG, "ADS1115 init failed");
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(s_TAG, "ADS1115 initialized on bus '%s'", bus_name);
  return ESP_OK;
}

esp_err_t star_bus_ads1115_configure(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     const ads1115_config_t*   config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Build config register (start single conversion, specified settings)
  uint16_t config_val = 0x8000 |                   // Start single conversion
                        (config->mux << 12) |      // MUX
                        (config->gain << 9) |      // PGA
                        (config->mode << 8) |      // Mode
                        (config->data_rate << 5) | // Data rate
                        (0 << 4) |                 // COMP_MODE
                        (0 << 3) |                 // COMP_POL
                        (0 << 2) |                 // COMP_LAT
                        (3 << 0);                  // COMP_QUE (disable)

  uint8_t   data[2] = {(uint8_t)(config_val >> 8), (uint8_t)(config_val & 0xFF)};
  size_t    bytes_written;
  esp_err_t ret =
    star_bus_i2c_write(manager, bus_name, data, 2, ADS1115_REG_CONFIG, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "ADS1115 configure failed");
    return ret;
  }

  ESP_LOGI(s_TAG,
           "ADS1115 configured: mux=%d gain=%d mode=%d rate=%d",
           config->mux,
           config->gain,
           config->mode,
           config->data_rate);
  return ESP_OK;
}

esp_err_t
star_bus_ads1115_read(const star_bus_manager_t* manager, const char* bus_name, int16_t* value)
{
  if (manager == NULL || bus_name == NULL || value == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read conversion register
  uint8_t   data[2];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, data, 2, ADS1115_REG_CONVERSION, &bytes_read);
  if (ret != ESP_OK || bytes_read != 2) {
    ESP_LOGE(s_TAG, "ADS1115 read failed");
    return ret;
  }

  *value = (int16_t)(data[0] << 8 | data[1]);
  return ESP_OK;
}

/* --- PCF8574 Implementation --- */

esp_err_t
star_bus_pcf8574_write(const star_bus_manager_t* manager, const char* bus_name, uint8_t value)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // PCF8574 has no registers, just write byte directly
  size_t    bytes_written;
  esp_err_t ret = star_bus_i2c_write(manager, bus_name, &value, 1, 0, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "PCF8574 write failed");
    return ret;
  }

  return ESP_OK;
}

esp_err_t
star_bus_pcf8574_read(const star_bus_manager_t* manager, const char* bus_name, uint8_t* value)
{
  if (manager == NULL || bus_name == NULL || value == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // PCF8574 has no registers, just read byte directly
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read_raw(manager, bus_name, value, 1, &bytes_read);
  if (ret != ESP_OK || bytes_read != 1) {
    ESP_LOGE(s_TAG, "PCF8574 read failed");
    return ret;
  }

  return ESP_OK;
}

/* --- AT24C256 Implementation --- */

esp_err_t star_bus_at24c256_write(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  uint16_t                  address,
                                  const uint8_t*            data,
                                  size_t                    len)
{
  if (manager == NULL || bus_name == NULL || data == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // AT24C256 uses 2-byte address
  // Write in pages (max 64 bytes per page)
  const size_t page_size = 64;

  for (size_t i = 0; i < len; i += page_size) {
    size_t   chunk_len = (len - i) > page_size ? page_size : (len - i);
    uint16_t addr      = address + i;

    // Write address MSB as register, then address LSB + data
    uint8_t write_buf[65]; // 1 byte addr_lsb + 64 bytes data max
    write_buf[0] = (uint8_t)(addr & 0xFF);
    memcpy(&write_buf[1], &data[i], chunk_len);

    size_t    bytes_written;
    esp_err_t ret = star_bus_i2c_write(manager,
                                       bus_name,
                                       write_buf,
                                       chunk_len + 1,
                                       (uint8_t)(addr >> 8),
                                       &bytes_written);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "AT24C256 write failed at address 0x%04X", addr);
      return ret;
    }

    // Wait for write cycle to complete (max 5ms)
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  return ESP_OK;
}

esp_err_t star_bus_at24c256_read(const star_bus_manager_t* manager,
                                 const char*               bus_name,
                                 uint16_t                  address,
                                 uint8_t*                  data,
                                 size_t                    len)
{
  if (manager == NULL || bus_name == NULL || data == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // AT24C256 uses 2-byte address
  // First write address, then read data
  uint8_t addr_bytes[2] = {(uint8_t)(address >> 8), (uint8_t)(address & 0xFF)};

  size_t    bytes_written;
  esp_err_t ret =
    star_bus_i2c_write(manager, bus_name, &addr_bytes[1], 1, addr_bytes[0], &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "AT24C256 address write failed");
    return ret;
  }

  // Read data
  size_t bytes_read;
  ret = star_bus_i2c_read_raw(manager, bus_name, data, len, &bytes_read);
  if (ret != ESP_OK || bytes_read != len) {
    ESP_LOGE(s_TAG, "AT24C256 read failed");
    return ret;
  }

  return ESP_OK;
}

/* --- DS3231 Implementation --- */

#define DS3231_REG_SECONDS (0x00)
#define DS3231_REG_TEMP_MSB (0x11)

esp_err_t star_bus_ds3231_init(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read seconds register to verify device presence
  uint8_t   seconds;
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, &seconds, 1, DS3231_REG_SECONDS, &bytes_read);
  if (ret != ESP_OK || bytes_read != 1) {
    ESP_LOGE(s_TAG, "DS3231 init failed");
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(s_TAG, "DS3231 initialized on bus '%s'", bus_name);
  return ESP_OK;
}

esp_err_t star_bus_ds3231_set_time(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   const ds3231_time_t*      time)
{
  if (manager == NULL || bus_name == NULL || time == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Convert to BCD and write
  uint8_t time_data[7] = {
    priv_dec_to_bcd(time->second),
    priv_dec_to_bcd(time->minute),
    priv_dec_to_bcd(time->hour),
    priv_dec_to_bcd(time->day),
    priv_dec_to_bcd(time->date),
    priv_dec_to_bcd(time->month),
    priv_dec_to_bcd(time->year),
  };

  size_t    bytes_written;
  esp_err_t ret =
    star_bus_i2c_write(manager, bus_name, time_data, 7, DS3231_REG_SECONDS, &bytes_written);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "DS3231 set time failed");
    return ret;
  }

  ESP_LOGI(s_TAG,
           "DS3231 time set: 20%02d-%02d-%02d %02d:%02d:%02d",
           time->year,
           time->month,
           time->date,
           time->hour,
           time->minute,
           time->second);
  return ESP_OK;
}

esp_err_t star_bus_ds3231_get_time(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   ds3231_time_t*            time)
{
  if (manager == NULL || bus_name == NULL || time == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read 7 bytes starting from seconds register
  uint8_t   time_data[7];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, time_data, 7, DS3231_REG_SECONDS, &bytes_read);
  if (ret != ESP_OK || bytes_read != 7) {
    ESP_LOGE(s_TAG, "DS3231 get time failed");
    return ret;
  }

  // Convert from BCD
  time->second = priv_bcd_to_dec(time_data[0] & 0x7F);
  time->minute = priv_bcd_to_dec(time_data[1] & 0x7F);
  time->hour   = priv_bcd_to_dec(time_data[2] & 0x3F);
  time->day    = priv_bcd_to_dec(time_data[3] & 0x07);
  time->date   = priv_bcd_to_dec(time_data[4] & 0x3F);
  time->month  = priv_bcd_to_dec(time_data[5] & 0x1F);
  time->year   = priv_bcd_to_dec(time_data[6]);

  return ESP_OK;
}

esp_err_t star_bus_ds3231_get_temperature(const star_bus_manager_t* manager,
                                          const char*               bus_name,
                                          float*                    temp_celsius)
{
  if (manager == NULL || bus_name == NULL || temp_celsius == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read 2 bytes of temperature data
  uint8_t   temp_data[2];
  size_t    bytes_read;
  esp_err_t ret =
    star_bus_i2c_read(manager, bus_name, temp_data, 2, DS3231_REG_TEMP_MSB, &bytes_read);
  if (ret != ESP_OK || bytes_read != 2) {
    ESP_LOGE(s_TAG, "DS3231 get temperature failed");
    return ret;
  }

  // Convert to temperature (10-bit value, 0.25C resolution)
  int16_t temp_raw = (int16_t)(temp_data[0] << 8 | temp_data[1]) >> 6;
  *temp_celsius    = (float)temp_raw * 0.25f;

  return ESP_OK;
}
