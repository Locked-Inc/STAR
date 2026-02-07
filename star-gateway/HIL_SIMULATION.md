# Hardware-in-the-Loop (HIL) Simulation Guide

## Overview

The STAR Gateway supports Hardware-in-the-Loop (HIL) simulation, allowing you to run the complete ROS2 stack on a development laptop without physical hardware. This is achieved through dependency injection at the transport layer, where a Unix Domain Socket replaces the SPI bus.

## Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│  Development Environment (Your Laptop)                      │
│                                                              │
│  ┌─────────────┐          ┌──────────────────────────────┐ │
│  │   ROS2      │          │  Gateway (Go)                │ │
│  │   Jazzy     │◄────────►│  - gRPC Services             │ │
│  │             │  gRPC    │  - HARQ + FEC                │ │
│  │  - Nav2     │          │  - Frame Encoding            │ │
│  │  - SLAM     │          │                              │ │
│  │  - Planning │          │  Transport Layer:            │ │
│  └─────────────┘          │  ┌─────────────────────────┐ │ │
│                           │  │ SocketTransport         │ │ │
│                           │  │ (Device interface)      │ │ │
│                           │  └───────────┬─────────────┘ │ │
│                           └──────────────┼───────────────┘ │
│                                          │                 │
│                                          │ Unix Socket     │
│                                          │ /tmp/star_rx72n │
│                           ┌──────────────┼───────────────┐ │
│                           │  Virtual RX72N Simulator    │ │
│                           │  - Echoes frames            │ │
│                           │  - Modifies first byte      │ │
│                           │  - Simulates motor control  │ │
│                           └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

**vs. Production (on Robot):**

```text
┌─────────────────────────────────────────────────────────────┐
│  Raspberry Pi 5 (Production)                                │
│                                                              │
│  ┌─────────────┐          ┌──────────────────────────────┐ │
│  │   ROS2      │          │  Gateway (Go)                │ │
│  │   Jazzy     │◄────────►│  - gRPC Services             │ │
│  │             │  gRPC    │  - HARQ + FEC                │ │
│  └─────────────┘          │  - Frame Encoding            │ │
│                           │                              │ │
│                           │  Transport Layer:            │ │
│                           │  ┌─────────────────────────┐ │ │
│                           │  │ SPITransport            │ │ │
│                           │  │ (Device interface)      │ │ │
│                           │  └───────────┬─────────────┘ │ │
│                           └──────────────┼───────────────┘ │
│                                          │                 │
│                                          │ SPI 10 MHz      │
│                                          │ /dev/spidev0.0  │
└──────────────────────────────────────────┼──────────────────┘
                                           │
                                  ┌────────▼────────┐
                                  │ RX72N Firmware  │
                                  │ - Motor Control │
                                  │ - Encoders      │
                                  │ - Current Sense │
                                  └─────────────────┘
```

## Quick Start

### Terminal 1: Start Virtual RX72N

```bash
cd star-gateway
go run ./cmd/virtual_rx72n/
```

**Expected Output:**

```text
Virtual RX72N Started. Waiting for Gateway...
   Socket: /tmp/star_rx72n.sock
   Max Frame Size: 2048 bytes
```

### Terminal 2: Start Gateway in Simulation Mode

```bash
cd star-gateway
export STAR_SIMULATION_MODE=true
go run ./cmd/star-gateway/
```

**Expected Output:**

```text
WARNING: RUNNING IN SIMULATION MODE (Virtual RX72N)
    Connecting to socket: /tmp/star_rx72n.sock
Initializing frame encoder/decoder...
Initializing FEC encoder/decoder...
Initializing HARQ handler...
...
gRPC server listening on :50051
HTTP/WebSocket server starting on :8080
```

### Terminal 3: Start ROS2 Nodes

```bash
cd star-ros2
source install/setup.bash
ros2 run star_gateway_bridge star_gateway_bridge_node
```

## Environment Variables

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `STAR_SIMULATION_MODE` | `true` / `false` | `false` | Enables HIL simulation mode |
| `SIM_LATENCY_MS` | `0` - `1000` | `10` | Simulated processing latency (ms) for control frames |

