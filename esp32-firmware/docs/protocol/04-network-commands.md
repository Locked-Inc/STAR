# Network and HTTP Commands

Network commands provide HTTP requests and TCP socket functionality for the ESP32. These commands enable the PYNQ board to perform web requests and establish TCP connections through the ESP32's WiFi interface.

## Implementation Status

| Command ID | Name | Status | Notes |
|------------|------|--------|-------|
| 0x20 | CMD_HTTP_GET | NOT IMPLEMENTED | Returns STATUS_ERROR |
| 0x21 | CMD_HTTP_POST | NOT IMPLEMENTED | Returns STATUS_ERROR |
| 0x22 | CMD_TCP_CONNECT | PLANNED | Protocol defined, no handler |
| 0x23 | CMD_TCP_SEND | PLANNED | Protocol defined, no handler |
| 0x24 | CMD_TCP_CLOSE | PLANNED | Protocol defined, no handler |

**IMPORTANT**: As of firmware v0.1.0, HTTP and TCP commands are defined in the protocol specification but are NOT yet implemented in the firmware. The HTTP GET and POST handlers exist but immediately return `STATUS_ERROR (0x01)`. TCP commands have no handler and will return `STATUS_INVALID_CMD (0x02)`.

## Command Overview

### HTTP Commands (0x20-0x21)

These commands enable HTTP GET and POST requests to web servers.

#### CMD_HTTP_GET (0x20)

Perform an HTTP GET request to retrieve data from a URL.

**Status**: NOT IMPLEMENTED - Currently returns STATUS_ERROR

#### CMD_HTTP_POST (0x21)

Perform an HTTP POST request to send data to a URL.

**Status**: NOT IMPLEMENTED - Currently returns STATUS_ERROR

### TCP Commands (0x22-0x24)

These commands enable raw TCP socket connections for custom protocols.

#### CMD_TCP_CONNECT (0x22)

Open a TCP connection to a remote host.

**Status**: PLANNED - No handler implemented

#### CMD_TCP_SEND (0x23)

Send data over an established TCP connection.

**Status**: PLANNED - No handler implemented

#### CMD_TCP_CLOSE (0x24)

Close an established TCP connection.

**Status**: PLANNED - No handler implemented

---

## CMD_HTTP_GET (0x20)

Perform an HTTP GET request to retrieve data from a web server.

### Current Implementation Status

The command handler exists in `/home/bsikar/Documents/git/STAR/esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_handler.c` (lines 174-191) but only logs a warning and returns an error:

```c
static void handle_cmd_http_get(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_GET command");

  if (packet->payload_len < sizeof(http_get_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  http_get_payload_t* payload = (http_get_payload_t*)packet->payload;

  ESP_LOGI(TAG, "HTTP GET request: URL='%s'", payload->url);

  /* TODO: Implement HTTP GET logic */
  ESP_LOGW(TAG, "HTTP functionality not yet implemented");
  protocol_send_error(k_status_error);
}
```

### Request Packet (Planned Specification)

```
+-------+------+--------+--------+------------------+
| Start | CMD  | Len_Lo | Len_Hi | URL (256 bytes)  |
+-------+------+--------+--------+------------------+
| 0xA5  | 0x20 | varies | varies | null-terminated  |
+-------+------+--------+--------+------------------+
```

**Payload Structure**: `http_get_payload_t`

```c
typedef struct __attribute__((packed)) {
  char url[256]; /* URL (null-terminated) */
} http_get_payload_t;
```

**Payload Fields**:
- `url` (256 bytes): Null-terminated URL string (e.g., "http://example.com/data")

### Response Packet (When Implemented)

```
+-------+------+--------+--------+--------+----------+--------------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | HTTP Response|
+-------+------+--------+--------+--------+----------+--------------+
| 0xA5  | 0xF0 | varies | varies | 0x00   | N bytes  | Response body|
+-------+------+--------+--------+--------+----------+--------------+
```

**Status Codes**:
- `0x00` (STATUS_OK): Request successful, response data follows
- `0x01` (STATUS_ERROR): Generic error (currently always returned)
- `0x03` (STATUS_TIMEOUT): Request timed out
- `0x20` (STATUS_HTTP_FAILED): HTTP-specific error

