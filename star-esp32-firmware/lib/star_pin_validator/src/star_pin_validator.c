/* esp32-firmware/components/star_pin_validator/star_pin_validator.c */

#include "star_pin_validator.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char* s_TAG = "pin_validator";

/* Default mutex timeout in milliseconds to prevent infinite wait deadlocks */
static const uint32_t s_pin_validator_mutex_timeout_ms = 5000;

/* Short timeout for quick query operations */
static const uint32_t s_pin_validator_query_timeout_ms = 100;

/* Global validator instance */
static star_pin_validator_t s_g_pin_validator = {0};

/* Static mutex for protecting lazy initialization of the main mutex */
static portMUX_TYPE s_g_init_spinlock = portMUX_INITIALIZER_UNLOCKED;

/* Flag to prevent operations during free */
static volatile bool s_g_is_freeing = false;

/**
 * @brief Initialize mutex if not already initialized (thread-safe)
 * @return ESP_OK if successful, otherwise an error code
 */
static esp_err_t internal_init_mutex(void)
{
  /* Quick check without lock for common case */
  if (s_g_pin_validator.mutex != NULL) {
    return ESP_OK;
  }

  /* Use spinlock for thread-safe lazy initialization */
  portENTER_CRITICAL(&s_g_init_spinlock);

  /* Double-check after acquiring spinlock */
  if (s_g_pin_validator.mutex == NULL) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (new_mutex == NULL) {
      portEXIT_CRITICAL(&s_g_init_spinlock);
      ESP_LOGE(s_TAG, "Failed to create pin validator mutex");
      return ESP_ERR_NO_MEM;
    }
    s_g_pin_validator.mutex = new_mutex;
    ESP_LOGD(s_TAG, "Pin validator mutex created");
  }

  portEXIT_CRITICAL(&s_g_init_spinlock);
  return ESP_OK;
}

