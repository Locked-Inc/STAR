# USB CDC Phase 4: Testing and Validation Roadmap

## Overview

**Goal**: Comprehensive testing and validation of USB CDC implementation across all 3 ports

**Prerequisites**:
- Phase 2 hardware testing passed (bulk transfers reliable)
- Phase 3 implementation complete (debug logging integrated)

**Priority**: HIGH - Ensures system reliability before production deployment

**Status**: ⬜ NOT STARTED (Blocked by Phases 2-3)

**Estimated Duration**: 3-4 hours

---

## Phase 4 Objectives

### Primary Goals

1. **Unit Testing**
   - Test individual USB CDC functions in isolation
   - Mock hardware dependencies for deterministic testing
   - Achieve >90% code coverage

2. **Integration Testing**
   - Test 3-port simultaneous operation
   - Verify port independence (no crosstalk)
   - Test USB stack integration with ThreadX

3. **Hardware Validation**
   - Large data transfers (1MB+) with CRC32 verification
   - Stress testing under high load
   - Cable hot-plug/unplug scenarios

4. **USB Protocol Compliance**
   - Wireshark/usbmon traffic analysis
   - USB 2.0 spec compliance verification
   - CDC-ACM spec compliance verification

5. **Performance Benchmarking**
   - Measure throughput (bytes/sec per port)
   - Measure latency (request → response time)
   - Measure CPU utilization during transfers

---

## Current Testing Status

### Existing Tests

**Phase 2 Tests** (10 tests, hardware-focused):
- USB enumeration
- Single/multi-packet transfers
- 3-port simultaneous
- Large transfers (1MB)
- Cable disconnect
- Wireshark capture
- 1-hour stability

**Phase 3 Tests** (9 tests, logging-focused):
- Backend selection
- Boot buffer
- Buffer overflow
- Non-blocking write
- Thread safety
- Statistics
- End-to-end logging
- Hot-plug
- Performance (1000 logs/sec)

**Phase 4 Adds**:
- Automated test suite
- Mock-based unit tests
- Integration test framework
- Continuous stress testing
- Protocol compliance validation

---

## Phase 4 Tasks

### Task 4.1: Create Unit Test Framework

**Goal**: Establish mock-based unit testing for USB CDC stack

**Duration**: 1 hour

**Test Framework Structure**:
```
tests/
  unit/
    test_rx_usb_cdc.c       # CDC class layer tests
    test_rx_usb_hw.c        # Hardware abstraction tests
    test_rx_usb_isr.c       # Interrupt handler tests
    test_rx_log_usb.c       # USB logging backend tests
  mocks/
    mock_rx72n_usb_regs.c   # Mock USB registers
    mock_tx_kernel.c        # Mock ThreadX kernel
    mock_crc32.c            # Mock CRC32 module
  integration/
    test_usb_cdc_3port.c    # 3-port integration test
    test_usb_logging.c      # Logging integration test
  CMakeLists.txt            # Test build configuration
```

**Mock Implementation Example**:
```c
/**
 * @file mock_rx72n_usb_regs.c
 * @brief Mock USB0 registers for unit testing
 */

#include "rx72n_usb_regs.h"
#include <stdint.h>
#include <stdbool.h>

/* Mock USB0 registers */
static USB0_Type s_mock_usb0 = {0};
static bool s_usb_initialized = false;

/**
 * @brief Get mock USB0 register structure
 */
USB0_Type* usb0(void)
{
  return &s_mock_usb0;
}

/**
 * @brief Reset mock USB registers to defaults
 */
void mock_usb_reset(void)
{
  s_mock_usb0 = (USB0_Type){0};
  s_usb_initialized = false;
}

/**
 * @brief Set mock USB enumeration state
 */
void mock_usb_set_enumerated(bool enumerated)
{
  s_usb_initialized = enumerated;
}

/**
 * @brief Simulate FIFO data available
 */
void mock_usb_fifo_push_data(const uint8_t* data, uint32_t len)
{
  /* Simulate data in CFIFO register */
  for (uint32_t i = 0; i < len; i += 2) {
    uint16_t word = data[i];
    if ((i + 1) < len) {
      word |= ((uint16_t)data[i + 1] << 8);
    }
    s_mock_usb0.cfifo = word;
  }
  s_mock_usb0.cfifoctr = (uint16_t)(len & k_usb_fifoctr_dtln_mask);
}
```

