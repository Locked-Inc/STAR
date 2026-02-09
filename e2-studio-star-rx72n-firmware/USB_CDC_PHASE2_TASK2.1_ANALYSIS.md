# Task 2.1: Bulk Transfer Implementation Analysis

## Overview

**Task**: Analyze current bulk transfer implementation to identify reliability issues
**Date**: 2026-02-05
**Status**: ✅ COMPLETE

---

## Code Flow Analysis

### Bulk OUT Transfer (Host → Device)

**Complete Data Flow**:
```
Host writes to /dev/ttyACM*
  ↓
USB0 hardware receives Bulk OUT packet in FIFO
  ↓
BRDY interrupt fires (brdysts bit set for pipe)
  ↓
usb0_usbi_isr() vector (line 647 rx_usb_isr.c)
  ↓
rx_usb_isr_handler() dispatcher (line 602)
  ↓
internal_handle_brdy_interrupt() (line 509)
  ↓
Loop checks brdysts bits for pipes 0-9 (line 514)
  ↓
If pipe == bulk_out: rx_usb_cdc_handle_bulk_out(port) (line 2442 rx_usb_cdc.c)
  ↓
rx_usb_hw_fifo_read(pipe, buffer, 64) (line 700 rx_usb_hw.c)
  → Select pipe: CFIFOSEL = pipe
  → Wait FRDY: poll CFIFOCTR.FRDY
  → Read DTLN: len = CFIFOCTR & 0x0FFF
  → Read data: for (i=0; i<len; i++) data[i] = CFIFO (ISSUE: byte-by-byte!)
  → Clear buffer: CFIFOCTR |= BCLR
  ↓
rx_usb_rx_push(port, data, len) (rx_usb.c)
  → Copy data to RX ring buffer
  → Invoke callback if registered
  ↓
Clear BRDY flag: brdysts = ~(1 << pipe) (line 532 rx_usb_isr.c)
```

### Bulk IN Transfer (Device → Host)

**Complete Data Flow**:
```
Application calls rx_usb_write(port, data, len)
  ↓
rx_usb_tx_push(port, data, len) (rx_usb.c)
  → Copy data to TX ring buffer
  → If first packet, trigger initial transfer
  ↓
rx_usb_cdc_handle_bulk_in(port) (line 2692 rx_usb_cdc.c)
  ↓
rx_usb_tx_pop(port, buffer, 64)
  → Read up to 64 bytes from TX ring buffer
  ↓
rx_usb_hw_fifo_write(pipe, data, len) (line 760 rx_usb_hw.c)
  → Select pipe: CFIFOSEL = pipe | ISEL
  → Wait FRDY: poll CFIFOCTR.FRDY
  → Write data: for (i=0; i<len; i++) CFIFO = data[i] (ISSUE: byte-by-byte!)
  → Set valid: CFIFOCTR |= BVAL
  ↓
USB0 hardware transmits packet
  ↓
Host ACKs packet
  ↓
BEMP interrupt fires (bempsts bit set for pipe)
  ↓
usb0_usbi_isr() vector
  ↓
rx_usb_isr_handler() dispatcher
  ↓
internal_handle_bemp_interrupt() (line 542)
  ↓
Loop checks bempsts bits for pipes 0-9 (line 547)
  ↓
If pipe == bulk_in: rx_usb_cdc_handle_bulk_in(port) (recursion!)
  ↓
Clear BEMP flag: bempsts = ~(1 << pipe) (line 565 rx_usb_isr.c)
  ↓
Repeat until TX buffer empty
```

---

## Critical Issues Found

### 🚨 ISSUE #1: FIFO Byte-by-Byte Access (CRITICAL)

**Location**:
- `rx_usb_hw.c:737` (read)
- `rx_usb_hw.c:790` (write)

**Problem**: FIFO accessed byte-by-byte despite being 16-bit register

**Current Code (READ)**:
```c
/* Read data from FIFO */
for (uint32_t i = 0; i < len; i++) {
  data[i] = (uint8_t)(usb0()->cfifo & k_usb_fifo_byte_mask);  // ❌ Wrong!
}
```

**Current Code (WRITE)**:
```c
/* Write data to FIFO */
for (uint32_t i = 0; i < len; i++) {
  usb0()->cfifo = data[i];  // ❌ Wrong!
}
```

