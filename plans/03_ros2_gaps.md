# ROS2 Gaps: star-ros2

## Status Summary

The ROS2 stack has solid infrastructure for communication (SPI bridge, gateway bridge) but is missing autonomous operation capabilities (SLAM, navigation, sensor integration). The most urgent gap is validating the SPI bridge on real hardware.

| Gap | Severity | Package | Effort |
|-----|----------|---------|--------|
| star_spi_bridge hardware testing | CRITICAL | star_spi_bridge | 4-8 hrs on hardware |
| Zero-velocity safety shutdown | HIGH | star_spi_bridge | 2 hrs |
| Safety monitor test completion | HIGH | star_safety_monitor | 4 hrs |
| star_bringup launch files | HIGH | star_bringup | 8-16 hrs |
| SetPIDGains service (custom type) | MEDIUM | star_gateway_bridge | 2 hrs |
| RPLiDAR C1 driver node | HIGH | (new package) | 2-3 days |
| IMU sensor driver node | MEDIUM | (new package) | 1-2 days |
| EKF sensor fusion (robot_localization) | HIGH | star_bringup | 1-2 days |
| SLAM (RTAB-Map) | MEDIUM | star_bringup | 2-3 days |
| Navigation stack (Nav2) | LOW | star_bringup | 3-5 days |
| 4-wheel kinematics fix | MEDIUM | star_spi_bridge | 4 hrs |

---

## Gap 1: star_spi_bridge Hardware Testing (CRITICAL)

### Problem

The `star_spi_bridge` code is written (420 lines node + 152 lines SPI driver + 397 lines tests), but it has **never been run on real hardware** (RPi5 + RX72N).

This is the critical path for first robot motion. Until this is validated, the robot cannot move.

### Hardware Test Plan

**Prerequisites:**
- RPi5 with ROS2 Jazzy installed
- RX72N motor controller connected via SPI (`/dev/spidev0.0`)
- `star-gateway` running (or virtual_rx72n for simulation)

**Step 1: Build on RPi5**
```bash
cd /workspaces/STAR/star-ros2
colcon build --packages-select star_spi_bridge --cmake-args -DCMAKE_BUILD_TYPE=Release
```

**Step 2: Launch SPI Bridge**
```bash
source install/setup.bash
ros2 launch star_spi_bridge star_spi_bridge.launch.py
```

**Step 3: Send Test Commands**
```bash
# Send velocity command
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist \
    "linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}" --rate 10

# Monitor telemetry
ros2 topic echo /odom/unfiltered
ros2 topic echo /battery_state
ros2 topic echo /joint_states
```

**Step 4: Verify Timing**
```bash
# Check that 100 Hz timer fires consistently
ros2 topic hz /joint_states
# Expected: ~100 Hz with <5% jitter
```

**Known Risks:**
- SPI device permissions may need udev rules
- 100 Hz polling might cause CPU overload on RPi5 (test with `htop`)
- RX72N SPI CS timing must match `spi_speed_hz = 10000000` setting

### Fixes Likely Needed

Based on code review, anticipate these issues:
1. SPI device path: `/dev/spidev0.0` vs `/dev/spidev0.1` (hardware-dependent)
2. SPI mode: May need `SPI_MODE_0` vs `SPI_MODE_3` depending on RX72N config
3. Byte order: Little-endian already assumed; verify CRC-32 byte order matches

---

## Gap 2: Zero-Velocity Safety Shutdown (HIGH)

### Problem

`star_spi_bridge/src/star_spi_driver_node.cpp:112`:
```cpp
// TODO(safety): Send final zero-velocity frame before deactivation
// Currently node deactivates without stopping motors
```

If the ROS2 node crashes or the system shuts down, the motors continue at their last commanded velocity. This is a **critical safety issue**.

### Fix

In `StarSpiDriverNode::on_deactivate()`:

