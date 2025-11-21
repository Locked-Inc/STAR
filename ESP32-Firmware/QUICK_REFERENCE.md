# SICK TiM561-2050101 - Quick Reference Card
## For Developers (1-Page Cheat Sheet)

---

## HARDWARE SPECS

| Parameter | Value |
|-----------|-------|
| **Range** | 0.05 - 10 m |
| **FOV** | 270° |
| **Resolution** | 0.33° (2,880 points/scan) |
| **Frequency** | 15 Hz (67 ms/scan) |
| **Laser** | 850 nm (Class 1, IR) |
| **Connector** | M12 D-coded Ethernet |
| **Power** | 9-28 VDC (separate connector) |

---

## M12 D-CODED PINOUT

```
Pin 1 (Yellow):   TX+ Ethernet    →  RJ45 Pin 1
Pin 2 (White):    TX- Ethernet    →  RJ45 Pin 2
Pin 3 (Orange):   RX+ Ethernet    →  RJ45 Pin 3
Pin 4 (Blue):     RX- Ethernet    →  RJ45 Pin 6
Shield:           Ground
```

---

## NETWORK SETUP

```
Protocol:     TCP/IP v4
Port:         2112
Default IP:   192.168.0.1 (DHCP, then fallback)
Timeout:      5 seconds recommended
Keepalive:    Every 10 seconds
Bandwidth:    ASCII ~225 KB/s, Binary ~135 KB/s @ 15 Hz
```

---

## COLA PROTOCOL QUICK START

### Commands
```
sRN LMDscandata        → Request single scan
sEN LMDscandata        → Enable continuous streaming
sEO LMDscandata        → Disable streaming
sWN LMDscandatacfg ... → Configure output
```

### Telegram Structure
```
┌─────────────────────────────────┐
│ STX │ Data... │ CRC16-CCITT │ ETX │
│ 0x02│ ...     │  (2 bytes)  │ 0x03│
└─────────────────────────────────┘
```

### Response Format (Simplified)
```
sRA LMDscandata
  [Version] [Device#] [Serial] [Status...] [TeleCounter] [ScanCounter]
  [DIST1] [Scale] [Offset] [StartAngle] [AngleStep] [Count] [Values...]
```

---

## DATA PARSING

### Distance Channel Format
```c
struct {
    uint32_t scale_factor;    // IEEE 754 (usually 0x3F800000 = 1.0)
    int32_t start_angle;      // Units: 1/10000 degree
    int32_t angle_step;       // Units: 1/10000 degree
    uint16_t count;           // Points in scan (2880 typical)
    uint16_t distances[2880]; // Raw distance in mm
} DIST1_channel;

// Conversion formulas:
double angle_deg = start_angle / 10000.0 + (i * angle_step / 10000.0);
float distance_m = (float)distances[i] * scale / 1000.0;
```

### Validation Rules
```
Valid range:     50 mm - 10,000 mm
Invalid marker:  0x0000 (no reflection), 0xFFFF (out of range)
Max jump:        500 mm between adjacent points
Quality:         If > 50% invalid points = poor data
```

---

## CODESTRUCTURES

### Main Handle
```c
SickTiM561Driver_t {
    int socket_fd;
    char sensor_ip[16];
    ConnectionState_t state;
    ScanData_t last_scan;
    ScanFrameBuffer_t scan_buffer;  // Ring buffer
    SecurityContext_t security;
    // ... callbacks, config, statistics
}
```

### Scan Data
```c
ScanData_t {
    ScanPoint_t points[2880];
    uint32_t point_count;
    uint32_t scan_counter;
    uint32_t timestamp_ms;
    float scale_factor;
    int32_t start_angle_units;
    int32_t angle_step_units;
}

ScanPoint_t {
    uint16_t distance_mm;
    double angle_degrees;
    uint8_t flags;  // SCANPOINT_VALID, SCANPOINT_OUT_OF_RANGE, etc.
}
```

