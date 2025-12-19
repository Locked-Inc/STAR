# Temperature Reading System - Remaining Implementation Tasks

## Overview

This document outlines the remaining tasks to complete the ESP32 temperature reading system with full protocol stack (Framing + ARQ + CRC-32 + nanopb). The system enables the Raspberry Pi 5 to query the ESP32 for DS18B20 temperature sensor readings via SPI using a reliable Request/Response pattern.

## Completed Work

✅ **Framing Layer (`lib/star_frame/`)**
- Header: `lib/star_frame/include/star_frame.h`
- Implementation: `lib/star_frame/src/star_frame.c`
- Frame format: `[SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)][PAYLOAD(0-1024)][CRC-32(4)]`
- CRC-32 validation using ESP32 ROM functions
- Static allocation (no malloc)

✅ **ARQ Layer (`lib/star_arq/`)**
- Header: `lib/star_arq/include/star_arq.h`
- Implementation: `lib/star_arq/src/star_arq.c`
- Stop-and-Wait ARQ protocol
- Automatic retries (3 attempts, 10ms timeout)
- Duplicate detection and filtering
- ACK/NACK handling

✅ **Library Configuration**
- All `library.json` files updated with correct GitHub repository URL
- CMakeLists.txt files created for both libraries
- Dependencies properly configured

## Remaining Tasks

### Task 1: Enable nanopb Generation in star-proto

**Status:** Blocked (someone is currently working on star-proto)

**When unblocked, complete these steps:**

1. **Fix the plugin name in `star-proto/buf.gen.yaml`:**
   ```yaml
   # Change line 47 from:
   # - plugin: nanopb_generator
   # To:
   - plugin: nanopb
     out: gen/nanopb
     opt:
       - --options-path=nanopb/
     strategy: all
   ```

2. **Verify nanopb is installed:**
   ```bash
   pip install nanopb
   # Should install nanopb 0.4.9+ and protobuf 6.33+
   ```

3. **Generate Protocol Buffer code:**
   ```bash
   cd /Users/bsikar/Documents/git/STAR/star-proto
   buf generate proto/
   ```

   This should generate:
   - `gen/nanopb/star/v1/common.pb.h`
   - `gen/nanopb/star/v1/common.pb.c`
   - `gen/nanopb/star/v1/telemetry.pb.h`
   - `gen/nanopb/star/v1/telemetry.pb.c`
   - And other proto files...

4. **Copy generated files to firmware:**
   ```bash
   mkdir -p /Users/bsikar/Documents/git/STAR/star-esp32-firmware/include/proto
   cp gen/nanopb/star/v1/*.{h,c} \
      /Users/bsikar/Documents/git/STAR/star-esp32-firmware/include/proto/
   ```

### Task 2: Add nanopb Dependency to platformio.ini

**File:** `/Users/bsikar/Documents/git/STAR/star-esp32-firmware/platformio.ini`

**Add to the `lib_deps` section:**
```ini
lib_deps =
    nanopb/Nanopb@^0.4.8
```

**Why:** This ensures the nanopb runtime library is available for encoding/decoding Protocol Buffer messages.

### Task 3: Implement main.c Application Logic

**File:** `/Users/bsikar/Documents/git/STAR/star-esp32-firmware/src/main.c`

**Current state:** Empty (`void app_main(void) {}`)

**Implementation steps:**

#### 3.1 Add Required Includes

```c
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "star_arq.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_sensor_ds18b20.h"

/* Protocol Buffer includes (after generation) */
#include "proto/common.pb.h"
#include "proto/telemetry.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

static const char* s_tag = "MAIN";
```

#### 3.2 Implement Initialization Sequence

```c
void app_main(void) {
    ESP_LOGI(s_tag, "STAR Temperature System Starting...");

    /* 1. Initialize bus manager */
    star_bus_manager_t bus_manager;
    esp_err_t ret = star_bus_manager_init(&bus_manager, "main", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Bus manager init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* 2. Create OneWire bus for DS18B20 (GPIO48) */
    star_bus_config_t* onewire = star_bus_config_create_onewire(
        "temp_onewire",
        GPIO_NUM_48,  /* IO48 - DS18B20 data pin */
        false         /* External power (not parasitic) */
    );
    star_bus_manager_add_bus(&bus_manager, onewire);

    /* 3. Create SPI3 peripheral for RPi5 communication */
    star_bus_config_t* spi = star_bus_config_create_spi_peripheral(
        "rpi_spi",
        SPI3_HOST,
        GPIO_NUM_11,  /* COPI (Controller Out, Peripheral In) */
        GPIO_NUM_13,  /* CIPO (Controller In, Peripheral Out) */
        GPIO_NUM_12,  /* SCLK (Serial Clock) */
        GPIO_NUM_10,  /* CS (Chip Select) */
        3,            /* Queue size */
        0             /* SPI mode 0 */
    );
    star_bus_manager_add_bus(&bus_manager, spi);

    /* 4. Initialize DS18B20 temperature sensor */
    star_ds18b20_handle_t temp_sensor;
    star_ds18b20_config_t temp_cfg = {
        .bus_manager = &bus_manager,
        .bus_name = "temp_onewire",
        .resolution = k_star_ds18b20_resolution_12_bit,  /* 0.0625°C precision, 750ms conversion */
        .use_rom = false,  /* Skip ROM addressing (single sensor) */
    };
    ret = star_sensor_ds18b20_init(&temp_sensor, &temp_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "DS18B20 init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* 5. Initialize ARQ layer */
    star_arq_handle_t arq;
    star_arq_config_t arq_cfg = {
        .bus_manager = &bus_manager,
        .spi_bus_name = "rpi_spi",
        .max_retries = 3,
        .retry_timeout_ms = 10,
    };
    ret = star_arq_init(&arq, &arq_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "ARQ init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(s_tag, "Initialization complete. Ready for requests.");

    /* 6. Main request/response loop */
    handle_requests(&temp_sensor, &arq);
}
```

