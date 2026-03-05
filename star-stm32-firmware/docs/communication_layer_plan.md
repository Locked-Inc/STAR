# STM32 Communication Layer - Phased Implementation Plan

## Context

The STAR project currently runs motor control on the Renesas RX72N with a mature communication
stack. The STM32F7 is being brought up as a replacement/parallel platform. The STM32 firmware is
currently bare-metal with no RTOS and no peripheral drivers -- just FPU init and an empty main
loop.

The goal is to port the same communication logic (not code verbatim) to the STM32, reusing the
same gateway, same protobuf schemas, and same wire protocol -- so the Go gateway sees the STM32
as identical to the RX72N from a protocol perspective.

### Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Primary transport | USB CDC (TinyUSB) | Simplifies dev workflow; gateway already has CDC transport |
| Frame protocol | Same as RX72N exactly | Gateway reuse with zero changes |
| Protobuf library | nanopb 0.4.9.1 | Same version as RX72N; reuse generated files from star-proto |
| RTOS | FreeRTOS | Native TinyUSB integration; ST's strategic direction |
| Secondary transport | SPI (future phase) | USB first; SPI can be added after Phase 5 |

---

## Architecture

```
+-------------------------------------------------------------------+
|  Application Layer                                                |
|  comm_task (100 Hz) | motor_stub_task | telemetry_task (10 Hz)   |
+-------------------------------------------------------------------+
                                |
+-------------------------------------------------------------------+
|  Communication Manager  (stm32_comm_manager)                      |
|  - Non-blocking poll                                              |
|  - 500 ms watchdog                                                |
|  - Callback-driven frame dispatch                                 |
+-------------------------------------------------------------------+
                                |
+-------------------------------------------------------------------+
|  Frame Protocol  (stm32_frame)                                    |
|  SYNC 0x55AA | SEQ 2B | LEN 2B | TYPE 1B | FLAGS 1B | ... | CRC32|
+-------------------------------------------------------------------+
                                |
+-------------------------------------------------------------------+
|  nanopb / WireMessage  (stm32_nanopb)                             |
|  VelocityCommand | EmergencyStopCommand | TelemetryData | ...     |
+-------------------------------------------------------------------+
                                |
+-------------------------------------------------------------------+
|  USB CDC Transport  (stm32_usb_cdc via TinyUSB)                   |
|  CDC ACM class | /dev/ttyACM0 on host | 12 Mbps USB FS            |
+-------------------------------------------------------------------+
                                |
                   STM32F7 USB OTG peripheral
```

The Go gateway requires **no changes** -- it uses its existing CDC transport
(`star-gateway/internal/transport/cdc.go`) pointed at `/dev/ttyACM0`.

---

## Wire Protocol Reference

Identical to RX72N. All fields little-endian.

```
+--------+------+------+------+-------+--------------+-------+
| SYNC   | SEQ  | LEN  | TYPE | FLAGS | PAYLOAD      | CRC32 |
| 2B     | 2B   | 2B   | 1B   | 1B    | 0-1024 B     | 4B    |
| 0xAA55 | u16  | u16  | u8   | u8    | nanopb bytes | u32   |
+--------+------+------+------+-------+--------------+-------+
```

Wire byte order: `0xAA 0x55` (SYNC low byte first).

### Frame Types

| Value | Name         | Direction          |
|-------|--------------|--------------------|
| 0x00  | PING         | Host -> STM32      |
| 0x01  | PONG         | STM32 -> Host      |
| 0x10  | COMMAND      | Host -> STM32      |
| 0x11  | RESPONSE     | STM32 -> Host      |
| 0x12  | ACK          | STM32 -> Host      |
| 0x13  | NACK         | STM32 -> Host      |
| 0xFE  | RESET_ACK    | STM32 -> Host      |
| 0xFF  | RESET        | Host -> STM32      |

### Frame Flags

