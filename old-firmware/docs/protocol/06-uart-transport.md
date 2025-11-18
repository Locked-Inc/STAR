# UART Transport Layer

## Overview

The UART transport layer provides reliable serial communication between the ESP32 and Raspberry Pi 5 board. This document covers the physical layer configuration, protocol implementation, buffer management, error handling, and best practices for robust communication.

### Key Features

- **Physical Layer**: 115200 baud, 8N1 (8 data bits, no parity, 1 stop bit)
- **Packet-Based Protocol**: Start marker, command byte, length field, variable payload
- **State Machine**: Robust byte-by-byte parsing with automatic error recovery
- **Buffer Management**: Configurable RX/TX buffers with overflow protection
- **Error Handling**: Automatic retry with exponential backoff
- **Statistics**: Comprehensive tracking of bytes, operations, and errors

---

## 1. Physical UART Configuration

### Hardware Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Baud Rate | 115200 | Default, configurable up to 921600 |
| Data Bits | 8 | Standard |
| Parity | None | Error detection via protocol layer |
| Stop Bits | 1 | Standard |
| Flow Control | None | Software flow control via protocol |

### ESP32 Pin Configuration

#### ESP32-WROOM-32 (4MB Flash)
```c
TX Pin: GPIO 17  // Transmit to Raspberry Pi
RX Pin: GPIO 16  // Receive from Raspberry Pi
```

#### ESP32-S3-WROOM-1-N16 (16MB Flash)
```c
TX Pin: GPIO 43  // Transmit to Raspberry Pi (TXD0)
RX Pin: GPIO 44  // Receive from Raspberry Pi (RXD0)
```

### Wiring Diagram

```
ESP32              Raspberry Pi 5
-----              -------
GPIO 17 (TX) ----> RX
GPIO 16 (RX) <---- TX
GND          ----- GND
```

**Important**:
- Connect TX on one device to RX on the other (crossover)
- Always connect grounds between devices
- ESP32 operates at 3.3V - ensure voltage compatibility
- Keep wire lengths under 30cm for reliable communication at 115200 baud

### Buffer Configuration

Default buffer sizes (configurable in code):

```c
RX Buffer: 2048 bytes  // Receiving from Raspberry Pi
TX Buffer: 2048 bytes  // Transmitting to Raspberry Pi
```

---

## 2. Setting Up UART Connections

### Linux

#### Finding Your UART Device

**Method 1: Before/After Comparison**
```bash
# Before plugging in adapter
ls /dev/ttyUSB*

# Plug in USB-UART adapter

# After plugging in
ls /dev/ttyUSB*
# The new device is your adapter (e.g., /dev/ttyUSB0)
```

**Method 2: Check Kernel Messages**
```bash
sudo dmesg | tail -20
# Look for lines like:
# [12345.678] usb 1-1: ch341-uart converter now attached to ttyUSB0
```

**Method 3: Device Information**
```bash
udevadm info /dev/ttyUSB0 | grep ID_SERIAL
# Shows detailed information about the USB device
```

#### Setting Permissions

**Temporary (current session only)**
```bash
sudo chmod 666 /dev/ttyUSB0
```

