# Testing Gaps

## Status Summary

Unit test coverage is good in some areas (Gateway 65%+, firmware Unity tests). Integration and end-to-end tests are almost entirely absent.

| Test Category | Status | Coverage | Gap |
|---------------|--------|----------|-----|
| Firmware Unit Tests (Unity) | ✅ 87 files | High | Not run in CI |
| Gateway Unit Tests (Go) | ✅ 30+ files | 65%+ | Complete |
| Proto Serialization Tests | ✅ Go/TS/nanopb | Complete | Complete |
| ROS2 Unit Tests | ⚠️ Partial | 1318 LOC | 3 tests skipped |
| UI Unit Tests | ❌ None | 0% | Zero test files |
| Virtual RX72N Tests | ✅ 432 LOC | Frame protocol | No motor dynamics |
| Firmware Integration Tests | ❌ None | 0% | No end-to-end tests |
| ROS2 Integration Tests | ❌ None | 0% | No E2E tests |
| Hardware-in-Loop Tests | ❌ None | 0% | No hardware test suite |

---

## Gap 1: UI Unit Tests (HIGH)

### Problem
Vitest is configured but zero test files exist. A broken component (e.g., E-Stop button) could ship undetected.

### Required Tests

```
star-ui/src/
├── components/
│   ├── ControllerView.test.tsx
│   ├── EmergencyStopButton.test.tsx  ← Critical safety component
│   ├── BatteryStatus.test.tsx
│   └── TelemetryDisplay.test.tsx
├── hooks/
│   ├── useGamepad.test.ts
│   ├── useControllerConnection.test.ts
│   └── useTelemetryStream.test.ts
└── services/
    └── ControllerService.test.ts
```

### Example: Gamepad Hook Test

```typescript
// src/hooks/useGamepad.test.ts
import { renderHook, act } from '@testing-library/react';
import { useGamepad } from './useGamepad';
import { vi, describe, it, expect, beforeEach } from 'vitest';

describe('useGamepad', () => {
    beforeEach(() => {
        // Mock Gamepad API
        vi.stubGlobal('navigator', {
            getGamepads: vi.fn().mockReturnValue([null]),
        });
    });

    it('returns disconnected when no gamepad present', () => {
        const { result } = renderHook(() => useGamepad());
        expect(result.current.connected).toBe(false);
        expect(result.current.linearVelocity).toBe(0);
        expect(result.current.angularVelocity).toBe(0);
    });

    it('applies deadzone to small stick inputs', () => {
        vi.stubGlobal('navigator', {
            getGamepads: vi.fn().mockReturnValue([{
                connected: true,
                axes: [0.05, 0.05],  // Below deadzone (0.1)
                buttons: [],
                id: 'Test Gamepad',
            }]),
        });

        const { result } = renderHook(() => useGamepad());
        act(() => {
            // Trigger gamepad poll
        });

        expect(result.current.linearVelocity).toBe(0);  // Deadzone applied
        expect(result.current.angularVelocity).toBe(0);
    });

    it('clamps velocity to [-1, 1]', () => {
        vi.stubGlobal('navigator', {
            getGamepads: vi.fn().mockReturnValue([{
                connected: true,
                axes: [2.0, -3.0],  // Out of range
                buttons: [],
                id: 'Test Gamepad',
            }]),
        });

        const { result } = renderHook(() => useGamepad());
        act(() => { /* trigger poll */ });

        expect(result.current.linearVelocity).toBeLessThanOrEqual(1);
        expect(result.current.angularVelocity).toBeGreaterThanOrEqual(-1);
    });
});
```

### Estimated Effort: 10-15 hours

---

## Gap 2: Firmware Integration Tests (HIGH)

### Problem
There is no test that verifies the full path: `RPi5 Gateway → SPI → RX72N → Motors → Encoder Feedback → Gateway`.

Individual units are tested, but their integration is not.

### Integration Test Suite: `tests/integration/`

