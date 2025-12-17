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

### Prefix Convention
All enum values MUST be prefixed with the enum type name:

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

All physical quantities MUST use MKS (Meter-Kilogram-Second) units:

| Quantity         | Unit          | Field Suffix    |
|------------------|---------------|-----------------|
| Length           | meters        | `_m`            |
| Velocity         | meters/second | `_mps`          |
| Acceleration     | m/s^2         | `_mps2`         |
| Angle            | radians       | `_rad`          |
| Angular velocity | rad/s         | `_rad_per_s`    |
| Mass             | kilograms     | `_kg`           |
| Force            | newtons       | `_n`            |
| Torque           | newton-meters | `_nm`           |
| Time             | seconds       | `_s` or `_us`   |
| Temperature      | celsius       | `_celsius`      |
| Current          | amperes       | `_a` or `_ma`   |
| Voltage          | volts         | `_v` or `_mv`   |
| Percentage       | 0-100         | `_percent`      |

### Example

```protobuf
message ImuData {
    // Pitch angle in radians (-pi to pi).
    double pitch_rad = 1;

    // Linear acceleration X in meters per second squared.
    double accel_x_mps2 = 2;

    // Angular velocity Z in radians per second.
    double gyro_z_rad_per_s = 3;
}
```

## Documentation Requirements

### Every Field MUST Be Documented
Include:
1. What the field represents
2. Units (if applicable)
3. Valid range (if applicable)

```protobuf
message VelocityCommand {
    // Left wheel velocity in meters per second.
    // Valid range: -2.0 to 2.0 m/s.
    // Positive values move forward.
    double left_velocity_mps = 1;
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

For ESP32 firmware, configure field sizes in `.options` files:

```
# motor_control.options
star.v1.RequestHeader.request_id    max_size:64
star.v1.RequestHeader.client_version max_size:32
star.v1.VelocityCommand.header      type:FT_STATIC
```

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
        ├── common.proto        # Shared types (RequestHeader, etc.)
        ├── motor_control.proto # Motor control service
        ├── telemetry.proto     # Telemetry service
        └── navigation.proto    # Navigation service
```

## References

- [Boston Dynamics Protobuf Style Guide](https://dev.bostondynamics.com/docs/protos/style_guide.html)
- [Google Protobuf Style Guide](https://protobuf.dev/programming-guides/style/)
- [Buf Lint Rules](https://buf.build/docs/lint/rules)