**Permanent (add user to dialout group)**
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in for changes to take effect
```

**Verify Permissions**
```bash
ls -l /dev/ttyUSB0
# Should show: crw-rw---- 1 root dialout ... /dev/ttyUSB0
```

#### Testing the Connection

Using `screen`:
```bash
screen /dev/ttyUSB0 115200
# Press Ctrl-A, then K to exit
```

Using `minicom`:
```bash
sudo minicom -D /dev/ttyUSB0 -b 115200
# Press Ctrl-A, then X to exit
```

### macOS

#### Finding Your Device

```bash
ls /dev/tty.*
# Look for something like /dev/tty.usbserial-*
```

Common patterns:
- `/dev/tty.usbserial-*` - FTDI adapters
- `/dev/tty.SLAB_USBtoUART` - Silicon Labs CP210x
- `/dev/tty.wchusbserial*` - CH340/CH341 adapters

#### Testing

```bash
screen /dev/tty.usbserial-1234 115200
```

### Windows

#### Finding Your COM Port

**Method 1: Device Manager**
1. Open Device Manager (Win+X, then M)
2. Expand "Ports (COM & LPT)"
3. Look for "USB Serial Port (COM3)" or similar
4. Note the COM port number

**Method 2: PowerShell**
```powershell
Get-WmiObject Win32_SerialPort | Select Name,DeviceID
```

#### Testing with PuTTY

1. Download PuTTY from https://www.putty.org/
2. Select "Serial" connection type
3. Enter COM port (e.g., COM3)
4. Set speed to 115200
5. Click "Open"

---

## 3. Packet Protocol Format

### Packet Structure

```
+-------+--------+--------+--------+-----------+
| Start |  CMD   | Len_Lo | Len_Hi |  Payload  |
+-------+--------+--------+--------+-----------+
| 0xA5  | 1 byte | 1 byte | 1 byte |  N bytes  |
+-------+--------+--------+--------+-----------+
```

### Field Descriptions

| Field | Size | Description |
|-------|------|-------------|
| Start | 1 byte | Always 0xA5 - packet synchronization marker |
| CMD | 1 byte | Command ID (see protocol commands documentation) |
| Len_Lo | 1 byte | Low byte of payload length (little-endian) |
| Len_Hi | 1 byte | High byte of payload length (little-endian) |
| Payload | 0-1024 bytes | Command-specific data |

### Constants

```c
#define PROTOCOL_START_MARKER      0xA5
#define PROTOCOL_HEADER_SIZE       4
#define PROTOCOL_MAX_PAYLOAD_SIZE  1024
```

---

## 4. Packet Synchronization and Framing

### State Machine

The ESP32 uses a state machine to parse incoming bytes and reconstruct packets:

```
k_rx_state_wait_start    -> Wait for 0xA5 start marker
k_rx_state_wait_cmd      -> Receive command byte
k_rx_state_wait_len_lo   -> Receive low byte of length
k_rx_state_wait_len_hi   -> Receive high byte of length
k_rx_state_wait_payload  -> Receive payload bytes
```

### State Transitions

```c
State: WAIT_START
+- Receive 0xA5 -> Move to WAIT_CMD
+- Any other byte -> Stay in WAIT_START

State: WAIT_CMD
+- Receive any byte -> Store CMD, move to WAIT_LEN_LO

State: WAIT_LEN_LO
+- Receive byte -> Store low byte of length, move to WAIT_LEN_HI

State: WAIT_LEN_HI
+- Length > 1024 -> RESET (invalid packet)
+- Length == 0 -> Parse packet (no payload), RESET
+- Length valid -> Move to WAIT_PAYLOAD

State: WAIT_PAYLOAD
+- Received all payload bytes -> Parse packet, RESET
+- More bytes needed -> Continue receiving
```

### Synchronization Example

```
Received bytes: [0x12, 0x34, 0xA5, 0x01, 0x00, 0x00]

Step 1: Receive 0x12 -> State: WAIT_START (discard)
Step 2: Receive 0x34 -> State: WAIT_START (discard)
Step 3: Receive 0xA5 -> State: WAIT_CMD (start of packet!)
Step 4: Receive 0x01 -> State: WAIT_LEN_LO (CMD = 0x01 = PING)
Step 5: Receive 0x00 -> State: WAIT_LEN_HI (len_lo = 0)
Step 6: Receive 0x00 -> Packet complete! (len_hi = 0, no payload)
```

---

## 5. Buffer Management and Flow Control

### Buffer Architecture

```
UART RX -> Hardware FIFO (128 bytes) -> Driver Buffer (2048 bytes) -> Application
                                                                         v
                                                                    State Machine
                                                                         v
                                                                    Packet Parser
```

### Buffer Configuration

```c
star_uart_config_t uart_config = {
    .port = UART_NUM_1,
    .baud_rate = 115200,
    .rx_buffer_size = 2048,  // RX buffer
    .tx_buffer_size = 2048,  // TX buffer
    .rx_thresh = 120,        // RX FIFO threshold (bytes)
    .tx_thresh = 10,         // TX FIFO threshold (bytes)
};
```

### Checking Buffer Space

```c
// Check how many bytes are available to read
size_t available;
esp_err_t ret = star_bus_uart_get_available(manager, "uart_bus", &available);
if (ret == ESP_OK) {
    printf("Available bytes: %zu\n", available);
}
```

### Clearing Buffers

```c
// Clear RX buffer (discard all received data)
esp_err_t ret = star_bus_uart_clear_rx(manager, "uart_bus");

