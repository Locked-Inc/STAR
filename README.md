# Handheld Controller

This project contains the robot gateway, server backend, and Android application for controlling a robot with a handheld device (Retroid Pocket 2S).

## Architecture Flow

```
Retroid Pocket 2S (Controller) 
    ↓ [WiFi/Network]
Robot Gateway (Port 8080)
    ↓ [Relays commands to robot movement system]
    ↓ [Sends telemetry/data via HTTP/REST]
Server Backend (Port 8081)
    ↓ [Stores in database]
PostgreSQL Database
```

## Modules

- `robot-gateway`: A lightweight Spring Boot application running **on the robot** that:
  - Receives commands from the handheld controller
  - Relays commands to the robot's movement system  
  - Forwards telemetry data to the server backend
  - **No persistent database** (uses in-memory H2 only for temporary data)
- `server-backend`: A Spring Boot application running **on your physical server** that:
  - Receives and stores all robot data (video, sensor data, position data, logs)
  - Manages persistent data storage with PostgreSQL
  - Provides APIs for data analysis and retrieval
- `android-app`: An Android application running on the **Retroid Pocket 2S** that sends control commands to the robot gateway

## Quick Setup

### Prerequisites

- Java 17+ (for robot gateway and server backend)
- Android SDK (for Android app)
- Git

### Robot Gateway Setup

1. Navigate to the robot gateway directory:
   ```bash
   cd robot-gateway
   ```

2. Build and run the robot gateway:
   ```bash
   ./gradlew bootRun
   ```

The robot gateway will be available at `http://localhost:8080` with an H2 in-memory database.

### Server Backend Setup

1. Navigate to the server backend directory:
   ```bash
   cd server-backend
   ```

2. Build and run the server backend:
   ```bash
   ./gradlew bootRun
   ```

The server backend will be available at `http://localhost:8081` with an H2 in-memory database.

### Android App Setup

1. Navigate to the android-app directory:
   ```bash
   cd android-app
   ```

2. Build the APK:
   ```bash
   ./gradlew assembleDebug
   ```

The APK will be generated at `android-app/build/outputs/apk/debug/HandheldController-debug.apk`

### Installation on Retroid Pocket

Transfer the generated APK to your Retroid Pocket device and install it via the Android package installer.

## Project Status

✅ Robot gateway compiles and runs successfully
✅ Server backend compiles and runs successfully  
✅ Android app compiles and builds APK successfully
✅ Basic project structure is set up for robot control functionality

## Development Notes

- **Robot gateway**: Uses H2 in-memory database only (no persistent storage needed on robot)
- **Server backend**: Uses H2 for development, **should use PostgreSQL for production** with proper Flyway migrations
- Android app uses Jetpack Compose UI framework
- Both modules include build configurations for Kotlin and proper dependency management

See the `README.md` file in each module for more detailed setup instructions.