```cpp
CallbackReturn StarSpiDriverNode::on_deactivate(
    const rclcpp_lifecycle::State & previous_state)
{
    (void)previous_state;

    // SAFETY: Send zero-velocity command before deactivating
    auto zero_cmd = std::make_shared<geometry_msgs::msg::Twist>();
    zero_cmd->linear.x = 0.0;
    zero_cmd->linear.y = 0.0;
    zero_cmd->angular.z = 0.0;

    // Build and send zero VelocityCommand directly (bypass ROS2 subscription)
    auto velocity_cmd = converter_->twistToVelocityCommand(*zero_cmd);
    auto wire_msg = buildWireMessage(velocity_cmd);

    for (int attempt = 0; attempt < 3; ++attempt) {
        if (driver_->send(wire_msg) == SpiDriver::Status::kOk) {
            RCLCPP_INFO(get_logger(), "Zero-velocity safety frame sent on deactivation");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Now deactivate publishers
    cmd_vel_sub_.reset();
    odom_pub_->on_deactivate();
    joint_states_pub_->on_deactivate();
    battery_state_pub_->on_deactivate();

    timer_->cancel();
    return CallbackReturn::SUCCESS;
}
```

### Estimated Effort: 2 hours

---

## Gap 3: Safety Monitor Test Completion (HIGH)

### Problem

`star_safety_monitor/test/test_safety_monitor.cpp` has 3 tests with `GTEST_SKIP`:

```cpp
TEST_F(SafetyMonitorTest, BatterySafetyChecks) {
    GTEST_SKIP() << "TODO: Test battery voltage/current monitoring";
}

TEST_F(SafetyMonitorTest, EmergencyStopTrigger) {
    GTEST_SKIP() << "TODO: Test E-Stop triggering logic";
}

TEST_F(SafetyMonitorTest, DiagnosticPublishing) {
    GTEST_SKIP() << "TODO: Test diagnostic message generation";
}
```

These cover critical safety functionality.

### Implementation

```cpp
TEST_F(SafetyMonitorTest, BatterySafetyChecks) {
    // Simulate low voltage
    auto battery_msg = std::make_shared<sensor_msgs::msg::BatteryState>();
    battery_msg->voltage = 9.5f;  // Below min_battery_voltage = 10.5V
    battery_msg->current = 5.0f;
    battery_msg->temperature = 25.0f;

    battery_sub_->publish(*battery_msg);
    rclcpp::spin_some(node_);

    // Should trigger E-Stop
    EXPECT_TRUE(node_->is_estop_active());

    // Simulate overcurrent
    battery_msg->voltage = 12.0f;
    battery_msg->current = 35.0f;  // Above max_battery_current = 30.0A
    battery_sub_->publish(*battery_msg);
    rclcpp::spin_some(node_);

    EXPECT_TRUE(node_->is_estop_active());
}

TEST_F(SafetyMonitorTest, EmergencyStopTrigger) {
    // Trigger E-Stop via heartbeat timeout
    // Don't publish diagnostics for > heartbeat_timeout_ms
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    rclcpp::spin_some(node_);

    // Verify E-Stop published
    auto estop_msg = estop_sub_->wait_for_message(std::chrono::seconds(1));
    ASSERT_TRUE(estop_msg.has_value());
    EXPECT_TRUE(estop_msg->data);

    // Verify recovery delay enforced
    publish_valid_diagnostics();
    EXPECT_TRUE(node_->is_estop_active());  // Still active during recovery delay
}

TEST_F(SafetyMonitorTest, DiagnosticPublishing) {
    // Activate the node
    activate_node();

    // Wait for diagnostic timer to fire
    auto diag_msg = diagnostics_sub_->wait_for_message(std::chrono::seconds(2));
    ASSERT_TRUE(diag_msg.has_value());
    EXPECT_FALSE(diag_msg->status.empty());

    // Check for safety status entry
    bool found_safety_status = false;
    for (const auto & status : diag_msg->status) {
        if (status.name == "safety_monitor") {
            found_safety_status = true;
            break;
        }
    }
    EXPECT_TRUE(found_safety_status);
}
```

### Estimated Effort: 4 hours

---

## Gap 4: star_bringup Launch Files (HIGH)

### Problem

`star_bringup/` is an empty skeleton with no launch files:
```
star_bringup/
├── CMakeLists.txt  (minimal, 31 lines)
└── package.xml     (skeleton)
```