// Flush TX buffer (wait for all data to be sent)
esp_err_t ret = star_bus_uart_flush(manager, "uart_bus");
```

### Software Flow Control

Since hardware flow control (RTS/CTS) is not used, the protocol implements software flow control:

1. **Receive Window**: Application should read from UART buffer faster than data arrives
2. **Back Pressure**: If ESP32 buffer is filling up, stop sending requests from Raspberry Pi
3. **Buffer Monitoring**: Check available space before sending large payloads

```python
# Python example: Check if ESP32 is ready
def wait_for_esp32_ready(ser, timeout=1.0):
    """Wait for ESP32 to process previous command"""
    start = time.time()
    while time.time() - start < timeout:
        if ser.in_waiting == 0:  # No data waiting means ESP32 processed it
            return True
        time.sleep(0.01)
    return False
```

---

## 6. Handling Partial Reads

### Problem

UART reads may not return a complete packet in one call. For example:

```
Read 1: [0xA5, 0x01]           # Start + CMD
Read 2: [0x05, 0x00]           # Len_Lo + Len_Hi
Read 3: [0x48, 0x65, 0x6C]     # Payload (partial)
Read 4: [0x6C, 0x6F]           # Payload (rest)
```

### ESP32 Solution (Byte-by-Byte State Machine)

The ESP32 firmware handles partial reads automatically using the state machine:

```c
// UART receive task (from pynq_wifi_transport.c)
while (1) {
    uint8_t data[256];
    size_t received = 0;

    // Read with timeout
    esp_err_t ret = star_bus_uart_read(
        g_bus_manager, g_bus_name,
        data, sizeof(data),
        &received, 100  // 100ms timeout
    );

    if (ret == ESP_OK && received > 0) {
        // Process each byte through state machine
        for (size_t i = 0; i < received; i++) {
            uart_rx_process_byte(data[i]);
        }
    }
}
```

### Python/Host Solution (Accumulate Until Complete)

```python
class UARTPacketReader:
    def __init__(self, ser):
        self.ser = ser
        self.buffer = bytearray()

    def read_packet(self, timeout=2.0):
        """Read a complete packet, handling partial reads"""
        start_time = time.time()

        while time.time() - start_time < timeout:
            # Read available data
            if self.ser.in_waiting > 0:
                self.buffer.extend(self.ser.read(self.ser.in_waiting))

            # Try to parse a complete packet
            packet = self._try_parse_packet()
            if packet:
                return packet

            time.sleep(0.01)

        raise TimeoutError("Packet read timeout")

    def _try_parse_packet(self):
        """Try to parse a complete packet from buffer"""
        # Need at least 4 bytes for header
        if len(self.buffer) < 4:
            return None

        # Find start marker
        try:
            start_idx = self.buffer.index(0xA5)
        except ValueError:
            # No start marker, clear buffer
            self.buffer.clear()
            return None

        # Remove bytes before start marker
        if start_idx > 0:
            self.buffer = self.buffer[start_idx:]

        # Check if we have full header
        if len(self.buffer) < 4:
            return None

        # Parse header
        cmd = self.buffer[1]
        payload_len = self.buffer[2] | (self.buffer[3] << 8)
        total_len = 4 + payload_len

        # Check if we have complete packet
        if len(self.buffer) < total_len:
            return None  # Still waiting for payload

        # Extract complete packet
        packet_data = bytes(self.buffer[:total_len])
        self.buffer = self.buffer[total_len:]  # Remove from buffer

        return {
            'cmd': cmd,
            'payload_len': payload_len,
            'payload': packet_data[4:] if payload_len > 0 else b''
        }

# Usage
reader = UARTPacketReader(ser)
packet = reader.read_packet(timeout=2.0)
print(f"Received command: 0x{packet['cmd']:02X}")
```

---

## 7. Handling Data Corruption

### Detection Strategies

#### 1. Start Marker Validation

The 0xA5 start marker provides synchronization. If corrupted data is received, the state machine will discard bytes until it finds a valid start marker.

```c
case k_rx_state_wait_start:
    if (byte == PROTOCOL_START_MARKER) {
        // Found start marker
        g_uart_rx_state = k_rx_state_wait_cmd;
    }
    // Any other byte is discarded
    break;
