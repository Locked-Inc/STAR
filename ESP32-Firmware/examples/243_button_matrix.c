/**
 * @file 243_button_matrix.c
 * @brief 4x4 button matrix keypad
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "KEYPAD";

static const gpio_num_t rows[] = {GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_14};
static const gpio_num_t cols[] = {GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_15, GPIO_NUM_2};
static const char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static char priv_scan_keypad(void)
{
  for (int r = 0; r < 4; r++) {
    gpio_set_level(rows[r], 0);
    for (int c = 0; c < 4; c++) {
      if (!gpio_get_level(cols[c])) {
        gpio_set_level(rows[r], 1);
        return keys[r][c];
      }
    }
    gpio_set_level(rows[r], 1);
  }
  return 0;
}

void button_matrix_example(void)
{
  for (int i = 0; i < 4; i++) {
    gpio_set_direction(rows[i], GPIO_MODE_OUTPUT);
    gpio_set_level(rows[i], 1);
    gpio_set_direction(cols[i], GPIO_MODE_INPUT);
    gpio_set_pull_mode(cols[i], GPIO_PULLUP_ONLY);
  }

  ESP_LOGI(s_tag, "4x4 keypad started");

  char last_key = 0;
  char code[10] = {0};
  int code_pos = 0;

  for (int i = 0; i < 500; i++) {
    char key = priv_scan_keypad();

    if (key && key != last_key) {
      ESP_LOGI(s_tag, "Key pressed: %c", key);

      if (key == '#') {
        code[code_pos] = 0;
        ESP_LOGI(s_tag, "Code entered: %s", code);
        code_pos = 0;
      } else if (key == '*') {
        code_pos = 0;
        ESP_LOGI(s_tag, "Code cleared");
      } else if (code_pos < 9) {
        code[code_pos++] = key;
      }
    }
    last_key = key;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void app_main(void) { button_matrix_example(); }
