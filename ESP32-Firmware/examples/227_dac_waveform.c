/**
 * @file 227_dac_waveform.c
 * @brief DAC waveform generation
 */

#include <esp_log.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "DAC_WAVE";

#define DAC_CHANNEL DAC_CHANNEL_1  /* GPIO25 */
#define SAMPLE_RATE (10000)
#define WAVE_FREQ (100)

void dac_waveform_example(void)
{
  dac_output_enable(DAC_CHANNEL);

  ESP_LOGI(s_tag, "DAC waveform generator started");

  /* Generate different waveforms */

  /* 1. Sine wave */
  ESP_LOGI(s_tag, "Generating sine wave at %d Hz", WAVE_FREQ);
  for (int i = 0; i < 5000; i++) {
    float phase = 2.0f * M_PI * WAVE_FREQ * i / SAMPLE_RATE;
    uint8_t value = (uint8_t)(127 + 127 * sinf(phase));
    dac_output_voltage(DAC_CHANNEL, value);
    esp_rom_delay_us(1000000 / SAMPLE_RATE);
  }

  /* 2. Square wave */
  ESP_LOGI(s_tag, "Generating square wave at %d Hz", WAVE_FREQ);
  for (int i = 0; i < 5000; i++) {
    int period_sample = i % (SAMPLE_RATE / WAVE_FREQ);
    uint8_t value = (period_sample < (SAMPLE_RATE / WAVE_FREQ / 2)) ? 255 : 0;
    dac_output_voltage(DAC_CHANNEL, value);
    esp_rom_delay_us(1000000 / SAMPLE_RATE);
  }

  /* 3. Triangle wave */
  ESP_LOGI(s_tag, "Generating triangle wave at %d Hz", WAVE_FREQ);
  for (int i = 0; i < 5000; i++) {
    int period_samples = SAMPLE_RATE / WAVE_FREQ;
    int pos = i % period_samples;
    uint8_t value;
    if (pos < period_samples / 2) {
      value = (uint8_t)(255 * pos * 2 / period_samples);
    } else {
      value = (uint8_t)(255 * (period_samples - pos) * 2 / period_samples);
    }
    dac_output_voltage(DAC_CHANNEL, value);
    esp_rom_delay_us(1000000 / SAMPLE_RATE);
  }

  /* 4. Sawtooth wave */
  ESP_LOGI(s_tag, "Generating sawtooth wave at %d Hz", WAVE_FREQ);
  for (int i = 0; i < 5000; i++) {
    int period_samples = SAMPLE_RATE / WAVE_FREQ;
    uint8_t value = (uint8_t)(255 * (i % period_samples) / period_samples);
    dac_output_voltage(DAC_CHANNEL, value);
    esp_rom_delay_us(1000000 / SAMPLE_RATE);
  }

  dac_output_voltage(DAC_CHANNEL, 0);
  dac_output_disable(DAC_CHANNEL);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { dac_waveform_example(); }
