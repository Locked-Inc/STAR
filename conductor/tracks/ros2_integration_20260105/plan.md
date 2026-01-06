# Plan: ROS2 Integration Research and Base Infrastructure

## Phase 1: Research and Documentation
- [x] Task: Research and document ROS2 integration strategy in `docs/sections/10_ros2_integration.tex`.
    - [x] Detail architecture for Visual-LiDAR fusion using [RTAB-Map](https://introlab.github.io/rtabmap/).
    - [x] Define coordinate frames (tf2 tree) per REP-105.
    - [x] Specify custom node topic/service/parameter interfaces.
    - [x] Document multi-machine communication (`ROS_DOMAIN_ID`).
    - [x] Document `udev` rules requirements.
- [x] Task: Integrate section into main documentation `docs/star_documentation.tex` and compile PDF.
- [x] Task: Conductor - User Manual Verification 'Phase 1: Research and Documentation' (Protocol in workflow.md)
  - Commit: d70c9931cb5d1385a9af5d845f19ed16aedf947a

## Phase 2: Infrastructure Setup (Docker/Devcontainer)
- [x] Task: Create `Dockerfile` and `.devcontainer/devcontainer.json` at the project root.
    - [x] Configure base image with ROS2 Jazzy.
    - [x] Set up user permissions and workspace directory.
    - [x] Configure hardware passthrough (SPI, ttyUSB, video, usb).
- [x] Task: Install and configure VS Code extensions (C++, ROS, Protobuf) in `devcontainer.json`.
- [x] Task: Verify environment by building the container and running `ros2 doctor`.
  - Commit: 8a3cf027995a7dd352426f82e695bf0ea6a906b6

- [x] Task: Conductor - User Manual Verification 'Phase 2: Infrastructure Setup' (Protocol in workflow.md)
  - Commit: 8a3cf027995a7dd352426f82e695bf0ea6a906b6

## Phase 3: Package Scaffolding
- [x] Task: Create `star-ros2` directory and initialize `ament_cmake` package skeletons.
    - [x] Scaffold `star_spi_bridge`.
    - [x] Scaffold `star_gateway_bridge`.
    - [x] Scaffold `star_safety_monitor`.
- [x] Task: Verify that all packages are recognized and built correctly using `colcon build`.
- [x] Task: Conductor - User Manual Verification 'Phase 3: Package Scaffolding' (Protocol in workflow.md)
  - Commit: (Manual commit required due to shell tool error)

## Phase 4: Backlog and Finalization
- [x] Task: Create GitHub issues for the detailed implementation of each custom node.
- [x] Task: Update `tech-stack.md` with ROS2-specific details and sensor fusion strategy (RTAB-Map).
- [x] Task: Conductor - User Manual Verification 'Phase 4: Backlog and Finalization' (Protocol in workflow.md)
  - Commit: (Manual commit required due to shell tool error)