---

## ESSENTIAL API CALLS

```c
// Initialize driver
sick_tim561_init(&handle, "192.168.0.100", 2112);

// Connect to sensor
sick_tim561_connect(&handle);

// Start scanning
sick_tim561_start_streaming(&handle);

// Get latest scan (non-blocking)
ScanData_t scan;
if (sick_tim561_get_latest_scan(&handle, &scan) == ESP_OK) {
    for (int i = 0; i < scan.point_count; i++) {
        if (scan.points[i].flags & SCANPOINT_VALID) {
            printf("Point %d: %.2f° = %.3f m\n",
                   i,
                   scan.points[i].angle_degrees,
                   scan.points[i].distance_mm / 1000.0);
        }
    }
}

// Stop scanning
sick_tim561_stop_streaming(&handle);

// Cleanup
sick_tim561_deinit(&handle);
```

---

## SECURITY CHECKLIST

- [ ] **Telegram validation**: `sick_validate_telegram()` before parsing
- [ ] **CRC verification**: `sick_verify_crc16()` on all messages
- [ ] **Distance validation**: `sick_validate_distance()` per point
- [ ] **Command whitelisting**: Only known commands accepted
- [ ] **Size limits**: Max telegram 64 KB, tokens 256 max
- [ ] **MAC verification**: `sick_verify_sensor_mac()` for OUI 00:30:24
- [ ] **Sequence checking**: `sick_check_scan_sequence()` for dropped scans
- [ ] **Memory monitoring**: `sick_monitor_memory_health()` periodically
- [ ] **Keepalive**: TCP keepalive every 10 seconds
- [ ] **Timeout handling**: Set TCP timeout to 5 seconds

---

## TROUBLESHOOTING MATRIX

| Problem | Cause | Solution |
|---------|-------|----------|
| Can't connect | Wrong IP or DHCP issue | Use SOPAS ET to find sensor |
| Port 2112 blocked | Network/firewall | `telnet IP 2112` to verify |
| CRC errors | Line noise | Use shielded cable, separate power |
| All distances 0xFFFF | Out of range | Check reflectivity/FOV alignment |
| Scan counter jumps | Dropped packets | Use binary CoLa-B, increase RX buffer |
| Heap exhaustion | Memory leak | Use ring buffers, check cleanup |
| Slow parsing | Too many sockets | Reduce CONFIG_LWIP_MAX_SOCKETS |

---

## MEMORY LAYOUT (ESP32)

```
Typical Usage per Sensor:
  ScanFrameBuffer (2×46 KB):     ~92 KB
  Telegram RX buffer:            ~65 KB (growable, usually 4-8 KB)
  Driver structures:             ~8 KB
  TCP socket buffers (lwIP):     ~24 KB
  ─────────────────────────────────────
  Total:                         ~200 KB (typical)
                                 Peak: ~300 KB
```

---

## ESP-IDF CONFIGURATION

```kconfig
# menuconfig settings:
CONFIG_LWIP_MAX_SOCKETS=4           # Limit sockets (ESP32 has limited RAM)
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=8192
CONFIG_LWIP_TCP_RCV_BUF=65535
CONFIG_LWIP_USE_IRAM_MEM=1         # Optional: IRAM optimization
```

---

## COMMON ISSUES & FIXES

### Issue: "Can't establish connection"
```c
// Check network connectivity first
if (ping(sensor_ip, 4, 100) != ESP_OK) {
    ESP_LOGE(TAG, "Sensor not reachable");
    return;
}

// Set timeout explicitly
struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```

### Issue: "CRC errors frequently"
```c
// Likely cause: EMI or cable issue
// Solution: Use shielded Category 6 Ethernet cable
// Ground the M12 connector properly
// Separate power cables from signal cables
// Check ground path at both ends
```

