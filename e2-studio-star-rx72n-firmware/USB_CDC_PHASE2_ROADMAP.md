# USB CDC Phase 2: Bulk Transfer Reliability Fixes Roadmap

## Overview

**Goal**: Fix bulk transfer reliability issues blocking USB CDC debug logging functionality.

**Priority**: 🚨 CRITICAL - Blocking USB debug logging implementation

**Status**: 🔵 IN PROGRESS

**Estimated Time**: 4-6 hours

---

## Problem Statement

**Current Issue**: USB CDC bulk transfers are unreliable, preventing use of USB for debug logging.

**Symptoms**:
- **Bulk IN (Device → Host)**: TX buffer drains but host receives nothing, no BEMP interrupt fires
- **Bulk OUT (Host → Device)**: BRDY interrupt fires but FIFO appears empty, no data read

**Impact**: Cannot use USB CDC for debug logging, forced to use UART workaround

**Root Causes (Suspected)**:
1. Improper FIFO handling (not clearing before transfer, incorrect read/write procedure)
2. Pipe state machine errors (NAK/BUF/STALL transitions)
3. Interrupt handling issues (BEMP/BRDY not properly enabled/cleared)
4. Data toggle bit synchronization problems
5. Race conditions between ISR and application code

---

## Phase 2 Tasks

### Task 2.1: Analyze Current Bulk Transfer Implementation

**Status**: ⬜ NOT STARTED

**Goal**: Understand current code flow for bulk IN/OUT transfers

**Files to Review**:
- `libs/rx_usb/src/rx_usb_hw.c` - Hardware FIFO access layer
- `libs/rx_usb/src/rx_usb_cdc.c` - CDC class bulk IN/OUT handlers
- `libs/rx_usb/src/rx_usb_isr.c` - BRDY/BEMP interrupt handlers (already reviewed)
- `libs/rx_usb/src/rx_usb.c` - Public API and ring buffer management

**Analysis Checklist**:
- [ ] Map complete data flow: application → ring buffer → FIFO → USB bus
- [ ] Identify all pipe state transitions (NAK → BUF, BUF → NAK, STALL handling)
- [ ] Check FIFO clear procedure (BCLR bit usage)
- [ ] Verify interrupt enable/disable sequences
- [ ] Check for critical section protection around shared state
- [ ] Identify magic numbers (should use new pipe bit constants)

**Deliverable**: Code flow diagram and issue list

---

### Task 2.2: Fix Bulk IN Transfer (Device → Host)

**Status**: ⬜ NOT STARTED

**Goal**: Ensure BEMP interrupt fires and data reaches host

**Known Issues**:
1. **No BEMP interrupt**: Interrupt may not be enabled for bulk IN pipes
2. **FIFO not cleared**: May need BCLR before first write
3. **PID state wrong**: Pipe may be stuck in NAK or STALL
4. **Double buffering**: DBLB bit may cause issues

**Investigation Steps**:
- [ ] Check BEMPENB register - are bulk IN pipes enabled?
- [ ] Check PIPECTR PID bits - are pipes in BUF state?
- [ ] Check CFIFOCTR BCLR usage - is FIFO cleared before write?
- [ ] Check PIPECFG DBLB bit - is double buffering enabled?
- [ ] Check sequence toggle (SQCLR/SQSET) - is it managed correctly?

**Manual References**:
- Ch40 Section 40.3.8 - Bulk Transfer Procedure
- Ch40 Section 40.3.9 - FIFO Buffer Access
- Ch40 Section 40.3.6 - Pipe Control State Machine

**Fixes to Implement**:
1. **Enable BEMP interrupt for bulk IN pipes**:
   ```c
   // Enable BEMP for pipes 1, 4, 7 (bulk IN)
   usb0()->bempenb |= k_usb_pipe_bit_1 | k_usb_pipe_bit_4 | k_usb_pipe_bit_7;
   ```

2. **Clear FIFO before first write**:
   ```c
   // Before writing to FIFO
   usb0()->cfifoctr |= k_usb_cfifoctr_bclr;
   while (usb0()->cfifoctr & k_usb_cfifoctr_bclr); // Wait for clear
   ```

3. **Set pipe to BUF state**:
   ```c
   // Clear SQCLR first (for first packet)
   usb0()->pipe1ctr |= k_usb_pipectr_sqclr;
   // Set PID to BUF
   usb0()->pipe1ctr = (usb0()->pipe1ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;
   ```

