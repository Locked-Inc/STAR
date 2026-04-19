# STAR - Spatial Topography Accessibility Robot

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ROS2](https://img.shields.io/badge/ROS2-Jazzy-22314E)](https://docs.ros.org/en/jazzy/)
[![ADA 2010](https://img.shields.io/badge/ADA-2010%20Standards-500000)](https://www.ada.gov/law-and-regs/design-standards/2010-stds/)

Distributed autonomous ground-robotics platform for continuous indoor ADA
compliance measurement.

Built by **Team Locked Inc.** for the Texas A&M ESET senior capstone,
Spring 2026.

---

## Why

- **~73%** of U.S. commercial buildings fail at least one ADA standard
  (industry estimate, Building Principles).
- **8,800** ADA Title III federal lawsuits filed in 2024 (+7% YoY,
  Seyfarth Shaw).
- **$118,225 / $236,451** DOJ civil penalty maximums (90 FR 29445,
  July 2025).
- **$800 to $50,000+** per manual CASp audit, 2-5 days on-site.
- **~70 million** U.S. adults live with a disability (CDC 2022 BRFSS).

The Americans with Disabilities Act has been federal law for 35 years.
The bottleneck is not the law; it is measurement. STAR provides the
measurement layer.

---

## What STAR is

A distributed robot with real-time and high-level compute split across
two boards:

### Raspberry Pi 5 (high-level, ROS2 Jazzy)

- **slam_toolbox async** mapping
- **robot_localization** EKF pose fusion
- **Nav2** autonomous navigation (NavFn A* + DWB + cost maps)
- **m-explore-ros2** frontier exploration
- **Go gateway** with gRPC-Web + WebSocket + HTTP
- **React + TypeScript UI** with 20 telemetry panels
- **Compliance engine** (Python + Open3D + reportlab)

### Custom RX72N PCB (real-time, ThreadX RTOS)

- **250 Hz discrete-time PID** on four DFRobot FIT0520 gearmotors via
  DRV8263H H-bridges
- **GPTW 32-bit PWM @ 20 kHz**, **MTU/TPU** quadrature encoders
- **BNO055 9-DoF IMU** I2C (chassis-authoritative pitch)
- **4 x HC-SR04** ultrasonic safety rangefinders
- **DS18B20** 1-Wire temperature, **BMP280** pressure
- **SPI 10 Mbps** to Pi 5 with HARQ Chase Combining + convolutional FEC
  (K=7, rate-1/2) + CRC-32 framing + **nanopb** Protocol Buffers

### Sensors on the Pi 5 side

- **SLAMTEC RPLiDAR C1** (360 deg, 10 Hz, 12 m, +/- 3 cm) - USB
- **Waveshare IMX219-83** stereo camera (dual Sony IMX219 8 MP,
  60 mm baseline, onboard **ICM20948** 9-DoF IMU) - dual MIPI CSI-2

Total BOM under $2,000.

---

## ADA compliance engine

Seven ADA 2010 checks specified; implementation status honestly labeled:

| Check | ADA section | Status |
|---|---|---|
| Ramp slope > 1:12 (4.76 deg) | 405.2 | **IMPLEMENTED** |
| Trip hazard > 0.25 in | 303 | STRETCH |
| Accessible path width < 36 in | 403.5 | STRETCH |
| Ramp width < 36 in | 405.5 | architected |
| Ramp landing < 60 x 60 in | 405.7 | architected |
| Door clear width < 32 in | 404.2.3 | architected |
| Door threshold > 0.5 in | 404.2.5 | architected |

The ramp-slope check uses RANSAC plane fitting on LiDAR returns
transformed into `base_link`, cross-validated against BNO055 pitch with
a +/- 0.5-degree agreement gate. Measurements are written to a CSV
validation log and compiled into a PDF audit report with georeferenced
violation pins at scan completion.

Trip hazard and path width are in-progress stretch implementations. The
remaining four are fully architected against the wired sensors with
module stubs in `compliance-engine/star_compliance/nodes/`.

---

## Validation

### Platform

- **143 / 143 ROS2 tests passing** across four packages.
- slam_toolbox produces `/map` at 0.5 Hz on RPi5 with full
  `map -> odom -> base_link -> laser_frame` TF chain.
- Nav2 executes autonomous point-to-point goals.
- m-explore-ros2 drives frontier exploration to completion.
- SPI transport diagnostic: 1.6% utilization at 10 Mbps.

### Ramp-slope compliance check

- Ground truth: **Wixey WR300** digital angle gauge (+/- 0.1 deg).
- Cross-validation between BNO055 pitch and LiDAR plane normal, gate
  set to +/- 0.5 deg.
- Results logged to `extras/validation_log.csv` and summarized in
  `extras/STAR_ValidationLog.xlsx`.

We report whatever n we actually capture, not imagined numbers.

---

## Repository layout

```
star-ros2/              ROS2 Jazzy packages (star_bringup, star_spi_bridge,
                        star_gateway_bridge, star_safety_monitor, sllidar_ros2)
star-rx72n-firmware/    Real-time firmware (C23, ThreadX, GPTW, MTU/TPU)
star-gateway/           Go service (gRPC-Web + WebSocket + HTTP, HARQ/FEC link)
star-ui/                React 19.2 + TypeScript + Vite + Zustand dashboard
star-proto/             Protocol Buffer schemas (star.v1.*, nanopb-compatible)
compliance-engine/      NEW: Python ADA compliance engine (this capstone)
schematic/              KiCad PCB designs for the custom RX72N board
docs/                   LaTeX technical documentation
matlab/                 Motor system-id and PID design
```

---

## Quickstart

```bash
git clone https://github.com/Locked-Inc/STAR.git
cd STAR

# ROS2 workspace
cd star-ros2
colcon build --symlink-install
source install/setup.bash

# Launch the stack
ros2 launch star_bringup star.launch.py

# Run autonomous exploration
ros2 launch star_bringup explore.launch.py

# Run the compliance engine alongside the exploration
ros2 launch star_compliance compliance.launch.py
# When the scan ends, star_audit_report_<timestamp>.pdf appears in the
# current directory.
```

Simulation (no hardware): `ros2 launch star_bringup gazebo_demo.launch.py`.

---

## Team

**Locked Inc.** - TAMU ESET Senior Capstone, Spring 2026.

- Team Lead - research, system integration, presentation
- Member 2 - motor control, electrical, PCB
- Hardware Lead - sensor integration, mechanical, validation
- Software Lead - ROS2, compliance engine, SLAM

Faculty advisor: (TBD)
Sponsor contacts under discussion:
- TAMU Disability Resources (user advocacy)
- TAMU Office of Risk, Ethics & Compliance (ADA program)
- TAMU Facilities Services / Office of the University Architect
  (remediation execution)

---

## References

Full BibTeX in `final_capstone/extras/references.bib`. Full
source-verification matrix (every number traced to a primary source) in
`final_capstone/research/source_verification.md`.

Key citations:

- Seyfarth Shaw, *ADA Title III Federal Lawsuit Numbers Rebound to 8,800 in 2024*, March 2025.
- Federal Register **90 FR 29445** (July 3, 2025) - DOJ civil penalty
  adjustment.
- CDC, *Disability and Health Data System*, 2022 BRFSS (released July 2024).
- Okoro et al., MMWR 2018; 67(32):882-887.
- Saha et al., *Project Sidewalk*, CHI 2019 (Best Paper).
- Weld et al., *Deep Learning for Automatically Detecting Sidewalk Accessibility Problems*, ASSETS 2019 (Best Student Paper).
- Hara et al., *Tohme*, UIST 2014.
- Sun et al., *Stereo and LiDAR Loosely Coupled SLAM Constrained Ground Detection*, Sensors 2024, 24(21):6828.
- Lang et al., *Gaussian-LIC*, arXiv:2404.06926, ICRA 2025.
- Turkan & Che, *Automated ADA Curb-Ramp Assessment with Mobile Lidar*, Oregon State PacTrans, 2020-2022.
- ADA 2010 Standards for Accessible Design, sections 303, 403.5, 404.2, 405.

---

## License

MIT. See `LICENSE`. Accessibility is a civil right; this tool should
not be gated behind licensing rent.

---

## Pilot partnerships

We are actively seeking pilot building partners on the Texas A&M College
Station campus. If you own or operate an institutional facility and want
to evaluate STAR in your space, email the Locked Inc. team or open an
issue tagged `pilot-partnership`.
