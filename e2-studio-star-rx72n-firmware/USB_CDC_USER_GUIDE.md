# USB CDC User Guide

**Target Audience**: End users, testers, field engineers

**Last Updated**: 2026-02-05

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Host OS Configuration](#host-os-configuration)
4. [Viewing Logs](#viewing-logs)
5. [Protocol Communication](#protocol-communication)
6. [Troubleshooting](#troubleshooting)
7. [FAQ](#faq)

---

## Overview

The RX72N firmware provides **three independent USB CDC-ACM virtual serial ports** over a single USB connection:

| Port | Device (Linux) | Purpose | Typical Use |
|------|----------------|---------|-------------|
| **Port 0 (Protocol)** | /dev/ttyACM0 | Binary protocol communication | Motor commands, telemetry, nanopb frames |
| **Port 1 (Decoded)** | /dev/ttyACM1 | ASCII frame dumps | Debugging, frame inspection |
| **Port 2 (Log)** | /dev/ttyACM2 | System logging | Runtime logs, diagnostics, errors |

**Benefits**:
- ✅ No external UART adapter required
- ✅ Real-time debugging without opening the case
- ✅ Simultaneous protocol communication and logging
- ✅ Standard USB drivers (no custom drivers needed)

---

## Hardware Requirements

### RX72N Board

- **USB Port**: Connect RX72N USB0 port to host computer
- **Pins**:
  - USB_DP (PA4) - Data Plus
  - USB_DM (PA5) - Data Minus
  - USB_VBUS (PA6) - VBUS detection (5V input)
- **Power**: Board must be powered (VBUS detection required)

### Host Computer

- **USB Port**: Any USB 2.0 or USB 3.x port
- **Cable**: Standard USB Type-A to Micro-B cable (or appropriate connector)
- **Operating System**: Linux, macOS, or Windows 10+

---

## Host OS Configuration

### Linux

**Kernel Support**: Built-in (cdc_acm driver)

**1. Connect USB cable**

Plug USB cable into RX72N board and Linux host.

**2. Verify enumeration**

```bash
# Check kernel messages
dmesg | tail -20
```

**Expected output**:
```
[ 1234.567890] usb 1-2: new full-speed USB device number 3 using xhci_hcd
[ 1234.789012] usb 1-2: New USB device found, idVendor=045b, idProduct=024f
[ 1234.789015] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 1234.789016] usb 1-2: Product: RX72N CDC Composite
[ 1234.789017] usb 1-2: Manufacturer: STAR Project
[ 1234.789018] usb 1-2: SerialNumber: RX72N-00001
[ 1234.890123] cdc_acm 1-2:1.0: ttyACM0: USB ACM device
[ 1234.890145] cdc_acm 1-2:1.2: ttyACM1: USB ACM device
[ 1234.890167] cdc_acm 1-2:1.4: ttyACM2: USB ACM device
```

**3. Check device nodes**

```bash
ls -l /dev/ttyACM*
```

**Expected output**:
```
crw-rw---- 1 root dialout 166, 0 Feb  5 10:00 /dev/ttyACM0
crw-rw---- 1 root dialout 166, 1 Feb  5 10:00 /dev/ttyACM1
crw-rw---- 1 root dialout 166, 2 Feb  5 10:00 /dev/ttyACM2
```

**4. Add user to dialout group** (if needed)

```bash
sudo usermod -a -G dialout $USER
# Log out and log back in for group change to take effect
```

### macOS

**Driver**: Built-in (AppleUSBCDC.kext)

**1. Connect USB cable**

**2. Verify enumeration**

```bash
# List USB devices
system_profiler SPUSBDataType | grep -A 10 "RX72N"
```

**3. Check device nodes**

```bash
ls -l /dev/cu.usbmodem*
```

**Expected output**:
```
crw-rw-rw-  1 root  wheel    9,   4 Feb  5 10:00 /dev/cu.usbmodem14101
crw-rw-rw-  1 root  wheel    9,   6 Feb  5 10:00 /dev/cu.usbmodem14103
crw-rw-rw-  1 root  wheel    9,   8 Feb  5 10:00 /dev/cu.usbmodem14105
```

**Port Mapping** (macOS):
- `/dev/cu.usbmodem14101` → Port 0 (Protocol)
- `/dev/cu.usbmodem14103` → Port 1 (Decoded)
- `/dev/cu.usbmodem14105` → Port 2 (Log)

### Windows

**Driver**: Built-in (usbser.sys - Windows 10+)

**1. Connect USB cable**

Windows will automatically install the USB CDC driver.

**2. Verify in Device Manager**

Open Device Manager (Win+X → Device Manager) and expand "Ports (COM & LPT)":

```
Ports (COM & LPT)
  ├─ RX72N CDC Protocol Port (COM3)
  ├─ RX72N CDC Decoded Port (COM4)
  └─ RX72N CDC Log Port (COM5)
```

**Note**: COM port numbers may vary depending on your system.

**3. If driver fails to install**

- Right-click the device → Update Driver
- Select "Search automatically for drivers"
- Windows will install the built-in usbser.sys driver

---

## Viewing Logs

### Linux

#### Option 1: screen (Recommended)

```bash
# View logs from Port 2 (Log Port)
screen /dev/ttyACM2 115200

# Exit: Press Ctrl+A, then K, then Y
```

#### Option 2: minicom

```bash
# First-time setup
sudo minicom -s
# Navigate to "Serial port setup"
# Set "Serial Device" to /dev/ttyACM2
# Set "Bps/Par/Bits" to 115200 8N1
# Save setup as "rx72n_log"

# View logs
minicom rx72n_log

# Exit: Press Ctrl+A, then X
```

#### Option 3: cat (simple but no terminal control)

```bash
cat /dev/ttyACM2
# Exit: Press Ctrl+C
```

#### Option 4: tail (continuous monitoring)

```bash
# Continuously monitor logs
tail -f /dev/ttyACM2

# Save to file while viewing
tail -f /dev/ttyACM2 | tee rx72n_logs.txt
```

### macOS

```bash
# View logs
screen /dev/cu.usbmodem14105 115200

# Exit: Press Ctrl+A, then K, then Y
```

### Windows

#### Option 1: PuTTY (Recommended)

1. Download PuTTY: https://www.putty.org/
2. Open PuTTY
3. Connection type: Serial
4. Serial line: COM5 (adjust to your Log Port COM number)
5. Speed: 115200
6. Click "Open"

#### Option 2: Tera Term

1. Download Tera Term: https://ttssh2.osdn.jp/
2. Open Tera Term
3. Setup → Serial Port
4. Port: COM5 (adjust to your Log Port COM number)
5. Baud rate: 115200
6. Data: 8 bit
7. Parity: none
8. Stop: 1 bit
9. Flow control: none

#### Option 3: Arduino Serial Monitor

1. Open Arduino IDE
2. Tools → Port → Select your Log Port (COM5)
3. Tools → Serial Monitor
4. Set baud rate to 115200

---

## Protocol Communication

### Sending Commands (Port 0 - Protocol Port)

**Linux/macOS**:
```bash
# Send binary protocol frame
echo -en '\x7E\x00\x08...' > /dev/ttyACM0

# Or use a custom tool
./star_protocol_tool --port /dev/ttyACM0 --command motor_set_velocity 1.5
```

**Windows**:
Use a protocol communication tool like:
- RealTerm
- Docklight
- Custom Python/C# application

### Viewing Frame Dumps (Port 1 - Decoded Port)

Decoded Port shows ASCII representation of protocol frames for debugging:

```bash
# Linux/macOS
screen /dev/ttyACM1 115200
```

**Example output**:
```
[12345.678] RX Frame: seq=123 len=32 type=0x01 CRC=0xABCD1234
[12345.890] TX Frame: seq=124 len=16 type=0x02 CRC=0x5678ABCD
```

---

## Troubleshooting

### Problem: No /dev/ttyACM* devices appear (Linux)

**Possible Causes**:

1. **USB cable not connected properly**
   - Solution: Disconnect and reconnect USB cable
   - Check: `lsusb` should show vendor ID 045b (Renesas)

2. **RX72N not powered**
   - Solution: Ensure board has power
   - Check: VBUS detection LED (if present)

3. **USB firmware not running**
   - Solution: Verify firmware flashed correctly
   - Check: Re-flash firmware, power cycle board

4. **Permission denied**
   - Solution: Add user to dialout group:
     ```bash
     sudo usermod -a -G dialout $USER
     # Log out and back in
     ```

5. **Another process using the port**
   - Check:
     ```bash
     sudo lsof | grep ttyACM
     ```
   - Solution: Kill the process or close the application

### Problem: Devices appear but no logs visible

**Possible Causes**:

1. **USB_LOG_MIRROR not enabled**
   - Solution: Rebuild firmware with `USB_LOG_MIRROR=1`
   - Check: Build configuration in e2 studio

2. **USB enumeration not complete**
   - Wait 200-500ms after power-on for boot buffer flush
   - Early logs buffered, will appear after USB ready

3. **Terminal settings incorrect**
   - Ensure baud rate set to 115200 (informational only)
   - Check: 8 data bits, No parity, 1 stop bit (8N1)

4. **Wrong port selected**
   - Verify you're reading from Port 2 (Log Port)
   - Linux: /dev/ttyACM2
   - macOS: Last /dev/cu.usbmodem*
   - Windows: Highest COM number (usually)

### Problem: Garbled or corrupted output

**Possible Causes**:

1. **Cable quality issues**
   - Solution: Use high-quality USB cable (< 1 meter)
   - Avoid USB hubs if possible

2. **Hardware issue (FIFO access)**
   - This should be fixed in Phase 2 (16-bit FIFO access)
   - If still occurs: Check firmware version, verify Phase 2 fixes applied

3. **Multiple programs reading same port**
   - Solution: Close other applications accessing the port
   - Check: `sudo lsof | grep ttyACM2`

### Problem: Logs cut off or missing (dropped logs)

**Possible Causes**:

1. **TX buffer full (high log rate)**
   - Expected behavior: Non-blocking policy drops logs on TX full
   - Check statistics:
     ```c
     usb_log_stats_t stats;
     rx_log_usb_get_stats(&stats);
     rx_log_info("STATS", "Dropped: %u bytes", stats.dropped_bytes);
     ```
   - Solution: Reduce log verbosity, increase host read rate

2. **Boot buffer overflow (>512B during enumeration)**
   - Warning will be logged: "Boot buffer overflow, some logs dropped"
   - Solution: Reduce early boot logs, or increase buffer size

3. **Host not reading fast enough**
   - Solution: Use faster terminal (screen instead of cat)
   - Consider saving to file instead of displaying

### Problem: Port numbering changes after reconnect (Linux)

**Cause**: Linux assigns ttyACM numbers dynamically based on enumeration order.

**Solutions**:

1. **Use udev rules** (persistent naming):

Create `/etc/udev/rules.d/99-rx72n-usb.rules`:
```
# RX72N USB CDC - Port 0 (Protocol)
SUBSYSTEM=="tty", ATTRS{idVendor}=="045b", ATTRS{idProduct}=="024f", \
  ENV{ID_USB_INTERFACE_NUM}=="00", SYMLINK+="rx72n_protocol"

# RX72N USB CDC - Port 1 (Decoded)
SUBSYSTEM=="tty", ATTRS{idVendor}=="045b", ATTRS{idProduct}=="024f", \
  ENV{ID_USB_INTERFACE_NUM}=="02", SYMLINK+="rx72n_decoded"

# RX72N USB CDC - Port 2 (Log)
SUBSYSTEM=="tty", ATTRS{idVendor}=="045b", ATTRS{idProduct}=="024f", \
  ENV{ID_USB_INTERFACE_NUM}=="04", SYMLINK+="rx72n_log"
```

Reload udev:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Now use:
- `/dev/rx72n_protocol` → Port 0
- `/dev/rx72n_decoded` → Port 1
- `/dev/rx72n_log` → Port 2

2. **Always connect to same physical USB port** (simpler but less robust)

---

## FAQ

### Q: What is the actual data rate?

**A**: USB CDC operates at Full-Speed (12 Mbps) regardless of the "baud rate" setting. The 115200 setting shown in terminal programs is informational only and has no effect on transfer speed.

**Actual performance**:
- USB Full-Speed: 12 Mbps theoretical
- Effective throughput: ~1.2 MB/s (80% efficiency)
- Per-port maximum: ~400 KB/s

### Q: Can I use all three ports simultaneously?

**A**: Yes! All three ports are independent and can be used concurrently:
- Port 0: Protocol communication (motor commands, telemetry)
- Port 1: Frame dumps (debugging)
- Port 2: Logs (diagnostics)

### Q: Do I need to configure anything in the firmware?

**A**: For logging on Port 2, ensure `USB_LOG_MIRROR=1` is defined during compilation. Otherwise, logs only appear on UART (SCI12).

**To enable**:
1. e2 studio: Project Properties → Compiler → Preprocessor → Add `USB_LOG_MIRROR=1`
2. Build firmware
3. Flash to RX72N

### Q: What happens if USB cable is unplugged?

**A**:
- USB state changes to `Detached`
- All pending transfers aborted
- Logs switch to UART-only (if `USB_LOG_MIRROR=1`)
- On reconnect, USB re-enumerates and logging resumes

### Q: How long does boot log buffering last?

**A**: Boot logs are buffered for **0-200ms** (typical USB enumeration time). Once USB reaches `Configured` state, the 512-byte boot buffer is flushed to Port 2, and normal logging continues.

### Q: Can I save logs to a file?

**A**: Yes!

**Linux/macOS**:
```bash
cat /dev/ttyACM2 > rx72n_logs.txt      # Until Ctrl+C
tail -f /dev/ttyACM2 > rx72n_logs.txt  # Continuous
```

**Windows**: Use PuTTY or Tera Term log file feature.

### Q: Why does my terminal show weird characters sometimes?

**A**: This can happen if:
1. **Non-ASCII data**: Ensure you're reading Port 2 (Log), not Port 0 (binary protocol)
2. **ANSI escape codes**: Some log messages may include color codes
3. **Buffer overflow**: Dropped bytes can cause mid-message corruption

Solution: Use terminal with proper UTF-8 and ANSI support (screen, minicom).

### Q: How do I verify Phase 2 fixes are applied?

**A**: Check firmware git log:
```bash
cd /workspaces/STAR/e2-studio-star-rx72n-firmware
git log --oneline | grep "Phase 2"
```

Look for commit: "Fix USB CDC bulk transfer reliability: All 3 critical issues"

Or check source code:
- `libs/rx_usb/src/rx_usb_hw.c:735-744` - Should use 16-bit FIFO access
- `libs/rx_usb/src/rx_usb_hw.c:793-810` - Should include BCLR sequence

---

## Getting Help

### Check Documentation

1. [USB_CDC_STATUS.md](USB_CDC_STATUS.md) - Implementation status
2. [USB_CDC_PHASE2_TESTING_GUIDE.md](USB_CDC_PHASE2_TESTING_GUIDE.md) - Hardware testing
3. [docs/sections/09_usb_cdc_protocol.tex](/workspaces/STAR/docs/sections/09_usb_cdc_protocol.tex) - Protocol specification

### Report Issues

If you encounter problems:
1. Check troubleshooting section above
2. Verify Phase 2 and Phase 3 fixes are applied
3. Collect logs and device information:
   ```bash
   # Linux
   dmesg | grep -i usb > usb_debug.txt
   lsusb -v -d 045b:024f >> usb_debug.txt

   # Include firmware version
   echo "Firmware: $(git describe --always)" >> usb_debug.txt
   ```
4. Open issue with collected information

---

**Document Version**: 1.0
**Last Updated**: 2026-02-05
**Firmware Branch**: bsikar/161_usb_cdc_debug
