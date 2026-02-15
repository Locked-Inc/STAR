---
name: pid-tune
description: Motor PID controller tuning workflow using MATLAB system identification and firmware integration
disable-model-invocation: false
argument-hint: "[motor-id] (optional)"
---

# PID Tuning Skill

Complete workflow for tuning motor PID controllers using MATLAB system identification and integrating gains into RX72N firmware.

## Overview

The STAR project uses a first-order transfer function model to design discrete-time PID controllers for motor velocity control. This workflow combines experimental step response data, MATLAB system identification, and firmware implementation.

## PID Tuning Workflow

### Step 1: Measure Motor Step Response

Collect experimental data from motor:
1. Apply step input (e.g., 50% PWM duty cycle)
2. Record motor velocity over time
3. Measure time constant (τ) - time to reach 63.2% of final value
4. Estimate DC gain (K) - steady-state output / input

**Example**: For STAR motors:
- τ ≈ 75ms (time constant)
- K ≈ 3.665 RPM/% duty cycle

### Step 2: Run MATLAB System Identification

Execute MATLAB scripts in sequence:

```matlab
% 1. Estimate first-order transfer function from step response
motor_model_1st_order

% 2. Design PID controller using pole placement or frequency domain methods
pid_design_velocity

% 3. Generate discrete coefficients for 100 Hz control loop
pid_discretize
```

**Output**: Discrete PID gains for firmware (Kp, Ki, Kd)

### Step 3: Update RX72N Firmware

Integrate new PID gains into firmware:

```c
// In rx_pid_config.h or motor control initialization
const rx_pid_config_t motor_pid_config = {
    .kp = 0.286f,          // From MATLAB pid_design_velocity
    .ki = 8.01f,           // From MATLAB pid_discretize
    .kd = 0.0f,            // Typically 0 for velocity control
    .output_min = -100.0f, // PWM duty cycle limits
    .output_max = 100.0f,
    .integral_min = -50.0f, // Anti-windup limits
    .integral_max = 50.0f,
};
```

### Step 4: Test Closed-Loop Performance

Validate controller on hardware:
1. Upload firmware with new gains
2. Command step velocity input (e.g., 0 → 100 RPM)
3. Monitor actual velocity response
4. Check for:
   - Settling time (< 500ms target)
   - Overshoot (< 10% target)
   - Steady-state error (< 1% target)
   - Stability (no oscillations)

### Step 5: Iterate if Needed

If performance is unsatisfactory:
- **Too slow / high steady-state error**: Increase Kp and/or Ki
- **Overshoot / oscillations**: Decrease Kp, add derivative term (Kd > 0)
- **Instability**: Reduce all gains by 50%, re-tune conservatively

Return to Step 2 with adjusted design criteria in MATLAB.

## Motor Model (STAR System)

**Transfer Function** (continuous-time):
```
G(s) = K / (τs + 1)
     = 3.665 / (0.075s + 1)
```

Where:
- K = 3.665 (DC gain, RPM per % duty cycle)
- τ = 75ms (time constant)

**Discretization**: Backward Euler at 100 Hz (Ts = 0.01s)

## MATLAB Scripts Reference

### `motor_model_1st_order.m`

**Purpose**: Estimate transfer function from step response data

**Inputs**:
- Time vector (seconds)
- Velocity response (RPM)
- Input step magnitude (% duty cycle)

**Outputs**:
- Time constant τ
- DC gain K
- Transfer function model G(s)

**Usage**:
```matlab
% Load experimental data
load('motor_step_response.mat');  % Contains time, velocity, input

% Run system identification
motor_model_1st_order

% View Bode plot and step response fit
```

### `pid_design_velocity.m`

**Purpose**: Design PID controller using pole placement

**Inputs**:
- Motor transfer function G(s) from previous step
- Desired closed-loop bandwidth (rad/s)
- Phase margin target (degrees)

**Outputs**:
- Continuous-time PID gains (Kp_c, Ki_c, Kd_c)
- Closed-loop transfer function
- Bode plot with margins

