# USB CDC Phase 2: Hardware Testing Guide

## Overview

**Purpose**: Validate bulk transfer reliability fixes on real RX72N hardware
**Prerequisites**: Phase 2 fixes committed (all 3 critical issues resolved)
**Duration**: 2-3 hours for complete validation
**Date**: 2026-02-05

---

## Critical Fixes to Validate

### Fix #1: 16-Bit FIFO Access ✅
- **Files**: rx_usb_hw.c lines 735-744 (read), 793-810 (write)
- **Change**: Byte-by-byte → 16-bit word access
- **Expected**: No data corruption in transfers

### Fix #2: FIFO Clear Before Write ✅
- **File**: rx_usb_hw.c lines 793-810
- **Change**: Added BCLR sequence before FIFO write
- **Expected**: First packet integrity maintained

### Fix #3: BEMP/BRDY Interrupts ✅
- **File**: rx_usb_cdc.c lines 1752-1759
- **Change**: Verified enabled, updated to named constants
- **Expected**: Interrupts fire reliably

---

## Test Environment Setup

### Hardware Required

1. **RX72N Target Board**
   - RX72N MCU with USB0 port
   - USB cable (target to host)
   - Debug probe (E2 Lite or similar)

2. **Host Computer**
   - Linux (preferred): Native CDC-ACM driver
   - Windows: Custom INF or WinUSB
   - macOS: Native CDC-ACM driver

3. **Tools**
   - Serial terminal (minicom, screen, PuTTY)
   - USB traffic analyzer (Wireshark + usbmon, or Beagle)
   - CRC32 calculator (crc32 command or Python)

### Software Setup

**Build firmware with fixes:**
```bash
cd /workspaces/STAR/e2-studio-star-rx72n-firmware
# Build for HardwareDebug (not Simulator!)
# Import project in e2 studio
# Build Configuration: HardwareDebug
# Flash to target via E2 Lite
```

**Linux host setup:**
```bash
# Check USB CDC devices appear
lsusb | grep "CDC"
ls -l /dev/ttyACM*

# Should see 3 ports:
# /dev/ttyACM0 - Port 0 (Protocol)
# /dev/ttyACM1 - Port 1 (Decoded)
# /dev/ttyACM2 - Port 2 (Log)

# Check kernel log
dmesg | tail -20
# Should see: "cdc_acm: USB Abstract Control Model driver for USB modems and ISDN adapters"
```

---

## Test Suite

### Test 1: USB Enumeration ✅ SANITY CHECK

**Objective**: Verify device enumerates with 3 CDC ports

**Procedure**:
1. Connect USB cable from RX72N to host
2. Power on RX72N
3. Check USB enumeration

**Expected Results**:
```bash
# lsusb output
Bus 001 Device 010: ID 045b:XXXX Renesas STAR RX72N CDC Composite

# dmesg output
[12345.678] usb 1-2: new full-speed USB device number 10 using xhci_hcd
[12345.789] usb 1-2: New USB device found, idVendor=045b, idProduct=XXXX
[12345.890] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[12345.901] usb 1-2: Product: STAR RX72N CDC Composite
[12346.012] cdc_acm 1-2:1.0: ttyACM0: USB ACM device
[12346.123] cdc_acm 1-2:1.2: ttyACM1: USB ACM device
[12346.234] cdc_acm 1-2:1.4: ttyACM2: USB ACM device

# 3 ports created
ls -l /dev/ttyACM*
crw-rw---- 1 root dialout 166, 0 Feb  5 06:00 /dev/ttyACM0
crw-rw---- 1 root dialout 166, 1 Feb  5 06:00 /dev/ttyACM1
crw-rw---- 1 root dialout 166, 2 Feb  5 06:00 /dev/ttyACM2
```

**Pass Criteria**: All 3 ports appear in /dev/

---

### Test 2: Single Packet Transfer (Bulk OUT)

**Objective**: Verify BRDY interrupt and FIFO read for single packet