| Bit | Name          | Meaning                         |
|-----|---------------|---------------------------------|
| 0   | REQUIRES_ACK  | Sender expects ACK frame back   |
| 1   | RETRANSMIT    | This is a retransmission        |
| 2   | PRIORITY      | High-priority frame             |

### CRC-32

IEEE 802.3, polynomial 0x04C11DB7, LSB-first (reflected), initial value 0xFFFFFFFF,
final XOR 0xFFFFFFFF. Covers all bytes from SYNC through end of PAYLOAD.

---

## Phase 1: USB CDC Transport (TinyUSB)

**Goal:** STM32 enumerates as a USB CDC ACM device. Host can open `/dev/ttyACM0` and
exchange raw bytes.

### Deliverables

**New library: `libs/stm32_usb_cdc/`**

```c
// libs/stm32_usb_cdc/inc/stm32_usb_cdc.h
stm32_err_t stm32_usb_cdc_init(void);
stm32_err_t stm32_usb_cdc_send(const uint8_t* data, uint32_t len);
stm32_err_t stm32_usb_cdc_receive(uint8_t* buf, uint32_t max_len, uint32_t* out_len);
bool        stm32_usb_cdc_is_connected(void);
```

**TinyUSB integration:**
- Vendored at `libs/tinyusb/` (git submodule, tag `0.17.0`)
- Board port: `Sources/tusb_config.h` configures CDC ACM, 1x endpoint pair
- Descriptors: `Sources/usb_descriptors.c` sets VID/PID, manufacturer, product strings
- STM32F767 target: USB OTG HS peripheral with internal FS PHY (`OTG_HS_INTERNAL_FS`)
- STM32F746 target: USB OTG HS peripheral with internal FS PHY (same)
- TinyUSB ISR hooks: `OTG_HS_IRQHandler` calls `tud_int_handler(0)`

**FreeRTOS integration:**
- FreeRTOS vendored at `libs/freertos/` (git submodule, tag `FreeRTOS-Kernel-11.1.0`)
- TinyUSB USB device task: `vUsbDeviceTask` -- calls `tud_task()` in a loop
- `FreeRTOSConfig.h` at `Sources/FreeRTOSConfig.h`
- Heap: FreeRTOS heap_4 allocator (no `malloc` in application code)

**Clock configuration:**
- `Sources/system_stm32f7xx.c`: PLL to 216 MHz from HSE or HSI
- USB 48 MHz derived from PLL48CLK (PLLQ output)
- Required before FreeRTOS scheduler starts

**Files to create:**

```
star-stm32-firmware/
  libs/
    tinyusb/                          (git submodule)
    freertos/                         (git submodule)
    stm32_usb_cdc/
      inc/stm32_usb_cdc.h
      src/stm32_usb_cdc.c
  Sources/
    tusb_config.h
    usb_descriptors.c
    FreeRTOSConfig.h
    system_stm32f7xx.c                (new -- clock init)
    main.c                            (update -- add clock + USB + FreeRTOS)
  CMakeLists.txt                      (update -- add TinyUSB, FreeRTOS, stm32_usb_cdc)
```

### Verification

1. `lsusb` on host shows `STM32 CDC device`
2. `cat /dev/ttyACM0` echoes back bytes sent with `echo`
3. Gateway CDC transport (`cdc.go`) opens port without error
4. `scripts/stm32/build.sh` still passes for both F767 and F746

---

## Phase 2: CRC-32 and Frame Protocol

**Goal:** Same frame encoding/decoding as RX72N. Gateway sends a valid 0x55AA frame,
STM32 validates CRC and parses the header.

### Deliverables

**New library: `libs/stm32_crc32/`**

```c
// libs/stm32_crc32/inc/stm32_crc32.h
uint32_t stm32_crc32_sw(const uint8_t* data, uint32_t len);   // IEEE 802.3 software
uint32_t stm32_crc32_hw(const uint8_t* data, uint32_t len);   // STM32 CRC peripheral
```

