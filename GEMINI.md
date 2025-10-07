# Project STAR: Gemini Development Guide

## Project Overview

STAR (Simultaneous Tracking And Robotics) is a sophisticated robotics project centered around a LiDAR SLAM capable robot. The project is composed of several key modules:

- **Hardware (`Schematic/`):** The core of the robot is a PYNQ-Z2 board (Xilinx Zynq-7020 SoC), with hardware designs managed in KiCad.
- **Embedded Linux (`pynq-image-build/`):** A custom Linux image for the PYNQ-Z2 is built using the Yocto Project. This image includes ROS support for future LiDAR SLAM integration.
- **Handheld Controller (`HandheldController/`):** An Android application built with Kotlin and Jetpack Compose, designed to run on a Retroid Pocket 2S for remote control.
- **Robot Gateway (`RobotGateway/`):** A Spring Boot and Kotlin application that runs on the robot, acting as a bridge between the handheld controller and the robot's systems.
- **Server Backend (`ServerBackend/`):** A Spring Boot and Kotlin backend for collecting and storing data from the robot, such as sensor readings, video, and telemetry.

## Building and Running

### Handheld Controller

- **Build:** `./gradlew assembleDebug`
- **Run:** Install the generated APK from `build/outputs/apk/debug/HandheldController-debug.apk` on an Android device.

### Robot Gateway

- **Build:** `./gradlew build`
- **Run:** `./gradlew bootRun`

### Server Backend

- **Build:** `./gradlew build`
- **Run:** `./gradlew bootRun`

### PYNQ-Z2 Linux Image

- **Setup:** Follow the instructions in `pynq-image-build/build-server-setup.md` to prepare a Linux build host.
- **Build:** The build is orchestrated by the scripts in the `pynq-image-build` directory. See the `pynq-image-build/README.md` for the detailed process.

## Development Conventions

- **Kotlin:** The Android application, Robot Gateway, and Server Backend are all written in Kotlin.
- **Spring Boot:** The Robot Gateway and Server Backend use the Spring Boot framework.
- **Gradle:** All software components are built using Gradle.
- **Static Analysis:** The `RobotGateway` and `ServerBackend` projects use Detekt for static analysis of the Kotlin code. You can run it with `./gradlew detekt`.
- **Database:** The `RobotGateway` and `ServerBackend` use H2 for development and are configured for PostgreSQL in production, with database migrations handled by Flyway.
- **Hardware:** Hardware schematics are designed in KiCad.
