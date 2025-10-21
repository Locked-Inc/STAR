# SHA256 Hash Verification - Deep Dive

## Question: The SHA256 check is done on the ESP32, right? How does this work exactly? When we download the update, where does it temporarily get saved before we check the hash?

Yes, the SHA256 verification happens **entirely on the ESP32**. Here's the complete technical explanation.

## Flash Memory Architecture

```
+-------------------------------------------------------------+
| ESP32 Flash Memory Layout (4MB Total)                      |
+-------------------------------------------------------------+
| Bootloader          | 32 KB   | First boot code           |
| Partition Table     | 8 KB    | Partition layout          |
| NVS                 | 16 KB   | WiFi credentials, settings|
| ota_0 (Slot A)      | 1024 KB | <-- Currently Running!    |
| ota_1 (Slot B)      | 1024 KB | <-- Download Target       |
| (Unused)            | ~1.9 MB | Reserved for future       |
+-------------------------------------------------------------+
```

## The Critical Insight: Direct-to-Flash Writing

**The firmware is NOT stored in RAM during download!**

This is crucial because:
- ESP32 has only 520 KB of RAM
- Firmware is ~1000 KB (larger than available RAM!)
- **Solution**: Write directly to flash memory during download

## Step-by-Step Download and Verification Process

### Phase 1: Download (Streaming to Flash)

```
ESP32 Running from ota_0          ESP32 Flash Memory
+------------------+              +------------------+
|  Main App        |              |  ota_0: v0.1.0   |
|  (Running)       |              |  [============]  |
|                  |              |                  |
|  WiFi Stack      |              |  ota_1: empty    |
|  (Active)        |              |  [            ]  |
|                  |              +------------------+
|  OTA Task        |                       v
|  (Background)    |              +------------------+
|    v             |              |  Download chunk  |
|  Download 4KB ---+--------------->  Write to flash  |
|    v             |              |  ota_1[0..4095]  |
|  Download 4KB ---+--------------->  Write to flash  |
|    v             |              |  ota_1[4096..]   |
|  Download 4KB ---+--------------->  Write to flash  |
|    v             |              |  ota_1[8192..]   |
|  ...             |              |  ...             |
+------------------+              +------------------+

Key Points:
- Each 4KB chunk is written DIRECTLY to flash
- Chunks are written sequentially to ota_1 partition
- Main app continues running from ota_0
- Only ~8KB of RAM used for download buffer
```

From `pynq_ota_manager.c:200-231`:

```c
/* Download and write firmware */
while (1) {
    // This function downloads 1-4KB chunks and writes them
    // DIRECTLY to the ota_1 partition in flash memory
    ret = esp_https_ota_perform(ota_handle);

    // Each call:
    // 1. Receives chunk from network (into ~4KB RAM buffer)
    // 2. Writes chunk to flash partition
    // 3. Frees RAM buffer
    // 4. Repeats for next chunk

    if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        break;  // Download complete
    }

    /* Update progress */
    int downloaded = esp_https_ota_get_image_len_read(ota_handle);
    g_status.bytes_downloaded = downloaded;
    g_status.progress = (uint8_t)((downloaded * 100) / g_status.total_bytes);
}
```

### Phase 2: SHA256 Verification (Reading from Flash)

After download completes, the firmware is already in flash. Now we verify it:

```
ESP32 Flash Memory                ESP32 RAM
+------------------+              +------------------+
|  ota_0: v0.1.0   |              |  SHA256 Context  |
|  [============]  |              |  (256 bytes)     |
|                  |              |                  |
|  ota_1: v0.2.0   |              |  Read Buffer     |
|  [============]  |--------------->  (1 KB)          |
|  (just downloaded|              |                  |
|   to flash)      |              |  Hash Engine     |
+------------------+              |  (mbedtls)       |
        |                         +------------------+
        |                                  |
        +----------------------------------+
         Read 1KB chunks and hash them
```

From `pynq_ota_manager.c:256-269`:

```c
/* Verify SHA256 hash if provided */
if (params->expected_sha256 && strlen(params->expected_sha256) > 0) {
    ESP_LOGI(TAG, "Verifying SHA256 hash...");

    // This function reads the firmware BACK from flash
    // and calculates its hash
    if (!verify_sha256(ota_handle, params->expected_sha256)) {
        ESP_LOGE(TAG, "SHA256 verification failed - aborting update");
        esp_https_ota_abort(ota_handle);

        // Firmware stays in ota_1 but is NOT marked bootable
        // ESP32 will continue booting from ota_0
        g_status.state = k_ota_failed;
        return;
    }
}
```

### Detailed SHA256 Calculation (pynq_ota_manager.c:65-133)

```c
static bool verify_sha256(esp_https_ota_handle_t ota_handle, const char* expected_sha256)
{
    /* Get the partition where firmware was written */
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    // This returns ota_1 partition handle

    /* Initialize SHA256 context */
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)

    /* Read the downloaded firmware back from flash in 1KB chunks */
    uint8_t buffer[1024];  // Only 1KB of RAM needed!
    size_t image_size = esp_https_ota_get_image_len_read(ota_handle);

    for (size_t offset = 0; offset < image_size; offset += sizeof(buffer)) {
        size_t to_read = (image_size - offset) < sizeof(buffer) ?
                         (image_size - offset) : sizeof(buffer);

        /* Read from flash partition */
        ret = esp_partition_read(update_partition, offset, buffer, to_read);

        /* Update running hash with this chunk */
        mbedtls_sha256_update(&ctx, buffer, to_read);
        // After this, buffer can be reused for next chunk
    }

    /* Finalize hash calculation */
    uint8_t sha256[32];
    mbedtls_sha256_finish(&ctx, sha256);

    /* Convert calculated hash to hex string */
    char calculated_sha256[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&calculated_sha256[i * 2], "%02x", sha256[i]);
    }
    calculated_sha256[64] = '\0';

    /* Compare hashes (case-insensitive) */
    for (int i = 0; i < 64; i++) {
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
```