```

#### 2. Length Validation

Payload length must be <= 1024 bytes:

```c
case k_rx_state_wait_len_hi:
    g_uart_rx_payload_len |= ((uint16_t)byte << 8);

    if (g_uart_rx_payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "Payload too large: %d bytes, resetting",
                 g_uart_rx_payload_len);
        uart_rx_reset();  // Discard packet
    }
    break;
```

#### 3. Command ID Validation

Check if received command ID is valid:

```c
bool is_valid_command(uint8_t cmd) {
    return (cmd == 0x01) ||  // PING
           (cmd == 0x02) ||  // RESET
           (cmd == 0x03) ||  // GET_VERSION
           // ... etc
           (cmd == 0xF0) ||  // RESPONSE
           (cmd == 0xFF);    // ERROR
}
```

### Error Recovery

#### Automatic Reset

If an invalid packet is detected, the state machine automatically resets:

```c
static void uart_rx_reset(void) {
    g_uart_rx_state = k_rx_state_wait_start;
    g_uart_rx_index = 0;
    g_uart_rx_payload_len = 0;
}
```

#### Retry Logic

The transport layer includes automatic retry with exponential backoff:

```c
// From pynq_wifi_transport.c
do {
    ret = star_bus_uart_write(g_bus_manager, g_bus_name, data, len);

    if (ret == ESP_OK) {
        error_handler_reset_state(&g_error_handler);
        return len;  // Success
    }

    // Determine if error is transient
    bool is_transient = (ret == ESP_ERR_TIMEOUT ||
                        ret == ESP_FAIL ||
                        ret == ESP_ERR_INVALID_STATE);

    if (!is_transient) {
        return -1;  // Permanent error
    }

    // Record error and retry
    RECORD_ERROR(&g_error_handler, ret, "UART write");

    if (error_handler_can_retry(&g_error_handler)) {
        vTaskDelay(pdMS_TO_TICKS(g_error_handler.current_retry_delay));
    }
} while (error_handler_can_retry(&g_error_handler));
```

### Python Error Handling Example

```python
def send_packet_with_retry(ser, packet, max_retries=3, timeout=2.0):
    """Send packet with automatic retry on failure"""
    for attempt in range(max_retries):
        try:
            # Clear buffers
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            # Send packet
            ser.write(packet)

            # Wait for response
            start = time.time()
            response = bytearray()

            while time.time() - start < timeout:
                if ser.in_waiting > 0:
                    response.extend(ser.read(ser.in_waiting))

                # Check if we have complete response
                if len(response) >= 4:
                    # Validate response
                    if response[0] == 0xA5:  # Valid start marker
                        return response

                time.sleep(0.01)

            # Timeout - retry
            print(f"Attempt {attempt + 1} timeout, retrying...")
            time.sleep(0.1 * (2 ** attempt))  # Exponential backoff

        except serial.SerialException as e:
            print(f"Serial error on attempt {attempt + 1}: {e}")
            time.sleep(0.1 * (2 ** attempt))

    raise RuntimeError("Failed to send packet after retries")
```

---

## 8. Python PySerial Configuration

### Installation

```bash
pip install pyserial
```

### Basic Configuration

```python
import serial
import time

# Open serial port
ser = serial.Serial(
    port='/dev/ttyUSB0',      # Linux
    # port='COM3',             # Windows
    # port='/dev/tty.usbserial-1234',  # macOS
    baudrate=115200,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=2,                 # Read timeout (seconds)
    write_timeout=2,           # Write timeout (seconds)
    xonxoff=False,             # No software flow control
    rtscts=False,              # No hardware flow control
    dsrdtr=False               # No hardware flow control
)

# Wait for port to open
time.sleep(0.5)

# Clear buffers
ser.reset_input_buffer()
ser.reset_output_buffer()

print(f"Connected to {ser.port} at {ser.baudrate} baud")
```

### Creating Packets

```python
def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)

    # Build header
    packet = bytes([
        0xA5,                    # Start marker
        cmd,                     # Command
        payload_len & 0xFF,      # Low byte of length
        (payload_len >> 8) & 0xFF  # High byte of length
    ])

    # Append payload
    packet += payload

    return packet

# Example: PING command
ping_packet = create_packet(0x01, b'Hello ESP32')
ser.write(ping_packet)
```

### Parsing Responses

```python
def parse_packet(data):
    """Parse a protocol packet"""
    if len(data) < 4:
        return None

    if data[0] != 0xA5:
        return None

    cmd = data[1]
    payload_len = data[2] | (data[3] << 8)

    if len(data) < 4 + payload_len:
        return None

    payload = data[4:4+payload_len]

    return {
        'cmd': cmd,
        'payload_len': payload_len,
        'payload': payload
    }

