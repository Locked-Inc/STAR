# USB CDC Phase 3: Debug Logging Integration Roadmap

## Overview

**Goal**: Replace UART logging with USB CDC Port 2 (Log Port) for all debug output

**Prerequisites**: Phase 2 hardware testing must pass (all 10 tests, ≥99.99% reliability)

**Priority**: HIGH - Enables real-time debugging without external UART adapter

**Status**: ⬜ NOT STARTED (Blocked by Phase 2 hardware testing)

**Estimated Duration**: 2-3 hours

---

## Phase 3 Objectives

### Primary Goals

1. **Integrate USB CDC with logging system**
   - Connect `rx_log_*()` macros to USB CDC backend
   - Add compile-time backend selection (`RX_LOG_USE_USB_CDC`)
   - Maintain UART backend as fallback option

2. **Wait for USB enumeration**
   - Buffer early boot logs until USB ready
   - Flush buffered logs after enumeration complete
   - Prevent log loss during USB initialization

3. **Non-blocking writes**
   - Return `k_rx_err_busy` if TX buffer full
   - Implement optional log dropping policy
   - Add backpressure handling

4. **Thread-safe operation**
   - Add mutex protection for multi-threaded logging
   - Ensure atomic writes across tasks
   - Prevent interleaved log messages

---

## Current Logging System Architecture

### Current Implementation

**Location**: `libs/rx_log/inc/rx_log.h`, `libs/rx_log/src/rx_log.c`

**Current Backend**: UART-only (SCI12)

**Log Levels**:
```c
typedef enum : uint8_t {
  k_log_level_none  = 0,  /**< No logging */
  k_log_level_error = 1,  /**< Errors only */
  k_log_level_warn  = 2,  /**< Warnings and errors */
  k_log_level_info  = 3,  /**< Info, warnings, errors */
  k_log_level_debug = 4,  /**< All messages */
} rx_log_level_t;
```

**Current API**:
```c
void rx_log_error(const char* tag, const char* msg);
void rx_log_warn(const char* tag, const char* msg);
void rx_log_info(const char* tag, const char* msg);
void rx_log_debug(const char* tag, const char* msg);
```

**Current Output Format**:
```
[12345.678] [ERROR] [motor_control] Encoder timeout
[12345.890] [WARN ] [usb_cdc     ] TX buffer 90% full
[12346.012] [INFO ] [main        ] System initialized
```

---

## Phase 3 Tasks

### Task 3.1: Add USB CDC Backend to Logging System

**Goal**: Create USB CDC backend alongside existing UART backend

**Files to Modify**:
- `libs/rx_log/inc/rx_log.h` - Add backend selection
- `libs/rx_log/inc/rx_log_config.h` - Add compile-time config
- `libs/rx_log/src/rx_log.c` - Add backend dispatch

**New Configuration** (`rx_log_config.h`):
```c
/**
 * @brief Logging backend selection
 *
 * Choose one:
 * - RX_LOG_BACKEND_UART - Use UART (SCI12) for logging
 * - RX_LOG_BACKEND_USB_CDC - Use USB CDC Port 2 for logging
 */
#define RX_LOG_BACKEND_UART     0
#define RX_LOG_BACKEND_USB_CDC  1

/* Select active backend (compile-time) */
#ifndef RX_LOG_BACKEND
#define RX_LOG_BACKEND RX_LOG_BACKEND_UART  /* Default: UART */
#endif

/* USB CDC backend settings */
#if (RX_LOG_BACKEND == RX_LOG_BACKEND_USB_CDC)
#define RX_LOG_USB_CDC_PORT      2           /* Use Port 2 (Log Port) */
#define RX_LOG_USB_BOOT_BUFFER   512         /* Buffer logs during USB init */
#define RX_LOG_USB_DROP_ON_FULL  1           /* Drop logs if TX full (no blocking) */
#endif
```

**Backend Dispatch** (`rx_log.c`):
```c
static rx_err_t internal_log_write(const char* msg, uint32_t len)
{
#if (RX_LOG_BACKEND == RX_LOG_BACKEND_UART)
  /* Existing UART implementation */
  return rx_uart_write(s_log_uart, (const uint8_t*)msg, len);

#elif (RX_LOG_BACKEND == RX_LOG_BACKEND_USB_CDC)
  /* New USB CDC implementation */
  if (!s_usb_enumerated) {
    /* Buffer until USB ready */
    return internal_buffer_log(msg, len);
  }

  rx_err_t err = rx_usb_cdc_write(RX_LOG_USB_CDC_PORT,
                                  (const uint8_t*)msg, len);
  if (err == k_rx_err_busy && RX_LOG_USB_DROP_ON_FULL) {
    s_log_stats.dropped_count++;
    return k_rx_ok;  /* Drop log, don't block */
  }

  return err;
#else
#error "Invalid RX_LOG_BACKEND selection"
#endif
}
```

