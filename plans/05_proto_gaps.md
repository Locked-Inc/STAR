# Protocol Buffers Gaps: star-proto

## Status Summary

The proto schemas are comprehensive and complete. The code generation pipeline (Go, TypeScript, nanopb C) works correctly. The main gaps are in the **nanopb .options files** which define field size constraints for the embedded firmware.

| Gap | Severity | File | Fix |
|-----|----------|------|-----|
| motor_status max_count:2 should be 4 | HIGH | motor_control.options | Change to max_count:4 |
| CellData cell voltages missing max_count | MEDIUM | battery_management.options | Add max_count:16 |
| CurrentSensorCalibration arrays missing | MEDIUM | configuration.options | Add max_count:4 |
| BatteryState temperatures array missing | LOW | battery_management.options | Add max_count:3 |
| FirmwareChunk missing in wire.proto | HIGH | wire.proto | Add new message type |

---

## Gap 1: motor_status max_count Mismatch (HIGH)

### Problem

`star-proto/nanopb/star/v1/motor_control.options`:
```
SetVelocityResponse.motor_status: max_count:2
```

The comment says "2 motors for differential drive" but:
- The firmware has **4 motors** (front_left, front_right, back_left, back_right)
- The VelocityCommand proto message has fields for all 4 motors
- The RX72N motor_control_task manages 4 independent PID loops

If firmware populates all 4 motor_status entries, nanopb will truncate to 2 when encoding.

### Fix

```diff
# star-proto/nanopb/star/v1/motor_control.options
-SetVelocityResponse.motor_status: max_count:2
+SetVelocityResponse.motor_status: max_count:4
```

Also update the comment to clarify this supports 4 motors.

### Impact

This is a data truncation bug. The RX72N may be sending status for motors 2 and 3 that gets silently dropped.

---

## Gap 2: CellData Voltages Missing (MEDIUM)

### Problem

`star-proto/nanopb/star/v1/battery_management.options` is missing constraints for:

```protobuf
// In battery_management.proto:
message BmsDeviceInfo {
    string chemistry = 3;   // ← No max_size constraint
}

message CellData {
    repeated uint32 cell_mv = 1;  // ← No max_count constraint
}
```

The BQ4050 BMS IC supports up to 16 cells. Without a max_count constraint, nanopb cannot allocate a static array for this field.

### Fix

```
# star-proto/nanopb/star/v1/battery_management.options
BmsDeviceInfo.chemistry: max_size:32

# Add for CellData:
CellData.cell_mv: max_count:16
BatteryState.temperatures.temp_deci_celsius: max_count:3
```

---

## Gap 3: CurrentSensorCalibration Arrays (MEDIUM)

### Problem

`star-proto/nanopb/star/v1/configuration.options`:
```
# Missing:
CurrentSensorCalibration.offset_ma: max_count:4
CurrentSensorCalibration.scale_factor: max_count:4
```

These are repeated `double` fields with 4 entries (one per motor). Without max_count, nanopb cannot allocate them statically.

### Fix

```
# star-proto/nanopb/star/v1/configuration.options (add these lines)
CurrentSensorCalibration.offset_ma: max_count:4
CurrentSensorCalibration.scale_factor: max_count:4
```

---

## Gap 4: FirmwareChunk Missing in wire.proto (HIGH)

### Problem

For OTA firmware updates to work, the `WireMessage` oneof needs a `FirmwareChunk` message type. Currently only these OTA-related types exist in the gateway service proto, not in wire.proto.

### Fix

Add to `star-proto/proto/star/v1/wire.proto`:

```protobuf
// Add to wire.proto imports:
import "star/v1/firmware_update.proto";

// New message type:
message FirmwareChunk {
    uint32 session_id = 1;     // From BeginUpdateRequest
    uint64 offset = 2;         // Byte offset in firmware image
    bytes data = 3;            // Chunk payload (max 512 bytes for embedded)
    uint32 chunk_crc = 4;      // CRC-32 of this chunk only
    bool is_last = 5;          // True for final chunk
}

// Add to WireMessage oneof payload:
message WireMessage {
    oneof payload {
        // ... existing entries ...
        FirmwareChunk firmware_chunk = 60;   // OTA firmware update chunk
        FirmwareBeginRequest begin_update = 61;
        FirmwareBeginResponse begin_response = 62;
        FirmwareFinalizeRequest finalize_update = 63;
        FirmwareFinalizeResponse finalize_response = 64;
    }
}
```

### nanopb options for FirmwareChunk

```
# wire.options (add):
star.v1.FirmwareChunk.data: max_size:512
```

512 bytes per chunk means a 512 KB firmware image requires 1024 chunks. This is acceptable for OTA.

---

## Full nanopb Options Audit

### motor_control.options — Current State

```
# Current (has issues):
star.v1.EmergencyStopRequest.reason: max_size:128
star.v1.SetVelocityResponse.motor_status: max_count:2  ← BUG: should be 4
```

### battery_management.options — Current State

```
# Current (has gaps):
# Missing: BmsDeviceInfo.chemistry max_size
# Missing: CellData.cell_mv max_count
# Missing: BatteryState.temperatures.temp_deci_celsius max_count
```

### configuration.options — Current State

```
# Current (has gaps):
star.v1.ConfigValidationError.field_name: max_size:128
star.v1.ConfigValidationError.message: max_size:256
star.v1.ConfigValidationResult.errors: max_count:8
star.v1.SystemConfiguration.motor_configs: max_count:4
# Missing: CurrentSensorCalibration array fields
```

### common.options — Complete

```
star.v1.RequestHeader.request_id: max_size:64
star.v1.RequestHeader.client_version: max_size:32
star.v1.ResponseHeader.request_id: max_size:64
star.v1.ResponseHeader.error_message: max_size:256
```

### telemetry.options — Complete

```
star.v1.StreamTelemetryRequest.fields: max_count:16
star.v1.StreamTelemetryRequest.fields: max_size:32
star.v1.SystemStatus.firmware_version: max_size:32
```

### diagnostics.options — Complete

```
star.v1.TransportHealthReport.transport_name: max_size:16
star.v1.TransportDiagnostics.transports: max_count:8
star.v1.TransportDiagnostics.active_transport: max_size:16
```

### wire.options — Partial (needs FirmwareChunk)

```
star.v1.EmergencyStopCommand.reason: max_size:128
# Missing: FirmwareChunk.data max_size (after adding to wire.proto)
```

---

## Summary of Required Changes

### Immediate (fix before first integration test)

1. **`motor_control.options`**: `max_count:2` → `max_count:4` for motor_status
2. **`battery_management.options`**: Add `CellData.cell_mv: max_count:16`
3. **`battery_management.options`**: Add `BmsDeviceInfo.chemistry: max_size:32`

### Before OTA implementation

4. **`wire.proto`**: Add `FirmwareChunk` and related messages to WireMessage oneof
5. **`wire.options`**: Add `FirmwareChunk.data: max_size:512`

### Nice to have

6. **`configuration.options`**: Add `CurrentSensorCalibration` array constraints
7. **`battery_management.options`**: Add temperature array constraint

### How to Regenerate After Changes

```bash
# From repo root:
make proto-gen

# This runs:
# 1. buf generate (Go, TypeScript, nanopb)
# 2. Copy nanopb outputs to firmware: make proto-gen-firmware
# 3. CI runs: buf lint + buf format + buf build

# Verify no warnings:
cd e2-studio-star-rx72n-firmware
cmake -S . -B build && cmake --build build
```

### Breaking Change Policy

Per `CLAUDE.md`, breaking proto changes are allowed if all consumers are updated in the same PR:
- Update `star-proto/nanopb/star/v1/motor_control.options`
- Regenerate nanopb C code
- Verify firmware builds with new max_count:4
- Update gateway tests if they rely on exact motor_status count
