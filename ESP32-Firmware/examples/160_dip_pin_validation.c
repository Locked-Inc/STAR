/**
 * @file 160_dip_pin_validation.c
 * @brief Dependency Inversion Principle pin validation patterns
 *
 * Demonstrates:
 * - Custom pin validator implementation
 * - Conflict detection and prevention
 * - Runtime pin management
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_core.h"
#include "star_pin_validator.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "DIP_PIN";

/* Custom pin tracking context */
#define MAX_TRACKED_PINS (40)

typedef struct {
  gpio_num_t pin;
  const char *owner;
  bool shared;
  bool in_use;
} pin_entry_t;

typedef struct {
  pin_entry_t pins[MAX_TRACKED_PINS];
  int count;
  int conflict_count;
} custom_pin_ctx_t;

/* Custom pin validator functions */
static esp_err_t custom_register_pin(void *ctx, gpio_num_t pin, const char *desc, bool shared)
{
  custom_pin_ctx_t* pctx = (custom_pin_ctx_t*)ctx;

  /* Check for conflicts */
  for (int i = 0; i < pctx->count; i++) {
    if (pctx->pins[i].pin == pin && pctx->pins[i].in_use) {
      if (!pctx->pins[i].shared || !shared) {
        pctx->conflict_count++;
        ESP_LOGE(s_tag, "CONFLICT: GPIO%d already used by '%s', requested by '%s'",
                 pin, pctx->pins[i].owner, desc);
        return ESP_ERR_INVALID_STATE;
      }
      /* Shared pin, allow */
      ESP_LOGI(s_tag, "Shared GPIO%d: '%s' + '%s'", pin, pctx->pins[i].owner, desc);
      return ESP_OK;
    }
  }

  /* Add new entry */
  if (pctx->count >= MAX_TRACKED_PINS) {
    ESP_LOGE(s_tag, "Pin tracker full");
    return ESP_ERR_NO_MEM;
  }

  pctx->pins[pctx->count].pin = pin;
  pctx->pins[pctx->count].owner = desc;
  pctx->pins[pctx->count].shared = shared;
  pctx->pins[pctx->count].in_use = true;
  pctx->count++;

  ESP_LOGI(s_tag, "Registered GPIO%d for '%s' (shared=%d)", pin, desc, shared);
  return ESP_OK;
}

static esp_err_t custom_unregister_pin(void *ctx, gpio_num_t pin, const char *desc)
{
  custom_pin_ctx_t* pctx = (custom_pin_ctx_t*)ctx;

  for (int i = 0; i < pctx->count; i++) {
    if (pctx->pins[i].pin == pin && pctx->pins[i].in_use) {
      pctx->pins[i].in_use = false;
      ESP_LOGI(s_tag, "Unregistered GPIO%d from '%s'", pin, desc);
      return ESP_OK;
    }
  }
  return ESP_ERR_NOT_FOUND;
}

static esp_err_t custom_validate_pins(void *ctx)
{
  custom_pin_ctx_t* pctx = (custom_pin_ctx_t*)ctx;

  ESP_LOGI(s_tag, "=== Pin Assignment Summary ===");
  for (int i = 0; i < pctx->count; i++) {
    if (pctx->pins[i].in_use) {
      ESP_LOGI(s_tag, "  GPIO%d: %s %s",
               pctx->pins[i].pin,
               pctx->pins[i].owner,
               pctx->pins[i].shared ? "(shared)" : "(exclusive)");
    }
  }

  if (pctx->conflict_count > 0) {
    ESP_LOGE(s_tag, "Validation FAILED: %d conflicts detected", pctx->conflict_count);
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(s_tag, "Validation PASSED: No conflicts");
  return ESP_OK;
}

void dip_pin_validation_example(void)
{
  /* Create custom pin context */
  custom_pin_ctx_t pin_ctx = {0};

  /* Build interface from custom functions */
  star_pin_interface_t custom_iface = {
    .ctx = &pin_ctx,
    .register_pin = custom_register_pin,
    .unregister_pin = custom_unregister_pin,
    .validate = custom_validate_pins
  };

  ESP_LOGI(s_tag, "Custom pin interface created");

  /* Initialize bus manager with custom pin validator */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Pin_Demo", NULL, &custom_iface);

  /* Register I2C pins (shared) */
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_21, "I2C_SDA", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_22, "I2C_SCL", true);

  /* Register SPI pins (shared among SPI devices) */
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_18, "SPI_CLK", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_19, "SPI_MISO", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_23, "SPI_MOSI", true);

  /* Register exclusive pins */
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_5, "SPI_CS_LCD", false);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_4, "DHT22_DATA", false);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_2, "LED_BUILTIN", false);

  /* Try to register conflicting pin (should fail) */
  ESP_LOGI(s_tag, "Attempting conflicting registration...");
  esp_err_t ret = STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_4, "MOTOR_PWM", false);
  if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "Conflict correctly detected!");
  }

  /* Add more I2C devices on same bus (should work) */
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_21, "I2C_MPU6050", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_22, "I2C_MPU6050", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_21, "I2C_BH1750", true);
  STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_22, "I2C_BH1750", true);

  /* Validate all pins */
  STAR_IFACE_VALIDATE_PINS(&custom_iface);

  /* Unregister some pins */
  STAR_IFACE_UNREGISTER_PIN(&custom_iface, GPIO_NUM_4, "DHT22_DATA");

  /* Now register the motor pin (should work) */
  ret = STAR_IFACE_REGISTER_PIN(&custom_iface, GPIO_NUM_4, "MOTOR_PWM", false);
  ESP_LOGI(s_tag, "Motor PWM registration: %s", ret == ESP_OK ? "OK" : "FAILED");

  /* Final validation */
  STAR_IFACE_VALIDATE_PINS(&custom_iface);

  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { dip_pin_validation_example(); }