**Unit Test Example**:
```c
/**
 * @file test_rx_usb_hw.c
 * @brief Unit tests for USB hardware abstraction layer
 */

#include "rx_usb_hw.h"
#include "mock_rx72n_usb_regs.h"
#include <assert.h>
#include <string.h>

/**
 * @brief Test: FIFO read with 16-bit access
 */
void test_usb_fifo_read_16bit(void)
{
  /* Setup */
  mock_usb_reset();
  uint8_t expected[64] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  mock_usb_fifo_push_data(expected, 6);

  /* Execute */
  uint8_t actual[64] = {0};
  uint32_t bytes_read = rx_usb_hw_fifo_read(actual, 6);

  /* Verify */
  assert(bytes_read == 6);
  assert(memcmp(actual, expected, 6) == 0);
}

/**
 * @brief Test: FIFO write with BCLR sequence
 */
void test_usb_fifo_write_with_bclr(void)
{
  /* Setup */
  mock_usb_reset();
  uint8_t data[64] = {0xAA, 0xBB, 0xCC, 0xDD};

  /* Execute */
  rx_err_t err = rx_usb_hw_fifo_write(data, 4);

  /* Verify */
  assert(err == k_rx_ok);
  assert(mock_usb_bclr_was_set());  /* Check BCLR was set before write */
  assert(mock_usb_fifo_data_matches(data, 4));
}

/**
 * @brief Test: FIFO write odd length (handles last byte correctly)
 */
void test_usb_fifo_write_odd_length(void)
{
  /* Setup */
  mock_usb_reset();
  uint8_t data[5] = {0x11, 0x22, 0x33, 0x44, 0x55};

  /* Execute */
  rx_err_t err = rx_usb_hw_fifo_write(data, 5);

  /* Verify */
  assert(err == k_rx_ok);
  assert(mock_usb_fifo_data_matches(data, 5));
}
```

**Success Criteria**:
- [ ] Mock USB registers implemented
- [ ] 20+ unit tests covering USB HW layer
- [ ] All tests pass
- [ ] Tests run in <1 second

---

### Task 4.2: Integration Testing Framework

**Goal**: Test USB CDC with real ThreadX and hardware

**Duration**: 1 hour

**Integration Test Structure**:
```c
/**
 * @file test_usb_cdc_3port.c
 * @brief Integration test: 3-port simultaneous operation
 */

#include "rx_usb_cdc.h"
#include "tx_api.h"
#include <assert.h>
#include <string.h>

/* Test task stacks */
#define STACK_SIZE 1024
static ULONG port0_stack[STACK_SIZE];
static ULONG port1_stack[STACK_SIZE];
static ULONG port2_stack[STACK_SIZE];

/* Test tasks */
static TX_THREAD port0_thread;
static TX_THREAD port1_thread;
static TX_THREAD port2_thread;

/* Test data */
static bool port0_success = false;
static bool port1_success = false;
static bool port2_success = false;

/**
 * @brief Port 0 test task: Write 1KB, verify echo
 */
static void port0_task_entry(ULONG thread_input)
{
  (void)thread_input;

  /* Generate test pattern */
  uint8_t tx_data[1024];
  for (uint32_t i = 0; i < 1024; i++) {
    tx_data[i] = (uint8_t)(i & 0xFF);
  }

  /* Write to Port 0 */
  rx_err_t err = rx_usb_cdc_write(0, tx_data, 1024);
  assert(err == k_rx_ok);

  /* Read echo back */
  uint8_t rx_data[1024];
  uint32_t bytes_read = 0;
  err = rx_usb_cdc_read(0, rx_data, 1024, &bytes_read);
  assert(err == k_rx_ok);
  assert(bytes_read == 1024);

  /* Verify data */
  port0_success = (memcmp(tx_data, rx_data, 1024) == 0);
}

/* Similar for port1_task_entry and port2_task_entry */

/**
 * @brief Run integration test: 3-port simultaneous
 */
void test_usb_cdc_3port_simultaneous(void)
{
  /* Create test tasks */
  tx_thread_create(&port0_thread, "port0", port0_task_entry, 0,
                   port0_stack, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
  tx_thread_create(&port1_thread, "port1", port1_task_entry, 0,
                   port1_stack, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
  tx_thread_create(&port2_thread, "port2", port2_task_entry, 0,
                   port2_stack, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

  /* Wait for all tasks to complete (max 10 seconds) */
  tx_thread_sleep(10000);

  /* Verify all ports succeeded */
  assert(port0_success);
  assert(port1_success);
  assert(port2_success);
}
```

