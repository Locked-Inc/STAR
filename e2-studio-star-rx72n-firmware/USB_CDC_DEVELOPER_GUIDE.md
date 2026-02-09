# USB CDC Developer Guide

**Target Audience**: Firmware developers, maintainers, contributors

**Last Updated**: 2026-02-05

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Code Organization](#code-organization)
3. [Adding New USB CDC Ports](#adding-new-usb-cdc-ports)
4. [Debugging Techniques](#debugging-techniques)
5. [Common Pitfalls](#common-pitfalls)
6. [Performance Optimization](#performance-optimization)
7. [Integration Guide](#integration-guide)
8. [Testing Strategy](#testing-strategy)

---

## Architecture Overview

### System Layers

```
┌─────────────────────────────────────────────────────┐
│  Application Layer                                  │
│  (motor_control_task, telemetry_task, etc.)        │
└─────────────────┬───────────────────────────────────┘
                  │ rx_usb_write/read, rx_log_*
┌─────────────────▼───────────────────────────────────┐
│  USB CDC API Layer                                  │
│  (libs/rx_usb/inc/rx_usb.h)                        │
│  - Port management (3 ports)                        │
│  - Ring buffer abstraction                          │
│  - Event callbacks                                  │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│  USB CDC Class Layer                                │
│  (libs/rx_usb/src/rx_usb_cdc.c)                    │
│  - CDC-ACM class implementation                     │
│  - Descriptor management                            │
│  - Class request handling                           │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│  USB Hardware Abstraction Layer                     │
│  (libs/rx_usb/src/rx_usb_hw.c)                     │
│  - FIFO access (16-bit word access)                │
│  - Pipe configuration                               │
│  - Interrupt management                             │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│  USB Interrupt Handler                              │
│  (libs/rx_usb/src/rx_usb_isr.c)                    │
│  - BRDY (Bulk Ready - data received)               │
│  - BEMP (Buffer Empty - data sent)                 │
│  - VBUS, Reset, Suspend/Resume                     │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│  RX72N USB0 Peripheral Hardware                     │
│  - 9 pipes (1 control + 8 configurable)            │
│  - 2KB shared FIFO                                  │
│  - Full-Speed transceiver (12 Mbps)                │
└─────────────────────────────────────────────────────┘
```

### Data Flow: Bulk OUT (Host → Device)

```
1. Host writes to /dev/ttyACM0 (Linux)
   ↓
2. USB host controller sends Bulk OUT packet (max 64 bytes)
   ↓
3. RX72N USB0 receives packet → writes to CFIFO
   ↓
4. BRDY interrupt fires (Bulk Ready)
   ↓
5. rx_usb_isr_handler() → rx_usb_brdy_isr()
   ↓
6. Read FIFO (16-bit word access) → copy to RX ring buffer
   ↓
7. Set k_usb_event_data_rx event (optional callback)
   ↓
8. Application calls rx_usb_read() → retrieves data from ring buffer
```

### Data Flow: Bulk IN (Device → Host)

```
1. Application calls rx_usb_write() → copies to TX ring buffer
   ↓
2. If FIFO available: write to CFIFO (16-bit word access)
   ↓
3. Set PID to BUF (transmit buffer ready)
   ↓
4. USB0 peripheral sends Bulk IN packet to host
   ↓
5. BEMP interrupt fires (Buffer Empty - packet sent)
   ↓
6. rx_usb_isr_handler() → rx_usb_bemp_isr()
   ↓
7. If more data in TX ring: write next chunk to FIFO
   ↓
8. If TX ring empty: set k_usb_event_tx_complete (optional callback)
```

### Debug Logging Architecture

```
┌─────────────────────────────────────────────────────┐
│  Application Logging                                │
│  rx_log_info(), rx_log_warn(), rx_log_error()      │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│  Log Macro Layer (rx_log.h)                        │
│  - Compile-time level filtering                    │
│  - Format string + variadic args                   │
│  - Expands to LOG_PUTS() + LOG_PUTINT() etc.      │
└─────────────┬───────────────────┬───────────────────┘
              │                   │
              │ UART              │ USB CDC (if USB_LOG_MIRROR=1)
              │                   │
┌─────────────▼──────┐  ┌─────────▼────────────────────┐
│  UART Backend      │  │  USB CDC Backend             │
│  (uart_debug_puts) │  │  (rx_log_usb_puts)           │
│  - Always enabled  │  │  - Boot buffering (512B)     │
│  - Synchronous     │  │  - Thread-safe (mutex)       │
│  - Simple          │  │  - Non-blocking (drops)      │
└────────────────────┘  └──────────────────────────────┘
```

---

## Code Organization

### Directory Structure

```
e2-studio-star-rx72n-firmware/
├── libs/
│   ├── rx_usb/                    # USB CDC driver
│   │   ├── inc/
│   │   │   └── rx_usb.h           # Public API (rx_usb_write/read, etc.)
│   │   └── src/
│   │       ├── rx_usb.c           # High-level API implementation
│   │       ├── rx_usb_cdc.c       # CDC-ACM class layer
│   │       ├── rx_usb_hw.c        # Hardware abstraction (FIFO access)
│   │       └── rx_usb_isr.c       # Interrupt handlers
│   │
│   ├── rx_core/                   # Core utilities
│   │   ├── inc/
│   │   │   └── rx_log.h           # Logging API (USB_LOG_MIRROR support)
│   │   └── src/
│   │       └── rx_log_usb.c       # USB CDC logging backend (Phase 3)
│   │
│   └── rx_hal/                    # Hardware abstraction
│       ├── inc/
│       │   └── rx72n_usb_regs.h   # USB register definitions (Phase 1)
│       └── src/
│           └── rx72n_usb.c        # USB register access functions
│
├── docs/sections/
│   └── 09_usb_cdc_protocol.tex    # Technical specification (Phase 5)
│
└── USB_CDC_*.md                    # Phase documentation, guides
```

### Key Files and Their Roles

| File | Lines | Purpose |
|------|-------|---------|
| **rx_usb.h** | ~1,200 | Public API, enums, typedefs, function declarations |
| **rx_usb.c** | ~800 | Ring buffer management, state tracking, port init |
| **rx_usb_cdc.c** | ~2,700 | CDC descriptors, class requests, enumeration |
| **rx_usb_hw.c** | ~950 | FIFO access (16-bit!), pipe config, low-level ops |
| **rx_usb_isr.c** | ~700 | Interrupt handlers (BRDY, BEMP, VBUS, Reset) |
| **rx72n_usb_regs.h** | ~1,000 | Register definitions (34 registers, 100+ bit fields) |
| **rx_log_usb.c** | ~680 | USB CDC logging backend (boot buffer, thread safety) |

---

## Adding New USB CDC Ports

### Current Limitations

**Hardware Limit**: RX72N USB0 has **9 pipes total**:
- 1 × Pipe 0 (control endpoint, reserved)
- 8 × Pipes 1-8 (configurable)

**CDC Requirement**: Each CDC-ACM port requires **3 pipes**:
- 1 × Interrupt IN (control endpoint)
- 1 × Bulk OUT (receive data)
- 1 × Bulk IN (transmit data)

**Maximum Ports**: (9 - 1) / 3 = **2.67 → 3 ports maximum**

### Adding a 4th Port (Hypothetical - Requires Hardware Change)

**Problem**: Cannot add 4th port with RX72N USB0 (insufficient pipes).

**Solutions**:
1. **Use RX72N USB1** (if available on your board variant)
   - Some RX72N packages have USB1 in addition to USB0
   - Requires separate USB driver instance
   - Doubles port capacity to 6 total

2. **Reduce pipes per CDC** (non-compliant, not recommended)
   - Remove interrupt endpoint (violates CDC-ACM spec)
   - Use only bulk endpoints
   - May not work with all host drivers

3. **Upgrade to different MCU** (e.g., RX72T has more resources)

### Modifying Port Configuration (Buffer Sizes, etc.)

**File**: `libs/rx_usb/inc/rx_usb.h`

**Current Configuration**:
```c
typedef enum : uint16_t {
  k_usb_port_proto_rx_size   = 1024,  // Protocol port RX
  k_usb_port_proto_tx_size   = 1024,  // Protocol port TX
  k_usb_port_decoded_rx_size = 256,   // Decoded port RX
  k_usb_port_decoded_tx_size = 512,   // Decoded port TX
  k_usb_port_log_rx_size     = 256,   // Log port RX
  k_usb_port_log_tx_size     = 1024,  // Log port TX
} rx_usb_port_buffer_sizes_t;
```

**To Change Buffer Sizes**:
1. Edit enum values above
2. Consider total RAM usage: Sum = 4352 bytes currently
3. Rebuild firmware
4. Test on hardware (verify no buffer overflows)

**Trade-offs**:
- **Larger buffers**: Better burst handling, more RAM usage
- **Smaller buffers**: Less RAM, risk of dropped data on bursts

---

## Debugging Techniques

### Enable Verbose Logging

**1. Increase Log Level** (build-time):

e2 studio: Project Properties → Compiler → Preprocessor → Add:
```
LOG_LEVEL=k_log_debug
```

**2. Enable USB CDC Logging** (see logs on USB):

Add:
```
USB_LOG_MIRROR=1
```

Result: Logs appear on **both** UART and USB CDC Port 2.

### Wireshark USB Capture (Linux)

**Capture USB traffic** for protocol analysis:

**1. Install usbmon kernel module**:
```bash
sudo modprobe usbmon
```

**2. Find USB bus**:
```bash
lsusb | grep 045b
# Output: Bus 001 Device 003: ID 045b:024f Renesas ...
```

**3. Start Wireshark capture**:
```bash
sudo wireshark
# Select interface: usbmon1 (for Bus 001)
# Filter: usb.device_address == 3
```

**4. Analyze packets**:
- **GET_DESCRIPTOR**: Device/Config/String descriptors
- **SET_ADDRESS**: Address assignment
- **SET_CONFIGURATION**: Configuration selection
- **SET_LINE_CODING**: Baud rate setting (informational)
- **Bulk OUT**: Data from host → device
- **Bulk IN**: Data from device → host

### Logic Analyzer Capture

**Signals to probe**:
- **USB_DP (PA4)**: Data Plus (differential pair)
- **USB_DM (PA5)**: Data Minus (differential pair)
- **USB_VBUS (PA6)**: VBUS detection (5V input)

**Tools**:
- Saleae Logic Analyzer (recommended, has USB decoder)
- Sigrok/PulseView (open-source alternative)

**Triggers**:
- Trigger on D+/D- differential transitions
- Look for USB packets (SYNC, PID, DATA, CRC)

### Common Debug Scenarios

#### Problem: USB Not Enumerating

**Symptoms**: No /dev/ttyACM* devices appear on Linux.

**Debug Steps**:

1. **Check VBUS detection**:
   ```c
   // In rx_usb.c or startup code
   if (!(USB0.INTSTS0.BIT.VBSTS)) {
     rx_log_error("USB", "VBUS not detected!");
   }
   ```

2. **Verify USB clock** (48 MHz from PLL):
   ```c
   // Check UPLLCR register (PLL control)
   rx_log_info("USB", "UPLLCR = 0x%04X", SYSTEM.UPLLCR.WORD);
   // Should be: 0x2401 (PLL enabled, locked)
   ```

3. **Check pull-up resistor** (D+ line, 1.5kΩ):
   ```c
   // Should be enabled after VBUS detect
   USB0.SYSCFG.BIT.DPRPU = 1;  // Enable D+ pull-up
   ```

4. **Monitor USB state transitions**:
   ```c
   rx_log_info("USB", "State: %d", rx_usb_get_state());
   // Should progress: Detached(0) → Attached(1) → ... → Configured(5)
   ```

#### Problem: Data Corruption

**Symptoms**: Garbled output, CRC errors, missing bytes.

**Root Cause (Phase 2 Fix)**: Byte-by-byte FIFO access instead of 16-bit word access.

**Verify Fix**:
```c
// In rx_usb_hw.c, rx_usb_fifo_read():
// CORRECT (16-bit word access):
for (uint32_t i = 0; i < len; i += 2) {
  const uint16_t word = usb0()->cfifo;  // Read 16 bits at once
  data[i] = (uint8_t)(word & 0xFF);
  if ((i + 1) < len) {
    data[i + 1] = (uint8_t)((word >> 8) & 0xFF);
  }
}

// WRONG (causes corruption):
for (uint32_t i = 0; i < len; i++) {
  data[i] = (uint8_t)(usb0()->cfifo & 0xFF);  // DON'T DO THIS!
}
```

**Check objdump** (verify compiler didn't optimize to byte access):
```bash
rx-elf-objdump -d build/firmware.elf | grep -A10 "rx_usb_fifo_read"
# Look for MOV.W instructions, NOT MOV.B
```

#### Problem: Logs Missing or Dropped

**Symptoms**: Some log messages don't appear on USB CDC Port 2.

**Causes**:

1. **Boot buffer overflow** (>512B during 0-200ms boot):
   ```c
   // Check statistics
   usb_log_stats_t stats;
   rx_log_usb_get_stats(&stats);
   rx_log_info("USB_LOG", "Dropped: %u / %u bytes",
               stats.dropped_bytes, stats.total_bytes);
   ```

2. **TX buffer full** (logging faster than USB can send):
   ```c
   // Increase TX buffer size in rx_usb.h:
   k_usb_port_log_tx_size = 2048,  // Was 1024
   ```

3. **USB not configured yet**:
   ```c
   // Check if USB ready
   if (!rx_usb_is_configured(k_usb_port_log)) {
     rx_log_warn("USB_LOG", "USB not ready, buffering logs");
   }
   ```

---

## Common Pitfalls

### 1. Byte-by-Byte FIFO Access ⚠️ CRITICAL

**Problem**: Accessing CFIFO register byte-by-byte causes data corruption.

**Why**: RX72N USB FIFO is 16-bit wide. Reading as bytes causes undefined behavior.

**Fix**: Always use 16-bit word access (see Phase 2 fixes in rx_usb_hw.c).

### 2. Missing BCLR Before Write ⚠️ HIGH

**Problem**: First packet contains stale data from previous transfer.

**Why**: FIFO not cleared before new write, leftover data appended.

**Fix**: Set BCLR bit, wait for hardware to clear it, then write:
```c
usb0()->cfifoctr |= k_usb_fifoctr_bclr;
while (usb0()->cfifoctr & k_usb_fifoctr_bclr);  // Wait for clear
// Now safe to write new data
```

### 3. Ignoring Return Values ⚠️ NASA Rule 7

**Problem**: Casting rx_usb_* return values to (void), missing errors.

**Why**: Violates NASA Power of 10 Rule 7 (check all return values).

**Fix**: Always check return values:
```c
// WRONG:
(void)rx_usb_write(port, data, len);

// CORRECT:
rx_err_t err = rx_usb_write(port, data, len);
if (err == k_rx_err_busy) {
  // Handle TX buffer full
} else if (err != k_rx_ok) {
  rx_log_error("USB", "Write failed: %d", err);
}
```

### 4. Calling rx_usb_write Before Enumeration

**Problem**: rx_usb_write returns k_rx_err_invalid_state.

**Why**: USB not in Configured state yet (enumeration takes 100-200ms).

**Fix**: Wait for enumeration or buffer logs:
```c
// Option 1: Wait for USB ready
while (!rx_usb_is_configured(k_usb_port_proto)) {
  tx_thread_sleep(10);  // 100ms polling
}

// Option 2: Use boot buffering (automatic in rx_log_usb.c)
rx_log_info("TAG", "This log is buffered if USB not ready");
```

### 5. Multi-Task Logging Without Mutex

**Problem**: Interleaved characters from multiple tasks logging concurrently.

**Why**: No mutual exclusion on LOG_PUTS macro.

**Fix**: Use Thread X mutex (already implemented in rx_log_usb.c):
```c
// In rx_log_usb.c (already done):
void rx_log_usb_puts(const char* str) {
  tx_mutex_get(&s_log_mutex, TX_WAIT_FOREVER);  // Lock
  internal_write_usb(str, strlen(str));
  tx_mutex_put(&s_log_mutex);  // Unlock
}
```

### 6. Modifying USB Registers from Non-ISR Context

**Problem**: Race condition between ISR and application code.

**Why**: USB interrupts modify register state concurrently.

**Fix**: Disable interrupts or use atomic operations:
```c
// Option 1: Disable USB interrupts temporarily
uint32_t saved = usb0()->intenb0;
usb0()->intenb0 = 0;  // Disable all interrupts
// ... modify registers ...
usb0()->intenb0 = saved;  // Re-enable

// Option 2: Use atomic bit operations (if supported)
__sync_fetch_and_or(&usb0()->intenb0, k_usb_intsts0_brdy);
```

### 7. Forgetting to Handle Cable Disconnect

**Problem**: Application doesn't detect USB cable unplug.

**Why**: No callback registered for k_usb_event_detached.

**Fix**: Register callback and handle event:
```c
void usb_event_callback(rx_usb_port_id_t port,
                        rx_usb_event_t event, void* ctx)
{
  if (event == k_usb_event_detached) {
    // Abort pending transfers
    // Switch to UART-only logging
    // Notify application layer
    g_usb_connected = false;
  }
}

// Register callback during init:
rx_usb_config_t config = {
  .callback = usb_event_callback,
  .ctx = NULL
};
rx_usb_init(&config);
```

---

## Performance Optimization

### 1. Reduce Log Volume (High-Frequency Paths)

**Problem**: Excessive logging in ISRs or high-frequency loops.

**Solution**: Use compile-time filtering:
```c
// Change global log level to reduce verbosity
// In e2 studio: -DLOG_LEVEL=k_log_warn  (only warnings and errors)

// Or conditionally compile verbose logs:
#if LOG_LEVEL >= k_log_debug
  rx_log_debug("MOTOR", "PID output: %.3f", output);
#endif
```

### 2. Increase Buffer Sizes for Burst Traffic

**Problem**: Dropped packets during telemetry bursts.

**Solution**: Increase TX buffer size for affected port:
```c
// In rx_usb.h:
typedef enum : uint16_t {
  k_usb_port_proto_tx_size = 2048,  // Was 1024
} rx_usb_port_buffer_sizes_t;
```

**Trade-off**: Uses more RAM (512KB total on RX72N).

### 3. Use DMA for FIFO Transfers (Future Optimization)

**Problem**: CPU-intensive FIFO read/write in ISR.

**Solution** (not yet implemented): Configure DMA controller to transfer FIFO ↔ RAM:
```c
// Pseudocode (DMA not yet implemented):
// Configure DMAC Channel 0 for USB FIFO → RAM
DMAC0.DMAMD.BIT.SZ = 1;  // 16-bit transfer
DMAC0.DMASAR = (uint32_t)&USB0.CFIFO;  // Source: FIFO
DMAC0.DMADAR = (uint32_t)rx_buffer;    // Destination: RAM
DMAC0.DMATCR = (len / 2);  // Transfer count (16-bit words)
DMAC0.DMAMD.BIT.DE = 1;  // Enable DMA
```

**Benefit**: Reduces ISR CPU usage from ~30% to ~5%.

### 4. Batch Small Writes

**Problem**: Calling rx_usb_write() for every log line (overhead).

**Solution**: Buffer multiple log lines, write in batch:
```c
// Accumulate logs in buffer:
static char log_batch[512];
static uint16_t log_batch_len = 0;

// Add log line:
log_batch_len += snprintf(&log_batch[log_batch_len],
                           sizeof(log_batch) - log_batch_len,
                           "[INFO] %s\r\n", msg);

// Flush when buffer >75% full or timeout:
if (log_batch_len > 384 || timeout_expired) {
  rx_usb_write(k_usb_port_log, log_batch, log_batch_len);
  log_batch_len = 0;
}
```

**Benefit**: Reduces USB overhead, improves throughput.

---

## Integration Guide

### Integrating with ThreadX Tasks

**Example: Motor Control Task**

```c
// File: src/tasks/motor_control_task.c

#include "rx_usb.h"
#include "motor_control_task.h"

static TX_THREAD motor_control_thread;
static uint8_t motor_control_stack[2048];

static void motor_control_task_entry(ULONG thread_input)
{
  (void)thread_input;

  // Wait for USB enumeration (optional)
  while (!rx_usb_is_configured(k_usb_port_proto)) {
    tx_thread_sleep(10);  // 100ms polling
  }

  rx_log_info("MOTOR", "USB ready, starting motor control");

  while (1) {
    // Read motor commands from USB CDC Port 0
    uint8_t rx_buf[256];
    uint32_t rx_len;
    rx_err_t err = rx_usb_read(k_usb_port_proto, rx_buf,
                                sizeof(rx_buf), &rx_len);

    if (err == k_rx_ok && rx_len > 0) {
      // Process command (decode nanopb, execute)
      process_motor_command(rx_buf, rx_len);

      // Send response
      uint8_t tx_buf[128];
      uint32_t tx_len = build_response(tx_buf, sizeof(tx_buf));
      rx_usb_write(k_usb_port_proto, tx_buf, tx_len);
    }

    // Send periodic telemetry
    send_telemetry();

    tx_thread_sleep(1);  // 10ms loop rate (100 Hz)
  }
}

void motor_control_task_create(void)
{
  UINT status = tx_thread_create(&motor_control_thread,
                                 "motor_control",
                                 motor_control_task_entry,
                                 0,  // Input
                                 motor_control_stack,
                                 sizeof(motor_control_stack),
                                 15,  // Priority
                                 15,  // Preempt threshold
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);

  if (status != TX_SUCCESS) {
    rx_log_error("MOTOR", "Task creation failed: %d", status);
  }
}
```

### Integrating with nanopb Protocol

**Example: Protobuf Encode/Decode**

```c
#include "rx_usb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "star/v1/motor_control.pb.h"  // Generated protobuf header

// Send protobuf message over USB CDC Port 0
void send_motor_telemetry(const star_v1_MotorTelemetry* telemetry)
{
  uint8_t buffer[128];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  if (pb_encode(&stream, star_v1_MotorTelemetry_fields, telemetry)) {
    rx_err_t err = rx_usb_write(k_usb_port_proto, buffer, stream.bytes_written);
    if (err != k_rx_ok) {
      rx_log_warn("PROTO", "USB write failed: %d", err);
    }
  } else {
    rx_log_error("PROTO", "Encode failed: %s", PB_GET_ERROR(&stream));
  }
}

// Receive protobuf message from USB CDC Port 0
bool receive_motor_command(star_v1_MotorCommand* command)
{
  uint8_t buffer[256];
  uint32_t len;

  rx_err_t err = rx_usb_read(k_usb_port_proto, buffer, sizeof(buffer), &len);
  if (err != k_rx_ok || len == 0) {
    return false;  // No data
  }

  pb_istream_t stream = pb_istream_from_buffer(buffer, len);
  if (!pb_decode(&stream, star_v1_MotorCommand_fields, command)) {
    rx_log_error("PROTO", "Decode failed: %s", PB_GET_ERROR(&stream));
    return false;
  }

  return true;
}
```

---

## Testing Strategy

### Unit Tests (Mock USB Registers)

**Approach**: Mock USB0 registers, test logic without hardware.

**Example** (using Unity test framework):

```c
// File: tests/test_usb_cdc.c

#include "unity.h"
#include "rx_usb.h"
#include "mock_rx72n_usb_regs.h"  // Mock registers

void test_rx_usb_write_null_ptr(void)
{
  rx_err_t err = rx_usb_write(k_usb_port_proto, NULL, 100);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_rx_usb_write_invalid_port(void)
{
  uint8_t data[10];
  rx_err_t err = rx_usb_write(99, data, 10);  // Invalid port
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_rx_usb_fifo_read_16bit_access(void)
{
  // Mock CFIFO register to return 0x1234
  mock_usb0_cfifo_set_next_read(0x1234);

  uint8_t data[2];
  rx_usb_fifo_read(data, 2);

  TEST_ASSERT_EQUAL_HEX8(0x34, data[0]);  // Low byte
  TEST_ASSERT_EQUAL_HEX8(0x12, data[1]);  // High byte
}
```

### Integration Tests (Hardware Required)

**Test Plan**: See [USB_CDC_PHASE2_TESTING_GUIDE.md](USB_CDC_PHASE2_TESTING_GUIDE.md)

**Key Tests**:
1. USB enumeration (3 ports appear)
2. Single packet IN/OUT
3. Multi-packet transfer (1KB, 1MB)
4. 3-port simultaneous operation
5. Cable disconnect/reconnect
6. 1-hour stability test (99.99% success rate)

### Continuous Integration (GitHub Actions)

**Workflow** (future - Phase 4):

```yaml
# .github/workflows/usb_cdc_tests.yml
name: USB CDC Tests

on: [push, pull_request]

jobs:
  unit_tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install GNURX toolchain
        run: sudo apt-get install gcc-rx-elf
      - name: Build tests
        run: |
          cd tests
          cmake ..
          make -j$(nproc)
      - name: Run unit tests
        run: ctest --output-on-failure

  hardware_tests:
    runs-on: self-hosted  # Requires RX72N board
    if: github.ref == 'refs/heads/main'
    steps:
      - uses: actions/checkout@v3
      - name: Flash firmware
        run: ./scripts/flash_rx72n.sh
      - name: Run hardware tests
        run: pytest tests/hardware/test_usb_cdc.py
```

---

## References

**Implementation Files**:
- [rx_usb.h](libs/rx_usb/inc/rx_usb.h) - Public API
- [rx_usb_hw.c](libs/rx_usb/src/rx_usb_hw.c) - FIFO access (Phase 2 fixes)
- [rx_log_usb.c](libs/rx_core/src/rx_log_usb.c) - Logging backend (Phase 3)

**Documentation**:
- [USB_CDC_STATUS.md](USB_CDC_STATUS.md) - Current implementation status
- [USB_CDC_USER_GUIDE.md](USB_CDC_USER_GUIDE.md) - End-user guide
- [09_usb_cdc_protocol.tex](/workspaces/STAR/docs/sections/09_usb_cdc_protocol.tex) - Technical spec

**Specifications**:
- RX72N User's Manual: Chapter 40 - USB 2.0 Full-Speed Module
- USB 2.0 Specification: Chapter 9 - Device Framework
- USB CDC-ACM Specification: Class Definitions for Communications Devices v1.2

**Standards**:
- [NASA Power of 10](docs/sections/06_nasa_power_of_10.tex) - Safety-critical coding rules
- [DOXYGEN_ROADMAP.md](DOXYGEN_ROADMAP.md) - Documentation standards

---

**Document Version**: 1.0
**Last Updated**: 2026-02-05
**Firmware Branch**: bsikar/161_usb_cdc_debug
