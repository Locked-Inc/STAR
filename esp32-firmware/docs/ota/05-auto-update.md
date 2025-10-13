# Auto-Update with SHA256 Hash

## Question: When the ESP32 is set to auto-update, how does this work with getting the hash of the file?

When auto-update is enabled, the ESP32 periodically queries a version API endpoint to check for new firmware. The server must provide the SHA256 hash in its JSON response.

## Auto-Update Flow

```
+-------------------------------------------------------------+
| Auto-Update Process (Every Hour by Default)                |
+-------------------------------------------------------------+
|                                                             |
| 1. Timer Expires (e.g., 1 hour since last check)          |
|    +-> Auto-check task wakes up                            |
|                                                             |
| 2. Check Prerequisites                                      |
|    +-> WiFi connected? [OK]                                   |
|    +-> No update in progress? [OK]                            |
|                                                             |
| 3. HTTP GET to Version API                                 |
|    GET https://robot-backend.com/api/esp32/version         |
|    v                                                        |
|    Response (JSON):                                         |
|    {                                                        |
|      "version": "0.2.0",                                    |
|      "url": "https://example.com/firmware-v0.2.0.bin",     |
|      "sha256": "a3f5e8b1..."  <-- Hash provided by server! |
|    }                                                        |
|                                                             |
| 4. Parse JSON and Compare Versions                         |
|    Server: 0.2.0 > Current: 0.1.0                          |
|    +-> Update available!                                    |
|                                                             |
| 5. Store URL and SHA256                                    |
|    g_status.update_url = "https://..."                     |
|    g_status.update_sha256 = "a3f5e8b1..."                  |
|                                                             |
| 6. Start Verified Update                                   |
|    ota_manager_start_update_verified(                      |
|      url = g_status.update_url,                            |
|      sha256 = g_status.update_sha256,  <-- From version API|
|      allow_downgrade = false            <-- No downgrade   |
|    )                                                        |
|                                                             |
| 7. Download, Verify, Install                               |
|    +-> SHA256 is checked during installation               |
|                                                             |
| 8. Reboot to New Firmware                                  |
|    +-> ESP32 now running v0.2.0                            |
|                                                             |
+-------------------------------------------------------------+
```

## Backend Version API Implementation

Your server MUST include the SHA256 hash in the version response.

### Example Backend (Python/Flask)

```python
from flask import Flask, jsonify, send_file
import hashlib
import os

app = Flask(__name__)

# Firmware database
FIRMWARE_DIR = '/var/www/firmware/'
FIRMWARE_DB = {
    "0.1.0": {
        "file": "esp32-firmware-v0.1.0.bin",
        "sha256": None,  # Will be calculated
        "release_date": "2025-01-10",
        "changelog": "Initial release"
    },
    "0.2.0": {
        "file": "esp32-firmware-v0.2.0.bin",
        "sha256": None,
        "release_date": "2025-01-15",
        "changelog": "Added SHA256 verification and auto-update"
    }
}

def calculate_sha256(filepath):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        # Read in 8KB chunks for memory efficiency
        for byte_block in iter(lambda: f.read(8192), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

# Pre-calculate hashes on startup
for version, info in FIRMWARE_DB.items():
    filepath = os.path.join(FIRMWARE_DIR, info['file'])
    if os.path.exists(filepath):
        info['sha256'] = calculate_sha256(filepath)
        print(f"Firmware v{version}: {info['sha256']}")

@app.route('/api/esp32/version')
def get_version():
    """
    Version check endpoint for ESP32 auto-update

    Returns JSON with latest version info including SHA256
    """
    latest_version = "0.2.0"  # Could be dynamic
    info = FIRMWARE_DB[latest_version]

    return jsonify({
        "version": latest_version,
        "url": f"https://robot-backend.com/firmware/{info['file']}",
        "sha256": info['sha256'],  # REQUIRED for secure auto-update!
        "min_version": "0.1.0",
        "release_date": info['release_date'],
        "changelog": info['changelog']
    })

@app.route('/firmware/<filename>')
def download_firmware(filename):
    """
    Firmware download endpoint
    """
    filepath = os.path.join(FIRMWARE_DIR, filename)
    if os.path.exists(filepath):
        return send_file(filepath, as_attachment=True)
    else:
        return "Firmware not found", 404

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=443, ssl_context='adhoc')
```

### Generate SHA256 Hash for Your Firmware

```bash
# On your server, after building firmware:
sha256sum esp32-firmware.bin
# Output: a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Store this hash in your database or return it from the version API
```

## ESP32 Implementation (Auto-Update Task)