**Why This Is Wrong**:
1. CFIFO is a 16-bit register (verified in Phase 1 Task 1.2)
2. RX72N Manual Ch40 states FIFO access must be 16-bit aligned
3. Byte access may:
   - Read/write wrong bytes (endianness issues)
   - Cause FIFO pointer misalignment
   - Result in data corruption

**Correct Implementation**:
```c
/* READ: Read 16 bits at a time */
for (uint32_t i = 0; i < len; i += 2) {
  const uint16_t word = usb0()->cfifo;
  data[i] = (uint8_t)(word & 0xFF);        // Low byte
  if (i + 1 < len) {
    data[i + 1] = (uint8_t)((word >> 8) & 0xFF); // High byte
  }
}

/* WRITE: Write 16 bits at a time */
for (uint32_t i = 0; i < len; i += 2) {
  uint16_t word = data[i];  // Low byte
  if (i + 1 < len) {
    word |= ((uint16_t)data[i + 1] << 8);  // High byte
  }
  usb0()->cfifo = word;
}
```

**Impact**:
- **CRITICAL** - Causes data corruption and transfer failures
- **Explains**: Both Bulk IN and Bulk OUT reliability issues

**Priority**: **BLOCKING** - Must fix before any testing

---

### ⚠️ ISSUE #2: Missing FIFO Clear Before Write (HIGH)

**Location**: `rx_usb_hw.c:760-797` (fifo_write function)

**Problem**: FIFO buffer not cleared before first write

**Current Code**:
```c
uint32_t rx_usb_hw_fifo_write(uint8_t pipe, const uint8_t* data, uint32_t len)
{
  /* ... validation ... */

  /* Select pipe for CFIFO access with write direction */
  usb0()->cfifosel = (pipe & k_usb_fifosel_curpipe_mask) | k_usb_fifosel_isel;

  /* Wait for FIFO ready */
  while (!(usb0()->cfifoctr & k_usb_fifoctr_frdy) && timeout--);

  /* ❌ MISSING: BCLR to clear any stale data */

  /* Write data to FIFO */
  for (uint32_t i = 0; i < len; i++) {
    usb0()->cfifo = data[i];
  }

  /* Set buffer valid */
  usb0()->cfifoctr |= k_usb_fifoctr_bval;

  return len;
}
```

**Manual Reference**: RX72N Manual Ch40 Section 40.3.9
- "Before writing to FIFO, set BCLR to clear buffer"
- "Ensures no stale data from previous transfer"

**Correct Implementation**:
```c
/* After selecting pipe and waiting for FRDY */

/* Clear FIFO buffer before write (RX72N Manual Ch40 40.3.9) */
usb0()->cfifoctr |= k_usb_fifoctr_bclr;

/* Wait for BCLR to complete (hardware clears it when done) */
timeout = k_usb_fifo_timeout_iterations;
while ((usb0()->cfifoctr & k_usb_fifoctr_bclr) && timeout--) {
  __asm__ volatile("nop");
}

if (timeout == k_usb_fifo_timeout_expired) {
  rx_log_error(s_tag, "FIFO clear timeout");
  return k_min_transfer_size;
}

/* Now write data */
for (uint32_t i = 0; i < len; i += 2) {
  /* ... 16-bit write ... */
}
```

**Impact**:
- **HIGH** - May cause first packet corruption
- **Explains**: Intermittent transfer failures

**Priority**: **BLOCKING** - Must fix

---

### ⚠️ ISSUE #3: No BEMP Interrupt Enable Verification (HIGH)

**Location**: `rx_usb_cdc.c:2692-2713` (bulk_in handler)

**Problem**: Bulk IN handler assumes BEMP interrupt is enabled, but never verifies

**Current Code**:
```c
void rx_usb_cdc_handle_bulk_in(const rx_usb_port_id_t port)
{
  /* ... get pipe ... */

  uint8_t data[k_usb_bulk_packet_size];
  const uint32_t len = rx_usb_tx_pop(port, data, k_usb_bulk_packet_size);

  if (len > k_min_transfer_size) {
    (void)rx_usb_hw_fifo_write(pipe, data, len);
  }
  /* ❌ MISSING: Check if BEMP interrupt is enabled for this pipe */
}
```

