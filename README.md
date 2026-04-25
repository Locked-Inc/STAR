# STAR

**Spatial Topography Accessibility Robot** -- a distributed robotics
platform for autonomous indoor ADA-compliance auditing.

## Overview

STAR combines custom PCB hardware, embedded firmware, control software,
ROS2 autonomy, and a browser UI in a single repo. It's a four-language
system (C, Go, C++, TypeScript) plus a Python ROS2 package that produces
ADA compliance audit reports.

## Repository at a glance

| Path                      | Role                                                |
|---------------------------|-----------------------------------------------------|
| `star-rx72n-firmware/`    | Motor + sensor controller firmware (C, RX72N, ThreadX) |
| `star-gateway/`           | Go gateway: gRPC + WebSocket bridge to ROS2 / UI    |
| `star-ros2/`              | ROS2 Jazzy workspace (autonomy + bridges, C++)      |
| `star-proto/`             | Protocol Buffer schemas (canonical, all languages)  |
| `star-ui/`                | Operator UI (TypeScript / React + Vite)             |
| `star-compliance/`        | ADA compliance engine (Python ROS2 package)         |
| `matlab/`                 | Motor system identification + PID design scripts    |
| `schematic/`              | KiCad PCB sources (STAR_MCU + TOM breakout)         |
| `final_docs/`             | Capstone deliverables (SDD, SSUM, BOM, deck, demo)  |
| `docs/`                   | Engineering docs, ADRs, bench wiring                |
| `infra/`                  | Pi 5 systemd / Caddy / cockpit infrastructure       |
| `monitoring/`             | Grafana dashboard JSON                              |
| `config/`                 | Cyclone DDS XML config                              |
| `docker/`                 | GNURX-only build container                          |
| `scripts/`                | Pi setup, ROS2 build, proto codegen, bench tooling  |

For a deep dive (per-directory contents, all 26 firmware libraries, ROS2
package roles, "where do I look if I want to..." lookup tables) see
**Section 14** of the compiled documentation
(`docs/star_documentation.pdf`) or its source at
[`docs/sections/14_repository_layout.tex`](docs/sections/14_repository_layout.tex).

## Getting started

| Goal                                          | Command                                                    |
|-----------------------------------------------|------------------------------------------------------------|
| First-time Pi 5 setup                         | `./bootstrap_pi.sh` then `sudo bash scripts/setup-pi5-host.sh` |
| Start the robot (real hardware)               | `./start.sh`                                               |
| Start the robot (simulation, dev mode)        | `./dev.sh`                                                 |
| Build firmware (from devcontainer)            | `make build-rx72n`                                         |
| Run firmware host tests                       | `make test-rx72n`                                          |
| Build the ROS2 workspace                      | `./build-ros2.sh`                                          |
| Run ROS2 tests                                | `./test-ros2.sh`                                           |
| Regenerate protobuf code                      | `make proto-gen`                                           |
| Build the documentation PDF                   | `cd docs && make`                                          |
| Flash firmware via E2 Lite                    | `scripts/flash-rx72n.sh`                                   |

## Documentation map

| Document                                               | Audience                                       |
|--------------------------------------------------------|------------------------------------------------|
| `README.md` (this file)                                | Anyone landing on the repo                     |
| `CLAUDE.md`                                            | Coding policy, terminology, NASA P10, Doxygen  |
| `docs/ARCHITECTURE.md`                                 | Pi 5 ops + supervisor + topology               |
| `docs/PI_DEPLOYMENT.md`                                | Bringing up a fresh Raspberry Pi 5             |
| `docs/bench/README.md`, `docs/bench/ad2_wiring.md`     | Bench operators (E2 Lite + AD2 + RX72N)        |
| `docs/decisions/ADR-001-arq-to-harq.md`                | The ARQ -> HARQ migration (shipped)            |
| `docs/encoder_pinout.md`                               | Quadrature encoder pin map                     |
| `docs/imx219_stereo_camera.md`                         | Stereo camera bring-up                         |
| `docs/ros2_code_review_checklist.md`                   | Reviewers of ROS2 C++                          |
| `docs/ROS2_FORMATTING.md`                              | ROS2 formatter (uncrustify) and pre-commit hook|
| `docs/sections/*.tex`                                  | Technical reference (compiled to one PDF)      |
| `docs/star_documentation.pdf`                          | Compiled engineering reference                 |
| `final_docs/`                                          | Capstone-submission deliverables               |

## Technology stack

- **Embedded:** C (C23) on Renesas RX72N + ThreadX RTOS, nanopb,
  custom HAL.
- **Wire protocol:** Protocol Buffers framed in
  `SYNC + SEQ + LEN + TYPE + FLAGS + payload + CRC-32`. Three
  transports: USB CDC (3-port composite, primary), SCI9 UART
  (fallback), SPI (ROS2 bridge). HARQ on SPI uses Chase Combining
  over a rate-1/2 K=7 convolutional code.
- **Pi 5 stack:** Go gateway, ROS2 Jazzy (Cyclone DDS), TypeScript UI,
  Python compliance engine, Foxglove bridge for the cockpit.
- **PCB:** KiCad. Two boards: STAR_MCU (production) and TOM (breakout
  variant for bench bring-up).

## License

MIT. See [LICENSE](LICENSE).

Copyright (c) 2026 Locked Inc.
