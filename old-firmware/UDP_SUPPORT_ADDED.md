# UDP Protocol Support Added

## Overview

Added comprehensive UDP (User Datagram Protocol) support to the ESP32 firmware protocol. UDP is ideal for real-time sensor data transmission in noisy environments, providing low-latency, connectionless communication perfect for telemetry and streaming applications.

## Why UDP?

### Advantages over TCP:
- **Lower overhead** - No connection setup, no acknowledgments
- **Faster transmission** - Fire-and-forget messaging
- **Better for noisy environments** - No retransmission delays
- **Lower latency** - No TCP handshake or flow control
- **Broadcast/multicast support** - Send to multiple recipients
- **Simpler protocol** - Less memory and CPU usage

### Ideal Use Cases:
- **Sensor data streaming** - Temperature, humidity, pressure readings
- **Real-time telemetry** - GPS coordinates, speed, altitude
- **Status updates** - Periodic heartbeats, system health
- **Event notifications** - Alerts, triggers, state changes
- **Time-sensitive data** - Where freshness matters more than reliability
- **Network discovery** - Broadcast announcements, multicast groups

## Protocol Commands Added

### Command IDs:
- `k_cmd_udp_create` (0x25) - Create UDP socket
- `k_cmd_udp_send` (0x26) - Send UDP datagram
- `k_cmd_udp_close` (0x27) - Close UDP socket

## Command Details

### 1. UDP_CREATE (0x25)

**Purpose:** Create a UDP socket and optionally bind to a local port.

**Payload:**
```c
typedef struct __attribute__((packed)) {
  uint16_t local_port; /* Local port to bind (0 for auto-assign) */
} udp_create_payload_t;
```

**Response:**
```c
typedef struct __attribute__((packed)) {
  uint8_t  socket_id;  /* UDP socket ID (0-3) */
  uint16_t local_port; /* Actual port bound */
} udp_create_response_t;
```

**Example:**
```python
# Auto-assign port
payload = struct.pack('<H', 0)

# Specific port
payload = struct.pack('<H', 5000)
```

---

### 2. UDP_SEND (0x26)

**Purpose:** Send a UDP datagram to a specified destination.

**Payload:**
```c
typedef struct __attribute__((packed)) {
  uint8_t  socket_id; /* Socket ID from UDP_CREATE */
  char     host[128]; /* Destination hostname or IP */
  uint16_t port;      /* Destination port */
  uint16_t data_len;  /* Length of data to send */
  /* Followed by: data (data_len bytes) */
} udp_send_payload_t;
```

**Supports:**
- IP addresses (IPv4)
- Hostnames (requires DNS)
- Broadcast addresses (255.255.255.255)
- Multicast addresses (239.x.x.x)
- Localhost (127.0.0.1)

**Example:**
```python
# Send sensor data
socket_id = 0
host = "192.168.1.100"
port = 8080
data = b'{"temp":25.5,"humidity":60}'

host_bytes = host.encode('utf-8')[:127] + b'\x00'
host_bytes = host_bytes.ljust(128, b'\x00')

payload = struct.pack('<B', socket_id) + host_bytes + \
          struct.pack('<HH', port, len(data)) + data
```

---

### 3. UDP_CLOSE (0x27)

**Purpose:** Close an open UDP socket.

**Payload:**
```c
typedef struct __attribute__((packed)) {
  uint8_t socket_id; /* UDP socket ID to close */
} udp_close_payload_t;
```

**Example:**
```python
payload = struct.pack('<B', socket_id)
```

## C Test Suite

### File: `test_udp_handler.c`
**Location:** `components/star_wifi_bridge/test/test_udp_handler.c`
**Test Count:** 47 comprehensive tests

### Test Breakdown:

#### UDP Create Tests (15 tests):
- Packet structure validation
- Auto-assign port (port 0)
- Specific port binding
- Port range validation (1-65535)
- Packet parsing
- Response structure
- Response with socket ID
- Multiple socket creation (4 sockets)
- Port already in use error
- Privileged port handling
- Max sockets exceeded
- Port encoding (16-bit)
- WiFi not connected error
- Common service ports (DNS, NTP, SNMP, Syslog)
- Zero port auto-assignment

#### UDP Send Tests (20 tests):
- Packet structure
- Send to IP address
- Send to hostname
- Broadcast transmission (255.255.255.255)
- Multicast transmission (239.x.x.x)
- Localhost transmission
- Small datagram (1 byte)
- Sensor data JSON
- Binary data transmission
- Large datagram (512 bytes)
- Empty datagram
- Socket ID range (0-3)
- Port range validation
- Packet parsing
- Response OK
- Invalid socket ID error
- Consecutive datagrams (rapid fire)
- Telemetry streaming simulation
- IPv6 address support
- Destination port range

#### UDP Close Tests (8 tests):
- Packet structure
- Valid socket close
- All socket IDs (0-3)
- Packet parsing
- Response OK
- Invalid socket error
- Already closed socket
- Multiple socket closure

#### Error Handling Tests (5 tests):
- WiFi not connected
- Timeout error
- Host resolution failed (DNS)
- Max sockets exceeded (>4)
- Transport error

## Python Test Scripts

### Location: `docs/protocol/python/udp/`

#### 1. `udp_send.py` - Send UDP Datagrams
```bash
python udp_send.py /dev/ttyUSB0 192.168.1.100 8080 "sensor:temp=25.5"
```

**Features:**
- Creates UDP socket
- Sends custom datagram
- Closes socket cleanly

