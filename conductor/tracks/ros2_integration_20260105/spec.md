# Specification: ROS2 Integration Research and Base Infrastructure

## Overview
This track focuses on the initial phase of integrating ROS2 into the STAR platform. It encompasses comprehensive research into best practices, the creation of an implementation roadmap, and the setup of a standardized development environment using Docker and VS Code Devcontainers. A key focus is establishing the architecture for **Visual-LiDAR Sensor Fusion** using [RTAB-Map](https://introlab.github.io/rtabmap/).

## User Stories
- As a developer, I want a documented strategy for ROS2 integration so that I understand how the high-level control stack interacts with the hardware.
- As a developer, I want a containerized development environment so that I can develop and test ROS2 nodes consistently across different machines with full hardware access (SPI, USB, Cameras).
- As a developer, I want a defined sensor fusion strategy so that I can implement robust mapping that handles both geometric (LiDAR) and visual (Camera) data.

## Functional Requirements
- **Research & Documentation:**
    - Complete the research document `docs/sections/10_ros2_integration.md` covering architecture and implementation plan.
    - **Fusion Strategy:** Detail the configuration for `rtabmap_ros` (Visual SLAM) and `robot_localization` (EKF) to fuse LiDAR, Depth Camera, and IMU data.
    - Document the `ROS_DOMAIN_ID` configuration strategy for multi-machine communication.
    - Define coordinate frames following the REP-105 standard (e.g., `map`, `odom`, `base_link`, `laser_frame`, `camera_link`).
    - Specify input/output topics, services, and parameters for the three custom nodes.
    - Document `udev` rules requirements for persistent device naming (LiDAR, Cameras, Motor Controller).
- **Infrastructure:**
    - Create a `.devcontainer` configuration at the project root.
    - Use a Dockerfile based on ROS2 Jazzy.
    - Configure the container for privileged access and explicit device passthrough for:
          - SPI: `/dev/spidev*`
          - LiDAR/Serial: `/dev/ttyUSB*`
          - Depth Cameras: `/dev/video*` and `/dev/bus/usb`.
    - Ensure VS Code extensions for C++, ROS, and Protobuf are pre-configured.
- **Scaffolding:**
    - Create the ROS2 package structure (using `ament_cmake`) for:
        - `star_spi_bridge`
        - `star_gateway_bridge`
        - `star_safety_monitor`
    - Each package skeleton must include standard directories: `src/`, `include/`, `launch/`, and `config/`.
- **Backlog Management:**
    - Create specific GitHub issues for the detailed implementation of each custom node based on the research findings.

## Non-Functional Requirements
- **Consistency:** The development environment must match the target ROS2 Jazzy distribution used in Yocto.
- **Standards Adherence:** Follow standard ROS2 conventions for package naming, message types, and coordinate frames.

## Acceptance Criteria
- Research document `docs/sections/10_ros2_integration.md` is complete and merged.
- Sensor Fusion architecture (RTAB-Map + EKF) is documented.
- Devcontainer successfully builds and allows `ros2` commands and hardware access (LiDAR + Camera) from VS Code terminal.
- Package skeletons for the three custom nodes exist and are recognized by `colcon build`.
- Follow-up implementation issues are created in the GitHub repository.

## Out of Scope
- Full implementation of SPI communication logic.
- Full implementation of gRPC/Gateway bridge logic.
- Fine-tuning of RTAB-Map or Nav2 parameters for final deployment.
- Hardware-specific optimizations (e.g., overclocking, GPU acceleration).
- Custom SLAM algorithm development.
