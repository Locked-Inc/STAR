/* esp32-firmware/components/star_error_handler/star_error_handler.c */

#include "star_error_handler.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#define TAG ("STAR ERROR HANDLER")

/* --- Functions --- */

/* clang-format off */
esp_err_t error_handler_init(error_handler_t* handler,
                             uint32_t         max_retries,
                             uint32_t         base_retry_delay,
                             uint32_t         max_retry_delay,
                             esp_err_t      (*reset_fn)(void* context),
                             void*            reset_context)
/* clang-format on */
{
  if (handler == NULL) {
    ESP_LOGE(TAG, "Init Error: Handler pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  /* Check if already initialized to prevent mutex leak */
  if (handler->mutex != NULL) {
    ESP_LOGE(TAG, "Init Error: Handler already initialized. Call deinit first.");
    return ESP_ERR_INVALID_STATE;
  }

  /* Initialize struct members first */
  handler->max_retries      = max_retries;
  handler->base_retry_delay = base_retry_delay > 0 ? base_retry_delay : 100;
  handler->max_retry_delay =
    max_retry_delay > handler->base_retry_delay ? max_retry_delay : handler->base_retry_delay;
  handler->current_retry_delay = handler->base_retry_delay;
  handler->reset_fn            = reset_fn;
  handler->reset_context       = reset_context;
  handler->last_error          = ESP_OK;
  handler->in_error_state      = false;
  handler->error_count         = 0;
  handler->current_retry       = 0;

  /* Create mutex last after other fields are set */
  handler->mutex = xSemaphoreCreateMutex();
  if (handler->mutex == NULL) {
    ESP_LOGE(TAG, "Mutex Error: Failed to create mutex during initialization");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

void error_handler_deinit(error_handler_t* handler)
{
  if (handler == NULL) {
    return;
  }

  SemaphoreHandle_t mutex_to_delete = handler->mutex;
  if (mutex_to_delete == NULL) {
    return;
  }

  /* Acquire mutex to ensure no other operation is in progress */
  if (xSemaphoreTake(mutex_to_delete, pdMS_TO_TICKS(5000)) != pdTRUE) {
    /* If we can't acquire mutex within timeout, do NOT proceed - another thread is using it */
    ESP_LOGE(TAG, "Deinit: Could not acquire mutex within timeout, aborting deinit");
    return;
  }

  /* Set to NULL while holding mutex to prevent new operations from starting */
  handler->mutex = NULL;

  /* Clear reset function pointers to prevent use-after-free */
  handler->reset_fn      = NULL;
  handler->reset_context = NULL;

  /* Give back the mutex before deleting (required by FreeRTOS) */
  xSemaphoreGive(mutex_to_delete);

  /* Now safe to delete */
  vSemaphoreDelete(mutex_to_delete);
}

/* clang-format off */
esp_err_t error_handler_record_error(error_handler_t* handler,
                                     esp_err_t        error,
                                     const char*      description,
                                     const char*      file,
                                     int32_t line,
                                     const char*      func)
/* clang-format on */
{
  if (handler == NULL) {
    return error;
  }

  const char* desc     = description ? description : "No description";
  const char* filename = file ? file : "Unknown file";
  const char* funcname = func ? func : "Unknown function";

  /* Atomically capture mutex pointer to prevent TOCTOU race */
  SemaphoreHandle_t mutex = handler->mutex;
  if (mutex == NULL) {
    ESP_LOGE(TAG,
             "Mutex Error: Mutex not initialized for error handler used in %s:%d",
             filename,
             line);
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Mutex Timeout: Failed to acquire mutex in %s:%d", filename, line);
    return ESP_ERR_TIMEOUT;
  }

  /* Re-check mutex validity after acquisition (handler could have been deinit'd) */
  if (handler->mutex == NULL) {
    xSemaphoreGive(mutex);
    ESP_LOGE(TAG, "Handler Error: Handler was deinitialized while waiting for mutex");
    return ESP_ERR_INVALID_STATE;
  }

  /* Only increment retry count if we haven't reached the max yet */
  uint32_t retry_count_to_log = handler->current_retry;
  if (!handler->in_error_state) {
    handler->in_error_state      = true;
    handler->current_retry       = 0;
    handler->current_retry_delay = handler->base_retry_delay;
    retry_count_to_log           = 0;
  } else if (handler->current_retry < handler->max_retries) {
    handler->current_retry++;
    retry_count_to_log    = handler->current_retry;
    uint64_t new_delay_64 = (uint64_t)handler->current_retry_delay * 2;
    if (new_delay_64 > handler->max_retry_delay) {
      handler->current_retry_delay = handler->max_retry_delay;
    } else if (new_delay_64 < handler->base_retry_delay) {
      handler->current_retry_delay = handler->base_retry_delay;
    } else {
      handler->current_retry_delay = (uint32_t)new_delay_64;
    }
    ESP_LOGI(TAG, "Backoff: Next retry delay: %" PRIu32 " ms", handler->current_retry_delay);
  } else {
    retry_count_to_log           = handler->max_retries;
    handler->current_retry_delay = handler->max_retry_delay;
    ESP_LOGI(TAG, "Backoff: Next retry delay: %" PRIu32 " ms", handler->current_retry_delay);
  }

  char log_buffer[256];
  snprintf(log_buffer,
           sizeof(log_buffer),
           "Desc: %s | Code: %d (%s) | Loc: %s:%" PRId32 " (%s) | Retry: %" PRIu32 "/%" PRIu32 "",
           desc,
           error,
           esp_err_to_name(error),
           filename,
           line,
           funcname,
           retry_count_to_log,
           handler->max_retries);
  log_buffer[sizeof(log_buffer) - 1] = '\0';

  ESP_LOGE(TAG, "Error Recorded: %s", log_buffer);

  handler->error_count++;
  handler->last_error = error;

  esp_err_t recovery_result = ESP_FAIL;

  if (handler->current_retry >= handler->max_retries) {
    ESP_LOGE(TAG,
             "Max Retries: Max retries (%" PRIu32 ") exceeded for error %d (%s).",
             handler->max_retries,
             error,
             esp_err_to_name(error));

    /* Capture reset function and context while holding mutex */
    esp_err_t (*reset_fn_local)(void* context) = handler->reset_fn;
    void* reset_context_local                  = handler->reset_context;

    if (reset_fn_local != NULL) {
      ESP_LOGI(TAG, "Reset Attempt: Attempting reset function after max retries...");

      /* Release mutex before calling potentially blocking reset function */
      xSemaphoreGive(mutex);

      /* Call reset with captured local copies */
      recovery_result = reset_fn_local(reset_context_local);

      /* Re-acquire mutex - use the original captured mutex pointer */
      if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Mutex Error: Failed to re-acquire mutex after reset attempt!");
        return ESP_ERR_TIMEOUT;
      }

      /* Verify handler wasn't deinitialized during reset */
      if (handler->mutex == NULL) {
        xSemaphoreGive(mutex);
        ESP_LOGE(TAG, "Handler Error: Handler was deinitialized during reset");
        return ESP_ERR_INVALID_STATE;
      }

      if (recovery_result == ESP_OK) {
        ESP_LOGI(TAG, "Reset Success: Reset function succeeded. Resetting error state.");
        handler->error_count         = 0;
        handler->current_retry       = 0;
        handler->current_retry_delay = handler->base_retry_delay;
        handler->last_error          = ESP_OK;
        handler->in_error_state      = false;
        xSemaphoreGive(mutex);
        return ESP_OK;
      } else {
        ESP_LOGE(TAG,
                 "Reset Failed: Reset function failed with code: %d (%s)",
                 recovery_result,
                 esp_err_to_name(recovery_result));
      }
    } else {
      ESP_LOGW(TAG, "No Reset: Max retries exceeded and no reset function provided.");
    }
    xSemaphoreGive(mutex);
    return error;
  } else {
    xSemaphoreGive(mutex);
    return error;
  }
}

bool error_handler_can_retry(error_handler_t* handler)
{
  bool can_retry = false;
  if (handler == NULL) {
    return false;
  }

  /* Atomically capture mutex pointer */
  SemaphoreHandle_t mutex = handler->mutex;
  if (mutex == NULL) {
    ESP_LOGE(TAG, "Mutex Error: Mutex not initialized for error handler (can_retry)");
    return false;
  }

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    /* Re-check mutex validity after acquisition */
    if (handler->mutex == NULL) {
      xSemaphoreGive(mutex);
      return false;
    }
    can_retry = (handler->in_error_state && (handler->current_retry < handler->max_retries));
    xSemaphoreGive(mutex);
  } else {
    ESP_LOGE(TAG, "Mutex Timeout: Failed to acquire mutex for can_retry check");
    can_retry = false;
  }
  return can_retry;
}

esp_err_t error_handler_reset_state(error_handler_t* handler)
{
  if (handler == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Atomically capture mutex pointer */
  SemaphoreHandle_t mutex = handler->mutex;
  if (mutex == NULL) {
    ESP_LOGE(TAG, "Mutex Error: Mutex not initialized for error handler (reset_state)");
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    /* Re-check mutex validity after acquisition */
    if (handler->mutex == NULL) {
      xSemaphoreGive(mutex);
      return ESP_ERR_INVALID_STATE;
    }
    if (handler->in_error_state) {
      ESP_LOGI(TAG, "State Reset: Resetting error handler state.");
    }
    handler->error_count         = 0;
    handler->current_retry       = 0;
    handler->current_retry_delay = handler->base_retry_delay;
    handler->last_error          = ESP_OK;
    handler->in_error_state      = false;
    xSemaphoreGive(mutex);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Mutex Timeout: Failed to acquire mutex for state reset");
    return ESP_ERR_TIMEOUT;
  }
}

/* --- Interface Adapter Functions --- */

static esp_err_t internal_iface_record_error(void*       ctx,
                                             esp_err_t   error,
                                             const char* message,
                                             const char* file,
                                             int         line,
                                             const char* func)
{
  error_handler_t* handler = (error_handler_t*)ctx;
  return error_handler_record_error(handler, error, message, file, line, func);
}

static bool internal_iface_can_retry(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;
  return error_handler_can_retry(handler);
}

static esp_err_t internal_iface_reset_state(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;
  return error_handler_reset_state(handler);
}

void error_handler_get_interface(star_error_interface_t* iface, error_handler_t* handler)
{
  if (iface == NULL) {
    return;
  }

  /* Validate handler before creating interface */
  if (handler == NULL || handler->mutex == NULL) {
    ESP_LOGE(TAG, "Interface Error: Invalid handler passed to get_interface");
    iface->record_error = NULL;
    iface->can_retry    = NULL;
    iface->reset_state  = NULL;
    iface->ctx          = NULL;
    return;
  }

  iface->record_error = internal_iface_record_error;
  iface->can_retry    = internal_iface_can_retry;
  iface->reset_state  = internal_iface_reset_state;
  iface->ctx          = handler;
}

/* --- Factory Functions --- */

error_handler_t* error_handler_create_default(void)
{
  /* Default parameters: 3 retries, 100ms base delay, 5000ms max delay, no reset function */
  return error_handler_create_custom(3, 100, 5000, NULL, NULL);
}

error_handler_t* error_handler_create_custom(uint32_t max_retries,
                                             uint32_t base_retry_delay,
                                             uint32_t max_retry_delay,
                                             esp_err_t (*reset_fn)(void* context),
                                             void* reset_context)
{
  /* Allocate memory for error handler */
  error_handler_t* handler = (error_handler_t*)malloc(sizeof(error_handler_t));
  if (handler == NULL) {
    ESP_LOGE(TAG, "Factory Error: Failed to allocate memory for error handler");
    return NULL;
  }

  /* Zero-initialize to ensure clean state */
  memset(handler, 0, sizeof(error_handler_t));

  /* Initialize the error handler */
  esp_err_t ret = error_handler_init(handler,
                                     max_retries,
                                     base_retry_delay,
                                     max_retry_delay,
                                     reset_fn,
                                     reset_context);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Factory Error: Failed to initialize error handler: %s", esp_err_to_name(ret));
    free(handler);
    return NULL;
  }

  ESP_LOGI(
    TAG,
    "Factory Success: Created error handler (retries=%lu, base_delay=%lums, max_delay=%lums)",
    (unsigned long)max_retries,
    (unsigned long)base_retry_delay,
    (unsigned long)max_retry_delay);

  return handler;
}

void error_handler_destroy_default(error_handler_t* handler)
{
  if (handler == NULL) {
    return; /* Safe no-op for NULL pointer */
  }

  /* Deinitialize (releases mutex) */
  error_handler_deinit(handler);

  /* Free the allocated memory */
  free(handler);

  ESP_LOGD(TAG, "Factory Destroy: Error handler destroyed and memory freed");
}

star_error_interface_t* star_error_interface_create_default(void)
{
  /* Create default error handler */
  error_handler_t* handler = error_handler_create_default();
  if (handler == NULL) {
    ESP_LOGE(TAG, "Failed to create default error handler");
    return NULL;
  }

  /* Allocate interface structure */
  star_error_interface_t* iface = (star_error_interface_t*)malloc(sizeof(star_error_interface_t));
  if (iface == NULL) {
    ESP_LOGE(TAG, "Failed to allocate error interface");
    error_handler_destroy_default(handler);
    return NULL;
  }

  /* Populate interface */
  error_handler_get_interface(iface, handler);

  ESP_LOGD(TAG, "Created default error interface");
  return iface;
}

void star_error_interface_destroy(star_error_interface_t* iface)
{
  if (iface == NULL) {
    return; /* Safe no-op for NULL pointer */
  }

  /* Extract the error handler from the context */
  error_handler_t* handler = (error_handler_t*)iface->ctx;

  /* Destroy the error handler */
  error_handler_destroy_default(handler);

  /* Free the interface */
  free(iface);

  ESP_LOGD(TAG, "Destroyed error interface");
}
