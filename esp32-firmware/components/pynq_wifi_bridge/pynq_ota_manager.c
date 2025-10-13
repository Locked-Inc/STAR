/* esp32-firmware/components/pynq_wifi_bridge/pynq_ota_manager.c */

#include "pynq_ota_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"
#include "pynq_wifi_manager.h"
#include "pynq_wifi_protocol.h"

static const char* TAG = "ota_manager";

/* Firmware version - update these with each release */
#define FIRMWARE_VERSION_MAJOR (0)
#define FIRMWARE_VERSION_MINOR (1)
#define FIRMWARE_VERSION_PATCH (0)

/* Module state */
static bool               g_initialized = false;
static ota_config_t       g_config;
static ota_status_t       g_status;
static TaskHandle_t       g_ota_task_handle = NULL;
static EventGroupHandle_t g_ota_event_group = NULL;

#define OTA_CANCEL_BIT (BIT0)

/**
 * @brief OTA update task parameters
 */
typedef struct {
  char* url;
  char* expected_sha256; /* NULL if no verification needed */
  bool  allow_downgrade;
} ota_update_params_t;

/**
 * @brief Free OTA update parameters
 */
static void ota_free_params(ota_update_params_t* params)
{
  if (params) {
    if (params->url)
      free(params->url);
    if (params->expected_sha256)
      free(params->expected_sha256);
    free(params);
  }
}

/**
 * @brief Verify SHA256 hash of downloaded firmware
 * @param ota_handle OTA handle with downloaded data
 * @param expected_sha256 Expected SHA256 hash (64 hex characters)
 * @return true if hash matches, false otherwise
 */
static bool verify_sha256(esp_https_ota_handle_t ota_handle, const char* expected_sha256)
{
  if (!expected_sha256 || strlen(expected_sha256) != 64) {
    ESP_LOGE(TAG, "Invalid SHA256 hash format");
    return false;
  }

  /* Get SHA256 from downloaded image */
  uint8_t   sha256[32];
  esp_err_t ret = esp_https_ota_get_img_desc(ota_handle, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get image descriptor");
    return false;
  }

  /* Calculate actual SHA256 - ESP-IDF OTA doesn't expose this directly,
   * so we need to calculate it ourselves from the partition */
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    ESP_LOGE(TAG, "Failed to get update partition");
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); /* 0 = SHA256, not SHA224 */

  /* Read partition in chunks and update hash */
  uint8_t buffer[1024];
  size_t  bytes_read = 0;
  size_t  image_size = esp_https_ota_get_image_len_read(ota_handle);

  for (size_t offset = 0; offset < image_size; offset += sizeof(buffer)) {
    size_t to_read =
      (image_size - offset) < sizeof(buffer) ? (image_size - offset) : sizeof(buffer);

    ret = esp_partition_read(update_partition, offset, buffer, to_read);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to read partition for hash verification");
      mbedtls_sha256_free(&ctx);
      return false;
    }

    mbedtls_sha256_update(&ctx, buffer, to_read);
    bytes_read += to_read;
  }

  mbedtls_sha256_finish(&ctx, sha256);
  mbedtls_sha256_free(&ctx);

  /* Convert calculated hash to hex string */
  char calculated_sha256[65];
  for (uint32_t i = 0; i < 32; i++) {
    sprintf(&calculated_sha256[i * 2], "%02x", sha256[i]);
  }
  calculated_sha256[64] = '\0';

  /* Compare hashes (case-insensitive) */
  for (uint32_t i = 0; i < 64; i++) {
    if (tolower(expected_sha256[i]) != tolower(calculated_sha256[i])) {
      ESP_LOGE(TAG, "SHA256 verification failed!");
      ESP_LOGE(TAG, "Expected: %s", expected_sha256);
      ESP_LOGE(TAG, "Actual:   %s", calculated_sha256);
      return false;
    }
  }

  ESP_LOGI(TAG, "SHA256 verification passed: %s", calculated_sha256);
  return true;
}

/**
 * @brief OTA update task
 * @param pvParameters Task parameters (ota_update_params_t*)
 */
