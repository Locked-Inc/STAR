/**
 * @file 206_smart_lock.c
 * @brief Smart door lock with keypad and BLE
 *
 * IMPORTANT: Bluetooth support is NOT enabled by default
 * To use this example, add the following to platformio.ini:
 *
 * [env]
 * build_flags =
 *     -DCONFIG_BT_ENABLED=1
 *     -DCONFIG_BLUEDROID_ENABLED=1
 * lib_deps =
 *     ...existing deps...
 *     bt
 */

#include <esp_log.h>
/* #include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_main.h> */
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SMART_LOCK";

#define SERVO_PIN   (GPIO_NUM_25)
#define LED_GREEN   (GPIO_NUM_26)
#define LED_RED     (GPIO_NUM_27)
#define BUZZER_PIN  (GPIO_NUM_32)

/* Keypad pins (4x3 matrix) */
#define ROW_PINS    {GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26}
#define COL_PINS    {GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32}

static const char *s_valid_pin = "1234";
static const char *s_ble_key = "UNLOCK2024";
static bool s_is_locked = true;

static void priv_beep(int count, int duration_ms)
{
  for (int i = 0; i < count; i++) {
    gpio_set_level(BUZZER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(BUZZER_PIN, 0);
    if (i < count - 1) vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void priv_set_lock_state(bool locked)
{
  s_is_locked = locked;
  if (locked) {
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_RED, 1);
    /* Servo to locked position */
    ESP_LOGI(s_tag, "LOCKED");
    priv_beep(1, 200);
  } else {
    gpio_set_level(LED_GREEN, 1);
    gpio_set_level(LED_RED, 0);
    /* Servo to unlocked position */
    ESP_LOGI(s_tag, "UNLOCKED");
    priv_beep(2, 100);
  }
}

static bool priv_verify_pin(const char *entered_pin)
{
  return strcmp(entered_pin, s_valid_pin) == 0;
}

static bool priv_verify_ble_key(const uint8_t *data, int len)
{
  if (len != strlen(s_ble_key)) return false;
  return memcmp(data, s_ble_key, len) == 0;
}

/* BLE callback disabled - uncomment when bt component is enabled
static void priv_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  // BLE advertising and connection handling
}
*/

void smart_lock_example(void)
{
  nvs_flash_init();

  /* Initialize GPIOs */
  gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(SERVO_PIN, GPIO_MODE_OUTPUT);

  priv_set_lock_state(true);

  /* BLE initialization disabled - enable bt component first
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);
  esp_bluedroid_init();
  esp_bluedroid_enable();
  esp_ble_gap_register_callback(priv_ble_gap_event_handler);
  */

  ESP_LOGI(s_tag, "Smart lock initialized (Bluetooth disabled - see file header for enabling)");

  char entered_pin[10] = {0};
  int pin_pos = 0;

  /* Simulated keypad input for demo */
  const char *demo_inputs[] = {"1234", "0000", "1234"};
  int demo_idx = 0;

  for (int i = 0; i < 100; i++) {
    /* Simulate keypad press every 5 seconds */
    if (i % 50 == 0 && demo_idx < 3) {
      const char *input = demo_inputs[demo_idx++];
      ESP_LOGI(s_tag, "PIN entered: %s", input);

      if (priv_verify_pin(input)) {
        priv_set_lock_state(!s_is_locked);  /* Toggle */
      } else {
        ESP_LOGI(s_tag, "Invalid PIN!");
        priv_beep(3, 100);  /* Error beep */
      }
    }

    /* Auto-lock after 30 seconds */
    if (!s_is_locked && i % 300 == 299) {
      ESP_LOGI(s_tag, "Auto-locking...");
      priv_set_lock_state(true);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* BLE cleanup disabled
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  */
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { smart_lock_example(); }
