# Copilot instructions for STAR

## Big picture
- STAR is a distributed robotics stack: UI (TypeScript) → gateway (Go) → ROS2 (C++) → SPI bridge → RX72N firmware (C) using protobuf over SPI. See [CLAUDE.md](CLAUDE.md) and [docs/sections/07_gateway_architecture.tex](docs/sections/07_gateway_architecture.tex).
- Gateway owns protocol layers 1–4 (SPI transport, framing, HARQ/FEC, protobuf) and exposes gRPC services (L5). See [star-gateway/CLAUDE.md](star-gateway/CLAUDE.md) and [star-gateway/internal](star-gateway/internal).
- ROS2 workspace targets Jazzy with RTAB-Map + robot_localization; devcontainer workflow is standard. See [star-ros2/README.md](star-ros2/README.md).
- Firmware is safety‑critical ThreadX for RX72N; no dynamic allocation and strict coding rules. See [star-rx72n-firmware/CLAUDE.md](star-rx72n-firmware/CLAUDE.md).

## Critical workflows (use exact commands)
- Firmware build/flash/debug are script-driven: [star-rx72n-firmware/build.sh](star-rx72n-firmware/build.sh), [star-rx72n-firmware/flash.sh](star-rx72n-firmware/flash.sh), [star-rx72n-firmware/debug.sh](star-rx72n-firmware/debug.sh).
- Gateway build/test: `go build ./cmd/star-gateway` and `go test ./...` from [star-gateway](star-gateway).
- ROS2 build/test in [star-ros2](star-ros2): `colcon build --symlink-install`, `colcon test`, then `colcon test-result --verbose`.
- Proto workflow in [star-proto](star-proto): `buf lint proto/`, `buf format --diff proto/`, `buf generate proto/ --template buf.gen.yaml --include-imports` (see [CLAUDE.md](CLAUDE.md)).

## Project-specific conventions
- Inclusive terminology only: Controller/Peripheral and COPI/CIPO; do not introduce legacy terms. See [CLAUDE.md](CLAUDE.md).
- No backward compatibility shims—update call sites directly. See [CLAUDE.md](CLAUDE.md).
- RX72N firmware forbids magic numbers: use enums for all integer constants, const only for floats; no malloc/free. See [CLAUDE.md](CLAUDE.md) and [star-rx72n-firmware/CLAUDE.md](star-rx72n-firmware/CLAUDE.md).
- Gateway protocol constants are centralized (e.g., sync word, payload size) in [star-gateway/internal/frame/frame.go](star-gateway/internal/frame/frame.go).
- ThreadX patterns: one task per file in [star-rx72n-firmware/src/tasks](star-rx72n-firmware/src/tasks) with `<task>_task_create()`; register access uses structs, not raw pointers (see [star-rx72n-firmware/CLAUDE.md](star-rx72n-firmware/CLAUDE.md)).

## Integration points
- Protobuf definitions live in [star-proto](star-proto) and are consumed by gateway via the `replace` directive in [star-gateway/go.mod](star-gateway/go.mod).
- SPI transport is the physical link between ROS2/gateway and RX72N; framing + HARQ/FEC are implemented in [star-gateway/internal](star-gateway/internal).
- ROS2 nodes publish/subscribe standard topics (`/cmd_vel`, `/odom`, `/scan`) per [star-ros2/README.md](star-ros2/README.md).
- Yocto image configuration for RPi5 lives in [star-yocto-config/README.md](star-yocto-config/README.md).