Software implementation ported directly from
`star-rx72n-firmware/libs/rx_crc/src/rx_crc32_sw.c`.
Hardware implementation uses STM32F7 CRC peripheral (CRC_DR, polynomial 0x04C11DB7).

**New library: `libs/stm32_frame/`**

```c
// libs/stm32_frame/inc/stm32_frame.h

typedef enum : uint8_t {
    k_stm32_frame_type_ping      = 0x00,
    k_stm32_frame_type_pong      = 0x01,
    k_stm32_frame_type_command   = 0x10,
    k_stm32_frame_type_response  = 0x11,
    k_stm32_frame_type_ack       = 0x12,
    k_stm32_frame_type_nack      = 0x13,
    k_stm32_frame_type_reset_ack = 0xFE,
    k_stm32_frame_type_reset     = 0xFF,
} stm32_frame_type_t;

typedef enum : uint8_t {
    k_stm32_frame_flag_none         = 0x00,
    k_stm32_frame_flag_requires_ack = 0x01,
    k_stm32_frame_flag_retransmit   = 0x02,
    k_stm32_frame_flag_priority     = 0x04,
} stm32_frame_flags_t;

typedef struct {
    uint16_t sequence;
    uint16_t length;
    uint8_t  type;
    uint8_t  flags;
} stm32_frame_header_t;

typedef struct {
    stm32_frame_header_t header;
    uint8_t  payload[k_stm32_frame_max_payload];  // 1024 bytes
    uint32_t crc;
} stm32_frame_t;

stm32_err_t stm32_frame_encode(const stm32_frame_t* frame,
                               uint8_t* wire_buf, uint32_t* wire_len);
stm32_err_t stm32_frame_decode(const uint8_t* wire_data, uint32_t wire_len,
                               stm32_frame_t* frame);
stm32_err_t stm32_frame_decode_with_resync(uint8_t* wire_data, uint32_t* wire_len,
                                           stm32_frame_t* frame);
```

**Host unit tests: `tests/test_stm32_frame.c`**

Compiled with host GCC (not ARM cross-compiler) via a separate CMake host target.
Tests: encode/decode round-trip, CRC corruption rejection, resync after garbage bytes.

**Files to create:**

```
star-stm32-firmware/
  libs/
    stm32_crc32/
      inc/stm32_crc32.h
      src/stm32_crc32.c
    stm32_frame/
      inc/stm32_frame.h
      src/stm32_frame.c
  tests/
    test_stm32_frame.c
    CMakeLists.txt                    (host-compiled test target)
  CMakeLists.txt                      (update)
```

### Verification

1. Host unit tests pass: `ctest --test-dir build-host`
2. Python script on host sends a valid 0x55AA PING frame over USB; STM32 replies with PONG
3. CRC corruption test: flip one byte, verify STM32 sends NACK and logs error

---

## Phase 3: nanopb and WireMessage

**Goal:** STM32 decodes `WireMessage` oneof to identify `VelocityCommand` and related
messages. STM32 encodes `TelemetryData` in response.

### Deliverables

**nanopb library: `libs/nanopb/`**

Vendored at version 0.4.9.1 (same as RX72N). The same generated files from
`star-proto/gen/nanopb/star/v1/` are included directly -- no regeneration needed.

**New library: `libs/stm32_nanopb/`**

```c
// libs/stm32_nanopb/inc/stm32_nanopb.h

// Decode
stm32_err_t stm32_nanopb_decode_wire_message(const uint8_t* buf, uint32_t len,
                                             star_v1_WireMessage* msg);
stm32_err_t stm32_nanopb_decode_velocity_command(const uint8_t* buf, uint32_t len,
                                                 star_v1_VelocityCommand* msg);
stm32_err_t stm32_nanopb_decode_estop_command(const uint8_t* buf, uint32_t len,
                                              star_v1_EmergencyStopCommand* msg);
stm32_err_t stm32_nanopb_decode_pid_config(const uint8_t* buf, uint32_t len,
                                           star_v1_PidConfig* msg);

// Encode
stm32_err_t stm32_nanopb_encode_telemetry_data(const star_v1_TelemetryData* msg,
                                               uint8_t* buf, uint32_t* len);
stm32_err_t stm32_nanopb_encode_encoder_data(const star_v1_EncoderData* msg,
                                             uint8_t* buf, uint32_t* len);
```

