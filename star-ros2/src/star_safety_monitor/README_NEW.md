# star_safety_monitor

Safety monitoring and watchdog system for STAR autonomous robot platform.

## Overview

This package provides platform integrity monitoring and safety watchdog functionality for the STAR robot. It monitors critical system parameters and can trigger emergency stops when safety thresholds are violated.

## Features

- [x] Hardware heartbeat monitoring (diagnostic messages)
- [x] Sensor health monitoring (via diagnostics)
- [x] Emergency stop triggering on critical violations
- [x] Diagnostic message publishing
- [x] Watchdog timer for critical nodes
- [x] Velocity limit enforcement
- [ ] Battery voltage/current monitoring (future: integrate with gateway)
- [ ] Thermal monitoring (future: integrate with gateway)

## Nodes

### `safety_monitor_node`

**Lifecycle node** that monitors platform health and publishes diagnostic messages.

#### Subscribed Topics

- `/odom` (nav_msgs/Odometry) - Odometry for velocity monitoring
- `/diagnostics` (diagnostic_msgs/DiagnosticArray) - System diagnostics for heartbeat monitoring

#### Published Topics

- `/diagnostics` (diagnostic_msgs/DiagnosticArray) - Safety diagnostics and health status
- `/emergency_stop` (std_msgs/Bool) - Emergency stop trigger (true when critical condition detected)

#### Parameters

- `heartbeat_timeout_ms` (int, default: 500) - Heartbeat timeout in milliseconds
- `max_linear_velocity` (double, default: 1.0) - Maximum linear velocity (m/s)
- `max_angular_velocity` (double, default: 2.0) - Maximum angular velocity (rad/s)
- `min_battery_voltage` (double, default: 10.5) - Minimum battery voltage (V)
- `max_battery_current` (double, default: 30.0) - Maximum battery current (A)
- `max_battery_temp` (double, default: 60.0) - Maximum battery temperature (°C)
- `publish_rate` (double, default: 10.0) - Diagnostic publish rate (Hz)
- `enable_auto_estop` (bool, default: true) - Automatically trigger E-Stop on critical violations
- `estop_recovery_delay` (double, default: 5.0) - Delay before allowing E-Stop recovery (seconds)

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

## Implementation Details

### Heartbeat Monitoring

The node subscribes to `/diagnostics` and tracks the timestamp of the last received diagnostic message. If no diagnostics are received within `heartbeat_timeout_ms`, the node:

1. Logs a warning
2. Sets `heartbeat_timeout_triggered` flag
3. Triggers emergency stop if `enable_auto_estop` is true

Recovery requires:

1. Heartbeat to be restored
2. Time elapsed >= `estop_recovery_delay`

### Velocity Limit Enforcement

The node calculates the magnitude of linear and angular velocities from odometry messages:

- Linear velocity: √(vx² + vy² + vz²)
- Angular velocity: √(ωx² + ωy² + ωz²)

If either exceeds configured limits, the `velocity_exceeded` flag is set and a warning is logged.

### Diagnostic Publishing

The node publishes diagnostic status messages containing:

1. **System Health**: Overall status (OK/WARN/ERROR) with detailed message
2. **Heartbeat Status**: Last seen timestamp for each tracked node
3. **Velocity Data**: Current linear/angular velocities and limit status
4. **Emergency Stop Status**: Whether emergency stop is currently active

### Lifecycle Management

The node uses ROS2 lifecycle (rclcpp_lifecycle) for proper state management:

- **UNCONFIGURED** → **INACTIVE**: Load parameters, create publishers/subscribers
- **INACTIVE** → **ACTIVE**: Start monitoring timers
- **ACTIVE** → **INACTIVE**: Stop timers, deactivate publishers
- **INACTIVE** → **FINALIZED**: Clean up resources

A lifecycle manager is included in the launch file to automatically handle these transitions.

## References

- Issue #139: Implement star_safety_monitor node
- Issue #176: E-Stop priority queue
- ROADMAP.md: Phase 3 safety systems

## License

MIT