static void ota_update_task(void* pvParameters)
{
  ota_update_params_t* params = (ota_update_params_t*)pvParameters;

  if (!params || !params->url) {
    ESP_LOGE(TAG, "Invalid OTA update parameters");
    ota_free_params(params);
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_error;
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Starting OTA update from: %s", params->url);

  if (params->expected_sha256) {
    ESP_LOGI(TAG, "SHA256 verification enabled: %s", params->expected_sha256);
  }

  if (params->allow_downgrade) {
    ESP_LOGI(TAG, "Version downgrade allowed");
  }

  g_status.state            = k_ota_downloading;
  g_status.progress         = 0;
  g_status.bytes_downloaded = 0;
  g_status.total_bytes      = 0;
  g_status.error_code       = k_status_ok;

  /* Configure HTTPS OTA */
  esp_http_client_config_t http_config = {
    .url               = params->url,
    .timeout_ms        = 10000,
    .keep_alive_enable = true,
#ifdef CONFIG_PYNQ_OTA_HTTPS_CERT_VERIFICATION
    .crt_bundle_attach = esp_crt_bundle_attach,
#else
    .skip_cert_common_name_check = true,
#endif
  };

  esp_https_ota_config_t ota_config = {
    .http_config = &http_config,
  };

  esp_https_ota_handle_t ota_handle = NULL;
  esp_err_t              ret        = esp_https_ota_begin(&ota_config, &ota_handle);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_ota_http_error;
    ota_free_params(params);
    vTaskDelete(NULL);
    return;
  }

  /* Get image size for progress tracking */
  int32_t image_size = esp_https_ota_get_image_size(ota_handle);
  if (image_size > 0) {
    g_status.total_bytes = image_size;
    ESP_LOGI(TAG, "Firmware size: %d bytes", image_size);
  }

  /* Download and write firmware */
  while (1) {
    /* Check for cancel */
    EventBits_t bits = xEventGroupGetBits(g_ota_event_group);
    if (bits & OTA_CANCEL_BIT) {
      ESP_LOGW(TAG, "OTA update cancelled");
      esp_https_ota_abort(ota_handle);
      g_status.state = k_ota_idle;
      ota_free_params(params);
      vTaskDelete(NULL);
      return;
    }

    ret = esp_https_ota_perform(ota_handle);

    if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      break;
    }

    /* Update progress */
    int32_t downloaded        = esp_https_ota_get_image_len_read(ota_handle);
    g_status.bytes_downloaded = downloaded;

    if (g_status.total_bytes > 0) {
      g_status.progress = (uint8_t)((downloaded * 100) / g_status.total_bytes);
    }

    ESP_LOGI(TAG,
             "Download progress: %d%% (%d/%d bytes)",
             g_status.progress,
             downloaded,
             g_status.total_bytes);
  }

  /* Check if download completed successfully */
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OTA download failed: %s", esp_err_to_name(ret));
    esp_https_ota_abort(ota_handle);
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_ota_http_error;
    ota_free_params(params);
    vTaskDelete(NULL);
    return;
  }

  /* Verify and finalize */
  g_status.state = k_ota_verifying;
  ESP_LOGI(TAG, "Verifying firmware...");

  if (!esp_https_ota_is_complete_data_received(ota_handle)) {
    ESP_LOGE(TAG, "OTA data incomplete");
    esp_https_ota_abort(ota_handle);
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_ota_http_error;
    ota_free_params(params);
    vTaskDelete(NULL);
    return;
  }

  /* Verify SHA256 hash if provided */
  if (params->expected_sha256 && strlen(params->expected_sha256) > 0) {
    ESP_LOGI(TAG, "Verifying SHA256 hash...");
    if (!verify_sha256(ota_handle, params->expected_sha256)) {
      ESP_LOGE(TAG, "SHA256 verification failed - aborting update");
      esp_https_ota_abort(ota_handle);
      g_status.state      = k_ota_failed;
      g_status.error_code = k_status_ota_sha256_mismatch;
      ota_free_params(params);
      vTaskDelete(NULL);
      return;
    }
  } else {
    ESP_LOGW(TAG, "Skipping SHA256 verification (no hash provided)");
  }

  /* Install new firmware */
  g_status.state = k_ota_installing;
  ESP_LOGI(TAG, "Installing firmware...");

  ret = esp_https_ota_finish(ota_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(ret));
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_ota_partition_error;
    ota_free_params(params);
    vTaskDelete(NULL);
    return;
  }

  g_status.state      = k_ota_complete;
  g_status.progress   = 100;
  g_status.error_code = k_status_ok;

  ESP_LOGI(TAG, "OTA update complete!");

  /* Auto-reboot if configured */
  if (g_config.auto_reboot) {
    ESP_LOGI(TAG, "Rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ota_free_params(params);
    esp_restart();
  }

  ota_free_params(params);
  vTaskDelete(NULL);
}

