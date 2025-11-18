# PYNQ-ESP32 Communication Protocol Overview

## Introduction

The PYNQ-ESP32 communication protocol is a binary protocol designed for efficient command and data exchange between the Raspberry Pi and ESP32 via UART. The protocol supports system commands, WiFi management, network operations, and OTA firmware updates.

## Protocol Characteristics

- **Transport**: UART (default 115200 baud, 8N1)
- **Format**: Binary packets with fixed 4-byte header + variable payload
- **Byte Order**: Little-endian for multi-byte values
- **Maximum Payload**: 1024 bytes
- **Start Marker**: 0xA5 for packet synchronization

## Packet Structure

```
+-------+--------+--------+--------+-----------+
| Start |  CMD   | Len_Lo | Len_Hi |  Payload  |
+-------+--------+--------+--------+-----------+
| 0xA5  | 1 byte | 2 bytes (LE)    | 0-1024 B  |
+-------+--------+--------+--------+-----------+
```

### Header Fields

| Field | Size | Description |
|-------|------|-------------|
| Start | 1 byte | Always 0xA5 - used for packet synchronization |
| CMD | 1 byte | Command ID (see command categories below) |
| Len_Lo | 1 byte | Low byte of payload length |
| Len_Hi | 1 byte | High byte of payload length |
| Payload | Variable | Command-specific data (0-1024 bytes) |

**Note**: Payload length is encoded as little-endian uint16:
```
length = Len_Lo | (Len_Hi << 8)
```

## Command Categories

Commands are organized into logical ranges:

### System Commands (0x01-0x0F)
- **0x01** - CMD_PING: Test connectivity
- **0x02** - CMD_RESET: Reset ESP32
- **0x03** - CMD_GET_VERSION: Get firmware version

### WiFi Commands (0x10-0x1F)
- **0x10** - CMD_WIFI_CONNECT: Connect to WiFi network
- **0x11** - CMD_WIFI_DISCONNECT: Disconnect from WiFi
- **0x12** - CMD_WIFI_STATUS: Get WiFi connection status
- **0x13** - CMD_WIFI_SCAN: Scan for WiFi networks

### Network/Data Commands (0x20-0x27)
- **0x20** - CMD_HTTP_GET: HTTP GET request
- **0x21** - CMD_HTTP_POST: HTTP POST request
- **0x22** - CMD_TCP_CONNECT: Open TCP connection
- **0x23** - CMD_TCP_SEND: Send TCP data
- **0x24** - CMD_TCP_CLOSE: Close TCP connection

### OTA Update Commands (0x28-0x2F)
- **0x28** - CMD_OTA_CHECK_UPDATE: Check if update available
- **0x29** - CMD_OTA_START_UPDATE: Start OTA update process
- **0x2A** - CMD_OTA_GET_STATUS: Get OTA update status

### Response/Status (0xF0-0xFF)
- **0xF0** - CMD_RESPONSE: Generic response
- **0xFF** - CMD_ERROR: Error response

## Response Packet Structure

All commands receive a response with command ID 0xF0 (CMD_RESPONSE):

```
+-------+--------+--------+--------+--------+----------+--------+
| Start |  CMD   | Len_Lo | Len_Hi | Status | Data_Len | Data   |
+-------+--------+--------+--------+--------+----------+--------+
| 0xA5  |  0xF0  | 2 bytes (LE)    | 1 byte | 2 bytes  | N bytes|
+-------+--------+--------+--------+--------+----------+--------+
```

### Response Status Codes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | STATUS_OK | Success |
| 0x01 | STATUS_ERROR | Generic error |
| 0x02 | STATUS_INVALID_CMD | Invalid command |
| 0x03 | STATUS_TIMEOUT | Operation timeout |
| 0x10 | STATUS_WIFI_FAILED | WiFi operation failed |
| 0x20 | STATUS_HTTP_FAILED | HTTP operation failed |
| 0x30 | STATUS_OTA_FAILED | OTA update failed |
| 0x31 | STATUS_OTA_PROGRESS | OTA update in progress |

## Communication Flow

### Basic Command-Response

```
PYNQ                                  ESP32
  |                                     |
  |  [0xA5][CMD][LEN][PAYLOAD]         |
  |------------------------------------>|
  |                                     |
  |      Process command                |
  |                                     |
  |  [0xA5][0xF0][LEN][STATUS][DATA]   |
  |<------------------------------------|
  |                                     |
```

### Asynchronous Operations

Some commands (OTA, WiFi connect) start long-running operations:

```
PYNQ                                  ESP32
  |                                     |
  | CMD_OTA_START_UPDATE                |
  |------------------------------------>|
  |                                     |
  | Response: STATUS_OTA_PROGRESS       |
  |<------------------------------------|
  |                                     |
  |      (Update running in background) |
  |                                     |
  | CMD_OTA_GET_STATUS (poll)           |
  |------------------------------------>|
  |                                     |
  | Response: {state, progress, ...}    |
  |<------------------------------------|
  |                                     |
  | ... (repeat until complete) ...     |
  |                                     |
```