**Procedure**:
```bash
# Open port
sudo minicom -D /dev/ttyACM0 -b 115200

# OR use echo
echo "Hello RX72N" | sudo tee /dev/ttyACM0
```

**Expected Behavior**:
1. Host sends 64-byte (or less) USB packet
2. RX72N BRDY interrupt fires for pipe 2 (Port 0 Bulk OUT)
3. ISR reads FIFO (16-bit word access)
4. Data copied to RX ring buffer
5. Application can read via rx_usb_read()

**Validation**:
- Check firmware logs (UART or USB Log port)
- Verify BRDY interrupt fired
- Verify data received without corruption
- Check FIFO DTLN matched packet size

**Pass Criteria**: Data received correctly, no corruption

---

### Test 3: Single Packet Transfer (Bulk IN)

**Objective**: Verify BEMP interrupt and FIFO write for single packet

**Procedure**:
```bash
# On RX72N firmware (application code):
# rx_usb_write(k_usb_port_proto, "Test response", 13);

# On host, read from port:
sudo cat /dev/ttyACM0
```

**Expected Behavior**:
1. Application writes to TX ring buffer
2. First packet written to FIFO (with BCLR)
3. USB0 hardware sends packet to host
4. Host ACKs packet
5. BEMP interrupt fires for pipe 1 (Port 0 Bulk IN)
6. ISR writes next packet (or completes if buffer empty)

**Validation**:
- Check firmware logs for FIFO write
- Verify BCLR executed before write
- Verify BEMP interrupt fired
- Verify host received data without corruption

**Pass Criteria**: Data transmitted correctly, no corruption

---

### Test 4: Multi-Packet Transfer (1KB Bulk OUT)

**Objective**: Verify sustained Bulk OUT with multiple 64-byte packets

**Procedure**:
```bash
# Generate 1KB test data with known pattern
python3 << 'EOF'
data = bytes(range(256)) * 4  # 1024 bytes, repeating 0-255 pattern
with open('/tmp/test_1kb.bin', 'wb') as f:
    f.write(data)
print(f"Generated 1KB test file, CRC32: {hex(crc32(data))}")
EOF

# Send to RX72N
sudo dd if=/tmp/test_1kb.bin of=/dev/ttyACM0 bs=64

# On RX72N, compute CRC32 of received data and log it
```

**Expected Behavior**:
1. Host sends 16 packets (1024 / 64 = 16)
2. BRDY interrupt fires 16 times
3. Each packet read from FIFO (16-bit access)
4. Data accumulated in RX ring buffer
5. Application reads and verifies CRC32

**Validation**:
- BRDY fired 16 times (check interrupt counter)
- All 1024 bytes received
- CRC32 matches expected value
- No data corruption or loss

**Pass Criteria**: CRC32 match, 0% packet loss

---

### Test 5: Multi-Packet Transfer (1KB Bulk IN)

**Objective**: Verify sustained Bulk IN with multiple 64-byte packets

**Procedure**:
```bash
# On RX72N firmware:
# Send 1KB with known pattern (0-255 repeated)
# uint8_t data[1024];
# for (int i=0; i<1024; i++) data[i] = i % 256;
# rx_usb_write(k_usb_port_proto, data, 1024);

# On host, receive and verify
sudo dd if=/dev/ttyACM0 of=/tmp/rx_1kb.bin bs=64 count=16

# Compute CRC32
python3 << 'EOF'
with open('/tmp/rx_1kb.bin', 'rb') as f:
    data = f.read()
    print(f"Received {len(data)} bytes, CRC32: {hex(crc32(data))}")
EOF
```

**Expected Behavior**:
1. Application writes 1024 bytes to TX buffer
2. First 64 bytes written to FIFO (with BCLR)
3. BEMP interrupt fires after each packet transmitted
4. ISR writes next 64 bytes (16 packets total)
5. Host receives all packets

**Validation**:
- BEMP fired 16 times
- All 1024 bytes transmitted
- CRC32 matches expected
- First packet had BCLR (no stale data)

