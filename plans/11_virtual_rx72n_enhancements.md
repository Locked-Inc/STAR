# Virtual RX72N Simulator Enhancements

## Current State

The virtual RX72N simulator at `star-gateway/cmd/virtual_rx72n/` is a well-implemented Go-based TCP/Unix socket server. It handles the full frame protocol and provides basic telemetry simulation.

**What Works:**
- ✅ Full SPI frame protocol (SYNC+SEQ+LEN+TYPE+FLAGS+PAYLOAD+CRC-32)
- ✅ PING/PONG heartbeat with counter echo
- ✅ RESET/RESET_ACK session synchronization
- ✅ Protobuf command processing (VelocityCommand, EmergencyStop)
- ✅ Simulated TelemetryData generation (static values)
- ✅ Sequence number tracking with wraparound at 0xFFFF
- ✅ ACK frame generation for frames with FlagRequiresAck
- ✅ Configurable latency injection (SIM_LATENCY_MS env var)
- ✅ Graceful shutdown with socket cleanup
- ✅ 432 lines of test coverage (control frames)

**What's Missing:**
- ❌ Motor dynamics simulation (G(s) = 3.665/(0.075s+1))
- ❌ Encoder feedback based on commanded velocity
- ❌ Fault injection (CRC errors, packet loss, latency spikes)
- ❌ BMS simulation (SOC discharge, voltage curves)
- ❌ Temperature simulation (DS18B20 ambient + load heating)
- ❌ Multi-client support (currently single connection)
- ❌ Configuration message handling (PidConfig, RetransmitConfig)

---

## Enhancement 1: Motor Dynamics Simulation (HIGH)

### Why Critical

Without motor dynamics, the simulator echoes static telemetry regardless of commands. Testing odometry, PID control, and motion planning requires simulated responses to velocity commands.

### Motor Model

From MATLAB system identification (`matlab/motor_model_1st_order.m`):
```
G(s) = K / (τs + 1)
where: K = 3.665 (rad/s per volt), τ = 0.075 s
```

Discrete-time (backward Euler at 100 Hz):
```
v[k] = (1 - e^(-T/τ)) · K · u[k] + e^(-T/τ) · v[k-1]
     = 0.1248 · K · u[k] + 0.8752 · v[k-1]
```

### File: `star-gateway/cmd/virtual_rx72n/motor_sim.go`

```go
package main

import (
    "math"
    "sync"
    "sync/atomic"
)

const (
    // Motor model parameters (from MATLAB system ID)
    motorGain         = 3.665  // DC gain: rad/s per volt
    motorTimeConstant = 0.075  // Time constant: seconds
    motorSamplePeriod = 0.010  // Control rate: 100 Hz

    // Wheel parameters
    wheelRadiusM    = 0.0325  // Wheel radius in meters
    wheelbaseM      = 0.150   // Wheelbase in meters
    encoderPPR      = 341     // Pulses per revolution
    encoderLinesMul = 4       // Quadrature multiplication
    ticksPerRev     = encoderPPR * encoderLinesMul // = 1364

    numMotors = 4
)

// MotorSim simulates a single brushed DC motor with encoder feedback.
type MotorSim struct {
    // Transfer function state
    velocityRadps float64 // Current angular velocity (rad/s)
    positionRad   float64 // Accumulated angle (rad)

    // Computed encoder ticks
    encoderTicks atomic.Int64

    mu sync.Mutex
}

// Update advances the motor simulation by one sample period.
// pwmDuty is normalized [-1.0, 1.0].
func (m *MotorSim) Update(pwmDuty float64) {
    m.mu.Lock()
    defer m.mu.Unlock()

    // Clamp duty cycle
    if pwmDuty > 1.0 {
        pwmDuty = 1.0
    } else if pwmDuty < -1.0 {
        pwmDuty = -1.0
    }

    // First-order discrete-time system:
    alpha := math.Exp(-motorSamplePeriod / motorTimeConstant)
    inputVoltage := pwmDuty * 12.0 // Assume 12V supply
    m.velocityRadps = (1-alpha)*motorGain*inputVoltage +
        alpha*m.velocityRadps

    // Integrate to get position
    m.positionRad += m.velocityRadps * motorSamplePeriod

    // Update encoder ticks
    ticks := int64(m.positionRad / (2 * math.Pi) * ticksPerRev)
    m.encoderTicks.Store(ticks)
}

// VelocityMps converts angular velocity to linear wheel speed.
func (m *MotorSim) VelocityMps() float32 {
    m.mu.Lock()
    defer m.mu.Unlock()
    return float32(m.velocityRadps * wheelRadiusM)
}

// EncoderTicks returns the cumulative encoder tick count.
func (m *MotorSim) EncoderTicks() int64 {
    return m.encoderTicks.Load()
}

// RobotSim simulates the complete 4-motor differential drive robot.
type RobotSim struct {
    motors [numMotors]*MotorSim

    // Robot pose (simulated dead-reckoning)
    x, y, yaw float64
    mu        sync.Mutex
}

func NewRobotSim() *RobotSim {
    r := &RobotSim{}
    for i := range r.motors {
        r.motors[i] = &MotorSim{}
    }
    return r
}

// HandleVelocityCommand converts m/s velocity command to per-motor duty cycles.
func (r *RobotSim) HandleVelocityCommand(
    frontLeftMps, frontRightMps, backLeftMps, backRightMps float64) {

    // Convert linear velocity to normalized duty cycle
    // v_wheel = omega * r → omega = v / r
    // duty = omega / (K * V_supply)
    rps := func(mps float64) float64 {
        return mps / wheelRadiusM
    }
    duty := func(radps float64) float64 {
        return radps / (motorGain * 12.0)
    }

    r.motors[0].Update(duty(rps(frontLeftMps)))
    r.motors[1].Update(duty(rps(frontRightMps)))
    r.motors[2].Update(duty(rps(backLeftMps)))
    r.motors[3].Update(duty(rps(backRightMps)))

    // Update robot pose via differential drive kinematics
    r.updatePose()
}

func (r *RobotSim) updatePose() {
    r.mu.Lock()
    defer r.mu.Unlock()

    // Average left/right for differential drive
    leftMps := (r.motors[0].VelocityMps() + float32(r.motors[2].velocityRadps*wheelRadiusM)) / 2
    rightMps := (r.motors[1].VelocityMps() + float32(r.motors[3].velocityRadps*wheelRadiusM)) / 2

    linear := float64(leftMps+rightMps) / 2
    angular := float64(rightMps-leftMps) / wheelbaseM

    // Dead-reckoning update
    r.x += linear * math.Cos(r.yaw) * motorSamplePeriod
    r.y += linear * math.Sin(r.yaw) * motorSamplePeriod
    r.yaw += angular * motorSamplePeriod
}
```

