# Gateway Gaps: star-gateway (Go)

## Status Summary

The gateway's transport and infrastructure layers are excellent and production-ready. The service layer has significant gaps — most critically FirmwareUpdateService which is 0% implemented.

| Gap | Severity | File | Line | Effort |
|-----|----------|------|------|--------|
| FirmwareUpdateService (all 10 RPCs) | CRITICAL | firmware.go | 22-50 | 20-30 hrs |
| E-Stop priority framing | HIGH | motor_control.go | 90 | 4 hrs |
| SetMotorPower not implemented | HIGH | motor_control.go | 139 | 2 hrs |
| SetPIDGains ROS2 forwarding | HIGH | gateway_service.go | 205 | 3 hrs |
| SetRetransmitConfig missing | MEDIUM | configuration.go | — | 2 hrs |
| GetSystemStatus stub | LOW | telemetry.go | 261 | 2 hrs |
| USB PID placeholder | LOW | manager/config.go | 84 | 0.5 hrs |
| Frame metadata lost | LOW | manager/reader.go | 73 | 1-2 hrs |

---

## Gap 1: FirmwareUpdateService — 100% UNIMPLEMENTED (CRITICAL)

### Problem

`star-gateway/internal/service/firmware.go` contains this pattern for ALL 10 methods:

```go
func (s *FirmwareService) BeginUpdate(ctx context.Context,
    req *starv1.BeginUpdateRequest) (*starv1.BeginUpdateResponse, error) {
    // TODO: Implement BeginUpdate - Initialize OTA partition, set expected size/CRC.
    return nil, status.Error(codes.Unimplemented, "not implemented")
}
```

Affected methods:
1. `BeginUpdate` - Start OTA session
2. `WriteChunk` - Write single chunk (unary)
3. `StreamChunks` - Bulk chunk streaming (client-streaming RPC)
4. `FinalizeUpdate` - Validate and commit
5. `AbortUpdate` - Cancel and cleanup
6. `GetUpdateProgress` - Query current progress
7. `StreamUpdateProgress` - Stream progress events (server-streaming)
8. `Reboot` - Reboot firmware into new image
9. `Rollback` - Revert to previous firmware
10. `MarkValid` - Mark current firmware as stable post-boot

### Implementation Architecture

```
UI click "Update Firmware"
    │
    ▼ gRPC BeginUpdate
Gateway FirmwareService.BeginUpdate()
    │ Creates OTA session (in-memory)
    │ Sends BeginUpdateRequest WireMessage via HARQ to RX72N
    ▼
RX72N firmware OTA state machine (see plans/01_firmware_gaps.md)
    │ Responds with BeginUpdateResponse
    │
    ▼ gRPC WriteChunk / StreamChunks (client streaming)
Gateway receives chunks from UI
    │ Buffers each chunk
    │ Sends FirmwareChunk WireMessage via HARQ to RX72N
    │ RX72N writes to OTA flash bank
    │
    ▼ gRPC FinalizeUpdate
Gateway FirmwareService.FinalizeUpdate()
    │ Sends FinalizeUpdate WireMessage
    │ RX72N validates CRC-32, prepares for reboot
    │
    ▼ gRPC Reboot
Gateway triggers firmware reboot
RX72N reboots, boots from new image
```

### Session Management Required

```go
// internal/service/firmware.go

type otaSession struct {
    sessionID    string
    totalSize    uint64
    bytesWritten uint64
    expectedCRC  uint32
    runningSHA   hash.Hash32
    startTime    time.Time
    mu           sync.Mutex
}

type FirmwareService struct {
    harq    *harq.Client
    session *otaSession  // Only one session at a time
    mu      sync.RWMutex
}
```

### Wire.proto Addition Needed

A `FirmwareChunk` message type must be added to `wire.proto`'s WireMessage oneof:

```protobuf
// In wire.proto
message FirmwareChunk {
    uint32 session_id = 1;
    uint64 offset = 2;        // Byte offset in image
    bytes  data = 3;          // Chunk data (max 512 bytes for nanopb)
    uint32 chunk_crc = 4;    // CRC-32 of this chunk
}

// Add to WireMessage oneof payload:
FirmwareChunk firmware_chunk = 60;
```

