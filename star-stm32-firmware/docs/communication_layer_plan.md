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
| nanopb dispatch | WireMessage which_payload | Decode WireMessage once, switch on oneof discriminator |

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

`stm32_usb_cdc_send()` behavior: blocks up to 20 ms waiting for TX FIFO space, then
returns `k_stm32_err_timeout`. Never drops bytes silently. Must call `tud_cdc_write_flush()`
after `tud_cdc_write()` or data will not transmit until the next SOF interval.

**TinyUSB integration:**
- Vendored at `libs/tinyusb/` (git submodule, tag `0.17.0`)
- Board port: `Sources/tusb_config.h` configures CDC ACM, 1x endpoint pair
- Descriptors: `Sources/usb_descriptors.c` sets VID/PID, manufacturer, product strings
- STM32F767 target: USB OTG HS peripheral with internal FS PHY (`OTG_HS_INTERNAL_FS`)
- STM32F746 target: USB OTG HS peripheral with internal FS PHY (same)
- TinyUSB ISR hooks: `OTG_HS_IRQHandler` calls `tud_int_handler(0)`

**NVIC priority (critical):**

In `stm32_usb_cdc_init()`, after enabling OTG_HS clock, set NVIC priority before enabling
the IRQ:

```c
// Must be numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY.
// Any ISR that touches FreeRTOS API (directly or via TinyUSB DMA callbacks)
// must observe this bound. Violation causes configASSERT or silent stack corruption.
NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
NVIC_EnableIRQ(OTG_HS_IRQn);
```

Document in `FreeRTOSConfig.h`: `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` must be `5`
on a 4-bit priority system (numerically >= all USB-related ISR priorities).

**USB RX StreamBuffer architecture:**

`vUsbDeviceTask` calls `tud_task()` in a loop and drains `tud_cdc_read()` into a FreeRTOS
`StreamBuffer` on each wakeup. `vCommTask` reads from the StreamBuffer, never from
`tud_cdc_read()` directly.

Rationale: USB SOF fires at 1 ms. `vCommTask` polls at 10 ms. During a 10 ms window,
up to 10 frames accumulate. TinyUSB's CDC RX FIFO is 512 bytes -- on resync with garbage
bytes, the FIFO can overflow silently (bytes are dropped with no notification). The
StreamBuffer decouples USB timing from frame parsing and provides a bounded, observable
receive path.

**USB disconnect fast-path:**

Register `tud_cdc_line_state_cb()`. When `dtr == false` (host disconnected or closed
the port), immediately call `stm32_shared_data_set_estop(true)`. Do not wait for the
500 ms watchdog. The watchdog remains armed as a fallback for other failure modes.

**FreeRTOS integration:**
- FreeRTOS vendored at `libs/freertos/` (git submodule, tag `FreeRTOS-Kernel-11.1.0`)
- TinyUSB USB device task: `vUsbDeviceTask` -- calls `tud_task()` in a loop
- `FreeRTOSConfig.h` at `Sources/FreeRTOSConfig.h`
- Heap: FreeRTOS heap_4 allocator (no `malloc` in application code)

**Clock configuration:**
- `Sources/system_stm32f7xx.c`: PLL to 216 MHz
- STM32 Nucleo F767ZI uses 8 MHz HSE. Add `STM32_USE_HSE` CMake option with HSI fallback.
- USB 48 MHz derived from PLL48CLK (PLLQ output). HSE/HSI selection must be correct for
  USB clock accuracy.
- Required before FreeRTOS scheduler starts.

**Toolchain requirement:**

`arm-none-eabi-gcc >= 13.x` is required for C23 typed enum syntax (`typedef enum : uint8_t`).
Add a CMakeLists.txt version check:

