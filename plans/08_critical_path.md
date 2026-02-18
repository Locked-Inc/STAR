# Critical Path to First Robot Motion

## Objective

Get the robot physically moving under remote control from the UI, with telemetry visible to the operator.

**Estimated Total Time:** 2-4 days (with hardware available)

---

## Prerequisites

- [ ] RPi5 with ROS2 Jazzy installed
- [ ] RX72N motor controller connected to RPi5 via SPI (`/dev/spidev0.0`)
- [ ] Motors wired to DRV8243S H-bridge
- [ ] Power supply (12V) to motor board
- [ ] SPI wiring verified against pinout: `docs/sections/03_hardware_pinout.tex`
- [ ] Laptop/desktop with browser for UI access

---

## Phase 1: Fix Nanopb Options Bug (30 minutes)

**Why First:** This affects all motor status messages. Fix before any integration testing.

```bash
# Fix motor_control.options
cd star-proto/nanopb/star/v1/
# Edit motor_control.options: change max_count:2 to max_count:4

# Regenerate
make proto-gen
make proto-gen-firmware

# Verify firmware builds
cd e2-studio-star-rx72n-firmware
./build.sh debug
```

**Success:** CMake build completes with zero warnings.

---

## Phase 2: Implement WireMessage Dispatcher (4-6 hours)

**Why:** Without the dispatcher, the RX72N cannot receive any commands from the RPi5. This is the most critical missing firmware component.

### Steps

1. Add handler function declarations to task headers:

```c
// e2-studio-star-rx72n-firmware/src/tasks/motor_control_task.h
rx_err_t motor_control_handle_velocity_command(
    const star_v1_VelocityCommand* cmd);
rx_err_t motor_control_handle_emergency_stop(
    const star_v1_EmergencyStopCommand* cmd);
```

2. Implement dispatch in `comm_task.c`:

```c
static rx_err_t internal_dispatch_wire_message(
    const star_v1_WireMessage* msg)
{
    RX_CHECK_NULL_PTR(msg, k_rx_err_null_ptr);

    switch (msg->which_payload) {
    case star_v1_WireMessage_velocity_command_tag:
        return motor_control_handle_velocity_command(
            &msg->payload.velocity_command);
    case star_v1_WireMessage_emergency_stop_command_tag:
        return motor_control_handle_emergency_stop(
            &msg->payload.emergency_stop_command);
    default:
        rx_log_warn(k_tag_comm, "Unknown payload: %d",
                    (int)msg->which_payload);
        return k_rx_ok;
    }
}
```

3. Implement handlers in `motor_control_task.c`:

```c
rx_err_t motor_control_handle_velocity_command(
    const star_v1_VelocityCommand* cmd)
{
    RX_CHECK_NULL_PTR(cmd, k_rx_err_null_ptr);

    UINT status = tx_mutex_get(&s_shared_data_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) { return k_rx_err_mutex; }

    /* Convert m/s velocity to PID setpoint */
    s_shared_data.motor_setpoints[k_motor_fl] = cmd->front_left_mps;
    s_shared_data.motor_setpoints[k_motor_fr] = cmd->front_right_mps;
    s_shared_data.motor_setpoints[k_motor_bl] = cmd->back_left_mps;
    s_shared_data.motor_setpoints[k_motor_br] = cmd->back_right_mps;
    s_shared_data.velocity_cmd_timestamp = tx_time_get();

    tx_mutex_put(&s_shared_data_mutex);
    return k_rx_ok;
}
```

4. Write Unity tests in `tests/test_comm_task_dispatch.c`

5. Run tests:
```bash
cd e2-studio-star-rx72n-firmware
./tests/run_tests.sh
```

**Success:** All tests pass. Motors respond to velocity commands from comm_task.

---

## Phase 3: Flash Firmware to RX72N (1-2 hours)

```bash
# Using e2 studio debugger or J-Link
cd e2-studio-star-rx72n-firmware
./build.sh debug

# Flash using e2 studio:
# 1. Connect USB-C to RX72N debug port
# 2. Open e2 studio, import project
# 3. Debug > Debug Configurations > HardwareDebug
# 4. Click Debug (automatically flashes + starts)

# OR using OpenOCD/J-Link command line:
# openocd -f rx72n.cfg -c "program star_rx72n.elf verify reset exit"
```

**Verify firmware is running:**
```bash
# Connect to debug UART (SCI9, 115200 baud)
screen /dev/ttyACM0 115200
# Should see boot sequence logs
```

**Success:** "ThreadX running" appears in UART console. LED status task is blinking.

---

## Phase 4: Build and Launch Gateway (30 minutes)

```bash
# On RPi5:
cd star-gateway
go build ./cmd/star-gateway

# Launch with SPI mode:
TRANSPORT_MODE=force-spi ./star-gateway

# OR with USB CDC:
TRANSPORT_MODE=force-usb ./star-gateway

# Verify gateway connected to RX72N:
# Look for "SPI transport connected" in gateway logs
# Look for PING/PONG frames in debug output
```