```c
/* tests/integration/test_velocity_command_integration.c */

/**
 * @brief Test that velocity commands flow from comm_task to motor_control_task.
 *
 * This test verifies:
 * 1. comm_task decodes WireMessage correctly
 * 2. Dispatcher routes to motor_control_handle_velocity_command()
 * 3. Motor setpoints are updated in shared_data
 * 4. Motor PID loop picks up new setpoints
 */
TEST(velocity_command_integration, setpoints_updated_after_command) {
    /* Setup: Initialize shared data */
    rx_shared_data_init(&s_shared_data);

    /* Build a velocity command */
    star_v1_WireMessage msg = star_v1_WireMessage_init_zero;
    msg.which_payload = star_v1_WireMessage_velocity_command_tag;
    msg.payload.velocity_command.front_left_mps  = 0.5f;
    msg.payload.velocity_command.front_right_mps = 0.5f;
    msg.payload.velocity_command.back_left_mps   = 0.5f;
    msg.payload.velocity_command.back_right_mps  = 0.5f;

    /* Dispatch through comm_task dispatcher */
    rx_err_t result = comm_task_dispatch_wire_message(&msg);

    /* Verify motor setpoints updated */
    TEST_ASSERT_EQUAL(k_rx_ok, result);

    bms_snapshot_t snap;
    motor_control_get_setpoints(&setpoints);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, setpoints.front_left_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, setpoints.front_right_mps);
}

TEST(velocity_command_integration, estop_overrides_velocity) {
    /* First set a velocity */
    star_v1_WireMessage vel_msg = /* ... 0.5 m/s */;
    comm_task_dispatch_wire_message(&vel_msg);

    /* Then send E-Stop */
    star_v1_WireMessage estop_msg = star_v1_WireMessage_init_zero;
    estop_msg.which_payload = star_v1_WireMessage_emergency_stop_command_tag;
    estop_msg.payload.emergency_stop_command.reason = /* "test" */;

    comm_task_dispatch_wire_message(&estop_msg);

    /* Verify motors stopped */
    motor_setpoints_t setpoints;
    motor_control_get_setpoints(&setpoints);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, setpoints.front_left_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, setpoints.front_right_mps);

    /* Verify E-Stop flag set */
    TEST_ASSERT_TRUE(motor_control_is_estop_active());
}
```

---

## Gap 3: End-to-End Integration Tests (HIGH)

### Problem
No test validates the complete path from UI to robot motion and back.

### E2E Test Harness Design

```
tests/e2e/
├── setup/
│   ├── launch_virtual_rx72n.sh      (start simulator)
│   ├── launch_gateway.sh            (start gateway in sim mode)
│   └── setup_ros2_env.sh            (configure ROS2)
├── test_motor_commands.py           (ROS2 → Gateway → Virtual RX72N)
├── test_telemetry_streaming.py      (Virtual RX72N → Gateway → UI)
├── test_estop_latency.py            (Measure E-Stop response time)
└── test_transport_failover.py       (USB→SPI failover)
```

### Example: Motor Command E2E Test

```python
#!/usr/bin/env python3
# tests/e2e/test_motor_commands.py

"""
End-to-end test: ROS2 cmd_vel → Gateway → Virtual RX72N
"""
import subprocess
import time
import json
import websocket
import pytest
import rclpy
from geometry_msgs.msg import Twist

GATEWAY_URL = "http://localhost:8080"
WS_TELEMETRY_URL = "ws://localhost:8080/ws/telemetry"

class TestMotorCommands:
    @pytest.fixture(autouse=True)
    def setup_ros2(self):
        rclpy.init()
        self.node = rclpy.create_node('e2e_test_node')
        self.publisher = self.node.create_publisher(Twist, '/cmd_vel', 10)
        yield
        self.node.destroy_node()
        rclpy.shutdown()

    def test_velocity_command_reaches_gateway(self):
        """Verify /cmd_vel reaches gateway and is dispatched."""
        received_telemetry = []

        def on_telemetry(ws, message):
            data = json.loads(message)
            received_telemetry.append(data)

        # Subscribe to telemetry WebSocket
        ws = websocket.WebSocketApp(WS_TELEMETRY_URL, on_message=on_telemetry)
        # Start WebSocket in background thread

        # Send velocity command via ROS2
        msg = Twist()
        msg.linear.x = 0.5  # 0.5 m/s forward
        for _ in range(10):
            self.publisher.publish(msg)
            rclpy.spin_once(self.node, timeout_sec=0.1)

        time.sleep(0.5)  # Wait for telemetry

        # Verify telemetry received (virtual RX72N echoes commands)
        assert len(received_telemetry) > 0, "No telemetry received from gateway"
        latest = received_telemetry[-1]
        assert 'motors' in latest, "No motor data in telemetry"

    def test_estop_response_time(self):
        """Verify E-Stop response latency < 100ms."""
        # Send velocity command
        msg = Twist()
        msg.linear.x = 1.0
        self.publisher.publish(msg)
        time.sleep(0.1)

        # Trigger E-Stop and measure time
        import time as t
        start = t.monotonic()
        import requests
        response = requests.post(f"{GATEWAY_URL}/emergency-stop",
                                 json={"reason": "e2e_test"})
        end = t.monotonic()

        assert response.status_code == 200
        assert (end - start) < 0.1, f"E-Stop took {end-start:.3f}s, expected <100ms"
```