### Python Implementation (For Future Use)

```python
#!/usr/bin/env python3
"""HTTP GET command - NOT YET IMPLEMENTED IN FIRMWARE"""

import serial
import struct
import time

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

def http_get(uart, url, timeout_seconds=10):
    """
    Send HTTP GET request (NOT IMPLEMENTED IN FIRMWARE v0.1.0)

    Args:
        uart: Serial connection
        url: URL to request (max 255 chars)
        timeout_seconds: Timeout for HTTP request

    Returns:
        dict: {'success': bool, 'status': int, 'data': bytes}
              Currently always returns success=False with STATUS_ERROR
    """
    # Validate URL length
    if len(url) >= 256:
        print("ERROR: URL too long (max 255 characters)")
        return {'success': False, 'status': 0x01, 'data': b''}

    # Create payload
    payload = url.encode('utf-8') + b'\x00'  # Null-terminated
    payload = payload.ljust(256, b'\x00')    # Pad to 256 bytes

    # Create and send packet
    packet = create_packet(0x20, payload)
    print(f"Sending HTTP GET: {url}")
    uart.write(packet)

    # Read response
    start_time = time.time()
    buffer = b''

    while time.time() - start_time < timeout_seconds:
        if uart.in_waiting:
            buffer += uart.read(uart.in_waiting)

            # Try to parse response
            if len(buffer) >= 4:
                if buffer[0] == 0xA5 and buffer[1] == 0xF0:
                    payload_len = buffer[2] | (buffer[3] << 8)

                    if len(buffer) >= 4 + payload_len:
                        # Complete packet received
                        status = buffer[4] if payload_len >= 1 else 0x01

                        # NOTE: Current firmware (v0.1.0) returns STATUS_ERROR
                        if status == 0x01:
                            print("WARNING: HTTP GET not implemented in firmware")
                            print("         ESP32 returned STATUS_ERROR (0x01)")
                            return {'success': False, 'status': status, 'data': b''}

                        # Parse data (for future implementation)
                        data_len = 0
                        response_data = b''
                        if payload_len >= 3:
                            data_len = buffer[5] | (buffer[6] << 8)
                            if len(buffer) >= 7 + data_len:
                                response_data = buffer[7:7+data_len]

                        return {
                            'success': (status == 0x00),
                            'status': status,
                            'data': response_data
                        }

        time.sleep(0.01)

    print("ERROR: Timeout waiting for response")
    return {'success': False, 'status': 0x03, 'data': b''}

def detect_not_implemented(response):
    """
    Detect if command is not implemented

    Args:
        response: Response dict from http_get()

    Returns:
        bool: True if command is not implemented
    """
    return response['status'] == 0x01  # STATUS_ERROR

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python http_get.py <serial_port> <url>")
        print("\nExample:")
        print("  python http_get.py /dev/ttyUSB0 http://httpbin.org/get")
        print("\nNOTE: HTTP GET is NOT IMPLEMENTED in firmware v0.1.0")
        print("      This script will demonstrate the error response")
        sys.exit(1)

    port = sys.argv[1]
    url = sys.argv[2]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Try HTTP GET
    response = http_get(uart, url)

    if detect_not_implemented(response):
        print("\n" + "="*60)
        print("EXPECTED BEHAVIOR: Command Not Implemented")
        print("="*60)
        print("The ESP32 firmware (v0.1.0) does not yet implement HTTP GET.")
        print("See firmware handler at:")
        print("  components/pynq_wifi_bridge/pynq_wifi_handler.c:174-191")
        print("\nTo implement HTTP GET, you need to:")
        print("  1. Add esp_http_client component to build")
        print("  2. Implement HTTP client logic in handler")
        print("  3. Return response data instead of error")
        print("="*60)
    elif response['success']:
        print(f"\n[OK] HTTP GET successful!")
        print(f"  Response length: {len(response['data'])} bytes")
        print(f"  Response data: {response['data'][:200]}")  # First 200 bytes
    else:
        print(f"\n[FAIL] HTTP GET failed with status 0x{response['status']:02X}")

    uart.close()

    sys.exit(0 if response['success'] else 1)
```