**Success Criteria**:
- [ ] Logs appear on USB CDC Port 2
- [ ] Compile-time backend selection works
- [ ] UART backend still compiles and works

---

### Task 3.2: Implement Boot Log Buffering

**Goal**: Buffer logs during USB initialization, flush after enumeration

**Problem**: USB CDC enumeration takes ~500ms after power-on. Early boot logs (first 500ms) would be lost if sent directly to USB.

**Solution**: Ring buffer for early logs, flush after USB ready

**New Structures**:
```c
/**
 * @brief Boot log buffer (used until USB enumeration complete)
 */
typedef struct {
  char data[RX_LOG_USB_BOOT_BUFFER];  /**< Ring buffer for early logs */
  uint16_t head;                       /**< Write index */
  uint16_t tail;                       /**< Read index (for flush) */
  uint16_t count;                      /**< Number of buffered bytes */
  bool overflow;                       /**< True if buffer overflowed */
} rx_log_boot_buffer_t;

static rx_log_boot_buffer_t s_boot_buffer = {0};
static volatile bool s_usb_enumerated = false;
```

**Buffer Implementation**:
```c
/**
 * @brief Buffer log message until USB ready
 *
 * @param[in] msg Message string
 * @param[in] len Message length
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, buffered
 * @retval k_rx_err_overflow Buffer full, log dropped
 */
static rx_err_t internal_buffer_log(const char* msg, uint32_t len)
{
  /* Check space available */
  if ((s_boot_buffer.count + len) > RX_LOG_USB_BOOT_BUFFER) {
    s_boot_buffer.overflow = true;
    s_log_stats.dropped_count++;
    return k_rx_err_overflow;
  }

  /* Write to ring buffer */
  for (uint32_t i = 0; i < len; i++) {
    s_boot_buffer.data[s_boot_buffer.head] = msg[i];
    s_boot_buffer.head = (s_boot_buffer.head + 1) % RX_LOG_USB_BOOT_BUFFER;
  }

  s_boot_buffer.count += len;
  return k_rx_ok;
}

/**
 * @brief Flush buffered logs to USB after enumeration
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All logs flushed
 * @retval k_rx_err_busy TX buffer full, try again
 */
static rx_err_t internal_flush_boot_logs(void)
{
  if (s_boot_buffer.count == 0) {
    return k_rx_ok;  /* Nothing to flush */
  }

  /* Send buffered data in chunks */
  while (s_boot_buffer.count > 0) {
    uint32_t chunk_size = (s_boot_buffer.count > 64) ? 64 : s_boot_buffer.count;

    rx_err_t err = rx_usb_cdc_write(RX_LOG_USB_CDC_PORT,
                                    (const uint8_t*)&s_boot_buffer.data[s_boot_buffer.tail],
                                    chunk_size);
    if (err != k_rx_ok) {
      return err;  /* TX buffer full, try again later */
    }

    s_boot_buffer.tail = (s_boot_buffer.tail + chunk_size) % RX_LOG_USB_BOOT_BUFFER;
    s_boot_buffer.count -= chunk_size;
  }

  /* Log overflow warning if buffer overflowed */
  if (s_boot_buffer.overflow) {
    rx_log_warn("rx_log", "Boot log buffer overflowed, some logs dropped");
  }

  return k_rx_ok;
}
```

**USB Enumeration Callback**:
```c
/**
 * @brief Notify logging system that USB is ready
 *
 * Called by USB stack after successful enumeration.
 * Triggers flush of buffered boot logs.
 */
void rx_log_notify_usb_ready(void)
{
  s_usb_enumerated = true;
  internal_flush_boot_logs();
}
```

**Success Criteria**:
- [ ] Early boot logs (first 500ms) buffered correctly
- [ ] All buffered logs flushed after USB enumeration
- [ ] Overflow detected if buffer too small
- [ ] No log loss during boot

---

### Task 3.3: Add Non-Blocking Write with Backpressure

**Goal**: Prevent blocking when USB TX buffer full