Without launch files, bringing up the entire system requires manually starting each node.

### Required Launch Files

```
star_bringup/
├── launch/
│   ├── star_system.launch.py          # Main entry point (includes all others)
│   ├── spi_bridge.launch.py           # star_spi_bridge lifecycle
│   ├── gateway_bridge.launch.py       # star_gateway_bridge
│   ├── safety_monitor.launch.py       # star_safety_monitor lifecycle
│   ├── sensors.launch.py              # LiDAR, IMU (when implemented)
│   └── lifecycle_manager.launch.py    # Orchestrate lifecycle nodes
├── config/
│   ├── hardware_params.yaml           # Motor model, wheel geometry
│   ├── gateway_params.yaml            # Gateway address, rates
│   └── safety_params.yaml            # Safety thresholds
└── rviz/
    └── star_default.rviz              # Default RViz2 config
```

### Main Launch File

```python
# star_bringup/launch/star_system.launch.py
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    bringup_dir = get_package_share_directory('star_bringup')
    spi_bridge_dir = get_package_share_directory('star_spi_bridge')
    safety_dir = get_package_share_directory('star_safety_monitor')

    return LaunchDescription([
        # Arguments
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('gateway_address', default_value='localhost:50051'),

        # Core nodes
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(bringup_dir, 'launch', 'spi_bridge.launch.py')
            )
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(bringup_dir, 'launch', 'gateway_bridge.launch.py')
            )
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(bringup_dir, 'launch', 'safety_monitor.launch.py')
            )
        ),
    ])
```

### Lifecycle Manager

The SPI bridge and safety monitor are lifecycle nodes. They need a managed lifecycle:

```python
# In star_system.launch.py, add lifecycle manager:
from launch_ros.actions import Node

lifecycle_manager = Node(
    package='nav2_lifecycle_manager',
    executable='lifecycle_manager',
    name='lifecycle_manager_star',
    output='screen',
    parameters=[{
        'autostart': True,
        'node_names': [
            'star_spi_driver',
            'safety_monitor',
        ],
    }]
)
```

### Estimated Effort: 8-16 hours

---

## Gap 5: SetPIDGains Custom Service Type (MEDIUM)

### Problem

In `star_gateway_bridge_node.hpp:138`:
```cpp
// TODO: Define custom service type for PID gains (kp, ki, kd).
// Currently using std_srvs/SetBool as placeholder
rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr set_pid_gains_srv_;
```

`std_srvs/SetBool` cannot carry PID gain values.

### Fix

Create a custom ROS2 service interface package or use the existing proto-generated message:

**Option A: Custom ROS2 service message**
```
star_interfaces/
├── srv/
│   └── SetPidGains.srv
│       ---
│       uint8 motor_id    # 0-3
│       float64 kp        # Proportional gain
│       float64 ki        # Integral gain
│       float64 kd        # Derivative gain
│       ---
│       bool success
│       string message
```

**Option B: Use existing proto** (simpler — reuse the generated C++ proto)
```cpp
// Change service type to use proto directly via a ROS2 wrapper
```

### Estimated Effort: 2 hours

---

## Gap 6: RPLiDAR C1 Driver Node (HIGH for SLAM)

### Problem

No ROS2 driver for RPLiDAR C1 exists in the project. The robot cannot do SLAM without lidar data.

### Solution

Use the existing `rplidar_ros` package from ROS2:

```bash
# Install
sudo apt install ros-jazzy-rplidar-ros

# Add to rosdep
# star_bringup/package.xml:
<exec_depend>rplidar_ros</exec_depend>
```

Create a launch file:
```python
# star_bringup/launch/sensors.launch.py
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='rplidar_ros',
            executable='rplidar_composition',
            output='screen',
            parameters=[{
                'serial_port': '/dev/ttyUSB0',
                'serial_baudrate': 460800,
                'frame_id': 'laser_frame',
                'angle_compensate': True,
                'scan_mode': 'Standard',
            }],
        ),
    ])
```

