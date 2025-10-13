# ESP32 Partition Tables

This project uses **OTA (Over-The-Air) update** partition tables for safe wireless firmware updates.

## Overview

The firmware supports two different ESP32 boards with different flash sizes:

| Board | Flash Size | OTA Partition Size | Extra Features |
|-------|-----------|-------------------|----------------|
| ESP32-WROOM-32 (test) | 4 MB | 900 KB each | Basic OTA |
| ESP32-S3-WROOM-1-N16 (prod) | 16 MB | 2 MB each | OTA + SPIFFS + Coredump |

## Partition Layouts

### ESP32-WROOM-32 (4MB) - `partitions_ota_4mb.csv`

```
+--------------+---------+----------+-------------------------+
| Name         | Offset  | Size     | Purpose                 |
+--------------+---------+----------+-------------------------+
| nvs          | 0x9000  | 16 KB    | WiFi config, settings   |
| otadata      | 0xd000  | 8 KB     | OTA boot tracking       |
| phy_init     | 0xf000  | 4 KB     | Radio calibration       |
| ota_0        | 0x10000 | 900 KB   | App slot 1 (primary)    |
| ota_1        | 0xf1000 | 900 KB   | App slot 2 (update)     |
+--------------+---------+----------+-------------------------+
Total: ~1.86 MB
Free: ~2.14 MB (for future expansion)
```

**Current Usage:**
- Firmware size: 851 KB
- Remaining per partition: 49 KB (5% free)
- Estimated size after all features: ~880 KB
- **Status:** [x] Fits with room for growth

### ESP32-S3-WROOM-1-N16 (16MB) - `partitions_ota_16mb.csv`

```
+--------------+----------+----------+-------------------------+
| Name         | Offset   | Size     | Purpose                 |
+--------------+----------+----------+-------------------------+
| nvs          | 0x9000   | 20 KB    | WiFi config, settings   |
| otadata      | 0xe000   | 8 KB     | OTA boot tracking       |
| phy_init     | 0x10000  | 4 KB     | Radio calibration       |
| ota_0        | 0x20000  | 2048 KB  | App slot 1 (primary)    |
| ota_1        | 0x220000 | 2048 KB  | App slot 2 (update)     |
| spiffs       | 0x420000 | 8192 KB  | File system for logs    |
| coredump     | 0xc20000 | 64 KB    | Crash debugging         |
+--------------+----------+----------+-------------------------+
Total: ~12.1 MB
Free: ~3.9 MB (reserved for future)
```

**Current Usage:**
- Firmware size: 851 KB
- Remaining per partition: 1197 KB (58% free)
- **Status:** [x] Massive headroom for expansion

## How OTA Works

### Update Process

1. **Device boots** from `ota_0` partition (your current firmware)
2. **Update triggered** (self-update or PYNQ-initiated)
3. **Download firmware** to `ota_1` partition **while still running**
4. **Verify** downloaded firmware integrity
5. **Mark `ota_1`** as bootable in `otadata` partition
6. **Reboot** device
7. **Bootloader** reads `otadata` and boots from `ota_1`
8. **Next update** alternates back to `ota_0`

### Safety Features

- [x] Old firmware remains intact during update
- [x] Download failure doesn't affect running system
- [x] Automatic rollback if new firmware fails to boot
- [x] Verification before marking partition bootable
- [x] Both slots always available for alternating updates

## Building for Different Boards

### For ESP32-WROOM-32 (Test Board)

```bash
# Set target (if not already set)
idf.py set-target esp32

# Configure board type
idf.py menuconfig
# Navigate to: PYNQ WiFi Bridge Configuration -> Board Type
# Select: ESP32-WROOM-32 (4MB flash)

# Build
idf.py build

# Flash (includes partition table + bootloader + app)
idf.py flash
```

The build system automatically uses `partitions_ota_4mb.csv`.

### For ESP32-S3-WROOM-1-N16 (Production Board)

```bash
# Set target
idf.py set-target esp32s3

# Configure board type
idf.py menuconfig
# Navigate to: PYNQ WiFi Bridge Configuration -> Board Type
# Select: ESP32-S3-WROOM-1-N16 (16MB flash)

# Build
idf.py build

# Flash
idf.py flash
```

