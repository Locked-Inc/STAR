# MATLAB PID Design for STAR Robot

This folder contains MATLAB scripts for PID controller design and motor system modeling for the STAR autonomous mapping robot.

## Quick Start

```matlab
% 1. Load motor parameters
motor_params

% 2. Create motor model
motor_model_1st_order

% 3. Design PID controller
pid_design_velocity

% 4. Discretize for ESP32
pid_discretize

% 5. Simulate closed-loop response
simulate_step_response
```

## Motor Specifications

Based on **DFRobot FIT0520** DC Gear Motor ([Product Page](https://www.dfrobot.com/product-1617.html)):

| Parameter | Value | Unit |
|-----------|-------|------|
| Rated Voltage | 6 | V |
| No-load Speed | 210 @ 0.13A | RPM |
| Stall Current | 3.2 | A |
| Stall Torque | 10 | kg·cm |
| Gear Ratio | 34.02:1 | - |
| Encoder PPR | 11 × 34.02 = 341.2 | counts/rev |
| Max Efficiency | 2.0 kg·cm @ 170 RPM @ 0.6A | - |
| Max Power | 5.2 kg·cm @ 110 RPM @ 1.1A | - |

### Derived Parameters

| Parameter | Value | Formula |
|-----------|-------|---------|
| Armature Resistance | 1.875 Ω | V_rated / I_stall |
| Torque Constant (Kt) | 0.307 N·m/A | T_stall / I_stall |
| Back-EMF Constant (Ke) | 0.262 V/(rad/s) | (V - I·R) / ω |
| No-load ω | 22.0 rad/s | 210 × 2π/60 |

## Script Descriptions

### Core Scripts

| Script | Description |
|--------|-------------|
| `motor_params.m` | Motor parameter definitions extracted from FTP |
| `motor_model_1st_order.m` | First-order transfer function model |
| `motor_model_2nd_order.m` | Full second-order model with electrical dynamics |
| `pid_design_velocity.m` | Automatic PID tuning for velocity control |
| `pid_discretize.m` | Discretization for 100 Hz ESP32 control loop |
| `simulate_step_response.m` | Closed-loop simulation and analysis |
| `cascaded_pid_design.m` | Cascaded control (current→velocity→position) |
| `system_id_template.m` | System identification from encoder data |

### Workflow

```
┌─────────────────┐
│  motor_params   │  ← Motor specifications from FTP
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ motor_model_*   │  ← Create transfer function model
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ pid_design_*    │  ← Design PID controller
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ pid_discretize  │  ← Convert to discrete for ESP32
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  simulate_*     │  ← Validate performance
└─────────────────┘
```

## ESP32 Integration

The MATLAB scripts output gains compatible with the ESP32 `star_pid` library:

```c
// Velocity PID (from pid_design_velocity.m @ 20 rad/s bandwidth)
// Transfer function: G(s) = 3.665 / (0.075s + 1)  [tau = 75ms from design doc]
star_pid_config_t velocity_pid_config = {
    .kp = 0.285647f,
    .ki = 8.008388f,
    .kd = 0.000000f,
    .output_min = -100.0f,   // -100% duty cycle
    .output_max = 100.0f,    // +100% duty cycle
    .integral_min = -50.0f,  // Anti-windup
    .integral_max = 50.0f,   // Anti-windup
};
star_pid_init(&velocity_pid, &velocity_pid_config);
```

### Velocity Loop Performance (Simulated)

| Metric | Value |
|--------|-------|
| Rise Time | 70 ms |
| Settling Time | 260 ms |
| Overshoot | 9.7% |
| Phase Margin | 69.2° |

### Control Loop Rate

- **ESP32 PID**: 100 Hz (Ts = 10 ms)
- **Encoder Feedback**: 48 counts per sample (expected at max speed)
- **Nav2 Commands**: 10 Hz position setpoints

## System Identification

To improve accuracy, collect step response data from the real motor:

1. Run step test on ESP32
2. Log encoder counts at 100 Hz
3. Import to MATLAB
4. Run `system_id_template.m`
5. Update `motor_params.m` with identified parameters

```matlab
% After collecting motor_step_data.csv:
system_id_template

% This will:
% - Fit transfer function to data
% - Validate model accuracy
% - Generate tuned PID gains
```

## Control Architecture

The STAR robot uses cascaded PID control:

```
Position Loop (10 Hz) ─┐
                       │
                       ▼
               ┌───────────────┐
Nav2 ──────────► Position PID  ├──────► Velocity Setpoint
               └───────────────┘
                       │
                       ▼
               ┌───────────────┐
Velocity ◄─────┤ Velocity PID  ├──────► Current Setpoint
Encoder        └───────────────┘
                       │
                       ▼
               ┌───────────────┐
Current ◄──────┤ Current PID   ├──────► PWM Duty Cycle
Sense          └───────────────┘
                       │
                       ▼
                   DRV8243
                   Motor Driver
```

Use `cascaded_pid_design.m` for full cascade tuning.

## Reference Documents

| Document | Location | Content |
|----------|----------|---------|
| FTP Electrical | `Locked_Inc_FTP/sections/15_implementation_electrical.tex` | Motor/driver specs |
| FTP Embedded | `Locked_Inc_FTP/sections/16_implementation_embedded.tex` | Control architecture |
| Control Primer | `docs/star_control_systems_primer.tex` | PID theory, initial gains |
| PID Theory | `star-esp32-firmware/docs/PID_THEORY.tex` | Discrete implementation |
| star_pid Library | `star-esp32-firmware/lib/star_pid/` | ESP32 PID source code |

## Computed PID Gains (from MATLAB)

**Model:** G(s) = 3.665 / (0.075s + 1) with τ = 75ms from design doc

### Velocity-Only Control (Simpler)
| Gain | Value |
|------|-------|
| Kp | 0.2856 |
| Ki | 8.0084 |
| Kd | 0.0 |

### Cascaded Control (from cascaded_pid_design.m)
| Loop | Kp | Ki | Kd | Bandwidth |
|------|----|----|----|-----------|
| Current (inner) | 0.0 | 375.5 | 0.0 | 200 rad/s |
| Velocity (middle) | 0.118 | 0.552 | 0.001 | 20 rad/s |
| Position (outer) | 1.94 | 0.72 | 0.18 | 2 rad/s |

**Note**: Time constant τ = 75ms from `docs/Protobuf_Protocol_Design_Analysis.tex`. Run `system_id_template.m` with real encoder data to refine.

## Requirements

- MATLAB R2020a or later
- Control System Toolbox
- System Identification Toolbox (for `system_id_template.m`)

## Troubleshooting

### Model doesn't match real motor
- Run `system_id_template.m` with actual encoder data
- Update `tau_est` in `motor_model_1st_order.m`

### Excessive overshoot
- Reduce target bandwidth in `pid_design_velocity.m`
- Increase derivative gain (Kd)

### Slow response
- Increase target bandwidth
- Verify encoder counts/sample is correct (48 expected)

### Oscillation
- Check bandwidth separation in `cascaded_pid_design.m`
- Inner loop should be 5-10x faster than outer loop

## Authors

STAR Project Team - Texas A&M University
ESET 419/420 Senior Design - Fall 2025
