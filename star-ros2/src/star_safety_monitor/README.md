# star_safety_monitor

Safety monitoring and watchdog system for STAR autonomous robot platform.

## Overview

This package provides platform integrity monitoring and safety watchdog functionality for the STAR robot. It monitors critical system parameters and can trigger emergency stops when safety thresholds are violated.

## Features

- [ ] Hardware heartbeat monitoring (SPI, gateway, motor controller)
- [ ] Sensor health monitoring (IMU, encoders, battery)
- [ ] Emergency stop triggering (E-Stop priority queue)
- [ ] Diagnostic message publishing
- [ ] Watchdog timer for critical nodes
- [ ] Battery voltage/current monitoring
- [ ] Thermal monitoring
- [ ] Velocity limit enforcement

## Nodes

### `safety_monitor_node`

**Lifecycle node** that monitors platform health and publishes diagnostic messages.

#### Subscribed Topics

- `/battery/state` (TBD) - Battery state from gateway
- `/diagnostics` (diagnostic_msgs/DiagnosticArray) - System diagnostics
- `/odom` (nav_msgs/Odometry) - Odometry for velocity monitoring

#### Published Topics

- `/diagnostics` (diagnostic_msgs/DiagnosticArray) - Safety diagnostics
- `/emergency_stop` (std_msgs/Bool) - Emergency stop trigger

#### Parameters

- `heartbeat_timeout_ms` (int, default: 500) - Heartbeat timeout in milliseconds
- `max_linear_velocity` (double, default: 1.0) - Maximum linear velocity (m/s)
- `max_angular_velocity` (double, default: 2.0) - Maximum angular velocity (rad/s)
- `min_battery_voltage` (double, default: 10.5) - Minimum battery voltage (V)
- `max_battery_current` (double, default: 30.0) - Maximum battery current (A)
- `publish_rate` (double, default: 10.0) - Diagnostic publish rate (Hz)

## Building

```bash
cd ~/star-ros2
colcon build --packages-select star_safety_monitor
```

## Running

```bash
ros2 launch star_safety_monitor safety_monitor.launch.py
```

## Testing

```bash
colcon test --packages-select star_safety_monitor
colcon test-result --verbose
```

## References

- Issue #139: Implement star_safety_monitor node
- Issue #176: E-Stop priority queue
- ROADMAP.md: Phase 3 safety systems

## License

MIT