All functions use static buffers (no malloc). Buffer size: 512 bytes (same as RX72N).
API modeled directly after `star-rx72n-firmware/libs/rx_nanopb/inc/rx_nanopb.h`.

**Host unit tests: `tests/test_stm32_nanopb.c`**

Encode a `VelocityCommand` with known values, decode it, verify fields match.
Encode a `TelemetryData` stub, verify serialized size is within 512 bytes.

**Files to create:**

```
star-stm32-firmware/
  libs/
    nanopb/                           (vendored 0.4.9.1)
    stm32_nanopb/
      inc/stm32_nanopb.h
      src/stm32_nanopb.c
  tests/
    test_stm32_nanopb.c
  CMakeLists.txt                      (update -- link nanopb + proto gen includes)
```

**Proto gen files referenced (read-only, not copied):**

```
star-proto/gen/nanopb/star/v1/wire.pb.h
star-proto/gen/nanopb/star/v1/wire.pb.c
star-proto/gen/nanopb/star/v1/motor_control.pb.h
star-proto/gen/nanopb/star/v1/motor_control.pb.c
star-proto/gen/nanopb/star/v1/telemetry.pb.h
star-proto/gen/nanopb/star/v1/telemetry.pb.c
star-proto/gen/nanopb/star/v1/common.pb.h
star-proto/gen/nanopb/star/v1/common.pb.c
```

### Verification

1. Host unit tests pass
2. USB loopback test: gateway sends `SetVelocity` at 1 Hz, STM32 decodes and sends back
   a stub `TelemetryData` -- gateway `StreamTelemetry` gRPC call receives it

---

## Phase 4: Communication Manager

**Goal:** Single-entry-point comm layer that polls USB, decodes frames, dispatches
messages, and enforces a 500 ms watchdog. Mirrors `rx_comm_manager` + `comm_task`.

### Deliverables

**New library: `libs/stm32_comm_manager/`**

```c
// libs/stm32_comm_manager/inc/stm32_comm_manager.h

typedef void (*stm32_comm_callback_t)(const stm32_frame_t* frame, void* ctx);

typedef struct {
    stm32_comm_callback_t on_frame;    // Called for each valid decoded frame
    void*                 ctx;         // User context passed to callback
    uint32_t              watchdog_ms; // Timeout before declaring comm lost (500)
} stm32_comm_manager_config_t;

stm32_err_t stm32_comm_manager_init(stm32_comm_manager_t* handle,
                                    const stm32_comm_manager_config_t* config);
stm32_err_t stm32_comm_manager_poll(stm32_comm_manager_t* handle);
stm32_err_t stm32_comm_manager_send(stm32_comm_manager_t* handle,
                                    const uint8_t* payload, uint32_t len);
stm32_err_t stm32_comm_manager_respond(stm32_comm_manager_t* handle,
                                       const stm32_frame_t* cmd_frame,
                                       const uint8_t* payload, uint32_t len);
bool        stm32_comm_manager_is_timeout(const stm32_comm_manager_t* handle);
```

**New library: `libs/stm32_shared_data/`**

Thread-safe data exchange between comm_task and motor/telemetry tasks.