### Testing the Current Behavior

```bash
# This will demonstrate the "not implemented" error
python http_get.py /dev/ttyUSB0 http://httpbin.org/get

# Expected output:
# Sending HTTP GET: http://httpbin.org/get
# WARNING: HTTP GET not implemented in firmware
#          ESP32 returned STATUS_ERROR (0x01)
#
# ============================================================
# EXPECTED BEHAVIOR: Command Not Implemented
# ============================================================
# The ESP32 firmware (v0.1.0) does not yet implement HTTP GET.
```

---

## CMD_HTTP_POST (0x21)

Perform an HTTP POST request to send data to a web server.

### Current Implementation Status

Similar to HTTP GET, the POST handler exists (lines 197-210) but is not implemented:

```c
static void handle_cmd_http_post(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_POST command");

  if (packet->payload_len < sizeof(http_post_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  /* TODO: Implement HTTP POST logic */
  ESP_LOGW(TAG, "HTTP functionality not yet implemented");
  protocol_send_error(k_status_error);
}
```

### Request Packet (Planned Specification)

```
+-------+------+--------+--------+---------+---------+-----+------+
| Start | CMD  | Len_Lo | Len_Hi | URL_Len | Data_Len| URL | Data |
+-------+------+--------+--------+---------+---------+-----+------+
| 0xA5  | 0x21 | varies | varies | 2 bytes | 2 bytes | ... | ...  |
+-------+------+--------+--------+---------+---------+-----+------+
```

**Payload Structure**: `http_post_payload_t` + URL + Data

```c
typedef struct __attribute__((packed)) {
  uint16_t url_len;  /* Length of URL string */
  uint16_t data_len; /* Length of POST data */
  /* Followed by: url (url_len bytes) + data (data_len bytes) */
} http_post_payload_t;
```

**Payload Format**:
1. Header: 4 bytes (url_len + data_len, both little-endian)
2. URL: `url_len` bytes (not null-terminated)
3. Data: `data_len` bytes (POST body)

**Example**:
- URL: "http://api.example.com/data" (27 bytes)
- Data: '{"temp":23.5,"humidity":65}' (28 bytes)
- Total payload: 4 + 27 + 28 = 59 bytes

### Response Packet (When Implemented)

Same format as HTTP GET response.

### Python Implementation (For Future Use)