### Estimated Effort: 20-30 hours
This is the most complex remaining feature. Requires:
- OTA session state machine in Go
- Wire.proto extension for FirmwareChunk
- Retry/resume logic for failed chunks
- Progress reporting goroutine
- Integration with RX72N OTA handler (firmware.go)

---

## Gap 2: E-Stop Priority Framing (HIGH)

### Problem

`star-gateway/internal/service/motor_control.go:90`:
```go
// TODO (tracked in GitHub issue): Implement priority framing for E-Stop.
// E-Stop should bypass normal HARQ queue and go immediately.
```

Emergency stop commands currently use `PriorityNormal` in the HARQ layer. Under heavy telemetry load, an E-Stop could be delayed by queued frames.

### Fix

The HARQ client needs to expose a priority parameter:

```go
// In harq package, expose priority:
type Priority int

const (
    PriorityNormal    Priority = 0
    PriorityHigh      Priority = 1
    PriorityEmergency Priority = 2  // Bypass all queuing
)

// In motor_control.go:
func (s *MotorControlService) EmergencyStop(ctx context.Context,
    req *starv1.EmergencyStopRequest) (*starv1.EmergencyStopResponse, error) {

    // Use PriorityEmergency to bypass normal queue
    result, err := s.harq.SendWithPriority(ctx, wireMsg, PriorityEmergency)
    // ...
}
```

### Implementation

1. Add `Priority` type to `internal/harq/harq.go`
2. Add `SendWithPriority()` to HARQ client interface
3. Update `motor_control.go` `EmergencyStop()` to use `PriorityEmergency`
4. Add unit tests: E-Stop bypasses queued normal frames

### Estimated Effort: 4 hours

---

## Gap 3: SetMotorPower Not Implemented (HIGH)

### Problem

`star-gateway/internal/service/motor_control.go:139`:
```go
func (s *MotorControlService) SetMotorPower(ctx context.Context,
    req *starv1.SetMotorPowerRequest) (*starv1.SetMotorPowerResponse, error) {
    // TODO (tracked in issue): Implement specific message for MotorPower
    return nil, status.Error(codes.Unimplemented, "not implemented")
}
```

`SetMotorPower` allows direct PWM bypass (for testing/calibration). Currently blocks raw power control.

### What's Needed

Check if `MotorPowerCommand` is already defined in `wire.proto` `WireMessage` oneof:

```protobuf
// wire.proto — check if this exists already:
message MotorPowerCommand {
    float motor_0_power = 1;   // -1.0 to 1.0 (normalized PWM)
    float motor_1_power = 2;
    float motor_2_power = 3;
    float motor_3_power = 4;
}
```

If not present, add to `wire.proto` and regenerate.

Then implement in `motor_control.go`:

```go
func (s *MotorControlService) SetMotorPower(ctx context.Context,
    req *starv1.SetMotorPowerRequest) (*starv1.SetMotorPowerResponse, error) {

    wireMsg := &starv1wire.WireMessage{
        Payload: &starv1wire.WireMessage_MotorPowerCommand{
            MotorPowerCommand: &starv1wire.MotorPowerCommand{
                Motor_0Power: req.Motor_0Power,
                Motor_1Power: req.Motor_1Power,
                Motor_2Power: req.Motor_2Power,
                Motor_3Power: req.Motor_3Power,
            },
        },
    }

    result, err := s.harq.Send(ctx, wireMsg, harq.PriorityNormal)
    // ... error handling ...

    return &starv1.SetMotorPowerResponse{
        Header: successHeader(req.Header),
    }, nil
}
```

### Estimated Effort: 2 hours

---

## Gap 4: SetPIDGains Not Forwarding to ROS2 (HIGH)

### Problem

`star-gateway/internal/service/gateway_service.go:205`:
```go
// TODO: Forward to ROS2 service when available
// Currently just stores in cache and returns success
func (s *GatewayService) SetPIDGains(ctx context.Context,
    req *starv1.SetPIDGainsRequest) (*starv1.SetPIDGainsResponse, error) {
    // Stores nothing, returns success silently
    return &starv1.SetPIDGainsResponse{}, nil
}
```

PID gain changes from the UI are silently dropped.

### Fix

The Gateway's `SetPIDGains` should:
1. Forward to the ROS2 `star_gateway_bridge` node via the `/set_pid_gains` ROS2 service
2. OR directly send a `PidConfig` WireMessage to RX72N via HARQ (bypassing ROS2)