When `STAR_SIMULATION_MODE=true`:
- Gateway uses **SocketTransport** (Unix socket)
- Connects to Virtual RX72N at `/tmp/star_rx72n.sock`
- No SPI hardware required

When `STAR_SIMULATION_MODE=false` (or unset):
- Gateway uses **SPITransport** (real hardware)
- Requires `/dev/spidev0.0` device
- Runs on Raspberry Pi 5 only

## Testing

### Unit Tests (No Hardware Required)

```bash
cd star-gateway
go test -short ./internal/transport/...
```

This runs all unit tests without requiring hardware or the Virtual RX72N.

### Integration Tests (Requires Virtual RX72N)

```bash
# Terminal 1: Start Virtual RX72N
go run ./cmd/virtual_rx72n/

# Terminal 2: Run integration tests
go test ./internal/transport/ -run TestSocketTransport_WithVirtualRX72N -v
```

**Expected Test Output:**

```text
=== RUN   TestSocketTransport_WithVirtualRX72N
--- PASS: TestSocketTransport_WithVirtualRX72N (0.00s)
PASS
```

The test verifies:
1. Connection to Virtual RX72N succeeds
2. Data is sent and received
3. First byte is modified to `0xFF` (simulator marker)
4. Remaining bytes are echoed correctly

### Manual Testing with grpcurl

```bash
# Terminal 1: Virtual RX72N
go run ./cmd/virtual_rx72n/

# Terminal 2: Gateway
export STAR_SIMULATION_MODE=true
go run ./cmd/star-gateway/

# Terminal 3: Send test command
grpcurl -plaintext -d '{"header": {"request_id": "test-001"}, "linear_velocity_mps": 1.0, "angular_velocity_rad_per_sec": 0.5}' \
  localhost:50051 star.v1.MotorControlService/SetVelocity
```

## Virtual RX72N Behavior

The Virtual RX72N simulator:

1. **Listens** on `/tmp/star_rx72n.sock`
2. **Accepts** connections from the Gateway
3. **Decodes** incoming frames (SYNC, header, CRC validation)
4. **Dispatches** control frames (PING/PONG, RESET/RESET_ACK) before protobuf processing
5. **Processes** command frames (protobuf unmarshal, response generation)
6. **Tracks** session state (TX sequence counter with reset support)
7. **Logs** all activity to stdout

### Example Log Output

```text
Virtual RX72N Started. Waiting for Gateway...
   Socket: /tmp/star_rx72n.sock
   Max Frame Size: 2048 bytes
   Simulated Latency: 10ms
Gateway connected from @
Received frame: seq=0, type=PING, flags=0, payload_len=4
PING received (counter=0), sending PONG
Sending frame: seq=0, type=PONG, payload_len=4
Received frame: seq=1, type=COMMAND, flags=1, payload_len=42
Sending frame: seq=1, type=ACK, payload_len=0
VelocityCommand: FL=1.00, FR=2.00, BL=3.00, BR=4.00 m/s
Sending frame: seq=1, type=RESPONSE, payload_len=128
Gateway disconnected
```

## Control Frame Support

The Virtual RX72N handles control frames at the transport layer, before any protobuf processing:

### PING/PONG Heartbeat

- **Receives:** `FrameTypePing` (0x00) with 4-byte big-endian counter payload
- **Responds:** `FrameTypePong` (0x01) echoing the same 4-byte counter
- Simulated latency (`SIM_LATENCY_MS`) is applied before responding

### RESET/RESET_ACK Session Reset

- **Receives:** `FrameTypeReset` (0xFF) with empty payload
- **Responds:** `FrameTypeResetAck` (0xFE) with empty payload
- Resets internal sequence counter to 0 after sending RESET_ACK
- Simulated latency (`SIM_LATENCY_MS`) is applied before responding

### Configurable Latency

Set `SIM_LATENCY_MS` to control response timing:

| Value | Use Case |
|-------|----------|
| `0` | Fast unit tests (no delay) |
| `10` | Default (simulates USB buffer processing) |
| `50+` | Stress testing timeout handling |

```bash
# Fast tests
SIM_LATENCY_MS=0 go run ./cmd/virtual_rx72n/

# Stress test heartbeat timeouts
SIM_LATENCY_MS=100 go run ./cmd/virtual_rx72n/
```

## Constants Reference

### Transport Layer

| Constant | Value | File | Description |
|----------|-------|------|-------------|
| `DefaultSocketPath` | `/tmp/star_rx72n.sock` | `transport/socket.go` | Unix socket path |
| `DefaultSocketTimeout` | 100 ms | `transport/socket.go` | I/O timeout |
| `SimulatorMarker` | `0xFF` | `cmd/virtual_rx72n/main.go` | First byte marker |
| `MaxFrameSize` | 2048 | `cmd/virtual_rx72n/main.go` | Max frame size |
| `PingPayloadSize` | 4 | `cmd/virtual_rx72n/main.go` | PING counter size (bytes) |
| `DefaultSimLatencyMs` | 10 | `cmd/virtual_rx72n/main.go` | Default processing latency |

### Gateway Main

| Constant | Value | File | Description |
|----------|-------|------|-------------|
| `grpcListenPort` | `:50051` | `cmd/star-gateway/main.go` | gRPC port |
| `httpListenPort` | `:8080` | `cmd/star-gateway/main.go` | HTTP/WebSocket port |
| `httpShutdownTimeout` | 5 s | `cmd/star-gateway/main.go` | Shutdown timeout |
| `grpcMaxMsgSize` | 10 MB | `cmd/star-gateway/main.go` | Max message size |

## Design Principles

This implementation follows strict coding standards:

### SOLID Principles

- **Interface Segregation**: `Device` interface with minimal methods (`Open`, `Transfer`, `Close`)
- **Dependency Inversion**: Gateway depends on `Device` interface, not concrete types

### Zero Magic Numbers

All constants are named:

```go
// ❌ BAD
conn.SetDeadline(time.Now().Add(100 * time.Millisecond))

// ✅ GOOD
conn.SetDeadline(time.Now().Add(DefaultSocketTimeout))
```

### Error Handling

All errors are wrapped:

```go
// ❌ BAD
return nil, err

// ✅ GOOD
return nil, fmt.Errorf("failed to connect to socket %s: %w", s.path, err)
```

### Inclusive Terminology

- **Controller/Peripheral** (not master/slave)
- **COPI/CIPO** (not MOSI/MISO)

## Troubleshooting

### Virtual RX72N Won't Start

**Error:** `bind: address already in use`

**Solution:** Remove stale socket file:
```bash
rm /tmp/star_rx72n.sock
```

### Gateway Can't Connect

**Error:** `failed to connect to socket: no such file or directory`

**Solution:** Ensure Virtual RX72N is running first.

### Tests Timeout

**Error:** `context deadline exceeded`

**Solution:** Check that Virtual RX72N is running and listening:
```bash
ls -l /tmp/star_rx72n.sock
# Should show: srwxrwxr-x ... /tmp/star_rx72n.sock
```

## Future Enhancements

Planned improvements for the Virtual RX72N:

1. **Motor Simulation**: Generate realistic encoder counts
2. **Battery Simulation**: Return voltage/current telemetry
3. **Fault Injection**: Simulate CRC errors, timeouts, NACK responses
4. **Recording/Playback**: Record sessions for regression testing

## References

- **Transport Interface**: `star-gateway/internal/transport/interface.go`
- **SPITransport**: `star-gateway/internal/transport/spi.go`
- **SocketTransport**: `star-gateway/internal/transport/socket.go`
- **Virtual RX72N**: `star-gateway/cmd/virtual_rx72n/main.go`
- **Gateway Main**: `star-gateway/cmd/star-gateway/main.go`
- **Architecture Docs**: `docs/sections/07_gateway_architecture.tex`
- **CLAUDE Guide**: `star-gateway/CLAUDE.md`