/**
 * @brief Auto-update check task
 * @param pvParameters Not used
 */
static void ota_check_task(void* pvParameters)
{
  (void)pvParameters;

  while (1) {
    /* Wait for check interval */
    vTaskDelay(pdMS_TO_TICKS(g_config.check_interval_ms));

    /* Only check if WiFi is connected and no update in progress */
    if (wifi_manager_get_status() != k_wifi_connected) {
      continue;
    }

    if (g_status.state != k_ota_idle) {
      continue;
    }

    ESP_LOGI(TAG, "Checking for updates...");

    /* Check for update */
    bool available = ota_manager_check_update();

    if (available && g_config.auto_update) {
      ESP_LOGI(TAG, "Update available - starting automatic update");

      /* Use stored URL and SHA256 from version check */
      const char* url =
        (strlen(g_status.update_url) > 0) ? g_status.update_url : g_config.update_url;

      const char* sha256 = (strlen(g_status.update_sha256) == 64) ? g_status.update_sha256 : NULL;

      if (sha256) {
        ESP_LOGI(TAG, "Auto-update will verify SHA256: %.16s...", sha256);
      } else {
        ESP_LOGW(TAG, "Auto-update proceeding WITHOUT hash verification!");
      }

      /* Start verified update with hash from version API */
      ota_manager_start_update_verified(url, sha256, false /* no downgrade in auto-update */);
    }
  }
}

bool ota_manager_init(const ota_config_t* config)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  if (!config || !config->update_url) {
    ESP_LOGE(TAG, "Invalid configuration");
    return false;
  }

  ESP_LOGI(TAG, "Initializing OTA manager");

  /* Copy configuration */
  memcpy(&g_config, config, sizeof(ota_config_t));

  /* Initialize status */
  memset(&g_status, 0, sizeof(ota_status_t));
  g_status.state              = k_ota_idle;
  g_status.current_version[0] = FIRMWARE_VERSION_MAJOR;
  g_status.current_version[1] = FIRMWARE_VERSION_MINOR;
  g_status.current_version[2] = FIRMWARE_VERSION_PATCH;

  /* Create event group */
  g_ota_event_group = xEventGroupCreate();
  if (!g_ota_event_group) {
    ESP_LOGE(TAG, "Failed to create event group");
    return false;
  }

  /* Start auto-check task if interval is set */
  if (g_config.check_interval_ms > 0) {
    BaseType_t ret = xTaskCreate(ota_check_task, "ota_check", 4096, NULL, 5, NULL);

    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Failed to create OTA check task");
      vEventGroupDelete(g_ota_event_group);
      return false;
    }

    ESP_LOGI(TAG,
             "Auto-update check enabled (interval: %" PRIu32 " ms)",
             g_config.check_interval_ms);
  }

  g_initialized = true;

  ESP_LOGI(TAG,
           "OTA manager initialized (version %d.%d.%d)",
           FIRMWARE_VERSION_MAJOR,
           FIRMWARE_VERSION_MINOR,
           FIRMWARE_VERSION_PATCH);

  return true;
}

#ifdef CONFIG_PYNQ_OTA_VERSION_CHECK_URL
/**
 * @brief Compare two versions (major.minor.patch)
 * @return 1 if v1 > v2, -1 if v1 < v2, 0 if equal
 */
static int compare_versions(const uint8_t* v1, const uint8_t* v2)
{
  for (uint32_t i = 0; i < 3; i++) {
    if (v1[i] > v2[i])
      return 1;
    if (v1[i] < v2[i])
      return -1;
  }
  return 0;
}

/**
 * @brief HTTP event handler for version check
 */
static esp_err_t version_check_http_event_handler(esp_http_client_event_t* evt)
{
  static char*   output_buffer = NULL;
  static int32_t output_len    = 0;

  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
      /* Accumulate response data */
      if (!esp_http_client_is_chunked_response(evt->client)) {
        if (output_buffer == NULL) {
          output_buffer = (char*)malloc(esp_http_client_get_content_length(evt->client) + 1);
          output_len    = 0;
        }
        if (output_buffer != NULL) {
          memcpy(output_buffer + output_len, evt->data, evt->data_len);
          output_len += evt->data_len;
          output_buffer[output_len] = '\0';
        }
      }
      break;

    case HTTP_EVENT_ON_FINISH:
      if (output_buffer != NULL) {
        /* Store in user_data for later processing */
        *(char**)evt->user_data = output_buffer;
        output_buffer           = NULL;
        output_len              = 0;
      }
      break;

    case HTTP_EVENT_DISCONNECTED:
      if (output_buffer != NULL) {
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;

    default:
      break;
  }

  return ESP_OK;
}
#endif /* CONFIG_PYNQ_OTA_VERSION_CHECK_URL */