**Integration Tests**:

1. **3-Port Independence**: Verify no crosstalk between ports
2. **Logging During Transfers**: Send Protocol Buffers on Port 0 while logging on Port 2
3. **Cable Hot-Plug**: Disconnect cable, verify recovery, reconnect
4. **ThreadX Integration**: Verify no priority inversion, no deadlocks
5. **Stress Test**: Run all 3 ports at max throughput for 5 minutes

**Success Criteria**:
- [ ] 5 integration tests implemented
- [ ] All tests pass on hardware
- [ ] No hangs or crashes
- [ ] No memory leaks (verified with ThreadX stats)

---

### Task 4.3: Hardware Validation Suite

**Goal**: Validate on real RX72N hardware with comprehensive tests

**Duration**: 1 hour

**Hardware Test Cases**:

**Test 1: Large File Transfer (10MB)**
```python
#!/usr/bin/env python3
"""
test_usb_large_transfer.py - Send 10MB file, verify CRC32
"""

import serial
import crc32c
import time

def test_large_transfer():
    # Connect to USB CDC Port 0
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=10)

    # Generate 10MB test data
    data = bytearray(10 * 1024 * 1024)
    for i in range(len(data)):
        data[i] = i & 0xFF

    # Calculate expected CRC32
    expected_crc = crc32c.crc32c(data)

    # Send data in 1KB chunks
    start_time = time.time()
    for offset in range(0, len(data), 1024):
        chunk = data[offset:offset+1024]
        ser.write(chunk)

    elapsed = time.time() - start_time

    # Read CRC32 response from device
    response = ser.read(8)  # "CRC32: " + 8 hex digits
    actual_crc = int(response[-8:], 16)

    # Verify
    assert actual_crc == expected_crc, f"CRC mismatch: {actual_crc:08X} != {expected_crc:08X}"

    # Calculate throughput
    throughput_mbps = (len(data) * 8) / elapsed / 1e6
    print(f"PASS: 10MB transferred in {elapsed:.2f}s ({throughput_mbps:.2f} Mbps)")

if __name__ == '__main__':
    test_large_transfer()
```

**Test 2: Stress Test (Continuous Operation)**
```python
#!/usr/bin/env python3
"""
test_usb_stress.py - Continuous transfers for 1 hour
"""

import serial
import random
import time

def test_stress():
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

    start_time = time.time()
    elapsed = 0
    total_bytes = 0
    errors = 0

    while elapsed < 3600:  # 1 hour
        # Random size transfer (1-4096 bytes)
        size = random.randint(1, 4096)
        data = bytes([random.randint(0, 255) for _ in range(size)])

        # Send
        try:
            ser.write(data)
            response = ser.read(size)

            if response != data:
                errors += 1
            else:
                total_bytes += size
        except serial.SerialException:
            errors += 1

        elapsed = time.time() - start_time

    # Report results
    success_rate = (1 - errors / (total_bytes / 1024)) * 100
    print(f"PASS: {total_bytes/1e6:.2f}MB transferred, {success_rate:.4f}% success rate")
    assert success_rate >= 99.99, "Success rate below 99.99%"

if __name__ == '__main__':
    test_stress()
```

**Test 3: Hot-Plug Test**
```python
#!/usr/bin/env python3
"""
test_usb_hotplug.py - Disconnect/reconnect cable 100 times
"""

import serial
import time
import subprocess

def test_hotplug():
    for i in range(100):
        print(f"Iteration {i+1}/100")

        # Connect
        ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

        # Send test data
        data = b"PING"
        ser.write(data)
        response = ser.read(4)
        assert response == data, f"Echo failed: {response} != {data}"

        # Disconnect (user must physically unplug)
        input("Unplug USB cable, then press Enter...")

        # Wait for disconnect
        time.sleep(1)

        # Reconnect (user must physically plug)
        input("Plug USB cable, then press Enter...")

        # Wait for enumeration
        time.sleep(2)

    print("PASS: 100 hot-plug cycles completed")

if __name__ == '__main__':
    test_hotplug()
```

