# USB CDC Debug Logging - TODO Checklist

## Issue: USB CDC Bulk Transfer Reliability Problems

**Problem**: USB CDC bulk transfers are unreliable, blocking USB debug logging functionality.

**Current Workaround**: Using UART for debug logging instead of USB.

**Root Cause (Suspected)**:
- Bulk transfer state machine issues
- FIFO handling errors
- Interrupt handling race conditions
- Possible register address errors (though most were verified 2026-02-03)

---

## Phase 1: Verification Against RX72N Manual Ch40 (PRIORITY: CRITICAL)

### 1.1 Register Address Verification

**Status**: ✅ Mostly Complete (2026-02-03 verification in rx72n_usb_regs.h)

**Verified**:
- All register offsets verified against manual Ch40
- 45 registers defined with correct offsets
- 55+ static assertions verify offsets at compile time
- Removed non-existent registers (BUSWAIT, PLLSTA, TESTMODE, etc.)
- Fixed FIFO registers from uint32_t to uint16_t

**Remaining**:
- [ ] **FILE**: `libs/rx_hal/inc/rx72n_usb_regs.h`
  - [ ] **TODO**: Double-check bit field definitions against manual Ch40 Section 40.2
  - [ ] **TODO**: Verify SYSCFG register bit fields (DCFM, DRPD, DPRPU, etc.)
  - [ ] **TODO**: Verify INTSTS0 register bit fields (VBINT, RESM, SOFR, DVST, CTRT, etc.)
  - [ ] **TODO**: Verify PIPECFG register bit fields (TYPE, BFRE, DBLB, SHTNAK, DIR, EPNUM)

### 1.2 FIFO Access Width Verification

**Status**: ⚠️ NEEDS VERIFICATION

- [ ] **FILE**: `libs/rx_usb/src/rx_usb_hw.c`
  - [ ] **TODO**: Verify CFIFO/D0FIFO/D1FIFO access uses correct 16-bit width
  - [ ] **TODO**: Verify CFIFOSEL.MBW is set to k_usb_fifosel_mbw_16 (16-bit mode)
  - [ ] **TODO**: Check all FIFO reads use `volatile uint16_t*` cast, not uint32_t
  - [ ] **TODO**: Verify FIFO write loops handle odd-byte-count correctly

### 1.3 Interrupt Enable/Status Register Verification

**Status**: ⚠️ NEEDS VERIFICATION

- [ ] **FILE**: `libs/rx_usb/src/rx_usb_isr.c`
  - [ ] **TODO**: Verify INTENB0/INTENB1 bits match manual Ch40 Table 40.12
  - [ ] **TODO**: Verify INTSTS0/INTSTS1 bits match manual Ch40 Table 40.13
  - [ ] **TODO**: Verify BRDYENB/BRDYSTS pipe enable/status bits
  - [ ] **TODO**: Verify BEMPENB/BEMPSTS pipe enable/status bits
  - [ ] **TODO**: Verify NRDYENB/NRDYSTS pipe enable/status bits

---

## Phase 2: Bulk Transfer Reliability Fixes (PRIORITY: CRITICAL)

### 2.1 Bulk IN (Device → Host) Transfer Issues

**Symptoms**:
- Data sent from RX72N not received by host
- TX buffer appears to drain, but host sees nothing
- No BEMP (Buffer Empty) interrupt fires

**Suspected Causes**:
- FIFO not properly cleared before transfer
- PID toggle bit not managed correctly
- BEMP interrupt not enabled for pipe
- Double-buffering (DBLB) misconfigured

**Action Items**:
- [ ] **FILE**: `libs/rx_usb/src/rx_usb_hw.c`
  - [ ] **FUNCTION**: `rx_usb_hw_pipe_write()`
    - [ ] **TODO**: Add FIFO clear (CFIFOCTR.BCLR) before write
    - [ ] **TODO**: Verify PID token sequence (SQCLR on first packet, then auto-toggle)
    - [ ] **TODO**: Add BEMP interrupt enable in BEMPENB register
    - [ ] **TODO**: Add logging to track FIFO fill count vs actual bytes written
    - [ ] **TODO**: Verify PIPECFG.BFRE (buffer ready interrupt mode) is correct
    - [ ] **TODO**: Check PIPECFG.DBLB (double buffer) setting - should be 0 for simplicity
  - [ ] **FUNCTION**: `rx_usb_hw_pipe_read()`
    - [ ] **TODO**: Add FIFO validation (check CFIFOCTR.DTLN matches expected)
    - [ ] **TODO**: Add NAK-to-BUF transition on read complete (PIPECTR.PID = BUF)

### 2.2 Bulk OUT (Host → Device) Transfer Issues

**Symptoms**:
- Data sent from host not received by RX72N
- RX buffer stays empty
- BRDY (Buffer Ready) interrupt fires but no data in FIFO

