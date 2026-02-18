# STAR Project: Master Gap Analysis
**Branch:** `feat/gap-analysis-and-planning`
**Date:** 2026-02-18
**Status:** Research complete, ready for implementation

---

## Executive Summary

The STAR (Simultaneous Tracking and Robotics) project has a strong foundation with excellent firmware, communication infrastructure, and Protocol Buffers definitions. However, significant gaps exist in the integration layer — particularly the "glue code" connecting all subsystems together for real robot operation.

**Overall System Completion: ~75%** *(revised up from 65% — firmware is more complete than initially assessed)*

---

## Completion Matrix

| Component | Completion | Status | Blockers |
|-----------|-----------|--------|----------|
| **RX72N Firmware Core** | 100% | ✅ Production Ready | None |
| **Firmware WireMessage Dispatch** | 100% | ✅ Complete | SetVelocity, E-Stop, PID, RetransmitConfig |
| **Firmware OTA Handler** | 0% | ❌ Not Started | Blocks firmware updates |
| **Firmware NVS Config** | 0% | ❌ Not Started | Blocks PID tuning persistence |
| **Gateway Transport/HARQ/FEC** | 100% | ✅ Production Ready | None |
| **Gateway MotorControlService** | 80% | ⚠️ Partial | E-Stop priority, SetMotorPower |
| **Gateway TelemetryService** | 90% | ⚠️ Partial | GetSystemStatus stubbed |
| **Gateway BatteryService** | 100% | ✅ Complete | None |
| **Gateway ConfigurationService** | 85% | ⚠️ Partial | SetRetransmitConfig missing |
| **Gateway FirmwareUpdateService** | 0% | ❌ Not Started | Critical for production |
| **Gateway GatewayService** | 85% | ⚠️ Partial | SetPIDGains not forwarding |
| **Proto Schemas** | 100% | ✅ Complete | None |
| **nanopb Options** | 85% | ⚠️ Partial | motor_status max_count:2 (should be 4) |
| **ROS2 star_gateway_bridge** | 100% | ✅ Complete | None |
| **ROS2 star_spi_bridge** | 85% | ⚠️ Untested | Never run on hardware |
| **ROS2 star_safety_monitor** | 40% | ⚠️ Incomplete | 3 tests skipped |
| **ROS2 star_bringup** | 5% | ❌ Empty Skeleton | No launch files |
| **ROS2 SLAM Stack** | 0% | ❌ Not Started | RTAB-Map, robot_localization |
| **UI Gamepad Control** | 100% | ✅ Working | None |
| **UI Telemetry Display** | 0% | ❌ Missing | No gRPC-Web bridge |
| **UI Battery Status** | 0% | ❌ Missing | — |
| **UI Emergency Stop** | 0% | ❌ Missing | Safety critical |
| **UI Configuration Panel** | 0% | ❌ Missing | — |
| **CI/CD Firmware** | 0% | ❌ Missing | No firmware build in CI |
| **CI/CD UI Build** | 0% | ❌ Missing | No UI build in CI |
| **CI/CD Docs** | 0% | ❌ Missing | No LaTeX build in CI |
| **Virtual RX72N Simulator** | 95% | ✅ Usable | No motor dynamics |
| **MATLAB PID Workflow** | 100% | ✅ Complete | None |
| **Hardware PCB Design** | 100% | ✅ FAB Ready | None |

---

## Critical Blockers (Must Fix Before Robot Moves)

### 1. UI Has No Telemetry Display (Priority: CRITICAL)
**What:** The UI only has gamepad control. There is no way to see robot state, battery, or sensor data.
**Impact:** Operator is completely blind to robot status. Cannot safely operate.
**Root Cause:** No gRPC-Web bridge between browser and Go gateway gRPC services.
**Effort:** 10-30 hours (bridge + UI components)
**See:** `plans/04_ui_gaps.md`