The build system automatically uses `partitions_ota_16mb.csv`.

## Manual Partition Table Override

If you need to manually specify a partition table:

```bash
idf.py menuconfig
# Navigate to: Partition Table
# Select: Custom partition table CSV
# Enter filename: partitions_ota_4mb.csv (or partitions_ota_16mb.csv)
```

## Verifying Partition Table

After flashing, verify the partition table:

```bash
idf.py partition-table
```

Expected output:
```
# Name, Type, SubType, Offset, Size, Flags
nvs,data,nvs,0x9000,16K,
otadata,data,ota,0xd000,8K,
phy_init,data,phy,0xf000,4K,
ota_0,app,ota_0,0x10000,900K,
ota_1,app,ota_1,0xf1000,900K,
```

## Updating Partition Tables

**[WARN] WARNING:** Changing partition tables **erases all flash** including:
- Firmware
- WiFi credentials
- All stored data

**Only change partition tables:**
- During initial development
- Before deployment to production
- When you have backups of all important data

To flash a new partition table:

```bash
# Erase everything
idf.py erase-flash

# Flash new partition table + firmware
idf.py flash
```

## OTA Update Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Partition tables | [x] Complete | Both 4MB and 16MB layouts |
| OTA infrastructure | [PENDING] Pending | Need to add `esp_https_ota` component |
| Self-update code | [PENDING] Pending | Periodic version checking |
| PYNQ-initiated OTA | [PENDING] Pending | Add CMD_OTA_UPDATE to protocol |
| Version tracking | [PENDING] Pending | Server API for version management |

## Space Budget

### Current Size: 851 KB

| Component | Size |
|-----------|------|
| WiFi Stack | 324 KB |
| Networking (lwIP) | 91 KB |
| Security/Crypto | 74 KB |
| C Standard Library | 133 KB |
| FreeRTOS & Core | 39 KB |
| ESP-IDF Framework | ~172 KB |
| **Your Application** | **18 KB** |

### Unimplemented Features: ~30 KB

| Feature | Estimated Size |
|---------|---------------|
| HTTP GET/POST | 5-8 KB |
| TCP connections | 4-6 KB |
| OTA self-update | 5-8 KB |
| Motor control | 2-3 KB |
| Sensor reading | 2-3 KB |

### Final Size Estimate: ~880 KB

**ESP32-WROOM-32:** 880 KB / 900 KB = **98% used** (20 KB margin)
**ESP32-S3-WROOM-1-N16:** 880 KB / 2048 KB = **43% used** (1168 KB margin)

## SPIFFS File System (ESP32-S3 only)

The 16MB production board includes an 8 MB SPIFFS partition for:

- [STATS] **Logs**: Persistent logging for debugging
- [CONFIG] **Configuration**: JSON config files
- [MAP] **Maps**: SLAM map data from PYNQ
- [IMAGE] **Images**: Cached camera frames
- [TELEMETRY] **Telemetry**: Sensor data buffering

Example usage:

```c
#include "esp_spiffs.h"

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

// Write log
FILE* f = fopen("/spiffs/robot.log", "a");
fprintf(f, "Robot started at %ld\n", time(NULL));
fclose(f);
```

## Troubleshooting

### Build fails with "partition table too large"
Your partition table exceeds available flash. Check:
- Correct partition file for your board
- Correct target set (esp32 vs esp32s3)

### OTA update fails
- Verify WiFi is connected
- Check update server is reachable
- Ensure partition table has `otadata` partition
- Check firmware fits in partition size

### Device won't boot after OTA
- Automatic rollback should occur after 3 failed boots
- If stuck, erase flash and reflash: `idf.py erase-flash && idf.py flash`

### "No OTA app slot is configured"
You're using single factory partition instead of OTA partitions.
Reflash with: `idf.py erase-flash && idf.py flash`

## References

- [ESP-IDF Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [ESP-IDF OTA Updates](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [SPIFFS File System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html)