4. **Disable double buffering** (if enabled):
   ```c
   // In pipe configuration
   usb0()->pipecfg &= ~k_usb_pipecfg_dblb;
   ```

**Verification**:
- [ ] BEMP interrupt fires after data written to FIFO
- [ ] Host receives correct data (use USB traffic capture)
- [ ] Multiple packets transfer correctly
- [ ] TX ring buffer drains as expected

---

### Task 2.3: Fix Bulk OUT Transfer (Host → Device)

**Status**: ⬜ NOT STARTED

**Goal**: Ensure BRDY interrupt correctly indicates data ready and FIFO read works

**Known Issues**:
1. **BRDY fires but no data**: FIFO may already be read or timing issue
2. **FIFO read timing**: May need to check FRDY before reading
3. **Pipe not re-enabled**: After read, pipe may not return to BUF state
4. **Data length mismatch**: DTLN may not match actual data

**Investigation Steps**:
- [ ] Check BRDYSTS clearing - is it cleared BEFORE or AFTER FIFO read?
- [ ] Check CFIFOCTR FRDY bit - is it checked before reading?
- [ ] Check CFIFOCTR DTLN field - does it match expected packet size?
- [ ] Check PIPECTR PID bits - is pipe set back to BUF after read?
- [ ] Check for race between ISR and application reading ring buffer

**Manual References**:
- Ch40 Section 40.3.8 - Bulk Transfer Procedure (OUT)
- Ch40 Section 40.3.9 - FIFO Buffer Access Sequence
- Ch40 Table 40.8 - CFIFOCTR Register

**Fixes to Implement**:
1. **Check FRDY before reading FIFO**:
   ```c
   // In BRDY handler
   if (!(usb0()->cfifoctr & k_usb_cfifoctr_frdy)) {
     // FIFO not ready, ignore this interrupt
     return;
   }
   ```

2. **Validate DTLN before read**:
   ```c
   const uint16_t dtln = usb0()->cfifoctr & k_usb_cfifoctr_dtln_mask;
   if (dtln == 0 || dtln > k_max_packet_size) {
     // Invalid packet size, log error
     rx_log_error(s_tag, "Invalid DTLN: %u", dtln);
     return;
   }
   ```

3. **Clear BRDY AFTER reading FIFO**:
   ```c
   // Read FIFO first
   for (uint16_t i = 0; i < dtln; i += 2) {
     data[i] = usb0()->cfifo;
   }
   // Then clear BRDY flag
   usb0()->brdysts = (uint16_t)~k_usb_pipe_bit_2;
   ```

4. **Return pipe to BUF state**:
   ```c
   // After successful read
   usb0()->pipe2ctr = (usb0()->pipe2ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;
   ```

**Verification**:
- [ ] BRDY interrupt fires when host sends data
- [ ] FIFO DTLN matches packet size
- [ ] Data correctly copied to RX ring buffer
- [ ] Multiple packets received correctly
- [ ] No data loss or corruption

---

### Task 2.4: Fix Pipe State Machine Transitions

**Status**: ⬜ NOT STARTED

**Goal**: Ensure correct NAK/BUF/STALL state transitions per RX72N manual

**Known Issues**:
1. **NAK not cleared**: May need explicit NAK clear before BUF transition
2. **STALL recovery**: STALL → IDLE may require SQCLR
3. **PBUSY not checked**: PID changes may require waiting for PBUSY=0

**Investigation Steps**:
- [ ] Check all PIPECTR PID writes for correct sequence
- [ ] Check if code waits for PBUSY bit to clear
- [ ] Check SQCLR/SQSET usage for sequence toggle management
- [ ] Map all pipe state transitions in current code

**Manual References**:
- Ch40 Section 40.3.6 - Pipe Control State Machine
- Ch40 Table 40.10 - PIPECTR Register Details
- Ch40 Figure 40.14 - Pipe State Transition Diagram

**State Transition Requirements** (from manual):

| From State | To State | Procedure |
|------------|----------|-----------|
| NAK | BUF | 1. Check PBUSY=0, 2. Write PID=BUF |
| BUF | NAK | Hardware automatic on packet complete |
| STALL | IDLE | 1. Write PID=NAK, 2. Set SQCLR, 3. Write PID=BUF |
| * | STALL | Write PID=STALL (error response) |