esp_err_t star_register_pin(gpio_num_t gpio_num, const char* desc, bool can_be_shared)
{
  esp_err_t ret;

  /* Check if freeing is in progress */
  if (s_g_is_freeing) {
    ESP_LOGE(s_TAG, "Cannot register pin: validator is being freed");
    return ESP_ERR_INVALID_STATE;
  }

  /* Validate inputs before taking mutex */
  if (gpio_num >= GPIO_NUM_MAX || gpio_num < 0) {
    ESP_LOGE(s_TAG, "Invalid GPIO number: %d (max allowed: %d)", gpio_num, GPIO_NUM_MAX - 1);
    return ESP_ERR_INVALID_ARG;
  }

  if (desc == NULL) {
    ESP_LOGE(s_TAG, "Description cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (strnlen(desc, PIN_VALIDATOR_DESC_MAX_LEN) >= PIN_VALIDATOR_DESC_MAX_LEN) {
    ESP_LOGE(s_TAG, "Description too long (max: %d characters)", PIN_VALIDATOR_DESC_MAX_LEN - 1);
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize mutex if needed */
  ret = internal_init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Atomically capture mutex pointer to prevent TOCTOU */
  SemaphoreHandle_t mutex = s_g_pin_validator.mutex;
  if (mutex == NULL) {
    ESP_LOGE(s_TAG, "Mutex not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  /* Take mutex */
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(s_pin_validator_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  /* Re-check freeing flag after acquiring mutex */
  if (s_g_is_freeing || s_g_pin_validator.mutex == NULL) {
    xSemaphoreGive(mutex);
    ESP_LOGE(s_TAG, "Validator was freed while waiting for mutex");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGD(s_TAG,
           "Registering pin %d, desc: '%s', can_be_shared: %d",
           gpio_num,
           desc,
           can_be_shared);

  s_g_pin_validator.initialized = true;

  /* Get current pin */
  star_pin_info_t* pin = &s_g_pin_validator.pins[gpio_num];

  /* If the pin is already in use check if it can be shared, error otherwise */
  if (pin->usage_count > 0) {
    ESP_LOGD(s_TAG, "Pin %d already in use (count: %d)", gpio_num, pin->usage_count);
    if (!pin->can_be_shared || !can_be_shared) {
      ESP_LOGE(s_TAG,
               "Pin %d cannot be shared (current config: %d, requested: %d)",
               gpio_num,
               pin->can_be_shared,
               can_be_shared);
      xSemaphoreGive(mutex);
      return ESP_ERR_INVALID_STATE;
    }
  } else {
    /* First time registering */
    ESP_LOGD(s_TAG, "First registration of pin %d", gpio_num);
    pin->can_be_shared = can_be_shared;
  }

  /* Overflow protection for usage_count */
  if (pin->usage_count == UINT8_MAX) {
    ESP_LOGE(s_TAG, "Pin %d usage count overflow (max: %d)", gpio_num, UINT8_MAX);
    xSemaphoreGive(mutex);
    return ESP_ERR_NO_MEM;
  }

  /* Increase the usage count */
  pin->usage_count += 1;

  /* Allocate/reallocate memory for user descriptions */
  if (pin->users == NULL) {
    pin->users = (char**)malloc(sizeof(char*));
    if (pin->users == NULL) {
      ESP_LOGE(s_TAG, "Failed to allocate memory for pin users");
      pin->usage_count -= 1;
      xSemaphoreGive(mutex);
      return ESP_ERR_NO_MEM;
    }
  } else {
    char** new_users = (char**)realloc(pin->users, sizeof(char*) * pin->usage_count);
    if (new_users == NULL) {
      ESP_LOGE(s_TAG, "Failed to reallocate memory for pin users");
      pin->usage_count -= 1;
      xSemaphoreGive(mutex);
      return ESP_ERR_NO_MEM;
    }
    pin->users = new_users;
  }

  /* Allocate and copy the description */
  pin->users[pin->usage_count - 1] = (char*)malloc(strlen(desc) + 1);
  if (pin->users[pin->usage_count - 1] == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate memory for description");
    pin->usage_count -= 1;
    xSemaphoreGive(mutex);
    return ESP_ERR_NO_MEM;
  }

  /* Use memcpy instead of strcpy for safety - length already validated */
  const size_t desc_len = strlen(desc);
  memcpy(pin->users[pin->usage_count - 1], desc, desc_len + 1);

  ESP_LOGD(s_TAG, "Pin %d registered successfully (usage count: %d)", gpio_num, pin->usage_count);

  /* Release mutex */
  xSemaphoreGive(mutex);

  return ESP_OK;
}

esp_err_t star_unregister_pin(gpio_num_t gpio_num, const char* desc)
{
  esp_err_t ret;

  /* Check if freeing is in progress */
  if (s_g_is_freeing) {
    ESP_LOGE(s_TAG, "Cannot unregister pin: validator is being freed");
    return ESP_ERR_INVALID_STATE;
  }

  /* Validate inputs before taking mutex */
  if (gpio_num >= GPIO_NUM_MAX || gpio_num < 0) {
    ESP_LOGE(s_TAG, "Invalid GPIO number: %d (max allowed: %d)", gpio_num, GPIO_NUM_MAX - 1);
    return ESP_ERR_INVALID_ARG;
  }

  if (desc == NULL) {
    ESP_LOGE(s_TAG, "Description cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize mutex if needed */
  ret = internal_init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Atomically capture mutex pointer to prevent TOCTOU */
  SemaphoreHandle_t mutex = s_g_pin_validator.mutex;
  if (mutex == NULL) {
    ESP_LOGE(s_TAG, "Mutex not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  /* Take mutex */
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(s_pin_validator_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  /* Re-check freeing flag after acquiring mutex */
  if (s_g_is_freeing || s_g_pin_validator.mutex == NULL) {
    xSemaphoreGive(mutex);
    ESP_LOGE(s_TAG, "Validator was freed while waiting for mutex");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGD(s_TAG, "Unregistering pin %d, desc: '%s'", gpio_num, desc);

  /* Get current pin */
  star_pin_info_t* pin = &s_g_pin_validator.pins[gpio_num];

  /* Check if pin is in use */
  if (pin->usage_count == 0) {
    ESP_LOGE(s_TAG, "Pin %d is not registered", gpio_num);
    xSemaphoreGive(mutex);
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
    ESP_LOGE(s_TAG, "Pin %d is registered but not with description '%s'", gpio_num, desc);
    xSemaphoreGive(mutex);
    return ESP_ERR_NOT_FOUND;
  }

  /* Free the description string */
  free(pin->users[found_index]);

  /* Shift remaining entries down (guard against underflow even though check above prevents it) */
  if (pin->usage_count > 1) {
    for (uint32_t i = (uint32_t)found_index; i < pin->usage_count - 1; i++) {
      pin->users[i] = pin->users[i + 1];
    }
  }

  /* Decrease usage count */
  pin->usage_count--;

  ESP_LOGD(s_TAG, "Pin %d unregistered (new usage count: %d)", gpio_num, pin->usage_count);

  /* If no more users, free the users array */
  if (pin->usage_count == 0) {
    free(pin->users);
    pin->users         = NULL;
    pin->can_be_shared = false;
    ESP_LOGD(s_TAG, "Pin %d has no more users, freed users array", gpio_num);
  } else {
    /* Reallocate to smaller size */
    char** new_users = (char**)realloc(pin->users, sizeof(char*) * pin->usage_count);
    if (new_users != NULL) {
      pin->users = new_users;
    }
    /* If realloc fails, keep old pointer (it's still valid, just not optimally sized) */
  }

  /* Release mutex */
  xSemaphoreGive(mutex);

  return ESP_OK;
}

esp_err_t star_validate_pins(void)
{
  esp_err_t ret;
  bool      conflicts_found = false;

  /* Check if freeing is in progress */
  if (s_g_is_freeing) {
    ESP_LOGE(s_TAG, "Cannot validate pins: validator is being freed");
    return ESP_ERR_INVALID_STATE;
  }

  /* Initialize mutex if needed */
  ret = internal_init_mutex();
  if (ret != ESP_OK) {
    return ret;
  }

  /* Atomically capture mutex pointer to prevent TOCTOU */
  SemaphoreHandle_t mutex = s_g_pin_validator.mutex;
  if (mutex == NULL) {
    ESP_LOGE(s_TAG, "Mutex not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  /* Take mutex */
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(s_pin_validator_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take pin validator mutex");
    return ESP_ERR_TIMEOUT;
  }

  /* Re-check freeing flag after acquiring mutex */
  if (s_g_is_freeing || s_g_pin_validator.mutex == NULL) {
    xSemaphoreGive(mutex);
    ESP_LOGE(s_TAG, "Validator was freed while waiting for mutex");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(s_TAG, "Validating all pin configurations");

  if (!s_g_pin_validator.initialized) {
    ESP_LOGW(s_TAG, "Pin validator not initialized, no pins registered");
    xSemaphoreGive(mutex);
    return ESP_OK;
  }

  /* Check each pin for conflicts */
  for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
    star_pin_info_t* pin = &s_g_pin_validator.pins[i];

    if (pin->usage_count > 1 && !pin->can_be_shared) {
      conflicts_found = true;
      ESP_LOGE(s_TAG, "Conflict on GPIO %d: Used %d times but not shareable", i, pin->usage_count);

      /* Log all users of this conflicting pin */
      for (uint32_t j = 0; j < pin->usage_count; j++) {
        ESP_LOGE(s_TAG, "  User %d: %s", j + 1, pin->users[j]);
      }
    } else if (pin->usage_count > 0) {
      ESP_LOGD(s_TAG,
               "GPIO %d: Used %d times, shareable: %d",
               i,
               pin->usage_count,
               pin->can_be_shared);
    }
  }

  /* Release mutex */
  xSemaphoreGive(mutex);

  if (conflicts_found) {
    ESP_LOGE(s_TAG, "Pin validation failed: conflicts detected");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(s_TAG, "Pin validation successful, no conflicts found");
  return ESP_OK;
}

esp_err_t star_free_pin_validator(void)
{
  esp_err_t ret;

  /* Set freeing flag early to prevent new operations */
  portENTER_CRITICAL(&s_g_init_spinlock);
  if (s_g_is_freeing) {
    portEXIT_CRITICAL(&s_g_init_spinlock);
    ESP_LOGW(s_TAG, "Free already in progress");
    return ESP_ERR_INVALID_STATE;
  }
  s_g_is_freeing = true;
  portEXIT_CRITICAL(&s_g_init_spinlock);

  /* Initialize mutex if needed */
  ret = internal_init_mutex();
  if (ret != ESP_OK) {
    s_g_is_freeing = false;
    return ret;
  }

  /* Check if mutex actually exists before trying to take/delete */
  if (s_g_pin_validator.mutex == NULL) {
    ESP_LOGW(
      s_TAG,
      "Attempting to free validator, but mutex was NULL (already freed or never initialized?)");
    s_g_pin_validator.initialized = false;
    s_g_is_freeing                = false;
    return ESP_OK;
  }

  /* Take mutex */
  if (xSemaphoreTake(s_g_pin_validator.mutex, pdMS_TO_TICKS(s_pin_validator_mutex_timeout_ms)) !=
      pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take pin validator mutex for free");
    s_g_is_freeing = false;
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(s_TAG, "Freeing pin validator resources");

  if (!s_g_pin_validator.initialized) {
    ESP_LOGW(s_TAG, "Pin validator not initialized, nothing to free");
    xSemaphoreGive(s_g_pin_validator.mutex);
    s_g_is_freeing = false;
    return ESP_OK;
  }

  /* Free all allocated memory for pin descriptions */
  for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
    star_pin_info_t* pin = &s_g_pin_validator.pins[i];

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

  s_g_pin_validator.initialized = false;

  /* Grab mutex handle before releasing and deleting */
  SemaphoreHandle_t mutex_to_delete = s_g_pin_validator.mutex;

  /* Use spinlock to safely set mutex to NULL (prevents race with init_mutex) */
  portENTER_CRITICAL(&s_g_init_spinlock);
  s_g_pin_validator.mutex = NULL;
  portEXIT_CRITICAL(&s_g_init_spinlock);

  /* Release mutex before deleting it */
  xSemaphoreGive(mutex_to_delete);

  /* Delete the mutex */
  vSemaphoreDelete(mutex_to_delete);

  /* Clear freeing flag */
  s_g_is_freeing = false;

  ESP_LOGI(s_TAG, "Pin validator resources freed successfully");
  return ESP_OK;
}

/* --- Interface Adapter Functions --- */

/**
 * @brief Adapter function for register_pin interface
 */
static esp_err_t iface_register_pin(void* ctx, int pin, const char* description, bool shared)
{
  (void)ctx; /* Unused - we use the global validator */
  return star_register_pin((gpio_num_t)pin, description, shared);
}

/**
 * @brief Adapter function for unregister_pin interface
 */
static esp_err_t iface_unregister_pin(void* ctx, int pin, const char* description)
{
  (void)ctx; /* Unused - we use the global validator */
  return star_unregister_pin((gpio_num_t)pin, description);
}

/**
 * @brief Adapter function for validate interface
 */
static esp_err_t iface_validate(void* ctx)
{
  (void)ctx; /* Unused - we use the global validator */
  return star_validate_pins();
}

void pin_validator_get_interface(star_pin_interface_t* iface)
{
  if (iface == NULL) {
    return;
  }

  iface->register_pin   = iface_register_pin;
  iface->unregister_pin = iface_unregister_pin;
  iface->validate       = iface_validate;
  iface->ctx            = NULL; /* Global validator, no context needed */
}

bool star_pin_validator_is_initialized(void)
{
  /* Quick check without mutex for common case */
  if (s_g_pin_validator.mutex == NULL) {
    return false; /* Not even mutex created, definitely not initialized */
  }

  /* Check if freeing is in progress */
  if (s_g_is_freeing) {
    return false;
  }

  /* Take mutex to safely read initialized flag */
  SemaphoreHandle_t mutex = s_g_pin_validator.mutex;
  if (mutex == NULL) {
    return false;
  }

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(s_pin_validator_query_timeout_ms)) != pdTRUE) {
    ESP_LOGW(s_TAG, "Failed to acquire mutex for is_initialized check");
    return false; /* Assume not initialized if can't check safely */
  }

  /* Re-check mutex validity after acquisition */
  if (s_g_pin_validator.mutex == NULL || s_g_is_freeing) {
    xSemaphoreGive(mutex);
    return false;
  }

  bool is_init = s_g_pin_validator.initialized;
  xSemaphoreGive(mutex);

  return is_init;
}