**Problem**: If logs generated faster than USB can send (>12 Mbps), TX buffer fills. Current implementation would block indefinitely.

**Solution**: Return `k_rx_err_busy` if TX full, implement drop policy

**Backpressure Policies**:

**Policy 1: Drop New Logs** (Recommended):
```c
#define RX_LOG_USB_DROP_ON_FULL  1

/* In internal_log_write() */
rx_err_t err = rx_usb_cdc_write(RX_LOG_USB_CDC_PORT, msg, len);
if (err == k_rx_err_busy) {
  s_log_stats.dropped_count++;
  return k_rx_ok;  /* Drop log, don't block */
}
```

**Policy 2: Retry with Timeout**:
```c
#define RX_LOG_USB_DROP_ON_FULL  0
#define RX_LOG_USB_RETRY_MS      10

/* In internal_log_write() */
uint32_t start_ms = tx_time_get();
rx_err_t err;
do {
  err = rx_usb_cdc_write(RX_LOG_USB_CDC_PORT, msg, len);
  if (err == k_rx_ok) break;
  tx_thread_sleep(1);  /* 1 tick = 1ms */
} while ((tx_time_get() - start_ms) < RX_LOG_USB_RETRY_MS);

if (err != k_rx_ok) {
  s_log_stats.dropped_count++;
}
```

**Policy 3: Rate Limiting**:
```c
#define RX_LOG_USB_MAX_LOGS_PER_SEC  1000

static uint32_t s_log_count_this_sec = 0;
static uint32_t s_last_reset_sec = 0;

/* In internal_log_write() */
uint32_t current_sec = tx_time_get() / 1000;
if (current_sec != s_last_reset_sec) {
  s_log_count_this_sec = 0;
  s_last_reset_sec = current_sec;
}

if (s_log_count_this_sec >= RX_LOG_USB_MAX_LOGS_PER_SEC) {
  s_log_stats.rate_limited_count++;
  return k_rx_err_busy;  /* Rate limit exceeded */
}

s_log_count_this_sec++;
```

**Recommended**: Use Policy 1 (Drop on Full) for simplicity and predictability.

**Success Criteria**:
- [ ] Logging never blocks caller
- [ ] Dropped log count tracked
- [ ] System remains responsive under heavy logging

---

### Task 3.4: Add Thread-Safe Logging with Mutex

**Goal**: Protect logging from concurrent access by multiple tasks

**Problem**: ThreadX has 7 tasks that may log simultaneously. Without synchronization, log messages could interleave mid-line.

**Solution**: Add mutex around `internal_log_write()`

**Mutex Implementation**:
```c
static TX_MUTEX s_log_mutex;

/**
 * @brief Initialize logging system
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 */
rx_err_t rx_log_init(void)
{
  /* Create mutex for thread-safe logging */
  UINT status = tx_mutex_create(&s_log_mutex, "log_mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    return k_rx_err_init_failed;
  }

#if (RX_LOG_BACKEND == RX_LOG_BACKEND_UART)
  /* Initialize UART backend */
  return internal_init_uart_backend();

#elif (RX_LOG_BACKEND == RX_LOG_BACKEND_USB_CDC)
  /* USB CDC initialized separately in main() */
  return k_rx_ok;
#endif
}

/**
 * @brief Thread-safe log write
 *
 * @param[in] msg Message string
 * @param[in] len Message length
 * @return rx_err_t Error code
 */
static rx_err_t internal_log_write_safe(const char* msg, uint32_t len)
{
  /* Acquire mutex (block until available) */
  UINT status = tx_mutex_get(&s_log_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_timeout;  /* Shouldn't happen with TX_WAIT_FOREVER */
  }

  /* Write log */
  rx_err_t err = internal_log_write(msg, len);

  /* Release mutex */
  tx_mutex_put(&s_log_mutex);

  return err;
}
```

**ISR-Safe Logging** (Optional):
```c
/**
 * @brief ISR-safe logging (no mutex, best-effort)
 *
 * Use from ISR context where mutex not available.
 * May result in interleaved logs if multiple ISRs fire.
 */
void rx_log_isr(const char* tag, const char* msg)
{
  /* Skip mutex, write directly */
  internal_log_write_unsafe(tag, msg);
}
```

**Success Criteria**:
- [ ] No interleaved log messages from concurrent tasks
- [ ] Mutex overhead acceptable (<10µs per log)
- [ ] No deadlocks or priority inversion