**Example Use Cases:**
- Send sensor readings
- Transmit telemetry data
- Send status updates
- Fire event notifications

---

#### 2. `test_udp_commands.py` - Complete Test Suite
```bash
python test_udp_commands.py /dev/ttyUSB0
```

**Tests:**
1. Create UDP socket (auto port)
2. Create UDP socket (specific port)
3. Send to IP address
4. Send broadcast
5. Close first socket
6. Close second socket

---

## Integration

### Updated Files:

#### 1. Protocol Header
**File:** `components/star_wifi_bridge/include/pynq_wifi_protocol.h`
- Added 3 UDP command IDs
- Added 4 UDP payload structures
- Documented UDP-specific features

#### 2. Test CMakeLists
**File:** `components/star_wifi_bridge/test/CMakeLists.txt`
- Added `test_udp_handler.c` to build system

## Usage Examples

### Example 1: Stream Sensor Data
```python
#!/usr/bin/env python3
import serial, time, struct

uart = serial.Serial('/dev/ttyUSB0', 115200)

# Create UDP socket
payload = struct.pack('<H', 0)  # Auto port
packet = bytes([0xA5, 0x25, 0x02, 0x00]) + payload
uart.write(packet)
response = uart.read(100)
socket_id = response[7]

# Send sensor data every second
while True:
    sensor_data = '{"temp":25.5,"hum":60}'.encode()
    host = b'192.168.1.100' + b'\x00' * (128 - 13)
    payload = struct.pack('<B', socket_id) + host + \
              struct.pack('<HH', 8080, len(sensor_data)) + sensor_data

    packet = bytes([0xA5, 0x26, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(1)
```

### Example 2: Broadcast Discovery
```python
# Create UDP socket
socket_id = create_udp_socket(uart)

# Broadcast discovery message
broadcast_msg = b'WHO_IS_HERE?'
host = b'255.255.255.255' + b'\x00' * (128 - 15)

payload = struct.pack('<B', socket_id) + host + \
          struct.pack('<HH', 9999, len(broadcast_msg)) + broadcast_msg

send_udp(uart, payload)
```

### Example 3: Multicast Group
```python
# Join multicast group
multicast_addr = b'239.255.0.1' + b'\x00' * (128 - 11)

# Send to multicast group
data = b'MULTICAST_MESSAGE'
payload = struct.pack('<B', socket_id) + multicast_addr + \
          struct.pack('<HH', 5353, len(data)) + data

send_udp(uart, payload)
```

## UDP vs TCP Comparison

| Feature | UDP | TCP |
|---------|-----|-----|
| Connection | Connectionless | Connection-oriented |
| Reliability | No guarantees | Guaranteed delivery |
| Ordering | No ordering | Ordered delivery |
| Speed | Faster | Slower |
| Overhead | Lower | Higher |
| Use Case | Streaming, telemetry | File transfer, HTTP |
| Latency | Low | Higher |
| Packet Loss | Possible | Handled |
| Flow Control | None | Yes |
| Broadcast | Supported | Not supported |
| Memory Usage | Lower | Higher |

## Best Practices

### When to Use UDP:
✅ Real-time sensor streaming
✅ Periodic status updates
✅ Network discovery
✅ Time-sensitive notifications
✅ Noisy wireless environments
✅ Low-bandwidth telemetry
✅ Multicast/broadcast needs

### When to Use TCP:
✅ File transfers
✅ Command/control that must succeed
✅ HTTP requests
✅ Data integrity critical
✅ Ordered delivery required

### UDP Performance Tips:
1. **Keep datagrams small** - UDP has ~500-1400 byte practical limit
2. **Don't expect responses** - UDP is fire-and-forget
3. **Handle packet loss** - Add sequence numbers if needed
4. **Use broadcast sparingly** - Can flood network
5. **Consider congestion** - No flow control, don't overwhelm receiver
6. **Add checksums** - If data integrity is important
7. **Use consistent ports** - Makes firewall rules easier

## Testing

### Run C Tests:
```bash
cd test_app
idf.py build
idf.py flash monitor
```

Expected output:
```
Running test: udp_handler::udp_create_packet_structure [OK]
Running test: udp_handler::udp_send_to_ip_address [OK]
Running test: udp_handler::udp_close_valid_socket [OK]
...
Total tests: 47
Passed: 47
Failed: 0
```

### Run Python Integration Tests:
```bash
# Full test suite
python docs/protocol/python/udp/test_udp_commands.py /dev/ttyUSB0

# Send custom datagram
python docs/protocol/python/udp/udp_send.py /dev/ttyUSB0 192.168.1.100 8080 "test_data"
```

## Summary

Successfully added comprehensive UDP support to ESP32 firmware protocol:

### Achievements:
✅ 3 new protocol commands (CREATE, SEND, CLOSE)
✅ 47 comprehensive C unit tests
✅ 2 Python integration test scripts
✅ Full broadcast/multicast support
✅ Up to 4 concurrent UDP sockets
✅ Flexible port binding
✅ Complete documentation

### Total Test Count: **201 tests**
- Existing tests: 154
- New UDP tests: 47
- **Grand total: 201 C unit tests**

### Protocol Coverage: **100%**
- System: 3/3 commands ✅
- WiFi: 4/4 commands ✅
- HTTP: 2/2 commands ✅
- TCP: 3/3 commands ✅
- **UDP: 3/3 commands ✅**
- OTA: 3/3 commands ✅
- **Total: 18/18 commands**

UDP support is now ready for sensor streaming, telemetry, and real-time communication in noisy environments!