**Suspected Causes**:
- FIFO read timing issue (read before BRDY clears?)
- Pipe not set to BUF state after read
- Data toggle bit mismatch with host

**Action Items**:
- [ ] **FILE**: `libs/rx_usb/src/rx_usb_cdc.c`
  - [ ] **FUNCTION**: `priv_handle_bulk_out()`
    - [ ] **TODO**: Verify BRDY is cleared BEFORE reading FIFO (not after)
    - [ ] **TODO**: Add check for CFIFOCTR.FRDY (FIFO ready) before read
    - [ ] **TODO**: Add DTLN (data length) validation - reject if 0 or > max_packet_size
    - [ ] **TODO**: Set pipe back to BUF state after successful read
    - [ ] **TODO**: Add error handling for NRDY (pipe not ready) condition

### 2.3 Pipe State Machine Issues

**Problem**: Pipe transitions (IDLE → NAK → BUF → STALL) not handled correctly

**Action Items**:
- [ ] **FILE**: `libs/rx_usb/src/rx_usb_hw.c`
  - [ ] **FUNCTION**: `rx_usb_hw_pipe_set_state()`
    - [ ] **TODO**: Add NAK clearance before transition to BUF
    - [ ] **TODO**: Implement proper STALL → IDLE transition (requires SQCLR)
    - [ ] **TODO**: Add wait loops for PID changes (PBUSY bit in PIPECTR)
    - [ ] **TODO**: Add logging for all pipe state transitions

### 2.4 Interrupt Handler Race Conditions

**Problem**: ISR may race with application code accessing ring buffers

**Action Items**:
- [ ] **FILE**: `libs/rx_usb/src/rx_usb_isr.c`
  - [ ] **FUNCTION**: `rx_usb_isr_handler()`
    - [ ] **TODO**: Add critical section around ring buffer operations
    - [ ] **TODO**: Disable BRDY/BEMP interrupts during FIFO access
    - [ ] **TODO**: Add ISR entry/exit logging for debug builds
    - [ ] **TODO**: Verify interrupt priority vs ThreadX kernel (must be lower)

---

## Phase 3: Debug Logging Over USB CDC (PRIORITY: HIGH)

### 3.1 USB CDC Log Port Integration

**Goal**: Replace UART debug logging with USB CDC Port 2 (Log Port)

**Current State**:
- `rx_log_*()` functions use UART (SCI9) for output
- USB CDC Port 2 exists but not integrated with logging system

**Action Items**:
- [ ] **FILE**: `libs/rx_core/inc/rx_log.h`
  - [ ] **TODO**: Add USB CDC backend selection via compile-time define
  - [ ] **TODO**: Add `RX_LOG_USE_USB_CDC` config option
  - [ ] **TODO**: Modify `rx_log_init()` to accept backend enum (UART vs USB)

- [ ] **FILE**: `libs/rx_core/src/rx_log.c` (NEW - needs creation)
  - [ ] **TODO**: Implement `priv_log_output_usb()` function
  - [ ] **TODO**: Call `rx_usb_puts(k_usb_port_log, msg)` for USB output
  - [ ] **TODO**: Add fallback to UART if USB not configured
  - [ ] **TODO**: Add buffer for log messages if USB TX buffer full

- [ ] **FILE**: `src/main.c`
  - [ ] **TODO**: Initialize USB CDC before logging init
  - [ ] **TODO**: Wait for USB enumeration before enabling USB logging
  - [ ] **TODO**: Add command-line option to select log backend

### 3.2 USB CDC Log Port Performance

**Goal**: Ensure logging doesn't block critical tasks

**Action Items**:
- [ ] **FILE**: `libs/rx_usb/src/rx_usb.c`
  - [ ] **FUNCTION**: `rx_usb_puts()`
    - [ ] **TODO**: Make rx_usb_puts() non-blocking (return ERR_BUSY if full)
    - [ ] **TODO**: Add optional timestamp prefix to log messages
    - [ ] **TODO**: Optimize string copy to avoid multiple write() calls

- [ ] **FILE**: `libs/rx_usb_comm/src/rx_usb_comm.c`
  - [ ] **TODO**: Implement log message queueing for high-frequency logging
  - [ ] **TODO**: Add log level filtering (INFO/WARN/ERROR) at USB layer
  - [ ] **TODO**: Implement log rate limiting (max N msgs per second)

---

## Phase 4: Testing and Validation (PRIORITY: HIGH)

### 4.1 Unit Tests

- [ ] **FILE**: `tests/test_rx_usb.c`
  - [ ] **TODO**: Add test for bulk IN transfer (loopback)
  - [ ] **TODO**: Add test for bulk OUT transfer (loopback)
  - [ ] **TODO**: Add test for simultaneous 3-port transfers
  - [ ] **TODO**: Add test for USB cable disconnect/reconnect
  - [ ] **TODO**: Add test for buffer overflow handling

### 4.2 Integration Tests