#### 3.3 Implement Request Handler Function

```c
static void handle_requests(star_ds18b20_handle_t* sensor, star_arq_handle_t* arq) {
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[512];

    while (true) {
        /* Receive request from RPi5 */
        size_t rx_len;
        esp_err_t ret = star_arq_receive(arq, rx_buffer, sizeof(rx_buffer), &rx_len, portMAX_DELAY);

        if (ret != ESP_OK) {
            ESP_LOGW(s_tag, "Receive failed: %s", esp_err_to_name(ret));
            continue;
        }

        /* Process telemetry request */
        handle_telemetry_request(sensor, arq, rx_buffer, rx_len, tx_buffer, sizeof(tx_buffer));
    }
}
```

#### 3.4 Implement Telemetry Request Handler with nanopb

```c
static void handle_telemetry_request(
    star_ds18b20_handle_t* sensor,
    star_arq_handle_t* arq,
    const uint8_t* request_buf,
    size_t request_len,
    uint8_t* response_buf,
    size_t response_buf_size)
{
    /* 1. Decode GetTelemetryRequest (nanopb) */
    star_v1_GetTelemetryRequest request = star_v1_GetTelemetryRequest_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(request_buf, request_len);

    if (!pb_decode(&istream, star_v1_GetTelemetryRequest_fields, &request)) {
        ESP_LOGE(s_tag, "Failed to decode request: %s", PB_GET_ERROR(&istream));
        return;
    }

    ESP_LOGI(s_tag, "Received GetTelemetryRequest: request_id=%s", request.header.request_id);

    /* 2. Read temperature from DS18B20 */
    float temperature_celsius;
    esp_err_t ret = star_sensor_ds18b20_read_temp(sensor, &temperature_celsius);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Temperature read failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(s_tag, "Temperature: %.2f °C", temperature_celsius);

    /* 3. Build GetTelemetryResponse */
    star_v1_GetTelemetryResponse response = star_v1_GetTelemetryResponse_init_zero;

    /* Copy request ID */
    strncpy(response.header.request_id, request.header.request_id, 64);

    /* Set status */
    response.header.status = star_v1_Status_STATUS_OK;

    /* Set timestamp (optional) */
    response.header.has_server_timestamp = true;
    response.header.server_timestamp.seconds = 0;  /* TODO: Use actual timestamp */
    response.header.server_timestamp.nanos = 0;

    /* Set telemetry data */
    response.has_telemetry = true;
    response.telemetry.temperature_celsius = temperature_celsius;

    /* 4. Encode response (nanopb) */
    pb_ostream_t ostream = pb_ostream_from_buffer(response_buf, response_buf_size);
    if (!pb_encode(&ostream, star_v1_GetTelemetryResponse_fields, &response)) {
        ESP_LOGE(s_tag, "Failed to encode response: %s", PB_GET_ERROR(&ostream));
        return;
    }

    /* 5. Send via ARQ */
    ret = star_arq_send(arq, response_buf, ostream.bytes_written, 1000);
    if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Sent temperature response: %.2f °C", temperature_celsius);
    } else {
        ESP_LOGE(s_tag, "Failed to send response: %s", esp_err_to_name(ret));
    }
}
```

### Task 4: Build and Test

**Build the firmware:**
```bash
cd /Users/bsikar/Documents/git/STAR/star-esp32-firmware
pio run -e esp32s3
```

**Expected output:**
- Successful compilation of all libraries
- No errors related to missing headers or undefined symbols
- Firmware binary generated in `.pio/build/esp32s3/firmware.bin`

**Upload and monitor:**
```bash
pio run -e esp32s3 --target upload
pio device monitor
```