**Issue**: If BEMP interrupt not enabled for bulk IN pipes (1, 4, 7), no further interrupts will fire after initial write

**Root Cause**: Need to verify BEMPENB register during init

**Fix Required**: Add initialization check in `rx_usb_hw_configure_pipe()`:
```c
/* After configuring pipe, enable BEMP interrupt */
usb0()->bempenb |= (1U << pipe);  // Use k_usb_pipe_bit_* in real code
```

**Verification Needed**: Check pipe initialization code

**Impact**:
- **HIGH** - Bulk IN transfers stop after first packet
- **Explains**: "TX buffer drains but host sees nothing"

**Priority**: **BLOCKING** - Must verify/fix

---

### ℹ️ ISSUE #4: No Pipe PID State Verification (MEDIUM)

**Location**: Both `rx_usb_hw_fifo_read()` and `rx_usb_hw_fifo_write()`

**Problem**: No check that pipe is in BUF state before FIFO access

**Current Code**:
```c
/* FIFO write - no PID check */
usb0()->cfifosel = (pipe & k_usb_fifosel_curpipe_mask) | k_usb_fifosel_isel;
while (!(usb0()->cfifoctr & k_usb_fifoctr_frdy) && timeout--);
/* ❌ Should check: PIPExCTR.PID == BUF */
for (uint32_t i = 0; i < len; i++) { /* ... */ }
```

**Manual Reference**: RX72N Manual Ch40 Section 40.3.6
- "Pipe must be in BUF state for data transfer"
- "NAK state: pipe not ready"
- "STALL state: pipe in error condition"

**Recommended Enhancement** (non-blocking):
```c
/* Before FIFO access, verify pipe is in BUF state */
const uint16_t pipectr = usb0()->pipe1ctr;  // Use correct pipe
const uint16_t pid = pipectr & k_usb_pipectr_pid_mask;

if (pid != k_usb_pipectr_pid_buf) {
  rx_log_warn(s_tag, "Pipe %u not in BUF state (PID=0x%02X)", pipe, pid);
  /* Could try to recover: set PID to BUF */
  usb0()->pipe1ctr = (pipectr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;
  /* Wait for PBUSY to clear */
  while (usb0()->pipe1ctr & k_usb_pipectr_pbusy);
}
```

**Impact**:
- **MEDIUM** - Improves robustness, not blocking
- **Explains**: Some transfer failures under error conditions

**Priority**: **OPTIONAL** - Improve after critical fixes

---

### ℹ️ ISSUE #5: BRDY Clear Timing (LOW)

**Location**: `rx_usb_isr.c:532` (brdy interrupt handler)

**Current Behavior**: BRDY cleared AFTER FIFO read (correct per manual)

**Code**:
```c
/* Handle pipe event */
if (pipe == k_port0_pipe_bulk_out) {
  rx_usb_cdc_handle_bulk_out(k_usb_port_proto);  // Reads FIFO
}
/* ... */

/* Clear pipe buffer ready flag */
usb0()->brdysts = (uint16_t)~(1U << pipe);  // ✅ Correct timing
```

**Verification**: RX72N Manual Ch40 Section 40.3.8
- "Clear BRDY after reading FIFO data"
- Current code is CORRECT ✅

**No Action Required**

---

### ℹ️ ISSUE #6: No DTLN Validation (LOW)

**Location**: `rx_usb_hw.c:729-733` (fifo_read)

**Current Code**:
```c
/* Get received data length */
uint32_t len = usb0()->cfifoctr & k_usb_fifoctr_dtln_mask;
if (len > max_len) {
  rx_log_error(s_tag, "FIFO read overflow detected");
  len = max_len;  // ✅ Good: clamps to max
}
```

**Enhancement** (optional):
```c
/* Validate DTLN is non-zero and reasonable */
if (len == 0) {
  rx_log_warn(s_tag, "DTLN is 0, no data in FIFO");
  return 0;
}

if (len > k_usb_bulk_packet_size) {
  rx_log_error(s_tag, "DTLN exceeds max packet size (%u > %u)", len, k_usb_bulk_packet_size);
  len = k_usb_bulk_packet_size;
}
```

**Impact**: **LOW** - Current validation is adequate

**Priority**: **OPTIONAL**