```cmake
if(CMAKE_C_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "arm-none-eabi-gcc >= 13.0 required for C23 typed enums")
endif()
```

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
//
// IMPORTANT: stm32_crc32_hw() is NOT thread-safe. The STM32 CRC peripheral is
// a single stateful hardware resource. Only ONE task may call stm32_crc32_hw()
// at a time. Policy: vCommTask uses stm32_crc32_hw(); vTelemetryTask (10 Hz)
// uses stm32_crc32_sw() to avoid a cross-task hardware resource conflict.
// If both tasks must use hardware CRC in future, add an xSemaphoreCreateMutex()
// guard with priority inheritance.
uint32_t    stm32_crc32_sw(const uint8_t* data, uint32_t len);   // IEEE 802.3 software
uint32_t    stm32_crc32_hw(const uint8_t* data, uint32_t len);   // STM32 CRC peripheral
stm32_err_t stm32_crc32_selftest(void);                          // Boot-time sanity check
```

**CRC hardware configuration (critical -- must match IEEE 802.3):**

IEEE 802.3 CRC-32 is reflected (LSB-first I/O). The STM32F7 CRC peripheral defaults to
MSB-first. Without `REV_IN` + `REV_OUT`, the hardware CRC will NOT match the software CRC
or the RX72N, and the gateway will NACK every frame silently.

```c
// stm32_crc32.c -- stm32_crc32_hw() implementation
uint32_t stm32_crc32_hw(const uint8_t* data, uint32_t len) {
    // Configure for IEEE 802.3 reflected CRC-32.
    // REV_IN (bits 5:4 = 0b11): reverse input bits per byte (LSB-first).
    // REV_OUT (bit 7): reverse output bit order.
    // RESET (bit 0): reset CRC accumulator to INIT value (0xFFFFFFFF).
    CRC->INIT = 0xFFFFFFFFU;
    CRC->CR   = CRC_CR_REV_IN_0 | CRC_CR_REV_IN_1 | CRC_CR_REV_OUT | CRC_CR_RESET;

    // Feed data as 8-bit writes. 32-bit writes require padding to 4-byte alignment
    // and produce wrong results for non-multiple-of-4 lengths.
    for (uint32_t i = 0u; i < len; i++) {
        *(volatile uint8_t*)&CRC->DR = data[i];
    }

    // Final XOR with 0xFFFFFFFF completes IEEE 802.3 computation.
    return CRC->DR ^ 0xFFFFFFFFU;
}
```

**Boot-time CRC self-test (must pass before vTaskStartScheduler):**

Call `stm32_crc32_selftest()` from `main()` before starting the scheduler. Assert on
mismatch. This catches REV_IN/REV_OUT misconfiguration, wrong initial value, and
wrong final XOR immediately at boot rather than during gateway integration.

```c
// stm32_crc32.c -- stm32_crc32_selftest() implementation
//
// IEEE check value: CRC-32 of ASCII "123456789" == 0x2144DF1C
// This is the standard test vector for IEEE 802.3 CRC-32.
stm32_err_t stm32_crc32_selftest(void) {
    static const uint8_t k_test_input[] = "123456789";
    static const uint32_t k_expected    = 0x2144DF1CU;
    typedef enum : uint8_t { k_len = 9u } selftest_t;

    uint32_t hw_result = stm32_crc32_hw(k_test_input, k_len);
    uint32_t sw_result = stm32_crc32_sw(k_test_input, k_len);

    if (hw_result != k_expected) { return k_stm32_err_crc_selftest_failed; }
    if (sw_result != k_expected) { return k_stm32_err_crc_selftest_failed; }
    if (hw_result != sw_result)  { return k_stm32_err_crc_selftest_failed; }
    return k_stm32_err_ok;
}
```

Software implementation ported directly from
`star-rx72n-firmware/libs/rx_crc/src/rx_crc32_sw.c`.

**New library: `libs/stm32_frame/`**

```c
// libs/stm32_frame/inc/stm32_frame.h

typedef enum : uint16_t {
    k_stm32_frame_max_payload    = 1024u,  /**< Maximum payload bytes per frame */
    k_stm32_frame_header_size    = 8u,     /**< SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) */
    k_stm32_frame_crc_size       = 4u,     /**< CRC-32 trailer */
    k_stm32_frame_max_wire_bytes = 1036u,  /**< k_stm32_frame_max_payload + header + crc */
} stm32_frame_sizes_t;

typedef enum : uint16_t {
    // Maximum bytes to scan when searching for a valid SYNC in garbage input.
    // Bounded to satisfy NASA Power of 10 Rule 2 (fixed loop upper bounds).
    k_stm32_frame_max_resync_scan_bytes = 1036u,  // == k_stm32_frame_max_wire_bytes
} stm32_frame_resync_t;

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
    uint8_t  payload[k_stm32_frame_max_payload];
    uint32_t crc;
} stm32_frame_t;

stm32_err_t stm32_frame_encode(const stm32_frame_t* frame,
                               uint8_t* wire_buf, uint32_t* wire_len);
stm32_err_t stm32_frame_decode(const uint8_t* wire_data, uint32_t wire_len,
                               stm32_frame_t* frame);