**Pass Criteria**: CRC32 match, 0% packet loss

---

### Test 6: Simultaneous 3-Port Transfer

**Objective**: Verify all 3 CDC ports work simultaneously

**Procedure**:
```bash
# Terminal 1: Port 0 (Protocol)
echo "Port0-Test" | sudo tee /dev/ttyACM0 &
sudo cat /dev/ttyACM0 &

# Terminal 2: Port 1 (Decoded)
echo "Port1-Test" | sudo tee /dev/ttyACM1 &
sudo cat /dev/ttyACM1 &

# Terminal 3: Port 2 (Log)
echo "Port2-Test" | sudo tee /dev/ttyACM2 &
sudo cat /dev/ttyACM2 &

# Let run for 10 seconds
sleep 10
killall cat
```

**Expected Behavior**:
- All 3 ports send/receive simultaneously
- No crosstalk between ports
- BRDY/BEMP interrupts for pipes 1-9 fire correctly
- Ring buffers independent

**Validation**:
- Each port receives its own data only
- No data corruption
- No port blocks another

**Pass Criteria**: All 3 ports work independently

---

### Test 7: Large Transfer (1MB Loopback)

**Objective**: Stress test with 1MB continuous transfer

**Procedure**:
```bash
# Generate 1MB test data
dd if=/dev/urandom of=/tmp/test_1mb.bin bs=1M count=1
crc32 /tmp/test_1mb.bin > /tmp/expected_crc32.txt

# Send to RX72N (loopback mode: echo back what's received)
sudo cat /tmp/test_1mb.bin > /dev/ttyACM0 &
sudo dd if=/dev/ttyACM0 of=/tmp/rx_1mb.bin bs=1M count=1

# Verify CRC32
crc32 /tmp/rx_1mb.bin
diff /tmp/expected_crc32.txt <(crc32 /tmp/rx_1mb.bin)
```

**Expected Behavior**:
- 1MB = 16,384 packets (64 bytes each)
- BRDY fires 16,384 times (RX)
- BEMP fires 16,384 times (TX)
- Transfer completes in ~8 seconds (Full-Speed USB ~1.2 MB/s)

**Validation**:
- Transfer completes without errors
- CRC32 matches (0% corruption)
- No timeouts
- No ring buffer overflows

**Pass Criteria**: CRC32 match, transfer time ~8-10 seconds

---

### Test 8: USB Cable Disconnect/Reconnect

**Objective**: Verify graceful handling of cable events

**Procedure**:
1. Start transfer: `sudo cat /dev/ttyACM0 &`
2. **Unplug USB cable** during transfer
3. Wait 5 seconds
4. **Plug cable back in**
5. Check enumeration

**Expected Behavior**:
- On disconnect:
  - VBUS interrupt fires (VBINT)
  - Device state → Detached
  - Transfer aborted gracefully (no crash)
  - Pipes reset
- On reconnect:
  - VBUS detected → Powered
  - USB RESET → Default
  - Re-enumeration begins
  - Host creates /dev/ttyACMx ports again

**Validation**:
- Firmware doesn't crash
- Clean state transition
- Successful re-enumeration
- Ports functional after reconnect

**Pass Criteria**: No crash, clean reconnect

---

### Test 9: Wireshark USB Traffic Capture

**Objective**: Verify USB protocol compliance at packet level

**Procedure**:
```bash
# Start usbmon capture (Linux)
sudo modprobe usbmon
sudo wireshark

# Wireshark → Capture → USB (usbmonX)
# Filter: usb.device_address == 10 (your RX72N device)

# Perform Test 4 (1KB Bulk OUT) while capturing

# Analyze:
# - Bulk OUT tokens
# - Data packets (64 bytes each)
# - ACK handshakes
# - No NAK/STALL errors
```