**Fixes to Implement**:
1. **Wait for PBUSY before PID change**:
   ```c
   // Before changing PID
   uint32_t timeout = 1000;
   while ((usb0()->pipe1ctr & k_usb_pipectr_pbusy) && timeout--) {
     // Wait for pipe idle
   }
   if (timeout == 0) {
     rx_log_error(s_tag, "PBUSY timeout");
     return k_rx_err_timeout;
   }
   ```

2. **Clear NAK explicitly before BUF**:
   ```c
   // Transition NAK → BUF
   usb0()->pipe1ctr = (usb0()->pipe1ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_nak;
   // Wait for PBUSY=0
   while (usb0()->pipe1ctr & k_usb_pipectr_pbusy);
   // Set to BUF
   usb0()->pipe1ctr = (usb0()->pipe1ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;
   ```

3. **STALL recovery sequence**:
   ```c
   // STALL → IDLE
   usb0()->pipe1ctr = (usb0()->pipe1ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_nak;
   usb0()->pipe1ctr |= k_usb_pipectr_sqclr;  // Clear sequence toggle
   while (usb0()->pipe1ctr & k_usb_pipectr_pbusy);
   usb0()->pipe1ctr = (usb0()->pipe1ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;
   ```

**Verification**:
- [ ] All PID transitions follow manual procedure
- [ ] No PBUSY timeout errors
- [ ] STALL recovery works (test with invalid request)
- [ ] Sequence toggle bit managed correctly

---

### Task 2.5: Add Critical Sections for Ring Buffer Access

**Status**: ⬜ NOT STARTED

**Goal**: Prevent race conditions between ISR and application code

**Known Issues**:
1. **No interrupt disable**: ISR and app may access ring buffer simultaneously
2. **Non-atomic index updates**: head/tail/count may become inconsistent

**Investigation Steps**:
- [ ] Identify all ring buffer access points (ISR and application)
- [ ] Check if ThreadX primitives used for synchronization
- [ ] Check if interrupt priority allows preemption
- [ ] Look for existing critical section macros

**Manual References**:
- ThreadX User Guide - Interrupt Management
- RX72N Manual Ch13 - Interrupt Controller (ICU)

**Fixes to Implement**:
1. **Disable BRDY/BEMP during ring buffer updates**:
   ```c
   // In application code reading RX buffer
   const uint16_t saved_intenb = usb0()->intenb0;
   usb0()->intenb0 &= ~k_usb_intsts0_brdy;  // Disable BRDY

   // Read from ring buffer (non-atomic)
   uint8_t data = rx_ring->buffer[rx_ring->tail];
   rx_ring->tail = (rx_ring->tail + 1) % RX_RING_SIZE;
   rx_ring->count--;

   usb0()->intenb0 = saved_intenb;  // Re-enable BRDY
   ```

2. **Use ThreadX interrupt control**:
   ```c
   #include "tx_api.h"

   // Disable interrupts
   UINT int_state = tx_interrupt_control(TX_INT_DISABLE);

   // Critical section - ring buffer access

   // Restore interrupts
   tx_interrupt_control(int_state);
   ```

3. **Add critical section macros**:
   ```c
   #define USB_ENTER_CRITICAL() \
     const uint16_t _saved_intenb = usb0()->intenb0; \
     usb0()->intenb0 &= ~(k_usb_intsts0_brdy | k_usb_intsts0_bemp)

   #define USB_EXIT_CRITICAL() \
     usb0()->intenb0 = _saved_intenb
   ```

**Verification**:
- [ ] No ring buffer corruption during stress test
- [ ] Ring buffer count stays consistent
- [ ] No ISR preemption during critical sections
- [ ] Interrupt latency remains acceptable (<50µs)

---

### Task 2.6: Replace Magic Numbers with Named Constants

**Status**: ⬜ NOT STARTED

**Goal**: Use new `usb_pipe_bits_t` enum throughout USB code

**Known Issues**:
- Magic numbers like `(1U << 2)` for pipe bits scattered throughout code
- Hard to search and maintain

**Files to Update**:
- `libs/rx_usb/src/rx_usb_isr.c` - Already uses magic numbers in loops
- `libs/rx_usb/src/rx_usb_cdc.c` - Pipe enable/disable
- `libs/rx_usb/src/rx_usb_hw.c` - Interrupt control