bool ota_manager_check_update(void)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "OTA manager not initialized");
    return false;
  }

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGW(TAG, "WiFi not connected - cannot check for updates");
    return false;
  }

#ifdef CONFIG_PYNQ_OTA_VERSION_CHECK_URL
  ESP_LOGI(TAG, "Checking for firmware updates from: %s", CONFIG_PYNQ_OTA_VERSION_CHECK_URL);

  char* response_buffer = NULL;

  /* Configure HTTP client */
  esp_http_client_config_t config = {
    .url           = CONFIG_PYNQ_OTA_VERSION_CHECK_URL,
    .event_handler = version_check_http_event_handler,
    .user_data     = &response_buffer,
    .timeout_ms    = 5000,
#ifdef CONFIG_PYNQ_OTA_HTTPS_CERT_VERIFICATION
    .crt_bundle_attach = esp_crt_bundle_attach,
#else
    .skip_cert_common_name_check = true,
#endif
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return false;
  }

  /* Perform HTTP GET request */
  esp_err_t err = esp_http_client_perform(client);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    if (response_buffer)
      free(response_buffer);
    return false;
  }

  int32_t status_code = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (status_code != 200) {
    ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
    if (response_buffer)
      free(response_buffer);
    return false;
  }

  if (response_buffer == NULL) {
    ESP_LOGE(TAG, "No response data received");
    return false;
  }

  ESP_LOGI(TAG, "Received version info: %s", response_buffer);

  /* Parse JSON response */
  cJSON* json = cJSON_Parse(response_buffer);
  free(response_buffer);

  if (json == NULL) {
    ESP_LOGE(TAG, "Failed to parse JSON response");
    return false;
  }

  /* Extract version string (e.g., "0.2.0") */
  cJSON* version_item = cJSON_GetObjectItem(json, "version");
  if (!cJSON_IsString(version_item)) {
    ESP_LOGE(TAG, "Missing or invalid 'version' field in JSON");
    cJSON_Delete(json);
    return false;
  }

  const char* version_str = version_item->valuestring;

  /* Parse version string into major.minor.patch */
  uint8_t new_version[3] = {0, 0, 0};
  if (sscanf(version_str, "%hhu.%hhu.%hhu", &new_version[0], &new_version[1], &new_version[2]) !=
      3) {
    ESP_LOGE(TAG, "Invalid version format: %s (expected X.Y.Z)", version_str);
    cJSON_Delete(json);
    return false;
  }

  ESP_LOGI(TAG,
           "Server version: %d.%d.%d, Current version: %d.%d.%d",
           new_version[0],
           new_version[1],
           new_version[2],
           g_status.current_version[0],
           g_status.current_version[1],
           g_status.current_version[2]);

  /* Compare versions */
  int32_t comparison = compare_versions(new_version, g_status.current_version);

  if (comparison > 0) {
    /* New version available */
    g_status.update_available = true;
    memcpy(g_status.new_version, new_version, sizeof(new_version));

    /* Extract and store update URL */
    cJSON* url_item = cJSON_GetObjectItem(json, "url");
    if (cJSON_IsString(url_item)) {
      strncpy(g_status.update_url, url_item->valuestring, sizeof(g_status.update_url) - 1);
      g_status.update_url[sizeof(g_status.update_url) - 1] = '\0';
      ESP_LOGI(TAG, "Update URL: %s", g_status.update_url);
    } else {
      ESP_LOGW(TAG, "No URL in version response, using default");
      strncpy(g_status.update_url, g_config.update_url, sizeof(g_status.update_url) - 1);
      g_status.update_url[sizeof(g_status.update_url) - 1] = '\0';
    }

    /* Extract and store SHA256 hash - CRITICAL FOR AUTO-UPDATE! */
    cJSON* sha256_item = cJSON_GetObjectItem(json, "sha256");
    if (cJSON_IsString(sha256_item) && strlen(sha256_item->valuestring) == 64) {
      strncpy(g_status.update_sha256, sha256_item->valuestring, sizeof(g_status.update_sha256) - 1);
      g_status.update_sha256[64] = '\0';
      ESP_LOGI(TAG, "Update SHA256: %s", g_status.update_sha256);
    } else {
      g_status.update_sha256[0] = '\0'; /* Empty = skip verification */
      ESP_LOGW(TAG, "No SHA256 hash in version response!");
      ESP_LOGW(TAG, "Auto-update will proceed WITHOUT hash verification!");
    }

    ESP_LOGI(TAG,
             "Firmware update available: %d.%d.%d",
             new_version[0],
             new_version[1],
             new_version[2]);
    cJSON_Delete(json);
    return true;
  } else if (comparison == 0) {
    ESP_LOGI(TAG, "Firmware is up to date");
  } else {
    ESP_LOGW(TAG, "Server version is older than current version");
  }

  g_status.update_available = false;
  cJSON_Delete(json);
  return false;