### Integration in main.go

```go
// Replace static telemetry generation:
func (s *Simulator) buildTelemetryData() *starv1.TelemetryData {
    return &starv1.TelemetryData{
        EncoderStatus: []*starv1.EncoderStatus{
            {
                MotorId:       0,
                Ticks:         s.robot.motors[0].EncoderTicks(),
                VelocityRadps: s.robot.motors[0].velocityRadps,
            },
            {
                MotorId:       1,
                Ticks:         s.robot.motors[1].EncoderTicks(),
                VelocityRadps: s.robot.motors[1].velocityRadps,
            },
            {
                MotorId:       2,
                Ticks:         s.robot.motors[2].EncoderTicks(),
                VelocityRadps: s.robot.motors[2].velocityRadps,
            },
            {
                MotorId:       3,
                Ticks:         s.robot.motors[3].EncoderTicks(),
                VelocityRadps: s.robot.motors[3].velocityRadps,
            },
        },
        BatteryState: s.battery.BuildProto(),
        SystemState: &starv1.SystemStatus{
            FirmwareVersion: "sim-1.0.0",
            UptimeMs:        uint64(time.Since(s.startTime).Milliseconds()),
        },
    }
}
```

---

## Enhancement 2: Fault Injection API (HIGH for testing)

### Why Needed

Without fault injection, cannot test:
- Gateway recovery from CRC errors
- HARQ retransmission logic
- Transport failover during link instability
- RX72N session reset behavior

### API Design

```go
// FaultConfig controls which faults to inject.
type FaultConfig struct {
    // CRC error rate: 0.0 = no errors, 1.0 = all frames corrupt
    CrcErrorRate float32 `json:"crc_error_rate"`

    // Packet drop rate: 0.0 = no drops, 1.0 = all frames dropped
    PacketLossRate float32 `json:"packet_loss_rate"`

    // Extra latency spike added to random frames
    LatencySpikeDuration time.Duration `json:"latency_spike_ms"`
    LatencySpikeRate     float32       `json:"latency_spike_rate"`

    // Sequence gap injection: insert gap in sequence numbers
    SequenceGapRate uint16 `json:"sequence_gap_rate"`
    SequenceGapSize uint16 `json:"sequence_gap_size"`
}
```

### HTTP Control Endpoint

```go
// Add HTTP control server to virtual_rx72n:
func (s *Simulator) startControlServer(port int) {
    mux := http.NewServeMux()

    // Fault injection
    mux.HandleFunc("/fault", func(w http.ResponseWriter, r *http.Request) {
        if r.Method == http.MethodPost {
            var cfg FaultConfig
            json.NewDecoder(r.Body).Decode(&cfg)
            s.setFaults(cfg)
            w.WriteHeader(http.StatusOK)
        }
        if r.Method == http.MethodDelete {
            s.clearFaults()
            w.WriteHeader(http.StatusOK)
        }
    })

    // Status endpoint
    mux.HandleFunc("/status", func(w http.ResponseWriter, r *http.Request) {
        json.NewEncoder(w).Encode(s.getStatus())
    })

    http.ListenAndServe(fmt.Sprintf(":%d", port), mux)
}
```

### Usage in Tests