```python
#!/usr/bin/env python3
"""HTTP POST command - NOT YET IMPLEMENTED IN FIRMWARE"""

import serial
import struct
import time
import json

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

def http_post(uart, url, data, timeout_seconds=10):
    """
    Send HTTP POST request (NOT IMPLEMENTED IN FIRMWARE v0.1.0)

    Args:
        uart: Serial connection
        url: URL to post to
        data: Data to send (bytes or string)
        timeout_seconds: Timeout for HTTP request

    Returns:
        dict: {'success': bool, 'status': int, 'data': bytes}
              Currently always returns success=False with STATUS_ERROR
    """
    # Convert data to bytes if needed
    if isinstance(data, str):
        data = data.encode('utf-8')

    url_bytes = url.encode('utf-8')

    # Validate lengths
    if len(url_bytes) > 65535:
        print("ERROR: URL too long")
        return {'success': False, 'status': 0x01, 'data': b''}

    if len(data) > 1020 - len(url_bytes) - 4:
        print("ERROR: POST data too large for single packet")
        return {'success': False, 'status': 0x01, 'data': b''}

    # Create payload: url_len (2B) + data_len (2B) + url + data
    payload = struct.pack('<HH', len(url_bytes), len(data))
    payload += url_bytes
    payload += data

    # Create and send packet
    packet = create_packet(0x21, payload)
    print(f"Sending HTTP POST: {url}")
    print(f"  Data size: {len(data)} bytes")
    uart.write(packet)

    # Read response (same logic as HTTP GET)
    start_time = time.time()
    buffer = b''

    while time.time() - start_time < timeout_seconds:
        if uart.in_waiting:
            buffer += uart.read(uart.in_waiting)

            if len(buffer) >= 4:
                if buffer[0] == 0xA5 and buffer[1] == 0xF0:
                    payload_len = buffer[2] | (buffer[3] << 8)

                    if len(buffer) >= 4 + payload_len:
                        status = buffer[4] if payload_len >= 1 else 0x01

                        # NOTE: Current firmware (v0.1.0) returns STATUS_ERROR
                        if status == 0x01:
                            print("WARNING: HTTP POST not implemented in firmware")
                            print("         ESP32 returned STATUS_ERROR (0x01)")
                            return {'success': False, 'status': status, 'data': b''}

                        # Parse response data (for future implementation)
                        data_len = 0
                        response_data = b''
                        if payload_len >= 3:
                            data_len = buffer[5] | (buffer[6] << 8)
                            if len(buffer) >= 7 + data_len:
                                response_data = buffer[7:7+data_len]

                        return {
                            'success': (status == 0x00),
                            'status': status,
                            'data': response_data
                        }

        time.sleep(0.01)

    print("ERROR: Timeout waiting for response")
    return {'success': False, 'status': 0x03, 'data': b''}

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python http_post.py <serial_port> <url> [json_data]")
        print("\nExample:")
        print('  python http_post.py /dev/ttyUSB0 http://httpbin.org/post \'{"key":"value"}\'')
        print("\nNOTE: HTTP POST is NOT IMPLEMENTED in firmware v0.1.0")
        sys.exit(1)

    port = sys.argv[1]
    url = sys.argv[2]

    # Default JSON data if not provided
    data = sys.argv[3] if len(sys.argv) > 3 else '{"test":"data"}'

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Try HTTP POST
    response = http_post(uart, url, data)

    if response['status'] == 0x01:
        print("\n" + "="*60)
        print("EXPECTED BEHAVIOR: Command Not Implemented")
        print("="*60)
        print("The ESP32 firmware (v0.1.0) does not yet implement HTTP POST.")
        print("See firmware handler at:")
        print("  components/pynq_wifi_bridge/pynq_wifi_handler.c:197-210")
        print("="*60)
    elif response['success']:
        print(f"\n[OK] HTTP POST successful!")
        print(f"  Response: {response['data']}")
    else:
        print(f"\n[FAIL] HTTP POST failed with status 0x{response['status']:02X}")

    uart.close()
```

---

## CMD_TCP_CONNECT (0x22)

Open a TCP socket connection to a remote host.

### Current Implementation Status

**NOT IMPLEMENTED** - No handler exists. Sending this command will return `STATUS_INVALID_CMD (0x02)`.

### Request Packet (Planned Specification)

```
+-------+------+--------+--------+----------------+------+
| Start | CMD  | Len_Lo | Len_Hi | Host (64 bytes)| Port |
+-------+------+--------+--------+----------------+------+
| 0xA5  | 0x22 | 0x42   | 0x00   | null-terminated| 2B   |
+-------+------+--------+--------+----------------+------+
```

**Payload** (planned):
- Host: 64-byte null-terminated hostname or IP address
- Port: 2-byte port number (little-endian)
- Total: 66 bytes

### Response Packet (When Implemented)

```
+-------+------+--------+--------+--------+----------+-----------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Socket ID |
+-------+------+--------+--------+--------+----------+-----------+
| 0xA5  | 0xF0 | 0x03   | 0x00   | 0x00   | 0x01,0x00| 1 byte    |
+-------+------+--------+--------+--------+----------+-----------+
```

**Data**: 1-byte socket ID (0-255) for subsequent operations

### Python Implementation (Future)

