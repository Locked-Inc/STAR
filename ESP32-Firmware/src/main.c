/* STAR Firmware - Main Application
 * ESP32 hardware abstraction and BMS communication
 */

#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "star_pin_validator.h"

static const char* TAG = "main";

/* Global bus manager */
static star_bus_manager_t g_bus_manager;

/* Error handler instance */
static error_handler_t g_error_handler;

/* Interface adapters */
static star_error_interface_t g_error_iface;
static star_pin_interface_t   g_pin_iface;

void app_main(void)
{
    ESP_LOGI(TAG, "STAR Firmware Starting");
    ESP_LOGI(TAG, "Version: %s", STAR_FIRMWARE_VERSION);

    /* Initialize NVS (required for persistent storage) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    /* Initialize error handler */
    ret = error_handler_init(&g_error_handler,
                             3,    /* max_retries */
                             100,  /* base_retry_delay (ms) */
                             5000, /* max_retry_delay (ms) */
                             NULL, /* reset_fn */
                             NULL  /* reset_context */);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Error handler initialized");

    /* Create interface adapters */
    error_handler_get_interface(&g_error_iface, &g_error_handler);
    pin_validator_get_interface(&g_pin_iface);
    ESP_LOGI(TAG, "Interface adapters created");

    /* Initialize bus manager with interfaces */
    ret = star_bus_manager_init(&g_bus_manager, "main", &g_error_iface, &g_pin_iface);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus manager: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bus manager initialized");

    /* Validate all registered pins for conflicts */
    ret = star_validate_pins();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pin validation failed! Check logs for conflicts.");
        return;
    }
    ESP_LOGI(TAG, "Pin validation passed");

    ESP_LOGI(TAG, "STAR Firmware Ready");

    /* Main application loop
     * Add your BMS communication and application logic here
     */
}