#else
  ESP_LOGW(TAG, "Version check URL not configured - cannot check for updates");
  g_status.update_available = false;
  return false;
#endif
}

bool ota_manager_start_update(const char* url)
{
  return ota_manager_start_update_verified(url, NULL, false);
}

bool ota_manager_start_update_verified(const char* url,
                                       const char* expected_sha256,
                                       bool        allow_downgrade)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "OTA manager not initialized");
    return false;
  }

  if (!url) {
    ESP_LOGE(TAG, "Invalid URL");
    return false;
  }

  if (g_status.state != k_ota_idle) {
    ESP_LOGW(TAG, "Update already in progress");
    return false;
  }

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    g_status.state      = k_ota_failed;
    g_status.error_code = k_status_ota_wifi_disconnected;
    return false;
  }

  /* Allocate parameters structure */
  ota_update_params_t* params = (ota_update_params_t*)calloc(1, sizeof(ota_update_params_t));
  if (!params) {
    ESP_LOGE(TAG, "Failed to allocate memory for OTA parameters");
    return false;
  }

  /* Duplicate URL string */
  params->url = strdup(url);
  if (!params->url) {
    ESP_LOGE(TAG, "Failed to allocate memory for URL");
    ota_free_params(params);
    return false;
  }

  /* Duplicate SHA256 hash if provided */
  if (expected_sha256 && strlen(expected_sha256) > 0) {
    params->expected_sha256 = strdup(expected_sha256);
    if (!params->expected_sha256) {
      ESP_LOGE(TAG, "Failed to allocate memory for SHA256 hash");
      ota_free_params(params);
      return false;
    }
  } else {
    params->expected_sha256 = NULL;
  }

  params->allow_downgrade = allow_downgrade;

  /* Clear cancel bit */
  xEventGroupClearBits(g_ota_event_group, OTA_CANCEL_BIT);

  /* Create OTA update task */
  BaseType_t ret = xTaskCreate(ota_update_task, "ota_update", 8192, params, 5, &g_ota_task_handle);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create OTA update task");
    ota_free_params(params);
    return false;
  }

  return true;
}

bool ota_manager_get_status(ota_status_t* status)
{
  if (!g_initialized || !status) {
    return false;
  }

  memcpy(status, &g_status, sizeof(ota_status_t));
  return true;
}

void ota_manager_cancel_update(void)
{
  if (!g_initialized) {
    return;
  }

  if (g_status.state == k_ota_downloading || g_status.state == k_ota_verifying) {
    ESP_LOGW(TAG, "Cancelling OTA update");
    xEventGroupSetBits(g_ota_event_group, OTA_CANCEL_BIT);
  }
}

void ota_manager_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing OTA manager");

  /* Cancel any ongoing update */
  ota_manager_cancel_update();

  /* Wait for task to finish */
  if (g_ota_task_handle) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  if (g_ota_event_group) {
    vEventGroupDelete(g_ota_event_group);
    g_ota_event_group = NULL;
  }

  g_initialized = false;

  ESP_LOGI(TAG, "OTA manager deinitialized");
}

void ota_manager_get_version(uint8_t* major, uint8_t* minor, uint8_t* patch)
{
  if (major)
    *major = FIRMWARE_VERSION_MAJOR;
  if (minor)
    *minor = FIRMWARE_VERSION_MINOR;
  if (patch)
    *patch = FIRMWARE_VERSION_PATCH;
}
