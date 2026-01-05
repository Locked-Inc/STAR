# Plan: ROS2 Integration Research and Base Infrastructure

## Phase 1: Research and Documentation
- [ ] Task: Research and document ROS2 integration strategy in `docs/sections/10_ros2_integration.tex`.
    - [ ] Detail architecture for Visual-LiDAR fusion using [RTAB-Map](https://introlab.github.io/rtabmap/).
    - [ ] Define coordinate frames (tf2 tree) per REP-105.
    - [ ] Specify custom node topic/service/parameter interfaces.
    - [ ] Document multi-machine communication (`ROS_DOMAIN_ID`).
    - [ ] Document `udev` rules requirements.
- [ ] Task: Integrate section into main documentation `docs/star_documentation.tex` and compile PDF.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Research and Documentation' (Protocol in workflow.md)

## Phase 2: Infrastructure Setup (Docker/Devcontainer)
- [ ] Task: Create `Dockerfile` and `.devcontainer/devcontainer.json` at the project root.
    - [ ] Configure base image with ROS2 Jazzy.
    - [ ] Set up user permissions and workspace directory.
    - [ ] Configure hardware passthrough (SPI, ttyUSB, video, usb).
- [ ] Task: Install and configure VS Code extensions (C++, ROS, Protobuf) in `devcontainer.json`.
- [ ] Task: Verify environment by building the container and running `ros2 doctor`.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Infrastructure Setup' (Protocol in workflow.md)

## Phase 3: Package Scaffolding
- [ ] Task: Create `star-ros2` directory and initialize `ament_cmake` package skeletons.
    - [ ] Scaffold `star_spi_bridge`.
    - [ ] Scaffold `star_gateway_bridge`.
    - [ ] Scaffold `star_safety_monitor`.
- [ ] Task: Verify that all packages are recognized and built correctly using `colcon build`.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Package Scaffolding' (Protocol in workflow.md)

## Phase 4: Backlog and Finalization
- [ ] Task: Create GitHub issues for the detailed implementation of each custom node.
- [ ] Task: Update `tech-stack.md` with ROS2-specific details and sensor fusion strategy (RTAB-Map).
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Backlog and Finalization' (Protocol in workflow.md)