// Resync algorithm (bounded by k_stm32_frame_max_resync_scan_bytes):
// 1. Scan forward byte-by-byte up to k_stm32_frame_max_resync_scan_bytes positions.
// 2. At each position, check for SYNC bytes 0xAA 0x55.
// 3. On SYNC found, attempt full frame decode from that position.
// 4. On valid CRC: consume bytes through end of frame, update wire_len, return ok.
// 5. On invalid CRC: advance past the SYNC bytes, continue scan.
// 6. Scan exhausted: return k_stm32_err_resync_failed.
// Modifies wire_data (slide window) and wire_len (bytes remaining after resync).
stm32_err_t stm32_frame_decode_with_resync(uint8_t* wire_data, uint32_t* wire_len,
                                           stm32_frame_t* frame);
```

**Host unit tests: `tests/test_stm32_frame.c`**

Compiled with host GCC (not ARM cross-compiler) via a separate CMake host target.
Tests:
- `stm32_crc32_selftest()` passes (IEEE check value `0x2144DF1C`)
- SW and HW CRC agree on 100 random inputs
- encode/decode round-trip preserves all header fields and payload
- CRC corruption (flip one byte) causes `stm32_frame_decode()` to return error
- Resync after N bytes of garbage preceding a valid frame
- Resync exhaustion returns `k_stm32_err_resync_failed`

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

1. Host unit tests pass: `ctest --test-dir build-host` (includes CRC selftest)
2. `stm32_crc32_selftest()` called from `main()` before scheduler -- assertion fires if
   REV_IN/REV_OUT misconfigured
3. Python script on host sends a valid 0x55AA PING frame over USB; STM32 replies with PONG
4. CRC corruption test: flip one byte, verify STM32 sends NACK and logs error

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

// ---------------------------------------------------------------------------
// Primary dispatch entry point (use in comm_task for all incoming COMMAND frames)
// ---------------------------------------------------------------------------
// Decodes the WireMessage envelope. After this call, branch on msg->which_payload
// using the nanopb-generated tag constants (star_v1_WireMessage_velocity_command_tag,
// star_v1_WireMessage_estop_command_tag, etc.) to access the correct oneof field.
// Do NOT use the per-type decode functions below for dispatch -- they cannot
// distinguish between an absent field and a zero-valued present field in oneof.
stm32_err_t stm32_nanopb_decode_wire_message(const uint8_t* buf, uint32_t len,
                                             star_v1_WireMessage* msg);

// ---------------------------------------------------------------------------
// Per-type decode functions (unit test use only -- NOT for comm_task dispatch)
// ---------------------------------------------------------------------------
stm32_err_t stm32_nanopb_decode_velocity_command(const uint8_t* buf, uint32_t len,
                                                 star_v1_VelocityCommand* msg);
stm32_err_t stm32_nanopb_decode_estop_command(const uint8_t* buf, uint32_t len,
                                              star_v1_EmergencyStopCommand* msg);
stm32_err_t stm32_nanopb_decode_pid_config(const uint8_t* buf, uint32_t len,
                                           star_v1_PidConfig* msg);

// ---------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------
stm32_err_t stm32_nanopb_encode_telemetry_data(const star_v1_TelemetryData* msg,
                                               uint8_t* buf, uint32_t* len);
stm32_err_t stm32_nanopb_encode_encoder_data(const star_v1_EncoderData* msg,
                                             uint8_t* buf, uint32_t* len);
```

All functions use static buffers (no malloc). Buffer size: 512 bytes (same as RX72N).
API modeled directly after `star-rx72n-firmware/libs/rx_nanopb/inc/rx_nanopb.h`.

**Host unit tests: `tests/test_stm32_nanopb.c`**

- Encode a `VelocityCommand` with known values, decode via `stm32_nanopb_decode_wire_message()`,
  verify `which_payload == star_v1_WireMessage_velocity_command_tag`, verify fields match.
- Encode a `TelemetryData` stub, verify serialized size is within 512 bytes.
- Verify that a buffer containing only a `VelocityCommand` message does NOT match
  `star_v1_WireMessage_estop_command_tag` after WireMessage decode (confirms oneof
  dispatch correctness vs. cascade false-positives).

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
2. USB loopback test: gateway sends `SetVelocity` at 1 Hz, STM32 decodes via
   `stm32_nanopb_decode_wire_message()` + `which_payload` switch, sends back
   a stub `TelemetryData` -- gateway `StreamTelemetry` gRPC call receives it

---

## Phase 4: Communication Manager

**Goal:** Single-entry-point comm layer that polls USB, decodes frames, dispatches
messages, and enforces a 500 ms watchdog. Mirrors `rx_comm_manager` + `comm_task`.

### Deliverables

**New library: `libs/stm32_comm_manager/`**