**Expected USB Traffic**:
```
1. SETUP (Get Descriptor - Device)
2. IN, DATA0, ACK
3. SETUP (Get Descriptor - Configuration)
4. IN, DATA0, ACK
5. SETUP (Set Configuration)
6. OUT, DATA0, ACK
7. [Bulk Transfers Begin]
8. OUT (Endpoint 1), DATA0 (64 bytes), ACK
9. OUT (Endpoint 1), DATA1 (64 bytes), ACK
10. [Data toggle alternates: DATA0, DATA1, DATA0, ...]
```

**Validation**:
- No NAK errors (pipe always ready)
- No STALL errors (no protocol violations)
- Data toggle sequence correct (DATA0/DATA1 alternates)
- Packet sizes correct (64 bytes for bulk)
- Throughput ~900 KB/s (Full-Speed bulk max)

**Pass Criteria**: No protocol errors, correct data toggle

---

### Test 10: Long-Duration Stability Test

**Objective**: Verify 99.99% reliability over 1 hour

**Procedure**:
```bash
#!/bin/bash
# run_stability_test.sh

START_TIME=$(date +%s)
PACKETS_SENT=0
PACKETS_FAILED=0
DURATION_SEC=3600  # 1 hour

while [ $(($(date +%s) - START_TIME)) -lt $DURATION_SEC ]; do
  # Send 1KB packet
  dd if=/dev/urandom of=/tmp/test.bin bs=1K count=1 2>/dev/null
  EXPECTED_CRC=$(crc32 /tmp/test.bin)

  # Send to RX72N, receive back (loopback)
  sudo dd if=/tmp/test.bin of=/dev/ttyACM0 bs=1K 2>/dev/null &
  sudo dd if=/dev/ttyACM0 of=/tmp/rx.bin bs=1K count=1 2>/dev/null
  wait

  # Verify
  ACTUAL_CRC=$(crc32 /tmp/rx.bin)
  PACKETS_SENT=$((PACKETS_SENT + 1))

  if [ "$EXPECTED_CRC" != "$ACTUAL_CRC" ]; then
    PACKETS_FAILED=$((PACKETS_FAILED + 1))
    echo "FAIL at packet $PACKETS_SENT: CRC mismatch"
  fi

  # Progress every 100 packets
  if [ $((PACKETS_SENT % 100)) -eq 0 ]; then
    ELAPSED=$(($(date +%s) - START_TIME))
    SUCCESS_RATE=$(echo "scale=4; 100 * (1 - $PACKETS_FAILED / $PACKETS_SENT)" | bc)
    echo "[$ELAPSED s] Packets: $PACKETS_SENT, Failures: $PACKETS_FAILED, Success: ${SUCCESS_RATE}%"
  fi
done

# Final report
SUCCESS_RATE=$(echo "scale=4; 100 * (1 - $PACKETS_FAILED / $PACKETS_SENT)" | bc)
echo "=== STABILITY TEST COMPLETE ==="
echo "Duration: 1 hour"
echo "Packets sent: $PACKETS_SENT"
echo "Packets failed: $PACKETS_FAILED"
echo "Success rate: ${SUCCESS_RATE}%"

if (( $(echo "$SUCCESS_RATE >= 99.99" | bc -l) )); then
  echo "PASS: Success rate >= 99.99%"
  exit 0
else
  echo "FAIL: Success rate < 99.99%"
  exit 1
fi
```

**Expected Results**:
- ~3600 packets over 1 hour
- Failures < 1 (0.027% failure = 99.99% success)
- No crashes or hangs

**Pass Criteria**: Success rate ≥ 99.99%

---

## Success Criteria Summary

| Test | Objective | Pass Criteria |
|------|-----------|---------------|
| 1 | Enumeration | 3 CDC ports appear |
| 2 | Single Bulk OUT | Data received correctly |
| 3 | Single Bulk IN | Data transmitted correctly |
| 4 | Multi-packet OUT (1KB) | CRC32 match, 0% loss |
| 5 | Multi-packet IN (1KB) | CRC32 match, 0% loss |
| 6 | 3-port simultaneous | All ports work independently |
| 7 | Large transfer (1MB) | CRC32 match, ~8-10 sec |
| 8 | Cable disconnect | No crash, clean reconnect |
| 9 | Wireshark capture | No NAK/STALL, correct toggle |
| 10 | 1-hour stability | ≥ 99.99% success rate |