**Expected behavior:**
1. ESP32 initializes successfully
2. Logs show: "Initialization complete. Ready for requests."
3. When RPi5 sends GetTelemetryRequest, ESP32:
   - Receives request
   - Reads temperature from DS18B20
   - Encodes and sends GetTelemetryResponse
   - Logs: "Sent temperature response: XX.XX °C"

## Hardware Connections Verification

Before testing, verify these connections on the ESP32-S3:

| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| **Temperature Sensor (DS18B20)** |
| DATA | GPIO48 (IO48) | 1-Wire data line |
| **SPI3 (RPi5 Communication)** |
| CS | GPIO10 (IO10) | Chip Select |
| COPI | GPIO11 (IO11) | Controller Out, Peripheral In |
| SCLK | GPIO12 (IO12) | Serial Clock |
| CIPO | GPIO13 (IO13) | Controller In, Peripheral Out |

## Troubleshooting Guide

### Common Issues and Solutions

**1. "CRC mismatch" errors:**
- **Cause:** Noisy SPI communication or incorrect wiring
- **Solution:** Check SPI connections, add ground wire, reduce SPI speed

**2. "Max retries exceeded" errors:**
- **Cause:** RPi5 not sending ACKs or timeout too short
- **Solution:** Increase `retry_timeout_ms` in ARQ config, verify RPi5 is running

**3. "Temperature read failed" errors:**
- **Cause:** DS18B20 not connected or wrong GPIO pin
- **Solution:** Verify GPIO48 connection, check 4.7kΩ pull-up resistor on data line

**4. "Failed to decode request" errors:**
- **Cause:** Protobuf schema mismatch or corrupted data
- **Solution:** Verify RPi5 and ESP32 use same proto files, regenerate nanopb code

**5. nanopb compilation errors:**
- **Cause:** Missing nanopb includes or wrong version
- **Solution:** Verify `platformio.ini` has `nanopb/Nanopb@^0.4.8`, rebuild

## Performance Characteristics

**Latency Breakdown (Request → Response):**
- DS18B20 conversion (12-bit): **750ms**
- nanopb decode: **~0.3ms**
- nanopb encode: **~0.5ms**
- SPI transfer (200 bytes @ 10 Mbps): **~0.16ms**
- ARQ overhead (ACK wait): **~10ms**
- **Total: ~761ms** (dominated by sensor conversion)

**Optimization:**
- Use 10-bit resolution → 187.5ms conversion time (0.25°C precision)
- Update `k_star_ds18b20_resolution_10_bit` in DS18B20 config

**Memory Usage:**
- SRAM: ~7.5 KB (~1.5% of 512 KB)
- Flash: ~112 KB (~0.7% of 16 MB)
- Well within budget!

## Testing Checklist

- [ ] Task 1: Protocol Buffers generated successfully
- [ ] Task 2: nanopb dependency added to platformio.ini
- [ ] Task 3: main.c implemented with all functions
- [ ] Task 4: Firmware builds without errors
- [ ] Hardware: DS18B20 connected to GPIO48
- [ ] Hardware: SPI3 connected to RPi5 (GPIO10-13)
- [ ] Upload firmware to ESP32-S3
- [ ] ESP32 logs "Initialization complete"
- [ ] RPi5 can send GetTelemetryRequest
- [ ] ESP32 responds with temperature data
- [ ] Temperature reading is accurate (±0.5°C)
- [ ] ARQ retries work (test by disconnecting SPI momentarily)
- [ ] CRC errors are detected and recovered

## Next Steps After Completion

Once the basic temperature system is working:

1. **Add error handling improvements:**
   - Implement watchdog timer
   - Add health monitoring
   - Log errors to NVS (non-volatile storage)

2. **Optimize performance:**
   - Switch to 10-bit DS18B20 resolution
   - Implement temperature caching (read periodically vs on-demand)
   - Add interrupt-driven SPI instead of polling

3. **Expand functionality:**
   - Add motor control integration
   - Implement streaming telemetry mode
   - Add configuration service for runtime parameter tuning

## References

- **Protocol Documentation:** `/Users/bsikar/Documents/git/STAR/docs/sections/01_nanopb_protocol.tex`
- **Hardware Pinout:** `/Users/bsikar/Documents/git/STAR/docs/sections/03_hardware_pinout.tex`
- **Implementation Plan:** `/Users/bsikar/.claude/plans/purring-skipping-dahl.md`
- **DS18B20 Datasheet:** [Maxim Integrated DS18B20](https://www.maximintegrated.com/en/products/sensors/DS18B20.html)
- **nanopb Documentation:** [https://jpa.kapsi.fi/nanopb/docs/](https://jpa.kapsi.fi/nanopb/docs/)

---

**Document Created:** December 19, 2025
**Status:** Framing and ARQ layers complete, Protocol Buffer integration pending
**Next Reviewer:** Person currently working on star-proto directory