Add static TF transform:
```python
# base_link to laser_frame transform (measure from CAD or hardware)
Node(
    package='tf2_ros',
    executable='static_transform_publisher',
    arguments=['0.15', '0', '0.10',   # x, y, z offset
               '0', '0', '0', '1',    # quaternion (no rotation)
               'base_link', 'laser_frame']
),
```

### Estimated Effort: 4-8 hours (installation + config + TF calibration)

---

## Gap 7: EKF Sensor Fusion (HIGH)

### Problem

Without EKF, raw wheel odometry drifts quickly. The `robot_localization` package fuses odometry + IMU for a better state estimate.

### Configuration

```yaml
# star_bringup/config/ekf_config.yaml
ekf_filter_node:
  ros__parameters:
    frequency: 50.0
    sensor_timeout: 0.1
    two_d_mode: true

    odom0: /odom/unfiltered
    odom0_config: [false, false, false,
                   false, false, false,
                   true,  true,  false,
                   false, false, true,
                   false, false, false]

    # imu0: /imu/data  (uncomment when IMU added)

    world_frame: odom
    odom_frame: odom
    base_link_frame: base_link
    map_frame: map
```

### Launch Integration

```python
# In star_system.launch.py:
Node(
    package='robot_localization',
    executable='ekf_node',
    name='ekf_node',
    output='screen',
    parameters=[os.path.join(config_dir, 'ekf_config.yaml')],
    remappings=[('odometry/filtered', '/odom')]
)
```

### Estimated Effort: 4-8 hours

---

## Gap 8: 4-Wheel Kinematics Mismatch (MEDIUM)

### Problem

`star_spi_bridge/src/spi_message_converter.cpp` implements differential drive with 2-wheel kinematics:

```cpp
// Current implementation (2-wheel model):
float left_velocity_mps = (linear - angular * wheel_base_ / 2.0f);
float right_velocity_mps = (linear + angular * wheel_base_ / 2.0f);
```

But the robot has **4 wheels**. The proto defines:
```
front_left, front_right, back_left, back_right
```

### Fix Options

**Option A:** Keep 2-wheel model (skid steering with front=back)
```cpp
// Set front and back to same value
msg.set_front_left_mps(left_velocity_mps);
msg.set_back_left_mps(left_velocity_mps);   // Same as front
msg.set_front_right_mps(right_velocity_mps);
msg.set_back_right_mps(right_velocity_mps);  // Same as front
```

**Option B:** Full 4-wheel model (more accurate for turning radius)
Requires verifying physical wheel layout and computing individual velocities from differential drive model with proper wheelbase/track geometry.

### Recommended: Option A for now (verify against hardware)

### Estimated Effort: 2-4 hours

---

## Complete ROS2 Status

```
star-ros2/
├── star_gateway_bridge  ✅ 100% Complete
│   ├── ForwardTelemetry (10 Hz push to gateway)
│   ├── GetTeleopCommand (50 Hz poll)
│   └── SetPIDGains      ⚠️ Needs custom service type
│
├── star_spi_bridge      ⚠️ 85% - Code complete, hardware untested
│   ├── SPI driver       ✅ Frame protocol implemented
│   ├── Message converter ✅ Twist→VelocityCommand
│   ├── Lifecycle node   ✅ Transitions implemented
│   ├── Zero-vel safety  ❌ TODO at line 112
│   └── 4-wheel kinematics ⚠️ Only 2-wheel model
│
├── star_safety_monitor  ⚠️ 40% - Core logic works, tests incomplete
│   ├── Heartbeat watchdog ✅ Working
│   ├── Velocity limits  ✅ Working
│   ├── Battery monitoring ⚠️ Implemented, not tested
│   ├── E-Stop triggering ⚠️ Implemented, not tested
│   └── Stall detection  ✅ Working
│
└── star_bringup         ❌ 5% - Empty skeleton
    ├── Main launch      ❌ Missing
    ├── Config files     ❌ Missing
    ├── Lifecycle mgr    ❌ Missing
    ├── LiDAR launch     ❌ Missing (no driver yet)
    └── EKF config       ❌ Missing
```