**Success Criteria**:
- [ ] 10MB transfer completes with correct CRC32
- [ ] 1-hour stress test ≥99.99% success rate
- [ ] 100 hot-plug cycles without errors

---

### Task 4.4: USB Protocol Compliance Testing

**Goal**: Verify USB 2.0 and CDC-ACM spec compliance

**Duration**: 45 minutes

**Wireshark Capture and Analysis**:

**Setup**:
```bash
# Linux: Use usbmon
sudo modprobe usbmon
sudo wireshark

# macOS: Use Wireshark USB capture
# Windows: Use USBPcap
```

**Protocol Checks**:

1. **USB 2.0 Descriptor Compliance**
   - [ ] Device descriptor valid
   - [ ] Configuration descriptor valid
   - [ ] Interface descriptors valid
   - [ ] Endpoint descriptors valid
   - [ ] String descriptors present

2. **CDC-ACM Compliance**
   - [ ] CDC Communication Interface Class (0x02)
   - [ ] CDC Data Interface Class (0x0A)
   - [ ] Functional descriptors present
   - [ ] SET_LINE_CODING supported
   - [ ] SET_CONTROL_LINE_STATE supported

3. **Bulk Transfer Compliance**
   - [ ] Bulk IN endpoints use correct PID (IN token)
   - [ ] Bulk OUT endpoints use correct PID (OUT token)
   - [ ] Data toggle bit alternates correctly
   - [ ] NAK handling correct
   - [ ] STALL handling correct (if applicable)

**Wireshark Filter Commands**:
```
# Show all USB traffic for device
usb.device_address == 3

# Show bulk transfers only
usb.endpoint_address.direction == 1 && usb.transfer_type == 0x03

# Show errors
usb.urb_status != 0

# Show control transfers (enumeration)
usb.transfer_type == 0x02
```

**Automated Analysis Script**:
```python
#!/usr/bin/env python3
"""
analyze_usb_pcap.py - Analyze Wireshark capture for compliance
"""

import pyshark

def analyze_usb_compliance(pcap_file):
    cap = pyshark.FileCapture(pcap_file, display_filter='usb')

    errors = []
    bulk_in_count = 0
    bulk_out_count = 0

    for pkt in cap:
        try:
            # Check for USB errors
            if hasattr(pkt.usb, 'urb_status') and int(pkt.usb.urb_status) != 0:
                errors.append(f"USB error: status={pkt.usb.urb_status}")

            # Count bulk transfers
            if pkt.usb.transfer_type == '0x03':  # Bulk
                if pkt.usb.endpoint_address_direction == '1':  # IN
                    bulk_in_count += 1
                else:
                    bulk_out_count += 1
        except AttributeError:
            pass

    print(f"Bulk IN transfers: {bulk_in_count}")
    print(f"Bulk OUT transfers: {bulk_out_count}")
    print(f"USB errors: {len(errors)}")

    assert len(errors) == 0, "USB protocol errors detected"

if __name__ == '__main__':
    analyze_usb_compliance('usb_capture.pcapng')
```

**Success Criteria**:
- [ ] All descriptors valid
- [ ] CDC-ACM spec compliant
- [ ] No USB protocol errors in 1-hour capture
- [ ] Data toggle bits correct

---

### Task 4.5: Performance Benchmarking

**Goal**: Measure throughput, latency, and CPU utilization

**Duration**: 30 minutes

**Benchmark Tests**:

**1. Throughput (Bytes/Sec Per Port)**
```c
/**
 * @brief Benchmark: USB CDC throughput measurement
 */
void benchmark_usb_throughput(void)
{
  const uint32_t test_size = 10 * 1024 * 1024;  /* 10 MB */
  const uint32_t chunk_size = 512;
  uint8_t data[chunk_size];

  /* Fill test data */
  for (uint32_t i = 0; i < chunk_size; i++) {
    data[i] = (uint8_t)(i & 0xFF);
  }

  /* Measure write throughput */
  uint32_t start_ticks = tx_time_get();

  for (uint32_t offset = 0; offset < test_size; offset += chunk_size) {
    rx_usb_cdc_write(0, data, chunk_size);
  }

  uint32_t elapsed_ticks = tx_time_get() - start_ticks;
  uint32_t elapsed_ms = elapsed_ticks;  /* 1 tick = 1 ms @ 1kHz */

  /* Calculate throughput */
  float throughput_mbps = (test_size * 8.0f) / (elapsed_ms / 1000.0f) / 1e6;

  rx_log_info("benchmark", "Throughput: %.2f Mbps", throughput_mbps);
}
```

**2. Latency (Request → Response Time)**
```c
/**
 * @brief Benchmark: Round-trip latency measurement
 */
void benchmark_usb_latency(void)
{
  const uint32_t iterations = 1000;
  uint8_t data[64] = "PING";
  uint8_t response[64];
  uint32_t total_latency_us = 0;

  for (uint32_t i = 0; i < iterations; i++) {
    /* Measure start time (assume CMT running at 240 MHz) */
    uint32_t start_cycles = CMT.CMCNT;

    /* Send request */
    rx_usb_cdc_write(0, data, 4);

    /* Wait for response */
    uint32_t bytes_read;
    rx_usb_cdc_read(0, response, 4, &bytes_read);

    /* Measure end time */
    uint32_t elapsed_cycles = CMT.CMCNT - start_cycles;
    uint32_t elapsed_us = (elapsed_cycles * 1000000) / 240000000;

    total_latency_us += elapsed_us;
  }

  uint32_t avg_latency_us = total_latency_us / iterations;
  rx_log_info("benchmark", "Average latency: %u µs", avg_latency_us);
}
```

**3. CPU Utilization**
```c
/**
 * @brief Benchmark: CPU utilization during USB transfers
 */
void benchmark_usb_cpu_utilization(void)
{
  /* Get initial ThreadX stats */
  ULONG initial_idle_time;
  tx_thread_performance_info_get(&idle_thread, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, &initial_idle_time, NULL);

  /* Run USB transfer workload for 10 seconds */
  uint32_t start_ticks = tx_time_get();
  while ((tx_time_get() - start_ticks) < 10000) {
    uint8_t data[512];
    rx_usb_cdc_write(0, data, 512);
  }

  /* Get final ThreadX stats */
  ULONG final_idle_time;
  tx_thread_performance_info_get(&idle_thread, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, &final_idle_time, NULL);

  /* Calculate CPU utilization */
  ULONG idle_time = final_idle_time - initial_idle_time;
  ULONG total_time = 10000;  /* 10 seconds */
  float cpu_utilization = (1.0f - (float)idle_time / total_time) * 100.0f;

  rx_log_info("benchmark", "CPU utilization: %.2f%%", cpu_utilization);
}
```

**Expected Performance**:
- **Throughput**: 8-12 Mbps per port (USB Full-Speed max 12 Mbps)
- **Latency**: <5 ms average (round-trip)
- **CPU Utilization**: <20% during max throughput

**Success Criteria**:
- [ ] Throughput ≥8 Mbps per port
- [ ] Latency ≤10 ms average
- [ ] CPU utilization ≤30% during transfers

---

## Test Automation

### Continuous Integration Setup

**CI/CD Pipeline** (GitHub Actions):
```yaml
name: USB CDC Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install GNURX toolchain
        run: sudo apt-get install gcc-rx-elf
      - name: Build unit tests
        run: |
          cd tests/unit
          cmake .. -DCMAKE_BUILD_TYPE=Debug
          make -j$(nproc)
      - name: Run unit tests
        run: |
          cd tests/unit
          ctest --output-on-failure

  integration-tests:
    runs-on: self-hosted  # Hardware required
    steps:
      - uses: actions/checkout@v2
      - name: Flash firmware
        run: |
          cd build
          openocd -f rx72n.cfg -c "program firmware.elf verify reset exit"
      - name: Run integration tests
        run: |
          python3 tests/hardware/test_usb_large_transfer.py
          python3 tests/hardware/test_usb_stress.py
```