**Option B (simpler) — direct HARQ dispatch:**

```go
func (s *GatewayService) SetPIDGains(ctx context.Context,
    req *starv1.SetPIDGainsRequest) (*starv1.SetPIDGainsResponse, error) {

    wireMsg := &starv1wire.WireMessage{
        Payload: &starv1wire.WireMessage_PidConfig{
            PidConfig: &starv1wire.PidConfig{
                MotorId: req.MotorId,
                Kp:      req.Kp,
                Ki:      req.Ki,
                Kd:      req.Kd,
            },
        },
    }

    _, err := s.harq.Send(ctx, wireMsg, harq.PriorityNormal)
    if err != nil {
        return nil, status.Errorf(codes.Internal, "failed to send PID config: %v", err)
    }

    return &starv1.SetPIDGainsResponse{
        Header: successHeader(req.Header),
    }, nil
}
```

Also requires updating `star_gateway_bridge_node.cpp` in ROS2 to properly implement the `/set_pid_gains` service (currently uses `std_srvs/SetBool` as placeholder — needs custom service type).

### Estimated Effort: 3 hours (Go side) + 2 hours (ROS2 side)

---

## Gap 5: SetRetransmitConfig Missing (MEDIUM)

### Problem

`configuration.go` implements 7 of the 8 `ConfigurationService` methods but is missing `SetRetransmitConfig` entirely. This method lets operators tune HARQ retry parameters at runtime.

### Implementation

```go
func (s *ConfigurationService) SetRetransmitConfig(ctx context.Context,
    req *starv1.SetRetransmitConfigRequest) (*starv1.SetRetransmitConfigResponse, error) {

    wireMsg := &starv1wire.WireMessage{
        Payload: &starv1wire.WireMessage_RetransmitConfig{
            RetransmitConfig: &starv1wire.RetransmitConfig{
                MaxRetransmits:  req.Config.MaxRetransmits,
                AckTimeoutMs:    req.Config.AckTimeoutMs,
                FecEnabled:      req.Config.FecEnabled,
            },
        },
    }

    _, err := s.harq.Send(ctx, wireMsg, harq.PriorityNormal)
    // ...
}
```

### Estimated Effort: 2 hours

---

## Gap 6: GetSystemStatus Stub (LOW)

### Problem

`telemetry.go:261`:
```go
// TODO: Implement SystemStatusRequest in wire.proto
// Currently returns placeholder SystemStatus
```

`GetSystemStatus` returns a hardcoded `SystemStatus` with mock data (no real connection status, fake firmware version, etc.).

### Fix

1. Add `SystemStatusRequest` to `wire.proto` WireMessage oneof
2. Send request to firmware, parse response
3. Or: Aggregate locally from known transport state + firmware version cache

### Estimated Effort: 2 hours

---

## Complete Gateway Service Status

```
internal/service/
├── motor_control.go
│   ├── SetVelocity         ✅ Working
│   ├── EmergencyStop       ⚠️ Missing priority framing (line 90)
│   ├── SetMotorPower       ❌ Unimplemented (line 139)
│   ├── StreamEncoders      ✅ Working
│   └── ControlStream       ✅ Working
│
├── telemetry.go
│   ├── GetTelemetry        ✅ Working
│   ├── StreamTelemetry     ✅ Working
│   └── GetSystemStatus     ⚠️ Stubbed (line 261)
│
├── battery.go              ✅ All 10 methods complete (80.38% coverage)
│
├── configuration.go
│   ├── GetConfiguration    ✅ Working
│   ├── SetConfiguration    ✅ Working
│   ├── ResetToDefaults     ✅ Working
│   ├── ValidateConfiguration ✅ Working
│   ├── SaveConfiguration   ✅ Working
│   ├── GetMotorPidConfig   ✅ Working
│   ├── SetMotorPidConfig   ✅ Working
│   └── SetRetransmitConfig ❌ MISSING (not in file)
│
├── firmware.go             ❌ All 10 methods are stubs (TODO comments)
│
└── gateway_service.go
    ├── ForwardTelemetry    ✅ Working (ROS2 pushes at 10 Hz)
    ├── GetTeleopCommand    ✅ Working (ROS2 polls at 50 Hz)
    └── SetPIDGains         ⚠️ Silently drops (line 205)
```