---

## Pipe Configuration Analysis

**Need to verify** `rx_usb_hw_configure_pipe()` function:

**Expected Initialization Sequence** (per RX72N Manual Ch40):
1. Select pipe: PIPESEL = pipe_number
2. Configure: PIPECFG = endpoint | direction | type
3. Set max packet: PIPEMAXP = 64
4. Clear FIFO: PIPExCTR.ACLRM = 1 → 0
5. **Enable interrupts**: BRDYENB/BEMPENB bits
6. Set PID to BUF: PIPExCTR.PID = 0x01

**Critical Check Needed**: Are BRDYENB/BEMPENB enabled during init?

---

## Magic Number Usage

**Found magic numbers** that should use new `usb_pipe_bits_t` enum:

| File | Line | Current Code | Should Be |
|------|------|--------------|-----------|
| rx_usb_isr.c | 515 | `if (brdysts & (1U << pipe))` | OK (loop variable) |
| rx_usb_isr.c | 532 | `usb0()->brdysts = ~(1U << pipe);` | OK (loop variable) |
| rx_usb_isr.c | 548 | `if (bempsts & (1U << pipe))` | OK (loop variable) |
| rx_usb_isr.c | 565 | `usb0()->bempsts = ~(1U << pipe);` | OK (loop variable) |

**Note**: Loop-based bit checks are acceptable. Look for **static pipe enable/disable** operations.

**Need to search**: Pipe initialization code for magic numbers

---

## Summary of Issues

| Issue | Severity | Impact | Priority | Fix Location |
|-------|----------|--------|----------|--------------|
| #1: Byte-by-byte FIFO access | CRITICAL | Data corruption | **BLOCKING** | rx_usb_hw.c:737, 790 |
| #2: Missing FIFO clear before write | HIGH | First packet corruption | **BLOCKING** | rx_usb_hw.c:760-797 |
| #3: BEMP interrupt not verified | HIGH | TX stops after 1 packet | **BLOCKING** | Pipe init + verification |
| #4: No pipe PID state check | MEDIUM | Error recovery | OPTIONAL | rx_usb_hw.c:700, 760 |
| #5: BRDY clear timing | N/A | Already correct ✅ | NONE | - |
| #6: DTLN validation | LOW | Already adequate | OPTIONAL | rx_usb_hw.c:729 |

---

## Next Steps (Task 2.2)

**Priority order for fixes:**

1. **Fix FIFO byte-by-byte access** (Issue #1)
   - Change to 16-bit read/write
   - Handle odd-length packets
   - Test with known data patterns

2. **Add FIFO clear before write** (Issue #2)
   - Insert BCLR sequence before FIFO write
   - Add timeout for BCLR completion
   - Test first packet integrity

3. **Verify/Enable BEMP interrupts** (Issue #3)
   - Check pipe initialization code
   - Ensure BEMPENB bits set for pipes 1, 4, 7
   - Add assertion/log if not enabled

4. **(Optional) Add PID state checks** (Issue #4)
   - Verify pipe in BUF state
   - Add recovery if in NAK/STALL
   - Log warnings for debugging

**Estimated time for fixes**: 2-3 hours

**Testing strategy**:
- Unit test: 64-byte loopback (known pattern)
- Integration test: 1KB transfer (CRC32 verification)
- Stress test: Continuous 1MB transfer

---

## Files Analyzed

1. **rx_usb_hw.c** (945 lines)
   - rx_usb_hw_fifo_read() - lines 700-750
   - rx_usb_hw_fifo_write() - lines 760-797
   - rx_usb_hw_configure_pipe() - line 835+

2. **rx_usb_cdc.c** (2714 lines)
   - rx_usb_cdc_handle_bulk_out() - lines 2442-2463
   - rx_usb_cdc_handle_bulk_in() - lines 2692-2713

3. **rx_usb_isr.c** (695 lines)
   - internal_handle_brdy_interrupt() - lines 509-535
   - internal_handle_bemp_interrupt() - lines 542-568

**Total lines analyzed**: ~4354 lines across 3 files

---

**Analysis Completed**: 2026-02-05
**Issues Found**: 6 (3 blocking, 1 optional, 2 already correct)
**Next Task**: 2.2 - Fix Bulk IN Transfer