### Issue: "Heap runs out after few hours"
```c
// Likely cause: Memory leak
// Solution:
// 1. Check all malloc() calls have matching free()
// 2. Verify ring buffer is working (not accumulating)
// 3. Call sick_monitor_memory_health() to track usage
// 4. Use heap_caps_get_info() in debugger
```

### Issue: "Points jump around randomly"
```c
// Likely cause: Invalid/corrupted measurements
// Solution:
DistanceValidation_t val = sick_validate_distance(raw, scale, last);
if (val != DISTANCE_VALID) {
    skip_point = true;  // Don't use invalid measurement
}
```

---

## PERFORMANCE METRICS

```
Typical Throughput:
  ┌─────────────────────────────────────┐
  │ 15 Hz scan rate                     │
  │ 2,880 points per scan               │
  │ ─────────────────────────────────   │
  │ 43,200 points/second                │
  │                                     │
  │ Network bandwidth (ASCII):          │
  │   ~15 KB per scan                   │
  │   225 KB/sec = 1.8 Mbps             │
  │                                     │
  │ Network bandwidth (Binary):         │
  │   ~9 KB per scan                    │
  │   135 KB/sec = 1.1 Mbps             │
  │                                     │
  │ CPU usage (100 MHz): ~1-2%          │
  │ Memory usage: ~200 KB                │
  │ Latency: <100 ms from sensor        │
  └─────────────────────────────────────┘
```

---

## REFERENCE FILES

| File | Purpose | Lines |
|------|---------|-------|
| `SICK_TIM561_TECHNICAL_GUIDE.md` | Complete reference | ~2,500 |
| `sick_tim561_driver.h` | API header | ~700 |
| `sick_tim561_security.c` | Security implementation | ~600 |
| `IMPLEMENTATION_SUMMARY.md` | Overview | ~1,000 |
| `QUICK_REFERENCE.md` | This card | ~400 |

---

## QUICK TEST SCRIPT

```c
// Minimal functional test
void lidar_test(void) {
    SickTiM561Driver_t lidar = {0};

    // 1. Initialize
    if (sick_tim561_init(&lidar, "192.168.0.100", 2112) != ESP_OK) {
        ESP_LOGE("TEST", "Init failed");
        return;
    }

    // 2. Connect (with timeout)
    if (sick_tim561_connect(&lidar) != ESP_OK) {
        ESP_LOGE("TEST", "Connect failed");
        return;
    }

    // 3. Request single scan
    ScanData_t scan;
    if (sick_tim561_request_single_scan(&lidar, &scan, 1000) == ESP_OK) {
        ESP_LOGI("TEST", "Got scan with %d points", scan.point_count);

        // 4. Print first valid point
        for (int i = 0; i < scan.point_count; i++) {
            if (scan.points[i].flags & SCANPOINT_VALID) {
                ESP_LOGI("TEST", "First point: %.2f° = %.3f m",
                        scan.points[i].angle_degrees,
                        scan.points[i].distance_mm / 1000.0);
                break;
            }
        }
    } else {
        ESP_LOGE("TEST", "Scan request failed");
    }

    // 5. Cleanup
    sick_tim561_deinit(&lidar);
    ESP_LOGI("TEST", "Test complete");
}
```

---

## KEY TAKEAWAYS

1. **Hardware**: M12 D-coded Ethernet, 100 Mbps, 2112 port
2. **Protocol**: COLA (ASCII or Binary), CRC16-CCITT, LMDscandata
3. **Data**: 2,880 points/scan, 15 Hz, 50mm-10m range, millimeters
4. **Security**: Validate telegrams, check distances, whitelist commands
5. **Memory**: Ring buffers, socket limits, heap monitoring
6. **Performance**: 43k points/sec, ~1-2% CPU, 200 KB RAM typical
7. **Debugging**: Use Wireshark to analyze telegrams, check CRC manually
8. **Production**: Implement keepalive, reconnection, statistics logging

---

**Quick Reference Version**: 1.0 | **Date**: 2025-11-20 | **Status**: Ready