**Success:** Gateway logs show "Transport connected" and telemetry is flowing (check with `grpcurl` or similar).

---

## Phase 5: Test ROS2 SPI Bridge (4-8 hours)

```bash
# On RPi5 with ROS2 Jazzy:
cd star-ros2

# Build star_spi_bridge
colcon build --packages-select star_spi_bridge --cmake-args -DCMAKE_BUILD_TYPE=Release

# Launch
source install/setup.bash
ros2 launch star_spi_bridge star_spi_bridge.launch.py

# In another terminal, send velocity command:
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist \
    "linear: {x: 0.1}" --rate 10 --times 30

# Verify motors spin:
# Watch RX72N LEDs change state
# Check UART console for motor command logs

# Verify telemetry coming back:
ros2 topic echo /odom/unfiltered --once
ros2 topic echo /joint_states --once
```

**Expected issues to debug:**
- SPI device path (`/dev/spidev0.0` vs `/dev/spidev0.1`)
- SPI permissions (may need udev rules for `star` user)
- Frame sync issues (look for CRC errors in firmware UART)

**Add udev rule if needed:**
```bash
# /etc/udev/rules.d/99-star-spi.rules
SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"
```

**Success:** Motors spin when `/cmd_vel` published. Odometry visible on `/odom/unfiltered`.

---

## Phase 6: Zero-Velocity Safety Fix (2 hours)

Before operating, implement the safety shutdown:

```cpp
// star-ros2/src/star_spi_bridge/src/star_spi_driver_node.cpp

CallbackReturn StarSpiDriverNode::on_deactivate(
    const rclcpp_lifecycle::State &)
{
    // Send zero velocity to motors
    auto zero_cmd = buildZeroVelocityCommand();
    for (int i = 0; i < 3; ++i) {
        if (driver_->send(zero_cmd) == SpiDriver::Status::kOk) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Deactivate publishers
    // ...
    return CallbackReturn::SUCCESS;
}
```

**Rebuild and retest.**

---

## Phase 7: Add Telemetry WebSocket to Gateway (4-6 hours)

So the operator can see robot state in the UI:

```go
// star-gateway/internal/app/gateway.go

// Add WebSocket telemetry endpoint
mux.HandleFunc("/ws/telemetry", s.handleWebSocketTelemetry)
mux.HandleFunc("/ws/battery", s.handleWebSocketBattery)

// Implementation:
func (g *Gateway) handleWebSocketTelemetry(w http.ResponseWriter, r *http.Request) {
    conn, err := upgrader.Upgrade(w, r, nil)
    if err != nil {
        return
    }
    defer conn.Close()

    // Subscribe to telemetry stream
    ch := g.dispatcher.Subscribe(wire.TypeTelemetryData)
    defer g.dispatcher.Unsubscribe(ch)

    for msg := range ch {
        // Serialize to JSON and send over WebSocket
        data, _ := json.Marshal(telemetryToJSON(msg))
        conn.WriteMessage(websocket.TextMessage, data)
    }
}
```

---

## Phase 8: Add Emergency Stop to UI (3 hours)

Before extended operation, add E-Stop button to UI (see `plans/04_ui_gaps.md` for code).

**Minimum implementation:**
- Large red button in UI
- Sends `POST /emergency-stop` to gateway
- Gateway calls `MotorControlService.EmergencyStop()`

---

## Phase 9: Launch Gateway Bridge (1 hour)

```bash
# On RPi5:
cd star-ros2
colcon build --packages-select star_gateway_bridge

source install/setup.bash
ros2 run star_gateway_bridge star_gateway_bridge_node \
    --ros-args -p gateway_address:=localhost:50051
```

**Success:** ROS2 topics visible. Telemetry flowing through full stack:
```
RX72N → SPI → Gateway → WebSocket → UI (telemetry display)
UI → WebSocket → Gateway → gRPC → ROS2 → SPI → RX72N (motor commands)
```

---

## Success Criteria for First Motion

- [ ] `ros2 topic pub /cmd_vel` causes physical motors to spin
- [ ] Encoder feedback visible on `/odom/unfiltered` and `/joint_states`
- [ ] Battery state visible on `/battery_state`
- [ ] UI gamepad control causes robot to move
- [ ] UI shows telemetry (motor velocities, encoder counts)
- [ ] E-Stop button stops motors within <500ms
- [ ] Robot stops when ROS2 node deactivates (zero-velocity frame sent)

---

## What Comes After First Motion

Once the robot moves, these are the next priorities:

1. **NVS Configuration** (4-6 hrs) — PID gains persist across reboot
2. **ROS2 star_bringup** (8-16 hrs) — Single launch command for full system
3. **EKF Sensor Fusion** (4-8 hrs) — Better odometry with robot_localization
4. **RPLiDAR Integration** (8-16 hrs) — 2D SLAM capability
5. **UI Battery/Motor Display** (10-20 hrs) — Operator situational awareness
6. **FirmwareUpdateService** (20-30 hrs) — OTA updates without physical access