Two headers: a public opaque-handle header and an internal header for unit tests.

```c
// libs/stm32_comm_manager/inc/stm32_comm_manager.h -- public (opaque handle)
typedef struct stm32_comm_manager_s stm32_comm_manager_t;

typedef void (*stm32_comm_callback_t)(const stm32_frame_t* frame, void* ctx);

typedef struct {
    stm32_comm_callback_t on_frame;    // Called for each valid decoded frame
    void*                 ctx;         // User context passed to callback
    uint32_t              watchdog_ms; // Timeout before declaring comm lost (500)
} stm32_comm_manager_config_t;

stm32_err_t stm32_comm_manager_init(stm32_comm_manager_t* handle,
                                    const stm32_comm_manager_config_t* config);
stm32_err_t stm32_comm_manager_poll(stm32_comm_manager_t* handle);
// Unsolicited send (telemetry): uses internal s_tx_sequence++
stm32_err_t stm32_comm_manager_send(stm32_comm_manager_t* handle,
                                    const uint8_t* payload, uint32_t len);
// Response to a received command: echoes cmd_frame->header.sequence in the reply SEQ field
stm32_err_t stm32_comm_manager_respond(stm32_comm_manager_t* handle,
                                       const stm32_frame_t* cmd_frame,
                                       const uint8_t* payload, uint32_t len);
bool        stm32_comm_manager_is_timeout(const stm32_comm_manager_t* handle);
```

```c
// libs/stm32_comm_manager/inc/stm32_comm_manager_internal.h -- unit test access
// Include this header ONLY in test files. Not part of the public API.
struct stm32_comm_manager_s {
    stm32_comm_manager_config_t config;
    uint16_t tx_sequence;      // Incremented for unsolicited sends; reset to 0 on RESET frame
    uint32_t last_rx_tick_ms;  // Timestamp of last valid frame received
    bool     is_timeout;       // Set true when (now - last_rx_tick_ms) > watchdog_ms
};
```

**Sequence number contract:**

- ACK and RESPONSE frames echo the originating command's `SEQ` field (from `cmd_frame->header.sequence`)
- Unsolicited telemetry frames use `handle->tx_sequence++` (wraps at UINT16_MAX)
- RESET frame reception resets `handle->tx_sequence = 0`
- Gateway treats gaps in telemetry SEQ as dropped frames: logs a warning but does not disconnect

**New library: `libs/stm32_shared_data/`**

Thread-safe data exchange between comm_task and motor/telemetry tasks.
Mutex: `xSemaphoreCreateMutex()` (not binary semaphore) for priority inheritance. This
prevents priority inversion when `vCommTask` (priority 5) could otherwise be blocked by
`vMotorStubTask` (priority 8) holding a non-inheriting lock.

```c
// libs/stm32_shared_data/inc/stm32_shared_data.h

typedef struct {
    float front_left_velocity_mps;
    float front_right_velocity_mps;
    float back_left_velocity_mps;
    float back_right_velocity_mps;
    uint32_t sequence;
} stm32_motor_command_t;

stm32_err_t stm32_shared_data_init(void);   // Creates FreeRTOS mutex via xSemaphoreCreateMutex()
stm32_err_t stm32_shared_data_set_motor_command(const stm32_motor_command_t* cmd);
stm32_err_t stm32_shared_data_get_motor_command(stm32_motor_command_t* cmd);
stm32_err_t stm32_shared_data_set_estop(bool active);
bool        stm32_shared_data_get_estop(void);
bool        stm32_shared_data_is_comm_timeout(void);
void        stm32_shared_data_set_last_comm_tick(uint32_t tick_ms);
```

**Communication task: `Sources/tasks/comm_task.c`**

FreeRTOS task `vCommTask` at 100 Hz (10 ms `vTaskDelay`), priority 5.

Frame dispatch (called for each received frame):