### 3. ROS2 SPI Bridge Untested (Priority: HIGH)
**What:** `star_spi_bridge` code is written but has never been run on real hardware (RPi5 + RX72N).
**Impact:** First physical robot motion requires this to work.
**Effort:** 4-8 hours on hardware
**See:** `plans/03_ros2_gaps.md`

---

## High Priority Gaps

### Gateway FirmwareUpdateService (0% Complete)
All 10 RPC methods in `star-gateway/internal/service/firmware.go` return `codes.Unimplemented`.
OTA firmware updates are completely blocked. **See:** `plans/02_gateway_gaps.md`

### Emergency Stop Priority Framing
E-Stop commands are sent through the normal HARQ queue. They should bypass queuing and go immediately.
`star-gateway/internal/service/motor_control.go:90` has a TODO for this.

### UI Emergency Stop Button
No E-Stop button exists in the UI. This is a critical safety gap.

### ROS2 Safety Monitor (40% Complete)
3 of 10 test cases are `GTEST_SKIP`. Battery safety checks and E-Stop triggering are not tested.

---

## Medium Priority Gaps

### nanopb Options Bug
`motor_control.options` has `SetVelocityResponse.motor_status: max_count:2` but firmware supports 4 motors.
**Fix:** Change to `max_count:4`

### Gateway SetPIDGains Not Forwarding
`gateway_service.go:205` has TODO: "Forward to ROS2 service when available."
PID tuning from the UI does nothing.

### SetMotorPower Not Implemented
`motor_control.go:139` returns `codes.Unimplemented`. Direct PWM control unavailable.

### SetRetransmitConfig Missing
Defined in proto, not implemented in `configuration.go`.

### BMS Telemetry Incomplete
Faults detected, but SOC/cell voltage not in telemetry stream.

### Host I2C (RIIC0) Unused
Pinout defined (P12/P13) but no firmware driver or task uses it.

---

## Low Priority / Nice to Have

### RPLiDAR C1 Integration
Listed in project overview but has zero firmware integration. Needs UART/SPI driver + task.

### Motor Dynamics Simulation
Virtual RX72N echoes static telemetry. Motor dynamics (G(s) = 3.665/(0.075s+1)) should be simulated.

### CI/CD Firmware Build
No GitHub Actions workflow builds or tests the RX72N firmware.

### CI/CD UI Build
No GitHub Actions workflow builds or tests the TypeScript UI.

### Documentation Build CI
LaTeX docs compile locally but not in CI.

### SLAM/Navigation Stack
RTAB-Map, robot_localization, Nav2 all referenced but not configured.

---

## Open Issue Branches Status

All 8 open worktree branches (issues 298-338) are effectively complete and ready to merge.
**See:** `plans/07_open_issues_status.md`

---

## Plan Files Index

| File | Coverage |
|------|----------|
| `01_firmware_gaps.md` | WireMessage dispatch, OTA, NVS config, BMS telemetry, RPLiDAR |
| `02_gateway_gaps.md` | FirmwareUpdateService, E-Stop priority, SetPIDGains, SetMotorPower |
| `03_ros2_gaps.md` | SPI bridge testing, safety monitor, bringup launch files, SLAM |
| `04_ui_gaps.md` | Telemetry display, battery, E-stop button, gRPC-Web bridge, tests |
| `05_proto_gaps.md` | nanopb options fixes, missing constraints |
| `06_ci_cd_gaps.md` | Firmware CI, UI CI, docs CI, top-level orchestration |
| `07_open_issues_status.md` | Status of all 8 open branches |
| `08_critical_path.md` | Minimum steps for first robot motion |
| `09_hardware_gaps.md` | RPLiDAR, Host I2C, BMS telemetry, simulator enhancements |
| `10_testing_gaps.md` | Integration tests, E2E tests, hardware validation suite |
| `11_virtual_rx72n_enhancements.md` | Motor dynamics, fault injection, multi-client support |
