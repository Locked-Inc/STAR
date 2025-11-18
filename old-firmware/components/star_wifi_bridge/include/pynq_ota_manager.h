/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_ota_manager.h */

#ifndef PYNQ_OTA_MANAGER_H
#define PYNQ_OTA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update state
 */
typedef enum {
  k_ota_idle = 0,    /* No update in progress */
  k_ota_checking,    /* Checking for updates */
  k_ota_downloading, /* Downloading firmware */
  k_ota_verifying,   /* Verifying downloaded firmware */
  k_ota_installing,  /* Installing firmware */
  k_ota_complete,    /* Update complete, ready to reboot */
  k_ota_failed,      /* Update failed */
} ota_state_t;

/**
 * @brief OTA configuration
 */
typedef struct {
  const char* update_url;        /* URL to check for updates */
  uint32_t    check_interval_ms; /* How often to check for updates */
  bool        auto_update;       /* Automatically download and install updates */
  bool        auto_reboot;       /* Automatically reboot after update */
} ota_config_t;

/**
 * @brief OTA status information
 */
typedef struct {
  ota_state_t state;              /* Current OTA state */
  uint8_t     progress;           /* Progress percentage (0-100) */
  uint32_t    bytes_downloaded;   /* Bytes downloaded so far */
  uint32_t    total_bytes;        /* Total bytes to download */
  bool        update_available;   /* Is an update available? */
  uint8_t     current_version[3]; /* Current firmware version (major.minor.patch) */
  uint8_t     new_version[3];     /* New firmware version */
  char        update_url[256];    /* Update URL from version check */
  char        update_sha256[65];  /* SHA256 hash from version check */
  uint8_t     error_code;         /* Detailed error code (protocol_status_t) if state is failed */
} ota_status_t;

/**
 * @brief Initialize OTA manager
 * @param config OTA configuration
 * @return true on success, false on failure
 */
bool ota_manager_init(const ota_config_t* config);

/**
 * @brief Check if update is available
 * @return true if update available, false otherwise
 */
bool ota_manager_check_update(void);

/**
 * @brief Start OTA update from URL
 * @param url URL of firmware binary
 * @return true if update started, false on failure
 */
bool ota_manager_start_update(const char* url);

/**
 * @brief Start OTA update from URL with SHA256 verification
 * @param url URL of firmware binary
 * @param expected_sha256 Expected SHA256 hash (64 hex characters), or NULL to skip verification
 * @param allow_downgrade If true, allow installing older firmware versions
 * @return true if update started, false on failure
 */
bool ota_manager_start_update_verified(const char* url,
                                       const char* expected_sha256,
                                       bool        allow_downgrade);

/**
 * @brief Get current OTA status
 * @param status Output status structure
 * @return true on success, false on failure
 */
bool ota_manager_get_status(ota_status_t* status);

/**
 * @brief Cancel ongoing OTA update
 */
void ota_manager_cancel_update(void);

/**
 * @brief Deinitialize OTA manager
 */
void ota_manager_deinit(void);

/**
 * @brief Get current firmware version
 * @param major Output major version
 * @param minor Output minor version
 * @param patch Output patch version
 */
void ota_manager_get_version(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* PYNQ_OTA_MANAGER_H */