From `pynq_ota_manager.c:304-335`:

```c
/**
 * @brief Auto-update check task
 */
static void ota_check_task(void* pvParameters)
{
  (void)pvParameters;

  while (1) {
    /* Wait for check interval (e.g., 1 hour) */
    vTaskDelay(pdMS_TO_TICKS(g_config.check_interval_ms));

    /* Only check if WiFi is connected and no update in progress */
    if (wifi_manager_get_status() != k_wifi_connected) {
      continue;
    }

    if (g_status.state != k_ota_idle) {
      continue;
    }

    ESP_LOGI(TAG, "Checking for updates...");

    /* Check for update - This does HTTP GET to version API */
    bool available = ota_manager_check_update();

    if (available && g_config.auto_update) {
      ESP_LOGI(TAG, "Update available - starting automatic update");

      /* CURRENT IMPLEMENTATION - Missing hash! */
      ota_manager_start_update(g_config.update_url);  // [ERROR] No SHA256
    }
  }
}
```

## Current Gap and Fix

### Problem

The current auto-update implementation doesn't pass the SHA256 hash:

```c
// Current code (pynq_ota_manager.c:327)
ota_manager_start_update(g_config.update_url);  // No hash passed!
```

### Solution

We need to store the hash from the version check and use it:

**Step 1: Extend ota_status_t to store server response**

```c
// In pynq_ota_manager.c, modify g_status structure:
typedef struct {
  ota_state_t state;
  uint8_t     progress;
  uint32_t    bytes_downloaded;
  uint32_t    total_bytes;
  bool        update_available;
  uint8_t     current_version[3];
  uint8_t     new_version[3];
  char        update_url[256];    // NEW: Store URL from server
  char        update_sha256[65];  // NEW: Store SHA256 from server
} ota_status_t;
```

**Step 2: Store hash during version check**

```c
// In ota_manager_check_update() function:
bool ota_manager_check_update(void)
{
  // ... existing HTTP GET and JSON parsing code ...

  if (comparison > 0) {
    /* New version available */
    g_status.update_available = true;
    memcpy(g_status.new_version, new_version, sizeof(new_version));

    /* Extract and store update URL */
    cJSON* url_item = cJSON_GetObjectItem(json, "url");
    if (cJSON_IsString(url_item)) {
      strncpy(g_status.update_url, url_item->valuestring,
              sizeof(g_status.update_url) - 1);
      g_status.update_url[sizeof(g_status.update_url) - 1] = '\0';
      ESP_LOGI(TAG, "Update URL: %s", g_status.update_url);
    } else {
      ESP_LOGW(TAG, "No URL in version response, using default");
      strncpy(g_status.update_url, g_config.update_url,
              sizeof(g_status.update_url) - 1);
    }

    /* Extract and store SHA256 hash - CRITICAL FOR AUTO-UPDATE! */
    cJSON* sha256_item = cJSON_GetObjectItem(json, "sha256");
    if (cJSON_IsString(sha256_item) &&
        strlen(sha256_item->valuestring) == 64) {
      strncpy(g_status.update_sha256, sha256_item->valuestring,
              sizeof(g_status.update_sha256) - 1);
      g_status.update_sha256[64] = '\0';
      ESP_LOGI(TAG, "Update SHA256: %s", g_status.update_sha256);
    } else {
      g_status.update_sha256[0] = '\0';  // Empty = skip verification
      ESP_LOGW(TAG, "No SHA256 hash in version response!");
      ESP_LOGW(TAG, "Auto-update will proceed WITHOUT hash verification!");
    }

    cJSON_Delete(json);
    return true;
  }

  // ... rest of function ...
}
```

**Step 3: Use stored hash in auto-update**

```c
// In ota_check_task() function:
if (available && g_config.auto_update) {
  ESP_LOGI(TAG, "Update available - starting automatic update");

  /* Use stored URL and SHA256 from version check */
  const char* url = (strlen(g_status.update_url) > 0) ?
                    g_status.update_url : g_config.update_url;

  const char* sha256 = (strlen(g_status.update_sha256) == 64) ?
                       g_status.update_sha256 : NULL;

  if (sha256) {
    ESP_LOGI(TAG, "Auto-update will verify SHA256: %.16s...", sha256);
  } else {
    ESP_LOGW(TAG, "Auto-update proceeding WITHOUT hash verification!");
  }

  /* Start verified update with hash */
  ota_manager_start_update_verified(url, sha256, false);
}
```

## Configuration via Kconfig

Enable auto-update in `idf.py menuconfig`:

```
PYNQ WiFi Bridge Configuration
  +-> OTA Update Configuration
      +-> OTA Update URL: https://robot-backend.com/api/esp32/firmware.bin
      +-> OTA Version Check URL: https://robot-backend.com/api/esp32/version
      +-> [*] Enable Automatic OTA Updates  <-- Enable this
      +-> (3600000) OTA Update Check Interval (milliseconds)  <-- 1 hour
      +-> [*] Automatically Reboot After Update
      +-> [ ] Enable HTTPS Certificate Verification
```

Generates these defines:

```c
// In sdkconfig.h:
#define CONFIG_PYNQ_OTA_AUTO_UPDATE 1
#define CONFIG_PYNQ_OTA_CHECK_INTERVAL_MS 3600000
#define CONFIG_PYNQ_OTA_UPDATE_URL "https://robot-backend.com/api/esp32/firmware.bin"
#define CONFIG_PYNQ_OTA_VERSION_CHECK_URL "https://robot-backend.com/api/esp32/version"
```

## Complete Auto-Update Flow Example

```
Time: 00:00 - ESP32 boots
+-> Firmware version: 0.1.0
+-> Auto-update enabled: YES
+-> Check interval: 1 hour
+-> Next check at: 01:00

Time: 01:00 - Auto-check timer expires
+-> WiFi status: Connected [OK]
+-> OTA state: Idle [OK]
+-> Initiating version check...

HTTP GET https://robot-backend.com/api/esp32/version
v
Server Response:
{
  "version": "0.2.0",
  "url": "https://robot-backend.com/firmware/esp32-v0.2.0.bin",
  "sha256": "a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d"
}
v
Version Comparison:
+-> Server: 0.2.0
+-> Current: 0.1.0
+-> Server > Current [OK] Update available!

Store in g_status:
+-> update_url = "https://robot-backend.com/firmware/esp32-v0.2.0.bin"
+-> update_sha256 = "a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d"

Start Verified Update:
+-> ota_manager_start_update_verified(url, sha256, false)

Time: 01:01 - Downloading firmware
+-> Progress: 0%
+-> Progress: 25%
+-> Progress: 50%
+-> Progress: 75%
+-> Progress: 100% (995,000 bytes)

Time: 01:02 - Verifying SHA256
+-> Reading firmware from flash...
+-> Calculating hash...
+-> Expected: a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d
+-> Actual:   a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3b5c7e9b1a3c5d7f9a1b3c5e7d
+-> Match! [OK]

Time: 01:03 - Installing firmware
+-> Marking ota_1 as bootable
+-> Update complete!

Time: 01:03 - Auto-reboot
+-> Rebooting in 3 seconds...

Time: 01:04 - ESP32 boots
+-> Bootloader loads from ota_1
+-> Firmware version: 0.2.0 [OK]
+-> Auto-update successful!

Time: 02:04 - Next auto-check
+-> Check for updates
+-> Server: 0.2.0, Current: 0.2.0
+-> Already up to date, no action needed
```

## Security Considerations

### Why SHA256 is Critical for Auto-Update

**Without hash verification:**
- Attacker could intercept HTTP request
- Server could be compromised
- Firmware could be corrupted during transfer
- ESP32 would install malicious/broken firmware

**With hash verification:**
- Downloaded firmware MUST match expected hash
- Man-in-the-middle attacks are detected
- Corrupted downloads are rejected
- ESP32 only installs verified firmware

### Best Practices

1. **Always provide SHA256 in version API**
   ```json
   {
     "version": "0.2.0",
     "sha256": "a3f5e8b1..."  // Required!
   }
   ```

2. **Use HTTPS for version API**
   - Enable `CONFIG_PYNQ_OTA_HTTPS_CERT_VERIFICATION=y` in production
   - Embed CA certificate in firmware

3. **Verify hash on server side too**
   - Calculate hash after building firmware
   - Store in database
   - Return same hash in version API

4. **Never skip hash verification in auto-update**
   - Manual update via PYNQ can skip if needed
   - Auto-update should ALWAYS verify

## Summary

**Q: How does auto-update get the SHA256 hash?**
**A:** From the version check API JSON response. Server must include `"sha256": "..."` field.

**Q: Where is the hash stored?**
**A:** In `g_status.update_sha256` after parsing the version API response.

**Q: Is hash verification optional?**
**A:**
- For **manual updates**: Optional (PYNQ can omit SHA256)
- For **auto-updates**: Should be REQUIRED (security)

**Q: What if server doesn't provide hash?**
**A:** Auto-update proceeds WITHOUT verification (logged as warning). This is unsafe for production!

**Current Status:** Needs the fix described above to properly use SHA256 in auto-updates.