**Tuning Parameters**:
```matlab
% Adjust these for desired performance
desired_bandwidth = 20;  % rad/s (higher = faster response)
phase_margin = 60;       % degrees (higher = more stable)
```

### `pid_discretize.m`

**Purpose**: Convert continuous PID to discrete form for firmware

**Inputs**:
- Continuous-time PID gains (Kp_c, Ki_c, Kd_c)
- Sampling time Ts = 0.01s (100 Hz control loop)

**Outputs**:
- Discrete PID gains (Kp, Ki, Kd) for firmware
- Z-domain transfer function
- Step response comparison (continuous vs discrete)

**Discretization Methods**:
- Proportional: Kp = Kp_c (no change)
- Integral: Ki = Kp_c * Ts (backward Euler)
- Derivative: Kd = Kp_c / Ts (backward difference)

## Example Workflow

```bash
# 1. Collect step response data on hardware
# (Upload data logging firmware, apply step input, save CSV)

# 2. Process in MATLAB
cd matlab/
matlab -batch "motor_model_1st_order; pid_design_velocity; pid_discretize"

# 3. Copy gains from MATLAB output
# Kp = 0.286
# Ki = 8.01
# Kd = 0.0

# 4. Update firmware
# Edit e2-studio-star-rx72n-firmware/src/motor_control_config.c

# 5. Build and upload
cd ../e2-studio-star-rx72n-firmware
# (Build in e² Studio or use CMake)

# 6. Test closed-loop response
# (Monitor via USB serial, plot step response)
```

## Troubleshooting

**Problem**: MATLAB can't find transfer function
- **Solution**: Check step response data quality (smooth, monotonic rise to steady-state)
- Ensure sufficient sampling rate (> 10× motor bandwidth)

**Problem**: Closed-loop is unstable (oscillations)
- **Solution**: Reduce all gains by 50%, verify motor model accuracy
- Check for sensor noise, mechanical resonances

**Problem**: Slow response or steady-state error
- **Solution**: Increase Ki for better tracking
- Verify motor isn't saturating (100% PWM duty)

**Problem**: Gains too aggressive for real hardware
- **Solution**: MATLAB assumes ideal motor model
- Add safety factor (multiply gains by 0.7) for first hardware test

## Integration with RX72N Firmware

**PID Controller Module**: `e2-studio-star-rx72n-firmware/src/rx_pid.c`

**Key Functions**:
- `rx_pid_init(pid_handle, &config)` - Initialize with MATLAB gains
- `rx_pid_compute(pid_handle, setpoint, measured, dt, &output)` - 100 Hz control loop
- `rx_pid_reset(pid_handle)` - Clear integral state (on mode changes)

**Control Loop** (runs at 100 Hz):
```c
float velocity_setpoint_rpm = 100.0f;  // Commanded velocity
float velocity_measured_rpm = encoder_read_velocity();
float pwm_output;

rx_pid_compute(&motor_pid, velocity_setpoint_rpm, velocity_measured_rpm,
               0.01f, &pwm_output);  // dt = 10ms

motor_set_pwm(pwm_output);  // Apply to motor driver
```

## Performance Targets

| Metric | Target | Current |
|--------|--------|---------|
| Settling time (2% band) | < 500ms | ~400ms |
| Overshoot | < 10% | ~5% |
| Steady-state error | < 1% | ~0.5% |
| Control frequency | 100 Hz | 100 Hz |
| Bandwidth (-3dB) | 20 rad/s | 18 rad/s |

## Related Files

- `matlab/motor_model_1st_order.m` - System identification
- `matlab/pid_design_velocity.m` - Controller design
- `matlab/pid_discretize.m` - Discrete implementation
- `e2-studio-star-rx72n-firmware/src/rx_pid.c` - PID implementation
- `e2-studio-star-rx72n-firmware/src/rx_pid.h` - PID API
- `e2-studio-star-rx72n-firmware/src/motor_control_task.c` - Control loop integration