### Gateway E2E Test (existing but needs enhancement)

`star-gateway/test/e2e/hil_test.go` exists (HIL test binary). Enhance it:

```go
// star-gateway/test/e2e/hil_test.go

func TestVelocityCommandEndToEnd(t *testing.T) {
    // Start virtual_rx72n in simulation mode
    sim := startVirtualRX72N(t)
    defer sim.Stop()

    // Connect gateway
    gw := startGateway(t, "STAR_SIMULATION_MODE=true")
    defer gw.Stop()

    // Send velocity command via gRPC
    conn := connectGRPC(t, "localhost:50051")
    client := starv1.NewMotorControlServiceClient(conn)

    resp, err := client.SetVelocity(ctx, &starv1.SetVelocityRequest{
        Header: &starv1.RequestHeader{},
        FrontLeftMps:  0.5,
        FrontRightMps: 0.5,
        BackLeftMps:   0.5,
        BackRightMps:  0.5,
    })

    require.NoError(t, err)
    assert.Equal(t, starv1.StatusCode_OK, resp.Header.StatusCode)

    // Verify virtual_rx72n received the command
    receivedCmd := sim.WaitForCommand(t, 500*time.Millisecond)
    assert.InDelta(t, 0.5, receivedCmd.FrontLeftMps, 0.001)
}

func TestTransportFailover(t *testing.T) {
    // Test USB → SPI failover
    // Start gateway with USB transport
    // Disconnect USB (simulate disconnect)
    // Verify gateway switches to SPI
    // Verify no commands are lost during transition
}
```

---

## Gap 4: Motor Dynamics in Virtual RX72N (HIGH for testing)

### Problem
The virtual RX72N simulator returns static telemetry regardless of commands. Meaningful testing of odometry, PID control, and motion planning requires simulated motor response.

### Motor Transfer Function
From MATLAB system identification:
```
G(s) = 3.665 / (0.075s + 1)

Discrete: (at 100 Hz)
y[k] = (1 - e^(-T/τ)) * K * u[k] + e^(-T/τ) * y[k-1]
     where T=0.01, τ=0.075, K=3.665
     = 0.1248 * K * u[k] + 0.8752 * y[k-1]
```

### Go Implementation

```go
// star-gateway/cmd/virtual_rx72n/motor_sim.go

type MotorSimulator struct {
    // First-order transfer function state
    K   float64  // DC gain (3.665 rad/s/V)
    tau float64  // Time constant (0.075 s)
    T   float64  // Sample period (0.01 s = 100 Hz)

    // State
    velocity float64  // Current velocity (rad/s)
    position float64  // Accumulated encoder ticks
    mu       sync.Mutex
}

func NewMotorSimulator() *MotorSimulator {
    return &MotorSimulator{K: 3.665, tau: 0.075, T: 0.01}
}

func (m *MotorSimulator) Update(pwmDuty float64) {
    m.mu.Lock()
    defer m.mu.Unlock()

    // First-order difference equation (backward Euler):
    // v[k] = (1 - exp(-T/tau)) * K * u[k] + exp(-T/tau) * v[k-1]
    alpha := math.Exp(-m.T / m.tau)
    m.velocity = (1-alpha)*m.K*pwmDuty + alpha*m.velocity

    // Integrate velocity to position
    m.position += m.velocity * m.T
}

func (m *MotorSimulator) EncoderTicks() int64 {
    m.mu.Lock()
    defer m.mu.Unlock()
    // 341.2 PPR × 4 (quadrature) = 1364.8 ticks/rev
    // velocity in rad/s → ticks
    const ticksPerRev = 1364
    const revsPerTick = 1.0 / ticksPerRev
    return int64(m.position / (2 * math.Pi) * ticksPerRev)
}
```