```c
// libs/stm32_shared_data/inc/stm32_shared_data.h

typedef struct {
    float front_left_velocity_mps;
    float front_right_velocity_mps;
    float back_left_velocity_mps;
    float back_right_velocity_mps;
    uint32_t sequence;
} stm32_motor_command_t;

stm32_err_t stm32_shared_data_init(void);
stm32_err_t stm32_shared_data_set_motor_command(const stm32_motor_command_t* cmd);
stm32_err_t stm32_shared_data_get_motor_command(stm32_motor_command_t* cmd);
stm32_err_t stm32_shared_data_set_estop(bool active);
bool        stm32_shared_data_get_estop(void);
bool        stm32_shared_data_is_comm_timeout(void);
void        stm32_shared_data_set_last_comm_tick(uint32_t tick_ms);
```

**Communication task: `Sources/tasks/comm_task.c`**

FreeRTOS task `vCommTask` at 100 Hz (10 ms `vTaskDelay`), priority 5.

Dispatch cascade on each received COMMAND frame:

```
1. Try stm32_nanopb_decode_velocity_command()  -> stm32_shared_data_set_motor_command()
2. Try stm32_nanopb_decode_estop_command()     -> stm32_shared_data_set_estop(true)
3. Try stm32_nanopb_decode_pid_config()        -> stm32_shared_data_set_pid_config()
4. Otherwise: log warning, discard
```

On successful decode: send ACK frame.
On watchdog timeout: call `stm32_shared_data_set_estop(true)`.

**Files to create:**

```
star-stm32-firmware/
  libs/
    stm32_comm_manager/
      inc/stm32_comm_manager.h
      src/stm32_comm_manager.c
    stm32_shared_data/
      inc/stm32_shared_data.h
      src/stm32_shared_data.c
  Sources/
    tasks/
      inc/comm_task.h
      comm_task.c
  CMakeLists.txt                      (update)
```

### Verification

1. Gateway connects to STM32 via USB, sends `VelocityCommand` at 100 Hz for 10 s
2. No frames dropped (check sequence numbers in gateway logs)
3. Watchdog test: stop gateway for 600 ms, verify STM32 enters estop (LED or debug print)
4. ACK frames returned for every COMMAND frame

---

## Phase 5: Application Stubs and Telemetry

**Goal:** Complete the round-trip -- STM32 sends stub `TelemetryData` back to the gateway
so the full end-to-end pipeline works identically to RX72N.

### Deliverables

**Motor stub task: `Sources/tasks/motor_stub_task.c`**

FreeRTOS task `vMotorStubTask`, priority 8 (same relative priority as RX72N motor task).
Waits on FreeRTOS event group bit `k_event_new_motor_cmd`.
Reads `stm32_shared_data_get_motor_command()`, logs velocities.
Currently a stub: does not drive any hardware. Returns encoder feedback 0.0 m/s.

**Telemetry task: `Sources/tasks/telemetry_task.c`**

FreeRTOS task `vTelemetryTask`, priority 3, runs at 10 Hz.
Builds `star_v1_TelemetryData` with stub encoder values from shared_data.
Wraps in `star_v1_WireMessage` with `telemetry_data` field set.
Encodes via `stm32_nanopb_encode_telemetry_data()`.
Sends via `stm32_comm_manager_send()`.
Sets `emergency_stop = true` in telemetry when estop is active.

**Updated `Sources/main.c`**

```c
int main(void) {
    // 1. FPU enable (already present)
    // 2. Clock init: 216 MHz, USB 48 MHz
    // 3. FreeRTOS shared data init
    // 4. USB CDC init (TinyUSB)
    // 5. Create tasks: vUsbDeviceTask, vCommTask, vMotorStubTask, vTelemetryTask
    // 6. vTaskStartScheduler()
}
```

**Files to create/modify:**

```
star-stm32-firmware/
  Sources/
    tasks/
      inc/motor_stub_task.h
      motor_stub_task.c
      inc/telemetry_task.h
      telemetry_task.c
    main.c                            (rewrite)
  CMakeLists.txt                      (update)
```

### End-to-End Verification

