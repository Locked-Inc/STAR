# STAR Protobuf Style Guide

This document defines the protobuf conventions for the STAR robot project, following
the [Boston Dynamics protobuf style guide](https://dev.bostondynamics.com/docs/protos/style_guide.html).

## General Rules

### Proto Version
- **Proto3 only** - Do not use proto2 syntax

### Formatting
- **Line limit**: 100 characters maximum
- **Indentation**: 4 spaces (no tabs)
- **Blank lines**: One blank line between message/enum definitions

### Package Naming
- **All lowercase** with dots as separators
- **Match directory structure**: `proto/star/v1/` -> `package star.v1;`
- **Include version**: Always version your APIs (v1, v2, etc.)

## Naming Conventions

| Element     | Convention          | Example                    |
|-------------|---------------------|----------------------------|
| Package     | lowercase.dots      | `star.v1`                  |
| Message     | PascalCase          | `VelocityCommand`          |
| Field       | snake_case          | `left_velocity_mps`        |
| Enum        | PascalCase          | `RobotMode`                |
| Enum Value  | SCREAMING_SNAKE     | `ROBOT_MODE_MANUAL`        |
| Service     | PascalCase + Suffix | `MotorControlService`      |
| RPC Method  | PascalCase          | `SetVelocity`              |

## Enum Rules

### Zero Value Convention
Every enum MUST have its zero value end with `_UNKNOWN`:

```protobuf
enum RobotMode {
    ROBOT_MODE_UNKNOWN = 0;    // Always first, always zero
    ROBOT_MODE_IDLE = 1;
    ROBOT_MODE_MANUAL = 2;
    ROBOT_MODE_AUTONOMOUS = 3;
}
```

> **Note:** Boston Dynamics uses `_UNKNOWN` suffix, while Google's style guide
> recommends `_UNSPECIFIED`. This project follows the Boston Dynamics convention
> for consistency with their robotics API patterns.

### Prefix Convention
All enum values MUST be prefixed with the enum type name in SCREAMING_SNAKE_CASE:

```protobuf
// CORRECT
enum ConnectionStatus {
    CONNECTION_STATUS_UNKNOWN = 0;
    CONNECTION_STATUS_CONNECTED = 1;
    CONNECTION_STATUS_DISCONNECTED = 2;
}

// WRONG - missing prefix
enum ConnectionStatus {
    UNKNOWN = 0;
    CONNECTED = 1;
}
```

## Units Convention (MKS)

All physical quantities MUST use MKS (Meter-Kilogram-Second) units with appropriate
suffixes. For embedded systems, prefer integer types with milli-/deci- prefixes
to avoid floating-point arithmetic.

### Standard Units

| Quantity         | Unit          | Field Suffix    | Type   |
|------------------|---------------|-----------------|--------|
| Length           | meters        | `_m`            | double |
| Velocity         | meters/second | `_mps`          | double |
| Acceleration     | m/s^2         | `_mps2`         | double |
| Angle            | radians       | `_rad`          | double |
| Angular velocity | rad/s         | `_rad_per_s`    | double |
| Mass             | kilograms     | `_kg`           | double |
| Force            | newtons       | `_n`            | double |
| Torque           | newton-meters | `_nm`           | double |
| Time             | seconds       | `_s`            | double |
| Time             | microseconds  | `_us`           | int64  |
| Time             | milliseconds  | `_ms`           | uint32 |
| Percentage       | 0-100         | `_percent`      | int32  |

### Embedded-Optimized Units (Integer Types)

For ESP32 and other embedded targets, use integer types with scaled units
to avoid floating-point operations:

| Quantity    | Unit             | Field Suffix       | Type   | Example         |
|-------------|------------------|--------------------|--------|-----------------|
| Voltage     | millivolts       | `_mv`              | uint32 | 3700 = 3.7V     |
| Current     | milliamps        | `_ma`              | int32  | -500 = -0.5A    |
| Power       | milliwatts       | `_mw`              | int32  | 1850 = 1.85W    |
| Temperature | deci-celsius     | `_deci_celsius`    | int32  | 253 = 25.3°C    |
| Capacity    | milliamp-hours   | `_mah`             | uint32 | 5200 = 5.2Ah    |

> **Why deci-celsius?** Many BMS chips (like BQ7850) and temperature sensors
> return values in 0.1°C resolution. Using deci-celsius preserves this precision
> without floating-point conversion.

### Example

```protobuf
// Standard floating-point for high-level control
message ImuData {
    // Pitch angle in radians (-pi to pi).
    double pitch_rad = 1;

    // Linear acceleration X in meters per second squared.
    double accel_x_mps2 = 2;
}

// Integer types for embedded/BMS data
message CellData {
    // Individual cell voltages in millivolts.
    // Valid range per cell: 2500-4500 mV typical.
    repeated uint32 cell_mv = 1;

    // Total pack voltage in millivolts.
    uint32 pack_mv = 2;
}

message TemperatureData {
    // Temperature readings in deci-celsius (0.1°C units).
    // Example: 253 = 25.3°C, -50 = -5.0°C.
    repeated int32 temp_deci_celsius = 1;
}
```

## Type Selection Guidelines

### When to Use Integer vs Floating-Point

| Use Case | Recommended Type | Rationale |
|----------|------------------|-----------|
| Sensor raw values | int32/uint32 | Matches ADC output, no conversion |
| BMS data (voltage, current, temp) | int32/uint32 with milli-/deci- | Matches chip registers |
| Timestamps | int64 (_us) or uint32 (_ms) | Microsecond precision |
| PID gains, control parameters | double | Precision needed for tuning |
| Wheel velocity, position | double | Standard robotics convention |
| Configuration thresholds | int32/uint32 | Consistency with sensor data |

### When to Use `optional`

Use `optional` for fields that may legitimately be absent:

```protobuf
message GetFirmwareInfoResponse {
    // Current running firmware info.
    FirmwareInfo current_firmware = 2;

    // Previous firmware info (for rollback), if available.
    // May be absent on devices that have never been updated.
    optional FirmwareInfo previous_firmware = 3;
}
```

## Documentation Requirements

### Every Field MUST Be Documented
Include:
1. What the field represents
2. Units (if applicable)
3. Valid range (if applicable)
4. Hardware mapping (if applicable)

```protobuf
message SafetyThresholds {
    // Motor overcurrent limit in milliamps.
    // Motors disabled if current exceeds this.
    // Valid range: 1000-20000 mA.
    uint32 overcurrent_threshold_ma = 1;

    // Emergency temperature cutoff in deci-celsius (0.1°C units).
    // System enters safe mode above this temperature.
    // Valid range: 500-1000 (50.0-100.0°C).
    int32 thermal_shutdown_deci_celsius = 2;
}
```

### Hardware Mapping Comments

When fields map directly to hardware registers or data structures, document this:

```protobuf
// BMS device information.
// Maps to bq7850_device_info_t.
message BmsDeviceInfo {
    // Firmware version (raw register value from BQ7850).
    // BQ7850 returns this as a 16-bit packed value.
    uint32 firmware_version = 2;
}
```

### Service Documentation
Document:
1. Service purpose
2. Expected usage patterns
3. Error conditions

## Request/Response Pattern

### Use RequestHeader and ResponseHeader
All requests SHOULD include a `RequestHeader`:
All responses SHOULD include a `ResponseHeader`:

```protobuf
message SetVelocityRequest {
    RequestHeader header = 1;
    VelocityCommand command = 2;
}

message SetVelocityResponse {
    ResponseHeader header = 1;
    // Response-specific fields...
}
```

## Schema Evolution Rules

1. **Never remove fields** - Mark as `reserved`
2. **Never change field numbers** - They define wire format
3. **Never change field types** - Binary incompatible
4. **Add new fields** with new field numbers only
5. **Use optional** for fields that may not be present in all versions

```protobuf
message VelocityCommand {
    double left_velocity_mps = 1;
    double right_velocity_mps = 2;

    // Reserved: removed in v1.2
    reserved 3;
    reserved "deprecated_field_name";

    // Added in v1.3
    optional double acceleration_limit_mps2 = 4;
}
```

## nanopb Considerations

For ESP32 firmware (no dynamic allocation), configure field sizes in `.options` files:

```
# Common string limits
star.v1.RequestHeader.request_id        max_size:64
star.v1.RequestHeader.client_version    max_size:32

# Array limits matching hardware
star.v1.CellData.cell_mv                max_count:16   # BQ7850 max cells
star.v1.TemperatureData.temp_deci_celsius max_count:3  # BQ7850 temp sensors

# Firmware chunk size
star.v1.FirmwareChunk.data              max_size:1024  # 1KB chunks
```

### nanopb Best Practices

1. **Match hardware limits**: Set `max_count` to actual hardware maximums
2. **Use power-of-2 sizes**: For buffers (256, 512, 1024 bytes)
3. **Document constraints**: Add comments in .options files explaining limits
4. **Test serialization size**: Verify messages fit in SPI/UART buffers

## Breaking Change Policy

Breaking changes are detected by `buf breaking` and include:
- Removing a field or enum value
- Changing a field number
- Changing a field type
- Renaming a package

All breaking changes MUST:
1. Be documented in CHANGELOG.md
2. Increment the API version (v1 -> v2)
3. Be approved by project maintainers

## File Organization

```
proto/
└── star/
    └── v1/
        ├── common.proto              # Shared types (RequestHeader, ResponseHeader, Status)
        ├── motor_control.proto       # Motor control service and PID configuration
        ├── telemetry.proto           # Telemetry streaming service
        ├── battery_management.proto  # BQ7850 BMS monitoring and control
        ├── configuration.proto       # Runtime NVS configuration management
        └── firmware_update.proto     # OTA firmware update service
```

## Java/Kotlin Options

All proto files SHOULD include Java options for Android/Kotlin interop:

```protobuf
option java_multiple_files = true;
option java_outer_classname = "MotorControlProto";
option java_package = "com.star.proto.v1";
```

## References

- [Boston Dynamics Protobuf Style Guide](https://dev.bostondynamics.com/docs/protos/style_guide.html)
- [Google Protobuf Style Guide](https://protobuf.dev/programming-guides/style/)
- [Buf Lint Rules](https://buf.build/docs/lint/rules)
- [nanopb Documentation](https://jpa.kapsi.fi/nanopb/docs/)
