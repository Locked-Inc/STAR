/* esp32-firmware/components/star_pin_validator/star_pin_validator.c */

#include "star_pin_validator.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char* TAG = "pin_validator";

/* Global validator instance */
static star_pin_validator_t g_pin_validator = {0};

/**
 * @brief Initialize mutex if not already initialized
 * @return ESP_OK if successful, otherwise an error code
 */
static esp_err_t init_mutex(void)
{
  if (g_pin_validator.mutex == NULL) {
    g_pin_validator.mutex = xSemaphoreCreateMutex();
    if (g_pin_validator.mutex == NULL) {
      ESP_LOGE(TAG, "Failed to create pin validator mutex");
      return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "Pin validator mutex created");
  }
  return ESP_OK;
}

esp_err_t star_register_pin(gpio_num_t gpio_num, const char* desc, bool can_be_shared)
{
  esp_err_t ret;

  /* Validate inputs before taking mutex */
  if (gpio_num >= GPIO_NUM_MAX || gpio_num < 0) {
    ESP_LOGE(TAG, "Invalid GPIO number: %d (max allowed: %d)", gpio_num, GPIO_NUM_MAX - 1);
    return ESP_ERR_INVALID_ARG;
  }

  if (desc == NULL) {
    ESP_LOGE(TAG, "Description cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (strlen(desc) >= PIN_VALIDATOR_DESC_MAX_LEN) {
    ESP_LOGE(TAG, "Description too long (max: %d characters)", PIN_VALIDATOR_DESC_MAX_LEN - 1);
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize mutex if needed */
  ret = init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Take mutex */
  if (xSemaphoreTake(g_pin_validator.mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGD(TAG, "Registering pin %d, desc: '%s', can_be_shared: %d", gpio_num, desc, can_be_shared);

  g_pin_validator.initialized = true;

  /* Get current pin */
  star_pin_info_t* pin = &g_pin_validator.pins[gpio_num];

  /* If the pin is already in use check if it can be shared, error otherwise */
  if (pin->usage_count > 0) {
    ESP_LOGD(TAG, "Pin %d already in use (count: %d)", gpio_num, pin->usage_count);
    if (!pin->can_be_shared || !can_be_shared) {
      ESP_LOGE(TAG,
               "Pin %d cannot be shared (current config: %d, requested: %d)",
               gpio_num,
               pin->can_be_shared,
               can_be_shared);
      xSemaphoreGive(g_pin_validator.mutex);
      return ESP_ERR_INVALID_STATE;
    }
  } else {
    /* First time registering */
    ESP_LOGD(TAG, "First registration of pin %d", gpio_num);
    pin->can_be_shared = can_be_shared;
  }

  /* Increase the usage count */
  pin->usage_count += 1;

  /* Allocate/reallocate memory for user descriptions */
  if (pin->users == NULL) {
    pin->users = (char**)malloc(sizeof(char*));
    if (pin->users == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for pin users");
      pin->usage_count -= 1;
      xSemaphoreGive(g_pin_validator.mutex);
      return ESP_ERR_NO_MEM;
    }
  } else {
    char** new_users = (char**)realloc(pin->users, sizeof(char*) * pin->usage_count);
    if (new_users == NULL) {
      ESP_LOGE(TAG, "Failed to reallocate memory for pin users");
      pin->usage_count -= 1;
      xSemaphoreGive(g_pin_validator.mutex);
      return ESP_ERR_NO_MEM;
    }
    pin->users = new_users;
  }

  /* Allocate and copy the description */
  pin->users[pin->usage_count - 1] = (char*)malloc(strlen(desc) + 1);
  if (pin->users[pin->usage_count - 1] == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for description");
    pin->usage_count -= 1;
    xSemaphoreGive(g_pin_validator.mutex);
    return ESP_ERR_NO_MEM;
  }

  strcpy(pin->users[pin->usage_count - 1], desc);

  ESP_LOGD(TAG, "Pin %d registered successfully (usage count: %d)", gpio_num, pin->usage_count);

  /* Release mutex */
  xSemaphoreGive(g_pin_validator.mutex);

  return ESP_OK;
}

esp_err_t star_unregister_pin(gpio_num_t gpio_num, const char* desc)
{
  esp_err_t ret;

  /* Validate inputs before taking mutex */
  if (gpio_num >= GPIO_NUM_MAX || gpio_num < 0) {
    ESP_LOGE(TAG, "Invalid GPIO number: %d (max allowed: %d)", gpio_num, GPIO_NUM_MAX - 1);
    return ESP_ERR_INVALID_ARG;
  }

  if (desc == NULL) {
    ESP_LOGE(TAG, "Description cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize mutex if needed */
  ret = init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Take mutex */
  if (xSemaphoreTake(g_pin_validator.mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGD(TAG, "Unregistering pin %d, desc: '%s'", gpio_num, desc);

  /* Get current pin */
  star_pin_info_t* pin = &g_pin_validator.pins[gpio_num];

  /* Check if pin is in use */
  if (pin->usage_count == 0) {
    ESP_LOGE(TAG, "Pin %d is not registered", gpio_num);
    xSemaphoreGive(g_pin_validator.mutex);
    return ESP_ERR_NOT_FOUND;
  }

  /* Find the matching description */
  int32_t found_index = -1;
  for (uint32_t i = 0; i < pin->usage_count; i++) {
    if (strcmp(pin->users[i], desc) == 0) {
      found_index = i;
      break;
    }
  }

  if (found_index == -1) {
    ESP_LOGE(TAG, "Pin %d is registered but not with description '%s'", gpio_num, desc);
    xSemaphoreGive(g_pin_validator.mutex);
    return ESP_ERR_NOT_FOUND;
  }

  /* Free the description string */
  free(pin->users[found_index]);

  /* Shift remaining entries down */
  for (uint32_t i = found_index; i < pin->usage_count - 1; i++) {
    pin->users[i] = pin->users[i + 1];
  }

  /* Decrease usage count */
  pin->usage_count--;

  ESP_LOGD(TAG, "Pin %d unregistered (new usage count: %d)", gpio_num, pin->usage_count);

  /* If no more users, free the users array */
  if (pin->usage_count == 0) {
    free(pin->users);
    pin->users         = NULL;
    pin->can_be_shared = false;
    ESP_LOGD(TAG, "Pin %d has no more users, freed users array", gpio_num);
  } else {
    /* Reallocate to smaller size */
    char** new_users = (char**)realloc(pin->users, sizeof(char*) * pin->usage_count);
    if (new_users != NULL) {
      pin->users = new_users;
    }
    /* If realloc fails, keep old pointer (it's still valid, just not optimally sized) */
  }

  /* Release mutex */
  xSemaphoreGive(g_pin_validator.mutex);

  return ESP_OK;
}

esp_err_t star_validate_pins(void)
{
  esp_err_t ret;
  bool      conflicts_found = false;

  /* Initialize mutex if needed */
  ret = init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Take mutex */
  if (xSemaphoreTake(g_pin_validator.mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(TAG, "Validating all pin configurations");

  if (!g_pin_validator.initialized) {
    ESP_LOGW(TAG, "Pin validator not initialized, no pins registered");
    xSemaphoreGive(g_pin_validator.mutex);
    return ESP_OK;
  }

  /* Check each pin for conflicts */
  for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
    star_pin_info_t* pin = &g_pin_validator.pins[i];

    if (pin->usage_count > 1 && !pin->can_be_shared) {
      conflicts_found = true;
      ESP_LOGE(TAG, "Conflict on GPIO %d: Used %d times but not shareable", i, pin->usage_count);

      /* Log all users of this conflicting pin */
      for (uint32_t j = 0; j < pin->usage_count; j++) {
        ESP_LOGE(TAG, "  User %d: %s", j + 1, pin->users[j]);
      }
    } else if (pin->usage_count > 0) {
      ESP_LOGD(TAG,
               "GPIO %d: Used %d times, shareable: %d",
               i,
               pin->usage_count,
               pin->can_be_shared);
    }
  }

  /* Release mutex */
  xSemaphoreGive(g_pin_validator.mutex);

  if (conflicts_found) {
    ESP_LOGE(TAG, "Pin validation failed: conflicts detected");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Pin validation successful, no conflicts found");
  return ESP_OK;
}

esp_err_t star_free_pin_validator(void)
{
  esp_err_t ret;

  /* Initialize mutex if needed */
  ret = init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Check if mutex actually exists before trying to take/delete */
  if (g_pin_validator.mutex == NULL) {
    ESP_LOGW(
      TAG,
      "Attempting to free validator, but mutex was NULL (already freed or never initialized?)");
    g_pin_validator.initialized = false;
    return ESP_OK;
  }

  /* Take mutex */
  if (xSemaphoreTake(g_pin_validator.mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take pin validator mutex for free");
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(TAG, "Freeing pin validator resources");

  if (!g_pin_validator.initialized) {
    ESP_LOGW(TAG, "Pin validator not initialized, nothing to free");
    xSemaphoreGive(g_pin_validator.mutex);
    return ESP_OK;
  }

  /* Free all allocated memory for pin descriptions */
  for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
    star_pin_info_t* pin = &g_pin_validator.pins[i];

    if (pin->users != NULL) {
      for (uint32_t j = 0; j < pin->usage_count; j++) {
        if (pin->users[j] != NULL) {
          free(pin->users[j]);
          pin->users[j] = NULL;
        }
      }

      free(pin->users);
      pin->users = NULL;
    }

    /* Reset pin data */
    pin->usage_count   = 0;
    pin->can_be_shared = false;
  }

  g_pin_validator.initialized = false;

  /* Grab mutex handle before releasing and deleting */
  SemaphoreHandle_t mutex_to_delete = g_pin_validator.mutex;
  g_pin_validator.mutex             = NULL;

  /* Release mutex before deleting it */
  xSemaphoreGive(mutex_to_delete);

  /* Delete the mutex */
  vSemaphoreDelete(mutex_to_delete);

  ESP_LOGI(TAG, "Pin validator resources freed successfully");
  return ESP_OK;
}