1. Flash STM32, connect USB to host running gateway
2. `grpc_cli call localhost:50051 star.v1.MotorControlService/SetVelocity \
   'velocity_command: {front_left_velocity_mps: 0.5}'`
   - STM32 receives, logs "FL=0.50 FR=0.00 BL=0.00 BR=0.00"
3. `grpc_cli call localhost:50051 star.v1.TelemetryService/StreamTelemetry "{}"`
   - Receives TelemetryData at 10 Hz with stub encoder values
4. `grpc_cli call localhost:50051 star.v1.MotorControlService/EmergencyStop "{}"`
   - STM32 enters estop; next TelemetryData shows `emergency_stop: true`
5. Unplug USB cable; after 500 ms STM32 enters estop (watchdog)
6. Reconnect USB; gateway sends a RESET frame; STM32 clears estop

---

## File Manifest (All Phases)

### New Files

```
star-stm32-firmware/
  docs/
    communication_layer_plan.md       (this document)
  libs/
    tinyusb/                          (git submodule, 0.17.0)
    freertos/                         (git submodule, FreeRTOS-Kernel-11.1.0)
    stm32_usb_cdc/
      inc/stm32_usb_cdc.h
      src/stm32_usb_cdc.c
    stm32_crc32/
      inc/stm32_crc32.h
      src/stm32_crc32.c
    stm32_frame/
      inc/stm32_frame.h
      src/stm32_frame.c
    nanopb/                           (vendored, 0.4.9.1)
    stm32_nanopb/
      inc/stm32_nanopb.h
      src/stm32_nanopb.c
    stm32_comm_manager/
      inc/stm32_comm_manager.h
      src/stm32_comm_manager.c
    stm32_shared_data/
      inc/stm32_shared_data.h
      src/stm32_shared_data.c
  Sources/
    tusb_config.h
    usb_descriptors.c
    FreeRTOSConfig.h
    system_stm32f7xx.c
    tasks/
      inc/comm_task.h
      inc/motor_stub_task.h
      inc/telemetry_task.h
      comm_task.c
      motor_stub_task.c
      telemetry_task.c
  tests/
    CMakeLists.txt
    test_stm32_frame.c
    test_stm32_nanopb.c
```

### Modified Files

```
star-stm32-firmware/
  CMakeLists.txt                      (add libs, tests, submodules)
  Sources/main.c                      (clock + FreeRTOS + task creation)
  .gitmodules                         (TinyUSB, FreeRTOS submodules)
```

### Files Reused (read-only, no changes)

```
star-proto/gen/nanopb/star/v1/        (wire, motor_control, telemetry, common .pb.h/.c)
star-gateway/internal/transport/cdc.go  (gateway CDC transport, no changes needed)
star-rx72n-firmware/libs/rx_frame/    (reference for wire format)
star-rx72n-firmware/libs/rx_nanopb/   (reference for API patterns)
star-rx72n-firmware/src/tasks/comm_task.c  (reference for dispatch logic)
```

---

## Coding Standards

All new STM32 firmware code follows the same STAR standards as the RX72N:

- `snake_case` functions and variables, `snake_case_t` types, `k_` prefix for enum values
- `#pragma once` in all headers
- C23 typed enums: `typedef enum : uint8_t { ... } name_t;`
- No magic numbers -- all constants are named typed enums
- Full Doxygen documentation on all public APIs
- NASA Power of 10: assertions, fixed loop bounds, no dynamic allocation
- SOLID: single-responsibility modules, dependency injection via function pointers
- `-Wall -Werror` (already enforced in CMakeLists.txt)
- Module prefix: `stm32_` (vs `rx_` on RX72N) to avoid name collisions if shared

---

## Dependencies Summary

| Library | Version | Source | License |
|---------|---------|--------|---------|
| TinyUSB | 0.17.0 | git submodule | MIT |
| FreeRTOS Kernel | 11.1.0 | git submodule | MIT |
| nanopb | 0.4.9.1 | vendored | zlib |
| star-proto nanopb gen | -- | star-proto/gen/nanopb | project |