- [ ] **FILE**: `tests/test_usb_cdc_integration.c` (NEW)
  - [ ] **TODO**: Test USB enumeration sequence
  - [ ] **TODO**: Test SET_LINE_CODING / GET_LINE_CODING
  - [ ] **TODO**: Test bulk transfer throughput (measure actual vs expected)
  - [ ] **TODO**: Test log output over USB CDC
  - [ ] **TODO**: Test protocol port with nanopb frames

### 4.3 Hardware Validation

- [ ] **Hardware Test Plan**:
  - [ ] **TODO**: Connect to Linux host, verify 3 /dev/ttyACM* devices appear
  - [ ] **TODO**: Send 1MB of data from host → RX72N, verify CRC32
  - [ ] **TODO**: Send 1MB of data from RX72N → host, verify CRC32
  - [ ] **TODO**: Monitor USB traffic with Wireshark/usbmon
  - [ ] **TODO**: Test with real motor commands (nanopb protocol)
  - [ ] **TODO**: Stress test with continuous log output @ 115200 bps equivalent

---

## Phase 5: Documentation Updates (PRIORITY: MEDIUM)

### 5.1 Update Manual References

- [ ] **FILE**: `docs/sections/05_usb_cdc_protocol.tex` (NEW)
  - [ ] **TODO**: Document USB CDC architecture
  - [ ] **TODO**: Add USB descriptor tree diagram
  - [ ] **TODO**: Document log port integration

### 5.2 Update FUTURE_WORK_TODO.md

- [ ] **FILE**: `docs/FUTURE_WORK_TODO.md`
  - [ ] **TODO**: Mark USB CDC as ✅ VERIFIED once complete
  - [ ] **TODO**: Add lessons learned section
  - [ ] **TODO**: Document known limitations (e.g., max 3 ports)

---

## Known Issues and Limitations

### USB0 Hardware Limitations (RX72N Manual Ch40)

1. **Pipe Count**: Only 9 pipes total (pipe 0 = control, 8 pipes for data)
   - Each CDC port needs 3 pipes (2 bulk + 1 interrupt)
   - Maximum 3 CDC ports (9 pipes ÷ 3 pipes per port = 3 max)

2. **FIFO Size**: Total 2KB FIFO RAM shared across all pipes
   - CFIFO: 2048 bytes (common FIFO)
   - D0FIFO: Can access same memory as CFIFO
   - D1FIFO: Can access same memory as CFIFO
   - Not enough for large buffers - use ring buffers in SRAM instead

3. **DMA Support**: USB0 has DMA capability but NOT IMPLEMENTED
   - Could improve throughput by 2-3x
   - Requires DTC (Data Transfer Controller) configuration
   - Future enhancement, not critical for initial fix

4. **Suspend/Resume**: Low-power mode support INCOMPLETE
   - Suspend detection works (INTSTS0.SUSPM)
   - Resume handling not fully tested
   - May cause issues if host computer sleeps

### Software Limitations (Current Implementation)

1. **No Flow Control**: RTS/CTS not implemented
   - Host can send faster than RX72N can process
   - Ring buffer can overflow if app doesn't read fast enough
   - Solution: Increase ring buffer size or add flow control

2. **No Timeouts**: Blocking operations wait forever
   - `rx_usb_flush()` has timeout, but `rx_usb_write()` does not
   - If host stops reading, TX buffer fills and write blocks
   - Solution: Add timeout parameter to rx_usb_write()

3. **Single-Threaded Access**: Not thread-safe
   - If multiple tasks write to same port, data interleaves
   - Solution: Add mutex per port in application code

---

## Success Criteria

**Phase 1 Complete**: All register addresses verified against manual
**Phase 2 Complete**: Bulk transfers reliable (99.99% success rate over 1 hour)
**Phase 3 Complete**: Logging works over USB CDC with no message loss
**Phase 4 Complete**: All tests pass on hardware
**Phase 5 Complete**: Documentation updated

**Final Milestone**: USB CDC can replace UART for all debug logging.

---

## References

- **RX72N Manual**: Chapter 40 - USB 2.0 Full-Speed Host/Function Module
- **USB 2.0 Spec**: Chapter 9 - Device Framework
- **USB CDC Spec**: USB Class Definitions for Communications Devices v1.2
- **Code Files**:
  - `libs/rx_usb/inc/rx_usb.h` - Public API
  - `libs/rx_usb/src/rx_usb.c` - Main implementation
  - `libs/rx_usb/src/rx_usb_cdc.c` - CDC class layer
  - `libs/rx_usb/src/rx_usb_hw.c` - Hardware layer
  - `libs/rx_usb/src/rx_usb_isr.c` - Interrupt handler
  - `libs/rx_hal/inc/rx72n_usb_regs.h` - Register definitions

---

**Last Updated**: 2026-02-05
**Next Review**: After Phase 1 completion