```python
#!/usr/bin/env python3
"""TCP Connect command - PLANNED, NOT YET IMPLEMENTED"""

import serial
import struct
import time

def tcp_connect(uart, host, port, timeout_seconds=5):
    """
    Open TCP connection (PLANNED - NOT IMPLEMENTED)

    Args:
        uart: Serial connection
        host: Hostname or IP address
        port: Port number (1-65535)
        timeout_seconds: Connection timeout

    Returns:
        dict: {'success': bool, 'socket_id': int or None}
              Currently returns STATUS_INVALID_CMD
    """
    # Validate inputs
    if len(host) >= 64:
        print("ERROR: Host too long (max 63 characters)")
        return {'success': False, 'socket_id': None}

    if port < 1 or port > 65535:
        print("ERROR: Invalid port number")
        return {'success': False, 'socket_id': None}

    # Create payload
    host_bytes = host.encode('utf-8') + b'\x00'
    host_bytes = host_bytes.ljust(64, b'\x00')  # Pad to 64 bytes
    port_bytes = struct.pack('<H', port)        # Little-endian uint16

    payload = host_bytes + port_bytes

    # Create packet
    packet_len = len(payload)
    packet = bytes([
        0xA5,  # Start marker
        0x22,  # CMD_TCP_CONNECT
        packet_len & 0xFF,
        (packet_len >> 8) & 0xFF
    ]) + payload

    print(f"Sending TCP_CONNECT: {host}:{port}")
    uart.write(packet)

    # Wait for response
    start_time = time.time()
    buffer = b''

    while time.time() - start_time < timeout_seconds:
        if uart.in_waiting:
            buffer += uart.read(uart.in_waiting)

            if len(buffer) >= 5:
                if buffer[0] == 0xA5 and buffer[1] == 0xF0:
                    status = buffer[4]

                    if status == 0x02:  # STATUS_INVALID_CMD
                        print("WARNING: TCP_CONNECT not implemented in firmware")
                        print("         No handler exists for this command")
                        return {'success': False, 'socket_id': None}

                    if status == 0x00:  # STATUS_OK
                        socket_id = buffer[7] if len(buffer) > 7 else None
                        return {'success': True, 'socket_id': socket_id}

                    return {'success': False, 'socket_id': None}

        time.sleep(0.01)

    print("ERROR: Timeout")
    return {'success': False, 'socket_id': None}

# Example usage (will fail with current firmware)
if __name__ == "__main__":
    import sys

    print("WARNING: TCP commands are not implemented in firmware v0.1.0")
    print("This script demonstrates the planned API only.\n")

    if len(sys.argv) < 4:
        print("Usage: python tcp_connect.py <serial_port> <host> <port>")
        sys.exit(1)

    port = sys.argv[1]
    host = sys.argv[2]
    tcp_port = int(sys.argv[3])

    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    result = tcp_connect(uart, host, tcp_port)

    uart.close()
```

---

## CMD_TCP_SEND (0x23)

Send data over an established TCP connection.

### Current Implementation Status

**NOT IMPLEMENTED** - No handler exists. Returns `STATUS_INVALID_CMD (0x02)`.

### Request Packet (Planned Specification)

```
+-------+------+--------+--------+-----------+-------------+
| Start | CMD  | Len_Lo | Len_Hi | Socket ID | Data        |
+-------+------+--------+--------+-----------+-------------+
| 0xA5  | 0x23 | varies | varies | 1 byte    | N bytes     |
+-------+------+--------+--------+-----------+-------------+
```

**Payload** (planned):
- Socket ID: 1 byte (from TCP_CONNECT)
- Data: Variable length data to send

---

## CMD_TCP_CLOSE (0x24)

Close an established TCP connection.

### Current Implementation Status

**NOT IMPLEMENTED** - No handler exists. Returns `STATUS_INVALID_CMD (0x02)`.

### Request Packet (Planned Specification)

```
+-------+------+--------+--------+-----------+
| Start | CMD  | Len_Lo | Len_Hi | Socket ID |
+-------+------+--------+--------+-----------+
| 0xA5  | 0x24 | 0x01   | 0x00   | 1 byte    |
+-------+------+--------+--------+-----------+
```

**Payload** (planned):
- Socket ID: 1 byte (from TCP_CONNECT)

---

## Implementation Guide

### To Implement HTTP GET/POST

The HTTP commands require the ESP-IDF HTTP client component. Here's what needs to be added:

#### 1. Update CMakeLists.txt

Add the HTTP client component dependency:

```cmake
# components/pynq_wifi_bridge/CMakeLists.txt
idf_component_register(
    SRCS "pynq_wifi_handler.c" "pynq_wifi_manager.c" "pynq_wifi_protocol.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif nvs_flash esp_http_client  # Add esp_http_client
)
```