## Memory Usage Breakdown

### During Download

```
+----------------------------------------+
| RAM Usage (while downloading):        |
+----------------------------------------+
| Main application:      ~200 KB        |
| WiFi stack:            ~80 KB         |
| OTA task stack:        8 KB           |
| HTTP download buffer:  4 KB (temp)    |
| TLS buffers:           ~16 KB         |
+----------------------------------------+
| Total:                 ~308 KB        |
| Free:                  ~212 KB        |
+----------------------------------------+

+----------------------------------------+
| Flash Usage:                           |
+----------------------------------------+
| ota_0 partition:       995 KB (old)   |
| ota_1 partition:       0-995 KB <---- |
|                        (streaming in)  |
+----------------------------------------+
```

### During SHA256 Verification

```
+----------------------------------------+
| RAM Usage (while verifying):          |
+----------------------------------------+
| Main application:      ~200 KB        |
| WiFi stack:            ~80 KB         |
| OTA task stack:        8 KB           |
| SHA256 context:        256 bytes      |
| Read buffer:           1 KB           |
+----------------------------------------+
| Total:                 ~289 KB        |
| Free:                  ~231 KB        |
+----------------------------------------+

+----------------------------------------+
| Flash Usage:                           |
+----------------------------------------+
| ota_0 partition:       995 KB (old)   |
| ota_1 partition:       995 KB (new)   |
|                        (reading back)  |
+----------------------------------------+
```

## What Happens on Hash Mismatch?

```
+---------------------------------------------+
| Scenario: SHA256 Verification Fails         |
+---------------------------------------------+
|                                             |
| 1. Download completes to ota_1 partition   |
|    +-> Firmware written: 995 KB            |
|    +-> Download progress: 100%             |
|                                             |
| 2. Read back and calculate hash            |
|    +-> Expected: a3f5e8b1c2d4...           |
|    +-> Actual:   b4f6e9c3d5a7...  [ERROR]       |
|                                             |
| 3. Hash mismatch detected!                 |
|    +-> Log error message                   |
|    +-> Call esp_https_ota_abort()          |
|    +-> Set state = OTA_FAILED              |
|    +-> Do NOT mark ota_1 as bootable       |
|                                             |
| 4. Result:                                  |
|    +-> ESP32 continues running from ota_0  |
|    +-> ota_1 contains bad firmware         |
|    +-> Next OTA will overwrite ota_1       |
|    +-> No reboot happens                   |
|                                             |
| 5. Raspberry Pi sees state = OTA_FAILED             |
|    +-> Can retry with correct hash          |
|                                             |
+---------------------------------------------+
```

## What Happens on Hash Match?

```
+---------------------------------------------+
| Scenario: SHA256 Verification Passes        |
+---------------------------------------------+
|                                             |
| 1. Download completes to ota_1 partition   |
|    +-> Firmware written: 995 KB            |
|                                             |
| 2. Calculate and verify hash                |
|    +-> Expected: a3f5e8b1c2d4...           |
|    +-> Actual:   a3f5e8b1c2d4...  [SUCCESS]       |
|    +-> Match!                               |
|                                             |
| 3. Install firmware                         |
|    +-> Call esp_https_ota_finish()         |
|    +-> Mark ota_1 as bootable              |
|    +-> Update partition metadata           |
|    +-> Set state = OTA_COMPLETE            |
|                                             |
| 4. Auto-reboot (if enabled)                 |
|    +-> Wait 3 seconds                       |
|    +-> Call esp_restart()                  |
|                                             |
| 5. Bootloader runs                          |
|    +-> Read partition table                 |
|    +-> Check which partition is bootable   |
|    +-> Boot from ota_1 (new firmware!)     |
|                                             |
| 6. New firmware running                     |
|    +-> Version is now 0.2.0                |
|                                             |
+---------------------------------------------+
```

## Why This Design is Brilliant

### 1. Memory Efficient
- **1MB firmware doesn't need 1MB RAM**
- Only 4KB buffer needed during download
- Only 1KB buffer needed during verification
- Entire process uses <10KB of RAM

### 2. Resilient
- Downloaded firmware can be verified without re-downloading
- If hash fails, just mark it invalid (no reboot needed)
- Can retry OTA without losing old firmware

### 3. Safe
- Old firmware stays intact until new one is verified
- Bootloader has automatic rollback (3 failed boots -> revert)
- Never in a state where device is unbootable

### 4. Fast
- No need to copy data between partitions
- Direct write to target partition
- Streaming verification (no second download)

## Summary

**Question**: Where does the firmware get saved temporarily?
**Answer**: It's written **directly to the ota_1 flash partition** during download. There is no "temporary" storage - it goes straight to its final destination.

**Question**: When is the hash checked?
**Answer**: **After** the download completes, the ESP32 reads the firmware back from flash and calculates the SHA256 hash.

**Question**: What if the hash fails?
**Answer**: The firmware stays in ota_1, but is **never marked as bootable**. ESP32 continues booting from ota_0. Next OTA update will overwrite the bad firmware.

**Question**: How much RAM does this use?
**Answer**: Only **~10KB** - a tiny download buffer (4KB) and hash calculation buffer (1KB). The 1MB firmware never enters RAM!