```c
// Frame type dispatch -- RESET handled before protobuf decode
switch (frame->header.type) {
    case k_stm32_frame_type_ping:
        stm32_comm_manager_respond(handle, frame, NULL, 0u);  // send PONG
        return;

    case k_stm32_frame_type_reset:
        stm32_shared_data_set_estop(false);
        handle->tx_sequence = 0u;
        stm32_comm_manager_respond_type(handle, frame, k_stm32_frame_type_reset_ack,
                                        NULL, 0u);
        return;

    case k_stm32_frame_type_command:
        break;  // fall through to WireMessage decode below

    default:
        stm32_log_warn(k_tag, "Unexpected frame type");
        return;
}

// WireMessage oneof dispatch for COMMAND frames.
// Decode the outer WireMessage once, then branch on which_payload.
// Do NOT use per-type decode functions here -- they cannot correctly
// distinguish absent from zero-valued fields in a protobuf oneof.
star_v1_WireMessage wire_msg = star_v1_WireMessage_init_zero;
if (stm32_nanopb_decode_wire_message(frame->payload, frame->header.length,
                                     &wire_msg) != k_stm32_err_ok) {
    stm32_log_err(k_tag, "WireMessage decode failed");
    stm32_comm_manager_send_nack(handle, frame);
    return;
}

switch (wire_msg.which_payload) {
    case star_v1_WireMessage_velocity_command_tag:
        stm32_shared_data_set_motor_command(&wire_msg.payload.velocity_command);
        // ACK is sent after vMotorStubTask reads shared data and yields (priority 8).
        // At 100 Hz this adds at most one motor task execution latency (~1 ms) to
        // ACK timing. Acceptable for current requirements.
        stm32_comm_manager_respond(handle, frame, NULL, 0u);
        break;

    case star_v1_WireMessage_estop_command_tag:
        stm32_shared_data_set_estop(true);
        stm32_comm_manager_respond(handle, frame, NULL, 0u);
        break;

    case star_v1_WireMessage_pid_config_tag:
        stm32_shared_data_set_pid_config(&wire_msg.payload.pid_config);
        stm32_comm_manager_respond(handle, frame, NULL, 0u);
        break;

    default:
        stm32_log_warn(k_tag, "Unknown WireMessage payload tag: %u",
                       (uint32_t)wire_msg.which_payload);
        stm32_comm_manager_send_nack(handle, frame);
        break;
}
```

On watchdog timeout: call `stm32_shared_data_set_estop(true)`.

**Files to create:**

```
star-stm32-firmware/
  libs/
    stm32_comm_manager/
      inc/stm32_comm_manager.h
      inc/stm32_comm_manager_internal.h
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
5. PING -> PONG round-trip verified

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
Encodes CRC via `stm32_crc32_sw()` (not `stm32_crc32_hw()` -- see Phase 2 thread-safety note).
Sends via `stm32_comm_manager_send()`.
Sets `emergency_stop = true` in telemetry when estop is active.

**Updated `Sources/main.c`**

```c
int main(void) {
    // 1. FPU enable (already present)
    // 2. Clock init: 216 MHz, USB 48 MHz (HSE on Nucleo F767ZI)
    // 3. stm32_crc32_selftest() -- assert on failure (boot-time sanity check)
    // 4. FreeRTOS shared data init
    // 5. USB CDC init (TinyUSB)
    // 6. Create tasks: vUsbDeviceTask, vCommTask, vMotorStubTask, vTelemetryTask
    // 7. vTaskStartScheduler()
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
5. Unplug USB cable:
   - `tud_cdc_line_state_cb()` fires immediately (< 10 ms): estop set
   - After 500 ms watchdog also confirms estop (fallback validation)
6. Reconnect USB; gateway sends a RESET frame (0xFF):
   - STM32 clears estop, resets `tx_sequence = 0`, sends RESET_ACK (0xFE)
   - Next TelemetryData shows `emergency_stop: false`

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
      inc/stm32_comm_manager_internal.h
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
  CMakeLists.txt                      (add libs, tests, submodules, GCC version check)
  Sources/main.c                      (clock + CRC selftest + FreeRTOS + task creation)
  .gitmodules                         (TinyUSB, FreeRTOS submodules)
```

### Files Reused (read-only, no changes)

```
star-proto/gen/nanopb/star/v1/        (wire, motor_control, telemetry, common .pb.h/.c)
star-gateway/internal/transport/cdc.go  (gateway CDC transport, no changes needed)
star-rx72n-firmware/libs/rx_frame/    (reference for wire format)
star-rx72n-firmware/libs/rx_nanopb/   (reference for API patterns)
star-rx72n-firmware/libs/rx_crc/      (reference for SW CRC implementation)
star-rx72n-firmware/src/tasks/comm_task.c  (reference for dispatch logic)
```

---

## Coding Standards

All new STM32 firmware code follows the same STAR standards as the RX72N:

- `snake_case` functions and variables, `snake_case_t` types, `k_` prefix for enum values
- `#pragma once` in all headers
- C23 typed enums: `typedef enum : uint8_t { ... } name_t;` (requires arm-none-eabi-gcc >= 13)
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
| arm-none-eabi-gcc | >= 13.x | toolchain | GPL |