#### 2. Implement HTTP GET Handler

Replace the TODO in `pynq_wifi_handler.c`:

```c
#include "esp_http_client.h"

static void handle_cmd_http_get(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_GET command");

  if (packet->payload_len < sizeof(http_get_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  http_get_payload_t* payload = (http_get_payload_t*)packet->payload;
  ESP_LOGI(TAG, "HTTP GET request: URL='%s'", payload->url);

  // Check WiFi connection
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    protocol_send_error(k_status_wifi_failed);
    return;
  }

  // Allocate buffer for response
  char* response_buffer = malloc(PROTOCOL_MAX_PAYLOAD_SIZE);
  if (!response_buffer) {
    ESP_LOGE(TAG, "Failed to allocate response buffer");
    protocol_send_error(k_status_error);
    return;
  }

  int response_len = 0;

  // Configure HTTP client
  esp_http_client_config_t config = {
    .url = payload->url,
    .timeout_ms = 10000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    ESP_LOGE(TAG, "Failed to init HTTP client");
    free(response_buffer);
    protocol_send_error(k_status_http_failed);
    return;
  }

  // Perform HTTP GET
  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    free(response_buffer);
    protocol_send_error(k_status_http_failed);
    return;
  }

  // Read response
  int content_length = esp_http_client_fetch_headers(client);
  int read_len = esp_http_client_read_response(client, response_buffer,
                                                PROTOCOL_MAX_PAYLOAD_SIZE);

  if (read_len >= 0) {
    response_len = read_len;
    ESP_LOGI(TAG, "HTTP GET success: %d bytes", response_len);
    protocol_send_response(k_status_ok, (uint8_t*)response_buffer, response_len);
  } else {
    ESP_LOGE(TAG, "Failed to read HTTP response");
    protocol_send_error(k_status_http_failed);
  }

  // Cleanup
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  free(response_buffer);
}
```

#### 3. Implement HTTP POST Handler

Similar implementation for POST, using `esp_http_client_set_post_field()`.

### To Implement TCP Commands

TCP commands require socket programming with lwIP:

#### 1. Add Socket Management

Create a socket manager to track multiple connections:

```c
#define MAX_SOCKETS 4

typedef struct {
  int socket_fd;
  bool in_use;
  char host[64];
  uint16_t port;
} tcp_socket_t;

static tcp_socket_t g_sockets[MAX_SOCKETS] = {0};
```

#### 2. Implement TCP_CONNECT Handler

```c
#include <sys/socket.h>
#include <netdb.h>

static void handle_cmd_tcp_connect(protocol_packet_t* packet)
{
  // Parse host and port from payload
  // Allocate socket ID
  // Create socket and connect
  // Return socket ID on success
}
```

#### 3. Add Handler to Switch Statement

```c
// In command_handler_process()
case k_cmd_tcp_connect:
  handle_cmd_tcp_connect(packet);
  break;
```

---

## Testing Not-Yet-Implemented Commands

### Detect Implementation Status