```go
func TestHARQRetransmission(t *testing.T) {
    sim := startVirtualRX72N(t)

    // Inject 50% packet loss
    resp, _ := http.Post(sim.ControlURL+"/fault", "application/json",
        strings.NewReader(`{"packet_loss_rate": 0.5}`))
    assert.Equal(t, 200, resp.StatusCode)

    // Send command — should succeed via HARQ retransmission
    client := connectGRPC(t)
    _, err := client.SetVelocity(ctx, &starv1.SetVelocityRequest{
        FrontLeftMps: 0.5,
    })
    require.NoError(t, err, "SetVelocity should succeed even with 50% packet loss")

    // Verify retransmissions occurred (check gateway metrics)
    metrics := getGatewayMetrics(t)
    assert.Greater(t, metrics.HARQRetransmits, 0)

    // Clear faults
    req, _ := http.NewRequest(http.MethodDelete, sim.ControlURL+"/fault", nil)
    http.DefaultClient.Do(req)
}
```

---

## Enhancement 3: BMS Simulation (MEDIUM)

### Simple Battery Model

```go
// star-gateway/cmd/virtual_rx72n/battery_sim.go

type BatterySim struct {
    // State
    chargePercent float64   // 0.0 - 1.0
    voltageV      float64   // Pack voltage
    currentA      float64   // Positive = discharge

    // Model parameters (LiPo 3S pack)
    nominalVoltageV  float64  // 11.1V (3S)
    fullVoltageV     float64  // 12.6V (4.2V per cell * 3)
    emptyVoltageV    float64  // 9.9V (3.3V per cell * 3)
    capacityAh       float64  // e.g., 5.0 Ah
    startTime        time.Time

    mu sync.Mutex
}

func (b *BatterySim) Update(motorPowerTotal float64, dt float64) {
    b.mu.Lock()
    defer b.mu.Unlock()

    // Estimate discharge current from motor power
    b.currentA = motorPowerTotal / b.voltageV

    // Simple coulomb counting
    b.chargePercent -= b.currentA * dt / (b.capacityAh * 3600)
    if b.chargePercent < 0 {
        b.chargePercent = 0
    }

    // Voltage curve (simplified linear)
    b.voltageV = b.emptyVoltageV +
        (b.fullVoltageV-b.emptyVoltageV)*b.chargePercent
}

func (b *BatterySim) BuildProto() *starv1.BatteryState {
    b.mu.Lock()
    defer b.mu.Unlock()
    return &starv1.BatteryState{
        VoltageV:       float32(b.voltageV),
        CurrentA:       float32(b.currentA),
        StateOfCharge:  float32(b.chargePercent),
        TemperatureCelsius: 25.0,  // Static for now
    }
}
```

---

## Enhancement 4: Configuration Message Handling (MEDIUM)

### What's Missing

When the Gateway sends PidConfig or RetransmitConfig, the simulator ignores them.

### Implementation

```go
func (s *Simulator) handleWireMessage(msg *starv1wire.WireMessage) []byte {
    switch p := msg.Payload.(type) {
    case *starv1wire.WireMessage_VelocityCommand:
        s.handleVelocityCommand(p.VelocityCommand)
        return s.buildAck(msg)

    case *starv1wire.WireMessage_EmergencyStopCommand:
        s.handleEmergencyStop(p.EmergencyStopCommand)
        return s.buildAck(msg)

    case *starv1wire.WireMessage_PidConfig:
        // Store PID gains for future reference
        s.pidConfig = p.PidConfig
        log.Printf("PID gains updated: Kp=%.3f Ki=%.3f Kd=%.3f",
            p.PidConfig.Kp, p.PidConfig.Ki, p.PidConfig.Kd)
        return s.buildAck(msg)

    case *starv1wire.WireMessage_RetransmitConfig:
        log.Printf("Retransmit config: MaxRetries=%d Timeout=%dms",
            p.RetransmitConfig.MaxRetransmits,
            p.RetransmitConfig.AckTimeoutMs)
        return s.buildAck(msg)

    default:
        log.Printf("Unknown WireMessage payload type: %T", msg.Payload)
        return nil
    }
}
```

---

## Enhancement Summary

| Enhancement | Priority | Effort | Unlocks |
|------------|----------|--------|---------|
| Motor dynamics simulation | HIGH | 1-2 days | Odometry testing, PID validation |
| Fault injection API | HIGH | 4-8 hrs | HARQ/FEC testing, resilience validation |
| BMS simulation | MEDIUM | 4-6 hrs | Battery depletion testing |
| Config message handling | MEDIUM | 2-3 hrs | PID tuning validation end-to-end |
| Multi-client support | LOW | 4-6 hrs | Concurrent UI connections |
| Thermal simulation | LOW | 4-6 hrs | Temperature sensor testing |

## Running the Simulator

```bash
# Build and run:
cd star-gateway
go build ./cmd/virtual_rx72n
./virtual_rx72n

# With custom settings:
SOCKET_PATH=/tmp/star_rx72n.sock \
SIM_LATENCY_MS=50 \
./virtual_rx72n

# Then run gateway in simulation mode:
STAR_SIMULATION_MODE=true \
SOCKET_PATH=/tmp/star_rx72n.sock \
./star-gateway
```