---

### Task 3.5: Add USB CDC Logging Statistics

**Goal**: Track logging performance and dropped messages

**Statistics Structure**:
```c
/**
 * @brief Logging system statistics
 */
typedef struct {
  uint32_t total_logs;          /**< Total log messages sent */
  uint32_t dropped_count;       /**< Logs dropped due to TX full */
  uint32_t rate_limited_count;  /**< Logs dropped due to rate limit */
  uint32_t boot_buffered;       /**< Logs buffered during USB init */
  uint32_t boot_overflow;       /**< Boot buffer overflows */
  uint32_t usb_write_errors;    /**< USB write errors */
} rx_log_stats_t;

static rx_log_stats_t s_log_stats = {0};

/**
 * @brief Get logging system statistics
 *
 * @param[out] stats Statistics structure to fill
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 */
rx_err_t rx_log_get_stats(rx_log_stats_t* stats)
{
  RX_CHECK_NULL_PTR(stats);

  tx_mutex_get(&s_log_mutex, TX_WAIT_FOREVER);
  *stats = s_log_stats;
  tx_mutex_put(&s_log_mutex);

  return k_rx_ok;
}
```

**Success Criteria**:
- [ ] Statistics accurate
- [ ] Statistics queryable at runtime
- [ ] Useful for debugging log loss

---

### Task 3.6: Update CMake Build System

**Goal**: Add compile-time backend selection to build system

**CMake Changes** (`CMakeLists.txt`):
```cmake
# Logging backend selection
option(RX_LOG_USE_USB_CDC "Use USB CDC for logging (default: UART)" OFF)

if(RX_LOG_USE_USB_CDC)
  target_compile_definitions(${TARGET_NAME} PRIVATE
    RX_LOG_BACKEND=1  # USB CDC backend
  )
  message(STATUS "Logging backend: USB CDC Port 2")
else()
  target_compile_definitions(${TARGET_NAME} PRIVATE
    RX_LOG_BACKEND=0  # UART backend (default)
  )
  message(STATUS "Logging backend: UART (SCI12)")
endif()
```

**Build Commands**:
```bash
# Build with UART logging (default)
cmake .. && make

# Build with USB CDC logging
cmake .. -DRX_LOG_USE_USB_CDC=ON && make
```

**Success Criteria**:
- [ ] CMake option works
- [ ] Both backends build without warnings
- [ ] Build system selects correct backend

---

## Integration with USB CDC Stack

### USB CDC Port Assignments

**Port 0** (Protocol Port):
- Purpose: nanopb Protocol Buffers over SPI
- Pipes: Bulk IN 1, Bulk OUT 2
- Used by: Gateway ↔ RX72N communication

**Port 1** (Decoded Port):
- Purpose: Human-readable command interface
- Pipes: Bulk IN 4, Bulk OUT 5
- Used by: Interactive debugging (unused for now)

**Port 2** (Log Port):
- Purpose: Debug logging output (this phase)
- Pipes: Bulk IN 7, Bulk OUT 8
- Used by: `rx_log_*()` → USB CDC backend

### USB Enumeration Sequence

1. **Power-On** (t=0ms)
   - Hardware init → `hardware_init()`
   - ThreadX kernel start → `tx_kernel_enter()`
   - USB stack init → `rx_usb_cdc_init()`

2. **USB Enumeration** (t=0-500ms)
   - Host detects device, requests descriptors
   - Device configures pipes, enables interrupts
   - CDC ports not ready for data

3. **Enumeration Complete** (t=500ms)
   - Host configures device
   - CDC ports ready for bulk transfers
   - `rx_log_notify_usb_ready()` called

4. **Logging Active** (t>500ms)
   - Boot logs flushed from buffer
   - New logs sent directly to USB
   - Real-time logging operational

---

## Testing Requirements

### Unit Tests

**Test 1: Backend Selection**
- Build with UART backend, verify logs on UART
- Build with USB CDC backend, verify logs on USB
- Verify no compilation errors

**Test 2: Boot Buffer**
- Generate 100 logs before USB enumeration
- Verify all logs buffered correctly
- Trigger enumeration, verify logs flushed
- Check no logs lost

**Test 3: Buffer Overflow**
- Generate 1KB of logs before USB enumeration (>512B buffer)
- Verify overflow detected
- Verify overflow warning logged after flush

**Test 4: Non-Blocking Write**
- Fill USB TX buffer completely
- Generate new log
- Verify log dropped (not blocked)
- Verify dropped count incremented