```python
#!/usr/bin/env python3
"""Test implementation status of network commands"""

import serial
import time

def test_command_implemented(uart, cmd_id, cmd_name, payload=b''):
    """Test if a command is implemented"""
    # Create packet
    payload_len = len(payload)
    packet = bytes([0xA5, cmd_id, payload_len & 0xFF, (payload_len >> 8) & 0xFF])
    packet += payload

    print(f"\nTesting {cmd_name} (0x{cmd_id:02X})...")
    uart.reset_input_buffer()
    uart.write(packet)

    # Wait for response
    time.sleep(0.5)
    response = uart.read(uart.in_waiting)

    if len(response) < 5:
        print(f"  Status: NO RESPONSE")
        return "no_response"

    if response[0] == 0xA5 and response[1] == 0xF0:
        status = response[4]

        if status == 0x00:
            print(f"  Status: IMPLEMENTED (returned STATUS_OK)")
            return "implemented"
        elif status == 0x01:
            print(f"  Status: STUB EXISTS (returns STATUS_ERROR)")
            return "stub"
        elif status == 0x02:
            print(f"  Status: NOT IMPLEMENTED (returned STATUS_INVALID_CMD)")
            return "not_implemented"
        else:
            print(f"  Status: UNKNOWN (status=0x{status:02X})")
            return "unknown"

    print(f"  Status: INVALID RESPONSE")
    return "invalid"

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python test_network_commands.py <serial_port>")
        sys.exit(1)

    port = sys.argv[1]
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    print("="*60)
    print("Network Commands Implementation Status")
    print("="*60)

    # Test HTTP GET (has stub handler)
    test_payload = b'http://example.com\x00' + b'\x00' * 237
    test_command_implemented(uart, 0x20, "CMD_HTTP_GET", test_payload)

    # Test HTTP POST (has stub handler)
    test_payload = b'\x10\x00\x04\x00' + b'http://test.com' + b'data'
    test_command_implemented(uart, 0x21, "CMD_HTTP_POST", test_payload)

    # Test TCP CONNECT (no handler)
    test_payload = b'localhost\x00' + b'\x00' * 54 + b'\x50\x00'
    test_command_implemented(uart, 0x22, "CMD_TCP_CONNECT", test_payload)

    # Test TCP SEND (no handler)
    test_payload = b'\x00test'
    test_command_implemented(uart, 0x23, "CMD_TCP_SEND", test_payload)

    # Test TCP CLOSE (no handler)
    test_payload = b'\x00'
    test_command_implemented(uart, 0x24, "CMD_TCP_CLOSE", test_payload)

    print("\n" + "="*60)
    uart.close()
```

### Expected Output (Firmware v0.1.0)

```
============================================================
Network Commands Implementation Status
============================================================

Testing CMD_HTTP_GET (0x20)...
  Status: STUB EXISTS (returns STATUS_ERROR)

Testing CMD_HTTP_POST (0x21)...
  Status: STUB EXISTS (returns STATUS_ERROR)

Testing CMD_TCP_CONNECT (0x22)...
  Status: NOT IMPLEMENTED (returned STATUS_INVALID_CMD)

Testing CMD_TCP_SEND (0x23)...
  Status: NOT IMPLEMENTED (returned STATUS_INVALID_CMD)

Testing CMD_TCP_CLOSE (0x24)...
  Status: NOT IMPLEMENTED (returned STATUS_INVALID_CMD)

============================================================
```

---

## Error Handling

### Status Codes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | STATUS_OK | Success (not yet returned) |
| 0x01 | STATUS_ERROR | Generic error (HTTP stubs return this) |
| 0x02 | STATUS_INVALID_CMD | Command not implemented (TCP returns this) |
| 0x03 | STATUS_TIMEOUT | Request timeout (future) |
| 0x10 | STATUS_WIFI_FAILED | WiFi not connected (future) |
| 0x20 | STATUS_HTTP_FAILED | HTTP-specific error (future) |

### Common Errors (Future)

| Error | Cause | Solution |
|-------|-------|----------|
| STATUS_WIFI_FAILED | WiFi not connected | Connect to WiFi first with CMD_WIFI_CONNECT |
| STATUS_TIMEOUT | Request took too long | Increase timeout or check network |
| STATUS_HTTP_FAILED | Invalid URL or server error | Check URL format and server status |
| STATUS_ERROR | Generic failure | Check ESP32 logs for details |

---

## Future Enhancements

### Planned Features

1. **HTTP Headers Support**: Allow custom headers in requests
2. **HTTPS Support**: Secure connections with certificate validation
3. **Chunked Transfer**: Support for large responses
4. **WebSocket Support**: Real-time bidirectional communication
5. **UDP Sockets**: Datagram support for low-latency applications

### Protocol Extensions

```c
// Example: HTTP GET with custom headers
typedef struct __attribute__((packed)) {
  char url[256];
  uint8_t num_headers;
  /* Followed by headers: "Key: Value\r\n" strings */
} http_get_extended_payload_t;
```

---

## See Also

- [Protocol Overview](01-overview.md)
- [System Commands](02-system-commands.md)
- [WiFi Commands](03-wifi-commands.md)
- [OTA Commands](05-ota-commands.md)
- [ESP-IDF HTTP Client Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html)