**Success Criteria**:
- [ ] CI pipeline configured
- [ ] Unit tests run on every commit
- [ ] Integration tests run on self-hosted hardware
- [ ] All tests pass before merge

---

## Documentation Requirements

### Test Reports

**Test Coverage Report**:
- Generate with `gcov` or `lcov`
- Target: >90% line coverage for USB CDC stack
- Exclude: Mock code, test harness

**Performance Report**:
- Throughput measurements per port
- Latency histograms (min, max, avg, p50, p95, p99)
- CPU utilization graphs

**Compliance Report**:
- Wireshark capture analysis
- USB 2.0 compliance checklist
- CDC-ACM compliance checklist

---

## Success Criteria

**Phase 4 Complete When**:

- [ ] Unit test framework implemented (20+ tests)
- [ ] Integration tests pass (5 tests)
- [ ] Hardware validation passes (3 tests)
- [ ] Protocol compliance verified (Wireshark analysis)
- [ ] Performance benchmarks meet targets
- [ ] CI/CD pipeline configured
- [ ] Test coverage >90%
- [ ] All documentation updated

---

## Known Limitations

**Testing Gaps**:
- No automated hot-plug test (requires manual cable operation)
- Limited EMI/EMC testing (requires specialized equipment)
- No USB-IF certification (optional, costly)

**Performance Limits**:
- USB Full-Speed max 12 Mbps (hardware limitation)
- ThreadX overhead adds latency (~1-2ms)
- Shared 2KB FIFO limits concurrent port throughput

---

## Files to Create/Modify

| File | Purpose |
|------|---------|
| `tests/unit/test_rx_usb_cdc.c` | CDC class unit tests |
| `tests/unit/test_rx_usb_hw.c` | HW abstraction unit tests |
| `tests/unit/test_rx_log_usb.c` | Logging backend unit tests |
| `tests/mocks/mock_rx72n_usb_regs.c` | Mock USB registers |
| `tests/integration/test_usb_cdc_3port.c` | 3-port integration test |
| `tests/hardware/test_usb_large_transfer.py` | 10MB transfer test |
| `tests/hardware/test_usb_stress.py` | 1-hour stress test |
| `tests/hardware/analyze_usb_pcap.py` | Wireshark analysis |
| `benchmarks/benchmark_usb_throughput.c` | Throughput measurement |
| `benchmarks/benchmark_usb_latency.c` | Latency measurement |
| `.github/workflows/usb_cdc_tests.yml` | CI/CD pipeline |

---

## Estimated Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| 4.1 | 1 hour | Phase 3 complete |
| 4.2 | 1 hour | Task 4.1 |
| 4.3 | 1 hour | Task 4.2 |
| 4.4 | 45 min | Task 4.3 |
| 4.5 | 30 min | Task 4.3 |
| CI/CD | 45 min | All tasks |
| **Total** | **4 hours** | Phases 2-3 validated |

---

## Risk Assessment

**Risk 1: Hardware Availability**
- **Impact**: Can't run integration/hardware tests
- **Mitigation**: Prioritize unit tests, use simulator for logic testing
- **Likelihood**: Low

**Risk 2: Test Failures Reveal Issues**
- **Impact**: Need to debug and fix, delaying Phase 5
- **Mitigation**: Phase 2 testing reduces risk, Phase 3 adds logging for diagnostics
- **Likelihood**: Medium

**Risk 3: Performance Below Targets**
- **Impact**: May need optimization, code refactoring
- **Mitigation**: Profiling, optimization can be deferred to Phase 5
- **Likelihood**: Low

---

## Next Steps

1. **Complete Phase 3**
   - Implement debug logging integration
   - Verify all 9 Phase 3 tests pass

2. **Begin Phase 4 Implementation**
   - Start with Task 4.1 (unit test framework)
   - Implement tasks 4.2-4.5 sequentially
   - Run tests continuously during development

3. **Validate on Hardware**
   - Run all hardware tests
   - Measure performance benchmarks
   - Generate compliance reports

4. **Proceed to Phase 5**
   - Update all documentation
   - Create final summary reports
   - Mark USB CDC as production-ready

---

**Created**: 2026-02-05
**Last Updated**: 2026-02-05
**Status**: ⬜ NOT STARTED (Blocked by Phases 2-3)