# Example: Read response
time.sleep(0.1)  # Wait for ESP32 to process
response = ser.read(256)
packet = parse_packet(response)

if packet:
    print(f"Command: 0x{packet['cmd']:02X}")
    print(f"Payload: {packet['payload']}")
```

### Complete Example

```python
#!/usr/bin/env python3
import serial
import sys
import time

# Constants
START_MARKER = 0xA5
CMD_PING = 0x01
CMD_GET_VERSION = 0x03
CMD_RESPONSE = 0xF0

def create_packet(cmd, payload=b''):
    """Create protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        START_MARKER,
        cmd,
        payload_len & 0xFF,
        (payload_len >> 8) & 0xFF
    ])
    return packet + payload

def parse_packet(data):
    """Parse protocol packet"""
    if len(data) < 4 or data[0] != START_MARKER:
        return None

    cmd = data[1]
    payload_len = data[2] | (data[3] << 8)

    if len(data) < 4 + payload_len:
        return None

    return {
        'cmd': cmd,
        'payload_len': payload_len,
        'payload': data[4:4+payload_len]
    }

def main():
    # Open serial port
    try:
        ser = serial.Serial(
            port='/dev/ttyUSB0',
            baudrate=115200,
            timeout=2
        )
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print(f"Connected to {ser.port}")

        # Test 1: PING
        print("\n=== PING Test ===")
        ping_packet = create_packet(CMD_PING, b'Hello')
        ser.write(ping_packet)
        time.sleep(0.1)

        response = ser.read(256)
        packet = parse_packet(response)
        if packet:
            print(f"Response: {packet['payload']}")

        # Test 2: GET_VERSION
        print("\n=== GET_VERSION Test ===")
        version_packet = create_packet(CMD_GET_VERSION)
        ser.reset_input_buffer()
        ser.write(version_packet)
        time.sleep(0.1)

        response = ser.read(256)
        packet = parse_packet(response)
        if packet and len(packet['payload']) >= 4:
            status = packet['payload'][0]
            major = packet['payload'][1]
            minor = packet['payload'][2]
            patch = packet['payload'][3]
            print(f"Firmware: {major}.{minor}.{patch}")

        ser.close()

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

---

## 9. Best Practices for Reliable Communication

### 1. Always Clear Buffers Before Important Operations

```python
# Clear any stale data
ser.reset_input_buffer()
ser.reset_output_buffer()

# Now send command
ser.write(packet)
```

### 2. Use Appropriate Timeouts

```python
# Short timeout for quick operations
ser.timeout = 0.5
response = ser.read(256)

# Longer timeout for network operations (WiFi scan, HTTP)
ser.timeout = 5.0
response = ser.read(1024)
```

### 3. Implement Retry Logic

```python
def reliable_send(ser, packet, max_retries=3):
    for attempt in range(max_retries):
        try:
            ser.write(packet)
            response = ser.read(256)
            if len(response) >= 4 and response[0] == 0xA5:
                return response
        except Exception as e:
            print(f"Attempt {attempt + 1} failed: {e}")

        time.sleep(0.1 * (2 ** attempt))  # Exponential backoff

    raise RuntimeError("Failed after retries")
```

### 4. Validate All Received Data

```python
def validate_packet(data):
    """Validate packet integrity"""
    if len(data) < 4:
        return False, "Too short"

    if data[0] != 0xA5:
        return False, "Invalid start marker"

    payload_len = data[2] | (data[3] << 8)
    if payload_len > 1024:
        return False, "Payload too large"

    if len(data) < 4 + payload_len:
        return False, "Incomplete packet"

    return True, "OK"

# Usage
is_valid, reason = validate_packet(response)
if not is_valid:
    print(f"Invalid packet: {reason}")
```

### 5. Add Inter-Command Delays

```python
# Send command
ser.write(ping_packet)
time.sleep(0.05)  # 50ms delay

# Send next command
ser.write(version_packet)
time.sleep(0.05)
```

### 6. Monitor Buffer Fill Level

```python
def check_buffer_health(ser):
    """Check if buffers are filling up"""
    if ser.in_waiting > 1024:
        print("WARNING: RX buffer is filling up!")
        # Read and discard old data
        ser.read(ser.in_waiting)
```

### 7. Use Context Managers

```python
with serial.Serial('/dev/ttyUSB0', 115200, timeout=2) as ser:
    time.sleep(0.5)
    ser.reset_input_buffer()

    # Do communication
    ser.write(packet)
    response = ser.read(256)

# Port automatically closed
```

### 8. Log Communication for Debugging

```python
import logging

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

def send_packet(ser, packet):
    logger.debug(f"TX: {packet.hex()}")
    ser.write(packet)

    response = ser.read(256)
    logger.debug(f"RX: {response.hex()}")
    return response
```

---

## 10. Troubleshooting Common UART Issues

### Issue 1: Permission Denied

**Symptom:**
```
serial.serialutil.SerialException: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

**Solution:**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Or temporarily
sudo chmod 666 /dev/ttyUSB0

# Log out and log back in
```

### Issue 2: Device Not Found

**Symptom:**
```
serial.serialutil.SerialException: [Errno 2] No such file or directory: '/dev/ttyUSB0'
```

**Solution:**
```bash
# Check if device exists
ls /dev/ttyUSB*

# Check kernel logs
dmesg | tail

# Check if driver is loaded
lsmod | grep usbserial
```

### Issue 3: Wrong Baud Rate

**Symptom:**
- Garbled data
- Random bytes received
- Communication works sometimes but not reliably

**Solution:**
```python
# Ensure both sides use same baud rate
ser = serial.Serial('/dev/ttyUSB0', baudrate=115200)  # Must match ESP32

# Try different baud rates to diagnose
for baud in [9600, 115200, 230400]:
    ser.baudrate = baud
    ser.write(test_packet)
    # Check response
```

### Issue 4: No Response from ESP32

**Symptom:**
- Packet sent successfully
- No response received
- `ser.read()` returns empty bytes

**Solution:**

**Check 1: Is ESP32 powered on?**
```bash
# Look for ESP32 boot messages on console UART
screen /dev/ttyUSB1 115200  # If you have console access
```

**Check 2: Are you using correct UART?**
```
ESP32 has 3 UARTs:
- UART0: Usually used for console/programming (GPIO1/GPIO3)
- UART1: Your application UART (GPIO17/GPIO16)
- UART2: Available for use

Make sure your wiring matches the GPIO pins configured in software!
```

**Check 3: Increase timeout**
```python
ser.timeout = 5.0  # Longer timeout
response = ser.read(256)
```

**Check 4: Check if ESP32 received the packet**
```python
# Add logging to see what was sent
print(f"Sent: {packet.hex()}")

# Wait longer
time.sleep(0.5)

# Check how many bytes are waiting
print(f"Waiting bytes: {ser.in_waiting}")
response = ser.read(ser.in_waiting)
print(f"Received: {response.hex()}")
```

### Issue 5: Data Corruption

**Symptom:**
- Start marker not found (0xA5)
- Length field has unrealistic values
- Payload contains garbage

**Solution:**

**Check 1: Cable quality**
- Use short, shielded cables (< 30cm recommended)
- Check for loose connections
- Avoid running UART cables parallel to power cables

**Check 2: Ground connection**
- Verify GND is connected between ESP32 and host
- Check ground continuity with multimeter

**Check 3: Voltage levels**
- ESP32 uses 3.3V logic
- Ensure USB-UART adapter is set to 3.3V (not 5V)
- Some adapters have a jumper to select voltage

**Check 4: Reduce baud rate**
```python
# Try lower baud rate for testing
ser.baudrate = 9600  # More reliable over long/noisy cables
```

**Check 5: Add delays**
```python
# Give ESP32 more time to process
ser.write(packet)
time.sleep(0.2)  # Increased delay
response = ser.read(256)
```

### Issue 6: Buffer Overflow

**Symptom:**
```
E (12345) star_uart: UART_BUFFER_FULL event
E (12346) star_uart: UART_FIFO_OVF event
```

**Solution:**

**Increase buffer size on ESP32:**
```c
star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
config.rx_buffer_size = 4096;  // Increase from 2048
config.tx_buffer_size = 4096;
```

**Read more frequently from Python:**
```python
# Poll frequently
while True:
    if ser.in_waiting > 0:
        data = ser.read(ser.in_waiting)
        process_data(data)
    time.sleep(0.01)  # 10ms polling
```

### Issue 7: Intermittent Communication

**Symptom:**
- Sometimes works, sometimes doesn't
- Works after power cycle
- Works for a while then stops

**Solution:**

**Check 1: Electromagnetic interference (EMI)**
- Move cables away from power supplies, motors, WiFi antennas
- Use shielded cables
- Add ferrite beads on UART cables

**Check 2: Power supply issues**
- Check if ESP32 power supply is stable
- Measure voltage with multimeter (should be 3.3V +/- 0.1V)
- Try different USB port or power supply

**Check 3: Thermal issues**
- Check if ESP32 is overheating
- Ensure adequate ventilation
- Add heatsink if needed

**Check 4: Reset state machine**
```python
# Add reset capability
def reset_uart(ser):
    """Reset UART communication"""
    ser.close()
    time.sleep(0.5)
    ser.open()
    time.sleep(0.5)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

# Use when communication fails
try:
    response = send_command(ser, packet)
except TimeoutError:
    print("Timeout, resetting UART...")
    reset_uart(ser)
    response = send_command(ser, packet)  # Retry
```

### Issue 8: Wrong Device

**Symptom:**
- No error opening port
- No response
- Random garbage received

**Solution:**

**Verify correct device:**
```bash
# Disconnect USB-UART adapter
ls /dev/ttyUSB* > before.txt

# Reconnect adapter
ls /dev/ttyUSB* > after.txt

# Compare
diff before.txt after.txt
# The new device is your adapter
```

**Check device type:**
```bash
# Get detailed info
udevadm info /dev/ttyUSB0 | grep -E "ID_VENDOR|ID_MODEL|ID_SERIAL"
```

---

## 11. Performance Considerations

### Theoretical Limits

At 115200 baud with 8N1 configuration:
- Each byte requires 10 bits (1 start, 8 data, 1 stop)
- Theoretical throughput: 115200 / 10 = 11520 bytes/sec
- Practical throughput: ~10 KB/sec (accounting for overhead)

### Protocol Overhead

Each packet has 4 bytes of header:
```
Overhead = 4 bytes / (4 + payload_len)

For 100-byte payload: 4 / 104 = 3.8% overhead
For 1024-byte payload: 4 / 1028 = 0.4% overhead
```

**Recommendation**: Use larger payloads when possible to reduce overhead.

### Latency

Round-trip latency (send command, receive response):
```
Latency = TX_time + Processing_time + RX_time

Example for 10-byte command + 20-byte response:
- TX time: 10 bytes * 10 bits / 115200 = 0.87 ms
- Processing: ~1-5 ms (depends on command)
- RX time: 20 bytes * 10 bits / 115200 = 1.74 ms
- Total: ~3-7 ms typical
```

### Optimizations

**1. Batch Commands**
```python
# Bad: Send commands one at a time with delays
for i in range(10):
    ser.write(cmd)
    time.sleep(0.1)  # 100ms delay each
# Total: 1000ms

# Good: Send multiple commands, read responses in batch
for cmd in commands:
    ser.write(cmd)
time.sleep(0.1)  # Single 100ms delay
for _ in commands:
    response = ser.read(256)
# Total: ~200ms
```

**2. Use Largest Packet Size**
```python
# Bad: Send data in small chunks
for chunk in chunks_of_10_bytes:
    send_packet(ser, chunk)

# Good: Send in larger chunks (up to 1024 bytes)
for chunk in chunks_of_1024_bytes:
    send_packet(ser, chunk)
```

**3. Increase Baud Rate (if reliable)**
```c
// ESP32 configuration
config.baud_rate = 230400;  // 2x faster than 115200
// or
config.baud_rate = 460800;  // 4x faster
// or
config.baud_rate = 921600;  // 8x faster (test thoroughly!)
```

```python
# Python configuration
ser = serial.Serial('/dev/ttyUSB0', baudrate=230400)
```

**Note**: Higher baud rates are more sensitive to cable quality and length. Test thoroughly!

**4. Avoid Polling, Use Blocking Reads**
```python
# Bad: Polling wastes CPU
while True:
    if ser.in_waiting > 0:
        data = ser.read(ser.in_waiting)
    time.sleep(0.001)  # CPU intensive

# Good: Blocking read with timeout
ser.timeout = 0.1
data = ser.read(256)  # Blocks until data or timeout
```

### Throughput Testing

```python
#!/usr/bin/env python3
import serial
import time

def throughput_test(ser, test_duration=10):
    """Measure UART throughput"""
    start = time.time()
    bytes_sent = 0
    bytes_received = 0

    # Create large payload (1024 bytes)
    payload = bytes(range(256)) * 4
    packet = create_packet(0x01, payload)

    while time.time() - start < test_duration:
        # Send
        ser.write(packet)
        bytes_sent += len(packet)

        # Receive
        response = ser.read(len(packet))
        bytes_received += len(response)

    elapsed = time.time() - start

    print(f"Duration: {elapsed:.2f} seconds")
    print(f"Bytes sent: {bytes_sent} ({bytes_sent/elapsed:.0f} bytes/sec)")
    print(f"Bytes received: {bytes_received} ({bytes_received/elapsed:.0f} bytes/sec)")

# Run test
with serial.Serial('/dev/ttyUSB0', 115200, timeout=1) as ser:
    time.sleep(0.5)
    ser.reset_input_buffer()
    throughput_test(ser, test_duration=10)
```

---

## References

- **ESP32 UART Driver**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html
- **PySerial Documentation**: https://pyserial.readthedocs.io/
- **Protocol Overview**: See `01-overview.md`
- **System Commands**: See `02-system-commands.md`
- **Implementation Files**:
  - `/components/star_bus/star_bus_uart.c`
  - `/components/star_bus/include/star_bus_uart.h`
  - `/components/star_wifi_bridge/pynq_wifi_transport.c`
  - `/components/star_wifi_bridge/include/pynq_wifi_transport.h`
- **Example Scripts**:
  - `/debugging_scripts/test_uart.py`
  - `/components/star_wifi_bridge/examples/uart_transport_example.c`

---

## Appendix A: Quick Reference

### ESP32 Functions

```c
// Initialize UART
esp_err_t star_bus_uart_init(star_bus_manager_t *manager,
                             const char *bus_name,
                             const star_uart_config_t *config);

// Write data
esp_err_t star_bus_uart_write(star_bus_manager_t *manager,
                              const char *bus_name,
                              const uint8_t *data,
                              size_t length);

// Read data
esp_err_t star_bus_uart_read(star_bus_manager_t *manager,
                             const char *bus_name,
                             uint8_t *data,
                             size_t buffer_size,
                             size_t *received,
                             uint32_t timeout_ms);

// Get available bytes
esp_err_t star_bus_uart_get_available(const star_bus_manager_t *manager,
                                      const char *bus_name,
                                      size_t *available);

// Clear RX buffer
esp_err_t star_bus_uart_clear_rx(star_bus_manager_t *manager,
                                 const char *bus_name);

// Flush TX buffer
esp_err_t star_bus_uart_flush(star_bus_manager_t *manager,
                              const char *bus_name);
```

### Python PySerial Functions

```python
import serial

# Open port
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=2)

# Read/Write
ser.write(data)
data = ser.read(size)

# Buffer management
ser.reset_input_buffer()
ser.reset_output_buffer()
bytes_waiting = ser.in_waiting

# Close
ser.close()
```

### Common Baud Rates

| Baud Rate | Bytes/sec | Use Case |
|-----------|-----------|----------|
| 9600 | ~960 | Very reliable, slow |
| 19200 | ~1920 | Reliable, slow |
| 38400 | ~3840 | Reliable |
| 57600 | ~5760 | Good balance |
| **115200** | **~11520** | **Default, recommended** |
| 230400 | ~23040 | Fast, test cable quality |
| 460800 | ~46080 | Very fast, short cables only |
| 921600 | ~92160 | Maximum, problematic |

### Error Codes

| ESP32 Error | Description | Recovery |
|-------------|-------------|----------|
| `ESP_OK` | Success | None needed |
| `ESP_ERR_INVALID_ARG` | Invalid parameters | Check arguments |
| `ESP_ERR_TIMEOUT` | Operation timeout | Retry |
| `ESP_FAIL` | General failure | Reset and retry |
| `ESP_ERR_NOT_FOUND` | Bus not initialized | Call init function |
| `ESP_ERR_NO_MEM` | Out of memory | Reduce buffer sizes |
| `ESP_ERR_INVALID_STATE` | Invalid state | Reset state machine |

---

**End of UART Transport Layer Documentation**