## Python Protocol Library

### Basic Packet Creation

```python
import struct

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        0xA5,  # Start marker
        cmd,   # Command ID
        payload_len & 0xFF,          # Len_Lo
        (payload_len >> 8) & 0xFF    # Len_Hi
    ])
    packet += payload
    return packet
```

### Packet Parsing

```python
def parse_packet(data):
    """Parse a protocol packet"""
    if len(data) < 4:
        return None

    if data[0] != 0xA5:  # Check start marker
        return None

    cmd = data[1]
    payload_len = data[2] | (data[3] << 8)  # Little-endian

    if len(data) < 4 + payload_len:
        return None  # Incomplete packet

    payload = data[4:4+payload_len]

    return {
        'cmd': cmd,
        'payload_len': payload_len,
        'payload': payload
    }
```

### Response Parsing

```python
def parse_response(data):
    """Parse a response packet"""
    packet = parse_packet(data)
    if not packet or packet['cmd'] != 0xF0:
        return None

    if len(packet['payload']) < 1:
        return None

    status = packet['payload'][0]
    data_len = 0
    response_data = b''

    if len(packet['payload']) >= 3:
        data_len = packet['payload'][1] | (packet['payload'][2] << 8)
        response_data = packet['payload'][3:3+data_len]

    return {
        'status': status,
        'data': response_data
    }
```

## Error Handling

### Timeout Handling

Commands should timeout if no response is received within a reasonable time:

```python
import serial
import time

def send_command_with_timeout(uart, cmd, payload=b'', timeout_seconds=5):
    """Send command and wait for response with timeout"""
    packet = create_packet(cmd, payload)
    uart.write(packet)

    start_time = time.time()
    buffer = b''

    while time.time() - start_time < timeout_seconds:
        if uart.in_waiting:
            buffer += uart.read(uart.in_waiting)

            # Try to parse response
            response = parse_response(buffer)
            if response:
                return response

        time.sleep(0.01)

    return None  # Timeout
```

### Retry Logic

For unreliable connections, implement retry:

```python
def send_command_with_retry(uart, cmd, payload=b'', max_retries=3):
    """Send command with automatic retry on failure"""
    for attempt in range(max_retries):
        response = send_command_with_timeout(uart, cmd, payload)

        if response and response['status'] == 0x00:  # STATUS_OK
            return response

        print(f"Attempt {attempt+1} failed, retrying...")
        time.sleep(0.5)

    return None  # All retries failed
```

## Best Practices

### 1. Always Check Start Marker

Before parsing any packet, verify the start marker:

```python
if data[0] != 0xA5:
    # Discard and resync
    return None
```

### 2. Validate Payload Length

Ensure the payload length doesn't exceed the maximum:

```python
if payload_len > 1024:
    return None  # Invalid
```

### 3. Handle Partial Packets

UART reads may return partial packets. Buffer data until a complete packet is received:

```python
buffer = b''

while True:
    chunk = uart.read(uart.in_waiting)
    buffer += chunk

    packet = parse_packet(buffer)
    if packet:
        # Process packet
        buffer = buffer[4 + packet['payload_len']:]  # Remove processed packet
```

### 4. Use Timeouts

Always use timeouts to avoid hanging forever:

```python
uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
```

### 5. Clear Buffers Before Critical Commands

```python
uart.reset_input_buffer()
uart.reset_output_buffer()
```

## Protocol Extension

To add new commands:

1. **Choose an unused command ID** in the appropriate range
2. **Define payload structure** using `__attribute__((packed))` in C
3. **Add command handler** in `pynq_wifi_handler.c`
4. **Update protocol header** `pynq_wifi_protocol.h`
5. **Add tests** in `test_protocol.c`
6. **Document the command** in this documentation

### Example: Adding a New Command

```c
// 1. In pynq_wifi_protocol.h
#define CMD_CUSTOM_COMMAND 0x50

typedef struct __attribute__((packed)) {
    uint8_t  param1;
    uint16_t param2;
    char     param3[32];
} custom_command_payload_t;

// 2. In pynq_wifi_handler.c
static void handle_cmd_custom_command(protocol_packet_t* packet) {
    custom_command_payload_t* payload = (custom_command_payload_t*)packet->payload;

    // Process command
    // ...

    protocol_send_response(k_status_ok, result_data, result_len);
}

// 3. Add to switch statement in command_handler_process()
case CMD_CUSTOM_COMMAND:
    handle_cmd_custom_command(packet);
    break;
```

## See Also

- [System Commands](02-system-commands.md)
- [WiFi Commands](03-wifi-commands.md)
- [Network Commands](04-network-commands.md)
- [OTA Commands](05-ota-commands.md)
- [UART Transport Layer](06-uart-transport.md)
