/**
 * @file 208_fft_vibration.c
 * @brief FFT vibration analysis
 *
 * NOTE: This example requires the esp-dsp component to be enabled.
 * To enable FFT processing, add esp-dsp to your platformio.ini:
 *
 *   [env:example_208_fft_vibration]
 *   lib_deps = esp-dsp
 *
 * Without esp-dsp, this example demonstrates MPU6050 sensor reading only.
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
/* #include <esp_dsp.h> */  /* Requires esp-dsp component - uncomment after adding to platformio.ini */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "FFT_VIB";

#define FFT_SIZE (256)
#define SAMPLE_RATE (500)  /* Hz */

void fft_vibration_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "FFT_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  float *samples = heap_caps_malloc(FFT_SIZE * sizeof(float), MALLOC_CAP_DEFAULT);
  float *fft_data = heap_caps_malloc(FFT_SIZE * 2 * sizeof(float), MALLOC_CAP_DEFAULT);

  /* DSP FFT initialization - requires esp-dsp component */
  /* dsps_fft2r_init_fc32(NULL, FFT_SIZE); */

  ESP_LOGI(s_tag, "Starting vibration analysis at %d Hz", SAMPLE_RATE);

  for (int batch = 0; batch < 20; batch++) {
    /* Collect samples */
    for (int i = 0; i < FFT_SIZE; i++) {
      mpu6050_accel_t accel;
      mpu6050_gyro_t gyro;
      float temp;
      star_sensor_mpu6050_read_all(&imu, &accel, &gyro, &temp);
      samples[i] = sqrtf(accel.x_g * accel.x_g +
                        accel.y_g * accel.y_g +
                        accel.z_g * accel.z_g);
      vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE));
    }

    /* FFT Processing - requires esp-dsp component */
    /* Prepare FFT input (real + imaginary interleaved) */
    /*
    for (int i = 0; i < FFT_SIZE; i++) {
      fft_data[i * 2] = samples[i];
      fft_data[i * 2 + 1] = 0;
    }

    // Apply window
    dsps_wind_hann_f32(samples, FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
      fft_data[i * 2] *= samples[i];
    }

    // Compute FFT
    dsps_fft2r_fc32(fft_data, FFT_SIZE);
    dsps_bit_rev_fc32(fft_data, FFT_SIZE);

    // Find dominant frequency
    float max_mag = 0;
    int max_bin = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
      float re = fft_data[i * 2];
      float im = fft_data[i * 2 + 1];
      float mag = sqrtf(re * re + im * im);
      if (mag > max_mag) {
        max_mag = mag;
        max_bin = i;
      }
    }

    float dominant_freq = (float)max_bin * SAMPLE_RATE / FFT_SIZE;
    ESP_LOGI(s_tag, "Dominant frequency: %.1f Hz (magnitude: %.2f)", dominant_freq, max_mag);

    // Detect vibration type
    if (dominant_freq < 20) {
      ESP_LOGI(s_tag, "Low frequency vibration - possible imbalance");
    } else if (dominant_freq < 100) {
      ESP_LOGI(s_tag, "Medium frequency - possible bearing issue");
    } else {
      ESP_LOGI(s_tag, "High frequency vibration - possible gear mesh");
    }
    */

    /* Placeholder: Calculate average magnitude from collected samples */
    float avg_mag = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
      avg_mag += samples[i];
    }
    avg_mag /= FFT_SIZE;
    ESP_LOGI(s_tag, "Batch %d: Average vibration magnitude: %.2f m/s²", batch, avg_mag);
    ESP_LOGI(s_tag, "FFT processing disabled - add esp-dsp component to enable frequency analysis");
  }

  free(samples);
  free(fft_data);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { fft_vibration_example(); }