**Overall Pass**: ALL 10 tests must pass

---

## Failure Analysis

### If Test Fails: Byte-by-Byte Access Still Present

**Symptom**: Data corruption, wrong CRC32

**Check**:
```bash
# Verify 16-bit access in binary
objdump -d rx_usb_hw.o | grep -A 10 "rx_usb_hw_fifo_read"
# Should see 16-bit loads (MOV.W), not byte loads (MOV.B)
```

**Fix**: Verify compiler optimizations didn't revert to byte access

---

### If Test Fails: FIFO Not Cleared

**Symptom**: First packet corrupted, subsequent packets OK

**Check**: Add logging before BCLR in rx_usb_hw_fifo_write()
```c
rx_log_debug("FIFO", "BCLR before write");
usb0()->cfifoctr |= k_usb_fifoctr_bclr;
```

**Fix**: Verify BCLR sequence executes, timeout not triggering

---

### If Test Fails: BEMP Not Firing

**Symptom**: TX stops after first packet

**Check**: Verify BEMPENB register
```c
uint16_t bempenb = usb0()->bempenb;
rx_log_info("USB", "BEMPENB = 0x%04X (expect 0x0092)", bempenb);
// Expected: 0x0092 = bits 1, 4, 7 set
```

**Fix**: Check SET_CONFIGURATION handler enables BEMP

---

## Testing Checklist

**Pre-Test**:
- [ ] Phase 2 fixes committed (3 critical issues)
- [ ] Firmware built for HardwareDebug
- [ ] Flashed to RX72N target
- [ ] USB cable connected
- [ ] Host OS ready (Linux preferred)

**Test Execution**:
- [ ] Test 1: Enumeration ✅
- [ ] Test 2: Single Bulk OUT ✅
- [ ] Test 3: Single Bulk IN ✅
- [ ] Test 4: Multi-packet OUT (1KB) ✅
- [ ] Test 5: Multi-packet IN (1KB) ✅
- [ ] Test 6: 3-port simultaneous ✅
- [ ] Test 7: Large transfer (1MB) ✅
- [ ] Test 8: Cable disconnect ✅
- [ ] Test 9: Wireshark capture ✅
- [ ] Test 10: 1-hour stability ✅

**Post-Test**:
- [ ] All tests passed
- [ ] Test results documented
- [ ] Issues logged (if any)
- [ ] Ready for Phase 3 (debug logging integration)

---

## Test Results Template

```markdown
# USB CDC Phase 2 Test Results

**Date**: YYYY-MM-DD
**Tester**: [Name]
**Firmware**: Commit [hash]
**Host OS**: Linux [version] / Windows [version] / macOS [version]

## Test Summary

| Test | Result | Notes |
|------|--------|-------|
| 1. Enumeration | PASS/FAIL | |
| 2. Single Bulk OUT | PASS/FAIL | |
| 3. Single Bulk IN | PASS/FAIL | |
| 4. Multi-packet OUT | PASS/FAIL | CRC32: 0xXXXXXXXX |
| 5. Multi-packet IN | PASS/FAIL | CRC32: 0xXXXXXXXX |
| 6. 3-port simultaneous | PASS/FAIL | |
| 7. Large transfer (1MB) | PASS/FAIL | Time: X.XX sec |
| 8. Cable disconnect | PASS/FAIL | |
| 9. Wireshark capture | PASS/FAIL | No NAK/STALL: YES/NO |
| 10. 1-hour stability | PASS/FAIL | Success rate: XX.XX% |

**Overall Result**: PASS / FAIL

## Issues Found

[List any issues encountered]

## Conclusion

[Summary of test results and readiness for Phase 3]
```

---

**Testing Guide Created**: 2026-02-05
**Next**: Execute tests on hardware, document results
**Then**: Proceed to Phase 3 if all tests pass