**Test 5: Thread Safety**
- Spawn 5 tasks, each logs 100 messages
- Verify 500 total logs received
- Verify no interleaved messages

**Test 6: Statistics**
- Generate 100 logs, drop 10, buffer 20
- Query statistics
- Verify counts correct

### Integration Tests

**Test 7: End-to-End Logging**
- Boot system with USB CDC backend
- Connect USB to host
- Verify logs appear in serial terminal
- Verify timestamps correct
- Verify log levels filter correctly

**Test 8: Hot-Plug**
- Disconnect USB cable during logging
- Reconnect USB
- Verify logging resumes
- Verify no crash or hang

**Test 9: Performance**
- Generate 1000 logs/sec for 60 seconds
- Verify >99% logs delivered
- Verify dropped count <1%

---

## Success Criteria

**Phase 3 Complete When**:

- [ ] USB CDC backend compiles and links
- [ ] Boot logs buffered and flushed correctly
- [ ] No blocking on full TX buffer
- [ ] Thread-safe logging works
- [ ] Statistics tracking accurate
- [ ] All 9 tests pass
- [ ] Documentation updated
- [ ] Code reviewed and committed

---

## Known Limitations

**Hardware**:
- USB Full-Speed max 12 Mbps (1.5 MB/s)
- Logging throughput limited by USB bandwidth
- Heavy logging may require rate limiting

**Software**:
- Boot buffer fixed size (512B default)
- Logs exceeding buffer size dropped
- No log persistence across resets

**ThreadX**:
- Mutex adds overhead (~5-10µs per log)
- ISR-safe logging not mutex-protected
- Priority inversion possible under heavy load

---

## Files to Modify

| File | Purpose | Changes |
|------|---------|---------|
| `libs/rx_log/inc/rx_log.h` | Public API | Add backend enums, stats API |
| `libs/rx_log/inc/rx_log_config.h` | Configuration | Add USB CDC backend config |
| `libs/rx_log/src/rx_log.c` | Implementation | Add backend dispatch, buffering, mutex |
| `CMakeLists.txt` | Build system | Add `RX_LOG_USE_USB_CDC` option |
| `src/main.c` | Main initialization | Add `rx_log_notify_usb_ready()` call |

---

## Documentation Updates

**Phase 5** (after Phase 3 complete):
- Update `docs/sections/05_usb_cdc_protocol.tex` with log port usage
- Add logging architecture diagram
- Document backend selection procedure
- Add troubleshooting guide for USB CDC logging

---

## Estimated Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| 3.1 | 45 min | Phase 2 testing complete |
| 3.2 | 30 min | Task 3.1 |
| 3.3 | 20 min | Task 3.2 |
| 3.4 | 20 min | Task 3.1 |
| 3.5 | 15 min | Task 3.1 |
| 3.6 | 10 min | Task 3.1 |
| Testing | 30 min | All tasks |
| **Total** | **2.5 hours** | Phase 2 validated |

---

## Risk Assessment

**Risk 1: USB Enumeration Delay**
- **Impact**: Boot logs lost if buffer too small
- **Mitigation**: Increase buffer size, add overflow detection
- **Likelihood**: Medium

**Risk 2: Logging Overhead**
- **Impact**: Mutex adds latency, may miss real-time deadlines
- **Mitigation**: Provide ISR-safe logging, measure overhead
- **Likelihood**: Low

**Risk 3: USB Disconnect During Logging**
- **Impact**: Logs lost, potential crash if not handled
- **Mitigation**: Add cable detect, buffer logs during disconnect
- **Likelihood**: Medium

---

## Next Steps

1. **Wait for Phase 2 Testing Results**
   - All 10 hardware tests must pass
   - Bulk transfers must be ≥99.99% reliable
   - If tests fail, return to Phase 2 debugging

2. **Begin Phase 3 Implementation**
   - Start with Task 3.1 (backend selection)
   - Implement tasks 3.2-3.6 sequentially
   - Test each task before proceeding

3. **Validate on Hardware**
   - Run all 9 Phase 3 tests
   - Measure logging performance
   - Verify no regressions

4. **Proceed to Phase 4**
   - Integration testing
   - Stress testing
   - Documentation

---

**Created**: 2026-02-05
**Last Updated**: 2026-02-05
**Status**: ⬜ NOT STARTED (Blocked by Phase 2 hardware testing)