### Integration in virtual_rx72n

```go
// main.go additions:

type Simulator struct {
    motors [4]*MotorSimulator
    // ...
}

func (s *Simulator) handleVelocityCommand(cmd *starv1wire.VelocityCommand) {
    // Convert m/s to motor duty cycle (via inverse kinematics)
    wheelRadius := 0.0325  // meters
    leftPwm  := cmd.FrontLeftMps / (wheelRadius * s.motors[0].K)
    rightPwm := cmd.FrontRightMps / (wheelRadius * s.motors[1].K)

    // Update motor simulators at 100 Hz
    s.motors[0].Update(leftPwm)
    s.motors[1].Update(rightPwm)
    // ...
}

func (s *Simulator) buildTelemetryResponse() *starv1.TelemetryData {
    return &starv1.TelemetryData{
        EncoderStatus: []*starv1.EncoderStatus{
            {MotorId: 0, Ticks: s.motors[0].EncoderTicks(),
             VelocityRadps: float32(s.motors[0].velocity)},
            // ...
        },
    }
}
```

---

## Gap 5: ROS2 Safety Monitor Tests (MEDIUM)

Three tests are `GTEST_SKIP` (see `plans/03_ros2_gaps.md` for implementations):
- `BatterySafetyChecks`
- `EmergencyStopTrigger`
- `DiagnosticPublishing`

Implement these tests to complete safety monitor coverage.

---

## Testing Priority Matrix

| Test | Safety Critical | CI Automated | Effort |
|------|----------------|--------------|--------|
| UI EmergencyStop tests | ✅ Yes | ❌ No | 3 hrs |
| Firmware WireMessage dispatch tests | ✅ Yes | ❌ No | 3 hrs |
| ROS2 safety monitor tests | ✅ Yes | ❌ No | 4 hrs |
| E2E estop latency test | ✅ Yes | ❌ No | 4 hrs |
| Motor dynamics simulation | No | ❌ No | 1-2 days |
| Gateway HIL test enhancements | No | ⚠️ Partial | 4-8 hrs |
| Transport failover E2E | No | ❌ No | 4-8 hrs |
| UI component tests | No | ❌ No | 10-15 hrs |
| Hardware validation suite | No | ❌ No (HW req'd) | 2-3 days |

---

## Test Infrastructure Requirements

### GitHub Actions Self-Hosted Runner (for Hardware Tests)

To run hardware-in-loop tests in CI, a self-hosted runner connected to RPi5 is needed:

```yaml
# .github/workflows/hil.yml
jobs:
  hardware-tests:
    runs-on: [self-hosted, rpi5]  # Needs physical RPi5 runner
    steps:
      - run: cd star-ros2 && colcon test
      - run: python3 tests/e2e/test_motor_commands.py
```

This requires a dedicated RPi5 connected to a GitHub runner agent. Not feasible for immediate CI but valuable for pre-release validation.

### Local Test Runner Script

```bash
#!/bin/bash
# scripts/run_all_tests.sh

set -e

echo "=== Running Firmware Unit Tests ==="
cd e2-studio-star-rx72n-firmware
./tests/run_tests.sh

echo "=== Running Gateway Tests ==="
cd ../star-gateway
go test ./... -count=1

echo "=== Running Proto Tests ==="
cd ../star-proto
cd tests/go && go test ./...
cd ../typescript && npm test

echo "=== Running ROS2 Tests ==="
cd /workspaces/STAR/star-ros2
colcon test --return-code-on-test-failure

echo "=== Running UI Tests ==="
cd /workspaces/STAR/star-ui
npm test -- --run

echo "All tests passed!"
```