**Changes**:
```c
// BEFORE
usb0()->brdysts = (uint16_t)~(1U << pipe);

// AFTER
usb0()->brdysts = (uint16_t)~(1U << pipe);  // Still OK in loop

// BEFORE
usb0()->bempenb |= (1U << 1) | (1U << 4) | (1U << 7);

// AFTER
usb0()->bempenb |= k_usb_pipe_bit_1 | k_usb_pipe_bit_4 | k_usb_pipe_bit_7;
```

**Verification**:
- [ ] All pipe bit operations use named constants
- [ ] No magic numbers for pipe indices
- [ ] Code more readable and searchable

---

## Success Criteria

**Bulk IN (Device → Host)**:
- [ ] BEMP interrupt fires reliably after FIFO write
- [ ] Host receives 100% of transmitted data
- [ ] TX ring buffer drains correctly
- [ ] No data corruption or packet loss
- [ ] Continuous 1MB transfer succeeds (CRC32 verified)

**Bulk OUT (Host → Device)**:
- [ ] BRDY interrupt fires when host sends data
- [ ] FIFO DTLN matches packet size
- [ ] All data copied to RX ring buffer
- [ ] No data loss or corruption
- [ ] Continuous 1MB receive succeeds (CRC32 verified)

**Stability**:
- [ ] 99.99% transfer success rate over 1 hour
- [ ] No ring buffer corruption under stress
- [ ] Correct behavior during USB cable disconnect/reconnect
- [ ] Proper error handling for all edge cases

**Code Quality**:
- [ ] All magic numbers replaced with named constants
- [ ] Critical sections protect shared state
- [ ] NASA Power of 10 compliance maintained
- [ ] Comprehensive logging for debug builds

---

## Testing Strategy

### Unit Tests (Simulator/Hardware)

1. **Test Bulk IN Single Packet**:
   - Write 64 bytes to TX buffer
   - Verify BEMP interrupt fires
   - Verify host receives exact data

2. **Test Bulk IN Multiple Packets**:
   - Write 1024 bytes (16 packets)
   - Verify all packets transmitted
   - Check sequence toggle bit progression

3. **Test Bulk OUT Single Packet**:
   - Host sends 64 bytes
   - Verify BRDY interrupt fires
   - Verify data in RX ring buffer

4. **Test Bulk OUT Multiple Packets**:
   - Host sends 1024 bytes (16 packets)
   - Verify all data received
   - Check for data corruption

### Integration Tests (Hardware Only)

5. **Loopback Test**:
   - Connect 3 ports: Protocol, Decoded, Log
   - Write to TX, read from RX on each port
   - Verify data integrity per port

6. **Stress Test**:
   - Continuous 1MB transfer (TX and RX simultaneously)
   - Run for 1 hour
   - Verify CRC32 of all data
   - Check error counters (should be 0)

7. **Cable Disconnect Test**:
   - Start bulk transfer
   - Unplug USB cable
   - Verify graceful handling (no crash)
   - Plug cable back in
   - Verify enumeration and resume

### Traffic Analysis

8. **Wireshark Capture**:
   - Capture USB bulk transfer traffic
   - Verify packet structure (token, data, handshake)
   - Check for NAK/STALL errors
   - Measure throughput (should be ~900 KB/s for Full-Speed)

---

## Documentation

Create detailed result documents for each task:

1. **USB_CDC_PHASE2_TASK2.1_ANALYSIS.md** - Code flow analysis
2. **USB_CDC_PHASE2_TASK2.2_BULK_IN_FIX.md** - Bulk IN fixes
3. **USB_CDC_PHASE2_TASK2.3_BULK_OUT_FIX.md** - Bulk OUT fixes
4. **USB_CDC_PHASE2_TASK2.4_STATE_MACHINE_FIX.md** - Pipe state fixes
5. **USB_CDC_PHASE2_TASK2.5_CRITICAL_SECTIONS.md** - Race condition fixes
6. **USB_CDC_PHASE2_SUMMARY.md** - Overall Phase 2 summary

---

## Next Steps After Phase 2

Once all bulk transfers reliable:
1. ✅ Commit all fixes with detailed commit messages
2. ✅ Update `USB_CDC_TODO.md` Phase 2 status
3. ✅ Create `USB_CDC_PHASE3_ROADMAP.md` for debug logging integration
4. 🔄 Proceed to Phase 3: Debug Logging Over USB CDC

---

**Started**: 2026-02-05
**Last Updated**: 2026-02-05
**Completed**: Not yet
