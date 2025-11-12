# STAR FlashManager - Coder Template & Hardware Development Platform

## Executive Summary

This document outlines the architecture and implementation plan for a full-stack hardware development platform that enables remote compilation in Coder workspaces with local hardware flashing capabilities. The system consists of:

1. **FlashManager Backend** - Spring Boot + GraphQL API (mirrors Projectum architecture)
2. **FlashManager Frontend** - React + Apollo Client web UI
3. **FlashAgent** - Rust-based local agent for hardware flashing
4. **Coder Template** - Complete development environment with all toolchains

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│  Coder Workspace (Remote Server)                                    │
│                                                                       │
│  ┌────────────────────┐         ┌──────────────────────────────┐   │
│  │ FlashManager       │         │ FlashManager Frontend        │   │
│  │ Backend            │◄────────┤ (React + Apollo Client)      │   │
│  │                    │ GraphQL │ http://localhost:5174        │   │
│  │ Spring Boot 3.1.5  │         │                              │   │
│  │ + GraphQL          │         │ Features:                    │   │
│  │ + PostgreSQL       │         │ - Trigger builds             │   │
│  │                    │         │ - Manage flash jobs          │   │
│  │ Port 8081          │         │ - View build logs (real-time)│   │
│  │                    │         │ - Manage devices             │   │
│  └────────┬───────────┘         └──────────────────────────────┘   │
│           │                                                          │
│           │ Build Artifacts: ESP32 firmware, Pi5 images,            │
│           │                  Android APKs, Spring Boot JARs         │
│           │                                                          │
└───────────┼──────────────────────────────────────────────────────────┘
            │
            │ GraphQL API + WebSocket Subscriptions
            │ JWT Authentication
            │ Poll-based (every 5 seconds)
            │
            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  Local Machine (Mac / Windows / Linux)                              │
│                                                                       │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │  FlashAgent (Rust)                                            │  │
│  │                                                                │  │
│  │  - Authenticates with JWT tokens                             │  │
│  │  - Polls for pending flash jobs                              │  │
│  │  - Downloads compiled binaries                               │  │
│  │  - Detects local USB devices                                 │  │
│  │  - Flashes hardware via CLI tools                            │  │
│  │  - Reports status back to server                             │  │
│  │                                                                │  │
│  │  Cross-platform binary (Mac, Windows, Linux, WSL2)           │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                          │
│           ↓                                                          │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Local USB Hardware                                            │ │
│  │                                                                 │ │
│  │  • ESP32-WROOM-32 / ESP32-S3 (via esptool.py)                 │ │
│  │  • Raspberry Pi 5 SD Card (via dd command)                    │ │
│  │  • Android Device (via adb install)                           │ │
│  └────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

## System Components

### 1. FlashManager Backend (Spring Boot + Kotlin)

**Location**: `STAR/FlashManager/backend/`

#### Technology Stack
Mirrors the Projectum backend architecture:

- **Language**: Kotlin 1.8.22 with JVM 17
- **Framework**: Spring Boot 3.1.5
- **API Layer**: Spring GraphQL (primary) + REST (binary downloads)
- **Database**: PostgreSQL 15 with JPA/Hibernate
- **Security**: Spring Security with JWT (jjwt library)
- **Migrations**: Flyway
- **Build Tool**: Gradle with Kotlin DSL
- **Testing**: JUnit 5 + MockK + Spring Boot Test

#### Database Entities (JPA)

```kotlin
// User Management
User {
  id: UUID
  email: String
  passwordHash: String
  role: UserRole (ADMIN, USER)
  createdAt: Timestamp
}

// Device Management
Device {
  id: UUID
  name: String
  apiKeyHash: String      // For JWT generation
  userId: UUID            // Owner
  status: DeviceStatus    // ONLINE, OFFLINE, BUSY
  lastSeen: Timestamp
  platform: Platform      // MAC, WINDOWS, LINUX, WSL2
  capabilities: List<DeviceCapability>  // ESP32, PI5, ANDROID
}

// Build Jobs
BuildJob {
  id: UUID
  component: Component    // ESP32, PI5_OS, ANDROID_APP, ROBOT_GATEWAY, SERVER_BACKEND
  branch: String          // Git branch to build
  status: JobStatus       // PENDING, RUNNING, COMPLETED, FAILED
  triggeredBy: UUID       // User ID
  startedAt: Timestamp
  completedAt: Timestamp
  binaryPath: String      // Path to compiled artifact
  binarySize: Long
  buildDuration: Long     // milliseconds
  exitCode: Int
}

// Build Logs
BuildLog {
  id: UUID
  buildJobId: UUID
  timestamp: Timestamp
  line: String
  level: LogLevel         // INFO, WARN, ERROR, DEBUG
}

// Flash Jobs
FlashJob {
  id: UUID
  buildJobId: UUID
  deviceId: UUID
  targetDevice: String    // e.g., "/dev/ttyUSB0", "sdX", "emulator-5554"
  status: JobStatus
  createdAt: Timestamp
  claimedAt: Timestamp    // When agent claimed the job
  completedAt: Timestamp
  priority: Int           // For job ordering
}

// Flash History
FlashHistory {
  id: UUID
  flashJobId: UUID
  result: FlashResult     // SUCCESS, FAILED, TIMEOUT
  duration: Long          // milliseconds
  errorMessage: String
  flashOutput: Text       // Complete flash tool output
}
```

#### GraphQL Schema

**Location**: `backend/src/main/resources/graphql/schema.graphqls`

```graphql
# ============================================================================
# Types
# ============================================================================

type User {
  id: ID!
  email: String!
  role: UserRole!
  createdAt: DateTime!
}

type Device {
  id: ID!
  name: String!
  userId: ID!
  user: User!
  status: DeviceStatus!
  lastSeen: DateTime!
  platform: Platform!
  capabilities: [DeviceCapability!]!
  activeFlashJob: FlashJob
}

type BuildJob {
  id: ID!
  component: Component!
  branch: String!
  status: JobStatus!
  triggeredBy: User!
  startedAt: DateTime
  completedAt: DateTime
  binarySize: Long
  buildDuration: Long
  exitCode: Int
  logs: [BuildLog!]!
}

type BuildLog {
  id: ID!
  buildJobId: ID!
  timestamp: DateTime!
  line: String!
  level: LogLevel!
}

type FlashJob {
  id: ID!
  buildJob: BuildJob!
  device: Device!
  targetDevice: String!
  status: JobStatus!
  createdAt: DateTime!
  claimedAt: DateTime
  completedAt: DateTime
  priority: Int!
  history: FlashHistory
}

type FlashHistory {
  id: ID!
  flashJob: FlashJob!
  result: FlashResult!
  duration: Long!
  errorMessage: String
  flashOutput: String
}

type DeviceRegistration {
  deviceId: ID!
  apiKey: String!        # Only returned once during registration
}

type AuthPayload {
  token: String!
  device: Device!
  expiresAt: DateTime!
}

# ============================================================================
# Enums
# ============================================================================

enum UserRole {
  ADMIN
  USER
}

enum DeviceStatus {
  ONLINE
  OFFLINE
  BUSY
}

enum Platform {
  MAC
  WINDOWS
  LINUX
  WSL2
}

enum DeviceCapability {
  ESP32
  PI5
  ANDROID
}

enum Component {
  ESP32_FIRMWARE
  PI5_OS
  ANDROID_APP
  ROBOT_GATEWAY
  SERVER_BACKEND
}

enum JobStatus {
  PENDING
  RUNNING
  COMPLETED
  FAILED
  CANCELLED
}

enum LogLevel {
  INFO
  WARN
  ERROR
  DEBUG
}

enum FlashResult {
  SUCCESS
  FAILED
  TIMEOUT
}

# ============================================================================
# Queries (Web UI)
# ============================================================================

type Query {
  # User queries
  me: User!
  users: [User!]!

  # Device queries
  devices: [Device!]!
  device(id: ID!): Device
  myDevices: [Device!]!

  # Build queries
  buildJobs(status: JobStatus, component: Component, limit: Int): [BuildJob!]!
  buildJob(id: ID!): BuildJob
  buildLogs(buildJobId: ID!, limit: Int, offset: Int): [BuildLog!]!

  # Flash queries
  flashJobs(status: JobStatus, deviceId: ID, limit: Int): [FlashJob!]!
  flashJob(id: ID!): FlashJob
  flashHistory(limit: Int): [FlashHistory!]!

  # Agent queries (authenticated as device)
  pollFlashJobs: [FlashJob!]!
}

# ============================================================================
# Mutations (Web UI)
# ============================================================================

type Mutation {
  # Authentication
  login(email: String!, password: String!): AuthPayload!
  register(email: String!, password: String!): AuthPayload!

  # Device management
  registerDevice(name: String!, platform: Platform!, capabilities: [DeviceCapability!]!): DeviceRegistration!
  updateDeviceStatus(deviceId: ID!, status: DeviceStatus!): Device!
  deleteDevice(deviceId: ID!): Boolean!

  # Build management
  triggerBuild(component: Component!, branch: String): BuildJob!
  cancelBuild(buildJobId: ID!): BuildJob!

  # Flash management
  createFlashJob(buildJobId: ID!, deviceId: ID!, targetDevice: String!, priority: Int): FlashJob!
  cancelFlashJob(flashJobId: ID!): FlashJob!

  # Agent mutations (authenticated as device)
  deviceLogin(apiKey: String!): AuthPayload!
  claimFlashJob(flashJobId: ID!): FlashJob!
  updateFlashStatus(flashJobId: ID!, status: JobStatus!, message: String): FlashJob!
  completeFlashJob(flashJobId: ID!, result: FlashResult!, duration: Long!, output: String, errorMessage: String): FlashHistory!
  updateDeviceHeartbeat: Device!
}

# ============================================================================
# Subscriptions (Real-time)
# ============================================================================

type Subscription {
  # Build log streaming
  buildLogStream(buildJobId: ID!): BuildLog!

  # Flash job updates for a specific device
  flashJobUpdated(deviceId: ID!): FlashJob!

  # Device status changes
  deviceStatusChanged: Device!

  # Build job status changes
  buildJobUpdated(buildJobId: ID!): BuildJob!
}
```

#### Key Services

```kotlin
// BuildService.kt
class BuildService {
  fun triggerBuild(component: Component, branch: String): BuildJob
  fun executeBuild(buildJob: BuildJob): BuildJob
  fun streamLogs(buildJob: BuildJob, line: String, level: LogLevel)
  fun cancelBuild(buildJobId: UUID): BuildJob
}

// DeviceService.kt
class DeviceService {
  fun registerDevice(name: String, platform: Platform, capabilities: List<DeviceCapability>): DeviceRegistration
  fun generateApiKey(deviceId: UUID): String
  fun authenticateDevice(apiKey: String): Device
  fun updateHeartbeat(deviceId: UUID): Device
  fun getOnlineDevices(): List<Device>
}

// FlashJobService.kt
class FlashJobService {
  fun createFlashJob(buildJobId: UUID, deviceId: UUID, targetDevice: String): FlashJob
  fun pollFlashJobs(deviceId: UUID): List<FlashJob>
  fun claimFlashJob(flashJobId: UUID, deviceId: UUID): FlashJob
  fun updateFlashStatus(flashJobId: UUID, status: JobStatus, message: String): FlashJob
  fun completeFlashJob(flashJobId: UUID, result: FlashResult, duration: Long, output: String?): FlashHistory
}

// BinaryStorageService.kt
class BinaryStorageService {
  fun storeBinary(buildJobId: UUID, file: File): String
  fun getBinaryPath(buildJobId: UUID): String
  fun streamBinary(buildJobId: UUID): InputStream
  fun deleteBinary(buildJobId: UUID)
}
```

#### REST Endpoints (Binary Downloads)

```kotlin
@RestController
@RequestMapping("/api/v1/binaries")
class BinaryController {

  @GetMapping("/{buildJobId}")
  @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
  fun downloadBinary(@PathVariable buildJobId: UUID): ResponseEntity<Resource>

  @GetMapping("/{buildJobId}/info")
  fun getBinaryInfo(@PathVariable buildJobId: UUID): BinaryInfo
}
```

#### Build System Integration

**ESP32 Build** (`star-pi5-os/` required in workspace):
```bash
cd esp32-firmware
source ~/esp/esp-idf/export.sh
idf.py build
# Output: build/star-firmware.bin
```

**Pi5 Build** (long-running, ~1-2 hours):
```bash
cd star-pi5-os
make build
# Output: buildroot-2025.02/output/images/sdcard.img
```

**Android Build**:
```bash
cd HandheldController
./gradlew assembleDebug
# Output: build/outputs/apk/debug/HandheldController-debug.apk
```

**RobotGateway Build**:
```bash
cd RobotGateway
./gradlew bootJar
# Output: build/libs/robot-gateway.jar
```

**ServerBackend Build**:
```bash
cd ServerBackend
./gradlew bootJar
# Output: build/libs/server-backend.jar
```

---

### 2. FlashManager Frontend (React + TypeScript)

**Location**: `STAR/FlashManager/frontend/`

#### Technology Stack
Mirrors the Projectum frontend architecture:

- **Framework**: React 19.1.1 with TypeScript
- **Build Tool**: Vite 7.1.7
- **GraphQL Client**: Apollo Client 4.0.7 with GraphQL-WS for subscriptions
- **State Management**: Redux Toolkit 2.9.0 (auth state)
- **Styling**: Tailwind CSS 3.4.17
- **Charts**: Recharts 3.2.1 (build statistics)
- **Testing**: Vitest 2.1.5 + React Testing Library

#### Pages

```
/login                          # Authentication
/dashboard                      # Overview
  - Active builds widget
  - Flash queue widget
  - Device status widget
  - Recent activity feed

/builds                         # Build management
  - Trigger new build form
  - Build history table
  - Filter by component/status

/builds/:id/logs                # Real-time log viewer
  - Streaming build logs (GraphQL subscription)
  - Auto-scroll with pause
  - Log level filtering
  - Download logs

/flash                          # Flash management
  - Flash job queue
  - Create flash job form
  - Flash history table
  - Status tracking

/devices                        # Device management
  - Registered devices table
  - Device registration form
  - Connection status (online/offline)
  - Device capabilities
  - API key generation
```

#### Key Components

```tsx
// BuildTrigger.tsx
// Dropdown to select component, input for branch, trigger build button
// Shows real-time build status after triggering

// BuildLogViewer.tsx
// Real-time streaming logs using GraphQL subscription
// Auto-scroll, search, filter by log level, download logs

// FlashJobQueue.tsx
// Table showing pending/running flash jobs
// Create new flash job: select build + device + target path
// Priority ordering

// DeviceStatus.tsx
// Real-time device status (online/offline/busy)
// Shows last seen timestamp
// Active flash job indicator

// FlashHistory.tsx
// Historical flash operations
// Success/failure indicators
// Duration, output logs, error messages
```

#### Apollo Client Setup

```typescript
// src/services/apollo/client.ts
import { ApolloClient, InMemoryCache, split, HttpLink } from '@apollo/client';
import { GraphQLWsLink } from '@apollo/client/link/subscriptions';
import { getMainDefinition } from '@apollo/client/utilities';
import { createClient } from 'graphql-ws';

const httpLink = new HttpLink({
  uri: 'http://localhost:8081/api/v1/graphql',
  headers: {
    authorization: localStorage.getItem('token') ? `Bearer ${localStorage.getItem('token')}` : '',
  },
});

const wsLink = new GraphQLWsLink(
  createClient({
    url: 'ws://localhost:8081/api/v1/graphql',
    connectionParams: {
      authorization: localStorage.getItem('token') ? `Bearer ${localStorage.getItem('token')}` : '',
    },
  })
);

const splitLink = split(
  ({ query }) => {
    const definition = getMainDefinition(query);
    return (
      definition.kind === 'OperationDefinition' &&
      definition.operation === 'subscription'
    );
  },
  wsLink,
  httpLink
);

export const client = new ApolloClient({
  link: splitLink,
  cache: new InMemoryCache(),
});
```

#### Real-time Build Log Streaming

```typescript
// src/components/BuildLogViewer.tsx
import { useSubscription } from '@apollo/client';
import { BUILD_LOG_STREAM } from '../graphql/subscriptions';

export function BuildLogViewer({ buildJobId }: { buildJobId: string }) {
  const { data, loading } = useSubscription(BUILD_LOG_STREAM, {
    variables: { buildJobId },
  });

  // Auto-scroll, render logs with syntax highlighting
  // Color-code by log level (INFO=white, WARN=yellow, ERROR=red)
}
```

---

### 3. FlashAgent (Rust)

**Location**: `STAR/FlashAgent/`

#### Technology Stack

- **Language**: Rust 1.75+
- **HTTP Client**: reqwest (with TLS)
- **GraphQL Client**: graphql_client
- **Serial Port**: serialport (USB device detection)
- **Config**: toml, serde
- **Logging**: tracing, tracing-subscriber
- **CLI**: clap
- **Async Runtime**: tokio

#### Project Structure

```
FlashAgent/
├── Cargo.toml
├── src/
│   ├── main.rs              # CLI entry point
│   ├── config.rs            # Configuration loading
│   ├── auth.rs              # JWT authentication
│   ├── graphql/
│   │   ├── mod.rs
│   │   ├── client.rs        # GraphQL client wrapper
│   │   ├── queries.rs       # GraphQL queries
│   │   └── mutations.rs     # GraphQL mutations
│   ├── poller.rs            # Poll flash jobs every 5 seconds
│   ├── downloader.rs        # Download binaries
│   ├── flasher/
│   │   ├── mod.rs
│   │   ├── esp32.rs         # ESP32 flashing via esptool.py
│   │   ├── pi5.rs           # Pi5 SD flashing via dd
│   │   └── android.rs       # Android flashing via adb
│   ├── devices.rs           # USB device detection
│   └── error.rs             # Error types
├── config.example.toml      # Example configuration
└── README.md
```

#### Configuration File

**Location**: `~/.star-flash-agent.toml`

```toml
[server]
url = "http://localhost:8081/api/v1/graphql"
api_key = ""  # Generated during device registration

[device]
name = "johns-macbook-pro"
platform = "MAC"  # MAC, WINDOWS, LINUX, WSL2
capabilities = ["ESP32", "PI5", "ANDROID"]

[polling]
interval_seconds = 5
retry_count = 3
retry_delay_seconds = 10

[flash]
binary_cache_dir = "/tmp/star-binaries"
max_cache_size_mb = 1024
cleanup_after_flash = false

[logging]
level = "info"  # trace, debug, info, warn, error
file = "/var/log/star-flash-agent.log"
```

#### CLI Commands

```bash
# Device registration (one-time setup)
flash-agent register --name "johns-macbook" --platform MAC --capabilities ESP32,PI5

# Start polling daemon
flash-agent start

# Run in foreground (for debugging)
flash-agent start --foreground

# Stop daemon
flash-agent stop

# Check status
flash-agent status

# List local USB devices
flash-agent devices

# Manual flash (bypasses web UI)
flash-agent flash --binary /path/to/firmware.bin --target /dev/ttyUSB0 --type ESP32

# View configuration
flash-agent config

# Edit configuration
flash-agent config --edit

# View logs
flash-agent logs

# Update agent
flash-agent update
```

#### Core Logic

**Poller** (`src/poller.rs`):
```rust
pub async fn poll_loop(client: &GraphQLClient, config: &Config) -> Result<()> {
    loop {
        // 1. Update heartbeat
        client.update_heartbeat().await?;

        // 2. Poll for pending flash jobs
        let jobs = client.poll_flash_jobs().await?;

        // 3. Process each job
        for job in jobs {
            // Claim the job
            client.claim_flash_job(job.id).await?;

            // Download binary
            let binary_path = download_binary(job.build_job_id, config).await?;

            // Flash device
            match flash_device(&job, &binary_path).await {
                Ok(output) => {
                    client.complete_flash_job(
                        job.id,
                        FlashResult::Success,
                        output.duration,
                        Some(output.stdout),
                        None,
                    ).await?;
                }
                Err(e) => {
                    client.complete_flash_job(
                        job.id,
                        FlashResult::Failed,
                        0,
                        None,
                        Some(e.to_string()),
                    ).await?;
                }
            }
        }

        // 4. Sleep for interval
        tokio::time::sleep(Duration::from_secs(config.polling.interval_seconds)).await;
    }
}
```

**ESP32 Flasher** (`src/flasher/esp32.rs`):
```rust
pub async fn flash_esp32(binary_path: &Path, port: &str) -> Result<FlashOutput> {
    let start = Instant::now();

    // Run esptool.py
    let output = Command::new("esptool.py")
        .args(&[
            "--port", port,
            "--baud", "921600",
            "write_flash",
            "0x10000", binary_path.to_str().unwrap(),
        ])
        .output()
        .await?;

    let duration = start.elapsed().as_millis() as i64;

    if output.status.success() {
        Ok(FlashOutput {
            duration,
            stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).to_string(),
        })
    } else {
        Err(FlashError::FlashFailed(
            String::from_utf8_lossy(&output.stderr).to_string()
        ))
    }
}
```

**Pi5 SD Flasher** (`src/flasher/pi5.rs`):
```rust
pub async fn flash_pi5_sd(image_path: &Path, device: &str) -> Result<FlashOutput> {
    let start = Instant::now();

    // Requires sudo
    let output = Command::new("sudo")
        .args(&[
            "dd",
            &format!("if={}", image_path.display()),
            &format!("of={}", device),
            "bs=4M",
            "status=progress",
            "conv=fsync",
        ])
        .output()
        .await?;

    let duration = start.elapsed().as_millis() as i64;

    if output.status.success() {
        Ok(FlashOutput {
            duration,
            stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).to_string(),
        })
    } else {
        Err(FlashError::FlashFailed(
            String::from_utf8_lossy(&output.stderr).to_string()
        ))
    }
}
```

**Android Flasher** (`src/flasher/android.rs`):
```rust
pub async fn flash_android(apk_path: &Path, device: &str) -> Result<FlashOutput> {
    let start = Instant::now();

    let output = Command::new("adb")
        .args(&[
            "-s", device,
            "install",
            "-r",  // Replace existing
            apk_path.to_str().unwrap(),
        ])
        .output()
        .await?;

    let duration = start.elapsed().as_millis() as i64;

    if output.status.success() {
        Ok(FlashOutput {
            duration,
            stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).to_string(),
        })
    } else {
        Err(FlashError::FlashFailed(
            String::from_utf8_lossy(&output.stderr).to_string()
        ))
    }
}
```

**Device Detection** (`src/devices.rs`):
```rust
pub fn list_serial_ports() -> Result<Vec<DeviceInfo>> {
    let ports = serialport::available_ports()?;

    Ok(ports.iter().map(|port| {
        DeviceInfo {
            path: port.port_name.clone(),
            device_type: detect_device_type(port),
            vendor_id: get_vendor_id(port),
            product_id: get_product_id(port),
        }
    }).collect())
}

fn detect_device_type(port: &SerialPortInfo) -> DeviceType {
    match port {
        SerialPortInfo::UsbPort(info) => {
            // ESP32 CP210x
            if info.vid == 0x10C4 && info.pid == 0xEA60 {
                DeviceType::ESP32
            }
            // ESP32 CH340
            else if info.vid == 0x1A86 && info.pid == 0x7523 {
                DeviceType::ESP32
            }
            else {
                DeviceType::Unknown
            }
        }
        _ => DeviceType::Unknown,
    }
}
```

#### Cross-Platform Builds

```bash
# Install cross-compilation targets
rustup target add x86_64-apple-darwin      # macOS Intel
rustup target add aarch64-apple-darwin     # macOS ARM (M1/M2)
rustup target add x86_64-pc-windows-gnu    # Windows
rustup target add x86_64-unknown-linux-gnu # Linux

# Build for all platforms
cargo build --release --target x86_64-apple-darwin
cargo build --release --target aarch64-apple-darwin
cargo build --release --target x86_64-pc-windows-gnu
cargo build --release --target x86_64-unknown-linux-gnu

# Outputs:
# target/x86_64-apple-darwin/release/flash-agent
# target/aarch64-apple-darwin/release/flash-agent
# target/x86_64-pc-windows-gnu/release/flash-agent.exe
# target/x86_64-unknown-linux-gnu/release/flash-agent
```

---

### 4. Coder Template

**Location**: `STAR/.coder/coder.yaml`

#### Dockerfile

**Location**: `STAR/.coder/images/Dockerfile`

```dockerfile
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    unzip \
    # ESP-IDF dependencies
    flex \
    bison \
    gperf \
    python3 \
    python3-pip \
    python3-venv \
    ccache \
    libffi-dev \
    libssl-dev \
    dfu-util \
    libusb-1.0-0 \
    # Buildroot dependencies
    gcc-aarch64-linux-gnu \
    cpio \
    rsync \
    bc \
    libncurses5-dev \
    dosfstools \
    mtools \
    genimage \
    # Android SDK dependencies
    openjdk-17-jdk \
    # PostgreSQL client
    postgresql-client \
    # Rust
    rustc \
    cargo \
    # Node.js
    nodejs \
    npm \
    # Utilities
    sudo \
    vim \
    htop \
    && rm -rf /var/lib/apt/lists/*

# Install ESP-IDF v5.5.1
RUN mkdir -p /opt/esp && cd /opt/esp && \
    git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git && \
    cd esp-idf && \
    ./install.sh esp32,esp32s3

# Install Android SDK
RUN mkdir -p /opt/android-sdk && cd /opt/android-sdk && \
    wget https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip && \
    unzip commandlinetools-linux-9477386_latest.zip -d cmdline-tools && \
    rm commandlinetools-linux-9477386_latest.zip && \
    yes | cmdline-tools/bin/sdkmanager --sdk_root=/opt/android-sdk --licenses && \
    cmdline-tools/bin/sdkmanager --sdk_root=/opt/android-sdk "platform-tools" "platforms;android-33" "build-tools;33.0.0"

# Environment variables
ENV ESP_IDF=/opt/esp/esp-idf
ENV IDF_PATH=/opt/esp/esp-idf
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH="$PATH:$ESP_IDF/tools:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/bin"

# Create coder user
RUN useradd -m -s /bin/bash coder && \
    echo "coder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER coder
WORKDIR /home/coder

# Install Node.js 20 via nvm
RUN curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash && \
    export NVM_DIR="$HOME/.nvm" && \
    [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" && \
    nvm install 20 && \
    nvm use 20

CMD ["/bin/bash"]
```

#### Template Configuration

**Location**: `STAR/.coder/coder.yaml`

```yaml
name: star-embedded-dev
display_name: STAR Embedded Development
description: Complete embedded systems development environment for ESP32, Raspberry Pi 5, and Android with remote flashing capabilities
icon: /icon/microchip.svg

# Use custom Docker image
build:
  dockerfile: .coder/images/Dockerfile
  context: .

# Compute resources
resources:
  cpu: 8
  memory: 16Gi
  disk: 50Gi

# Ports
ports:
  - name: FlashManager Backend
    port: 8081
    protocol: http
    access: public
  - name: FlashManager Frontend
    port: 5174
    protocol: http
    access: public
  - name: PostgreSQL
    port: 5432
    protocol: tcp
    access: private

# Environment variables
env:
  - name: DATABASE_URL
    value: "jdbc:postgresql://localhost:5432/flashmanager"
  - name: DATABASE_USERNAME
    value: "flashmanager"
  - name: DATABASE_PASSWORD
    value: "flashmanager123"
  - name: JWT_SECRET
    value: "change-this-in-production-use-env-var"
  - name: JWT_EXPIRATION
    value: "86400000"  # 24 hours
  - name: BINARY_STORAGE_PATH
    value: "/home/coder/binaries"

# Startup script
startup_script: |
  #!/bin/bash
  set -e

  echo "🚀 Starting STAR FlashManager services..."

  # Start PostgreSQL container
  cd /home/coder/Projectum/STAR/FlashManager/backend
  ./gradlew dockerStart

  # Wait for PostgreSQL
  echo "⏳ Waiting for PostgreSQL..."
  until pg_isready -h localhost -p 5432; do
    sleep 1
  done

  # Start backend
  echo "🔧 Starting FlashManager backend..."
  ./gradlew bootRun > /tmp/backend.log 2>&1 &

  # Wait for backend to be ready
  echo "⏳ Waiting for backend..."
  until curl -s http://localhost:8081/actuator/health > /dev/null; do
    sleep 2
  done

  # Start frontend
  echo "🎨 Starting FlashManager frontend..."
  cd /home/coder/Projectum/STAR/FlashManager/frontend
  npm install
  npm run dev > /tmp/frontend.log 2>&1 &

  echo "✅ FlashManager is running!"
  echo ""
  echo "🌐 Frontend: http://localhost:5174"
  echo "🔌 Backend:  http://localhost:8081"
  echo "📊 GraphQL:  http://localhost:8081/api/v1/graphql"
  echo ""
  echo "📖 Next steps:"
  echo "  1. Open the frontend in your browser"
  echo "  2. Register an account"
  echo "  3. Download and install FlashAgent on your local machine"
  echo "  4. Register your local device in the web UI"
  echo "  5. Start building and flashing!"
```

---

## Implementation Plan

### Phase 1: Backend Foundation (8-10 hours)

1. **Project Setup**
   - Create Spring Boot project structure
   - Configure Gradle build
   - Set up project dependencies

2. **Database Setup**
   - Configure PostgreSQL connection
   - Set up Flyway migrations
   - Create initial schema (V1)

3. **Domain Entities**
   - Create JPA entities (User, Device, BuildJob, FlashJob, etc.)
   - Define entity relationships
   - Add validation annotations

4. **Security Setup**
   - Configure Spring Security
   - Implement JWT authentication
   - Create user registration/login

5. **Repository Layer**
   - Create JPA repositories
   - Add custom query methods

---

### Phase 2: GraphQL API (6-8 hours)

6. **GraphQL Schema**
   - Define complete schema (types, queries, mutations, subscriptions)
   - Document all operations

7. **Query Resolvers**
   - Implement device queries
   - Implement build job queries
   - Implement flash job queries

8. **Mutation Resolvers**
   - Device registration and management
   - Build triggering
   - Flash job creation
   - Agent mutations (login, claim, update)

9. **Subscription Resolvers**
   - Build log streaming
   - Flash job updates
   - Device status changes

10. **GraphQL Security**
    - Add authentication to GraphQL context
    - Implement authorization checks

---

### Phase 3: Build System Integration (8-10 hours)

11. **BuildService Implementation**
    - Design build orchestration
    - Create build job executor

12. **ESP32 Build Integration**
    - Wrap `idf.py build` command
    - Capture build output
    - Handle errors

13. **Pi5 Build Integration**
    - Wrap Buildroot `make build`
    - Long-running job handling
    - Progress tracking

14. **Android Build Integration**
    - Wrap `./gradlew assembleDebug`
    - Handle Gradle output

15. **Backend Build Integration**
    - Wrap `./gradlew bootJar` for both backends

16. **Build Log Streaming**
    - Real-time log capture
    - GraphQL subscription implementation
    - Log level detection

17. **Binary Storage**
    - File system storage implementation
    - Binary metadata tracking
    - REST endpoint for downloads

---

### Phase 4: Frontend (10-12 hours)

18. **Project Setup**
    - Create React app with Vite
    - Install dependencies (Apollo, Redux, Tailwind)
    - Configure TypeScript

19. **Apollo Client Setup**
    - Configure HTTP and WebSocket links
    - Set up authentication
    - Configure cache

20. **Authentication Flow**
    - Login page
    - Registration page
    - Protected routes
    - JWT token management

21. **Dashboard Page**
    - Active builds widget
    - Flash queue widget
    - Device status widget
    - Recent activity feed

22. **Builds Page**
    - Build trigger form (component selector, branch input)
    - Build history table
    - Status filtering
    - Pagination

23. **Build Log Viewer**
    - Real-time log streaming (subscription)
    - Auto-scroll with pause
    - Log level filtering
    - Search functionality
    - Download logs

24. **Flash Management Page**
    - Flash job queue table
    - Create flash job form (select build + device + target)
    - Flash history table
    - Status tracking

25. **Device Management Page**
    - Registered devices table
    - Device registration form
    - Connection status indicators
    - API key generation and display

---

### Phase 5: Flash Agent (12-15 hours)

26. **Rust Project Setup**
    - Create Cargo project
    - Add dependencies (reqwest, graphql_client, serialport, etc.)
    - Configure project structure

27. **Configuration**
    - Define config schema (TOML)
    - Implement config loading
    - Validate configuration

28. **GraphQL Client**
    - Set up graphql_client with schema
    - Implement query functions
    - Implement mutation functions

29. **Authentication**
    - JWT token management
    - Device login flow
    - Token refresh logic

30. **Device Registration**
    - Registration command
    - Store API key securely
    - Platform and capability detection

31. **Polling Mechanism**
    - Implement poll loop
    - Heartbeat updates
    - Error handling and retry logic

32. **Binary Download**
    - HTTP download with progress
    - Caching mechanism
    - Checksum verification

33. **ESP32 Flasher**
    - Detect ESP32 devices
    - Wrap esptool.py
    - Capture output
    - Error handling

34. **Pi5 SD Flasher**
    - Detect SD card devices
    - Wrap dd command (with sudo)
    - Progress tracking
    - Safety checks

35. **Android Flasher**
    - Detect Android devices (adb)
    - Wrap adb install
    - Handle multiple devices

36. **Status Reporting**
    - Report flash progress
    - Complete job with results
    - Error reporting

37. **CLI Interface**
    - Implement clap commands
    - Register, start, stop, status, devices
    - Config management

38. **Cross-Platform Builds**
    - Set up cross-compilation
    - Build for Mac (Intel + ARM)
    - Build for Windows
    - Build for Linux
    - Test on each platform

---

### Phase 6: Coder Template (4-6 hours)

39. **Dockerfile**
    - Create multi-stage build
    - Install all toolchains (ESP-IDF, Buildroot, Android SDK)
    - Install Node.js, Java, Rust
    - Set up environment variables

40. **Coder YAML**
    - Define template metadata
    - Configure resources
    - Set up ports
    - Define environment variables

41. **Startup Script**
    - Start PostgreSQL
    - Start backend
    - Start frontend
    - Health checks

42. **Documentation**
    - Template usage guide
    - Environment setup
    - Troubleshooting

---

### Phase 7: Testing & Documentation (6-8 hours)

43. **Backend Testing**
    - Unit tests for services
    - Integration tests for GraphQL
    - Repository tests

44. **Frontend Testing**
    - Component tests
    - Integration tests
    - E2E tests with Playwright

45. **Agent Testing**
    - Unit tests for modules
    - Integration tests with mock server
    - End-to-end testing on real hardware

46. **Cross-Platform Testing**
    - Test agent on Mac
    - Test agent on Windows
    - Test agent on Linux
    - Test agent on WSL2

47. **Documentation**
    - Architecture overview
    - API documentation
    - Agent setup guide
    - Troubleshooting guide
    - Development guide

48. **Demo & Polish**
    - Create demo video
    - Polish UI/UX
    - Fix bugs
    - Performance optimization

---

## File Structure

```
STAR/
├── FlashManager/                        # Main application directory
│   ├── backend/                         # Spring Boot backend
│   │   ├── src/
│   │   │   ├── main/
│   │   │   │   ├── kotlin/com/star/flashmanager/
│   │   │   │   │   ├── FlashManagerApplication.kt
│   │   │   │   │   ├── controllers/
│   │   │   │   │   │   └── BinaryController.kt
│   │   │   │   │   ├── domain/
│   │   │   │   │   │   ├── entities/
│   │   │   │   │   │   │   ├── User.kt
│   │   │   │   │   │   │   ├── Device.kt
│   │   │   │   │   │   │   ├── BuildJob.kt
│   │   │   │   │   │   │   ├── BuildLog.kt
│   │   │   │   │   │   │   ├── FlashJob.kt
│   │   │   │   │   │   │   └── FlashHistory.kt
│   │   │   │   │   │   └── enums/
│   │   │   │   │   │       ├── UserRole.kt
│   │   │   │   │   │       ├── DeviceStatus.kt
│   │   │   │   │   │       ├── Platform.kt
│   │   │   │   │   │       ├── Component.kt
│   │   │   │   │   │       └── JobStatus.kt
│   │   │   │   │   ├── dto/
│   │   │   │   │   ├── graphql/
│   │   │   │   │   │   ├── resolvers/
│   │   │   │   │   │   │   ├── QueryResolver.kt
│   │   │   │   │   │   │   ├── MutationResolver.kt
│   │   │   │   │   │   │   └── SubscriptionResolver.kt
│   │   │   │   │   │   ├── types/
│   │   │   │   │   │   └── config/
│   │   │   │   │   │       └── GraphQLConfig.kt
│   │   │   │   │   ├── repositories/
│   │   │   │   │   │   ├── UserRepository.kt
│   │   │   │   │   │   ├── DeviceRepository.kt
│   │   │   │   │   │   ├── BuildJobRepository.kt
│   │   │   │   │   │   ├── BuildLogRepository.kt
│   │   │   │   │   │   ├── FlashJobRepository.kt
│   │   │   │   │   │   └── FlashHistoryRepository.kt
│   │   │   │   │   ├── services/
│   │   │   │   │   │   ├── UserService.kt
│   │   │   │   │   │   ├── DeviceService.kt
│   │   │   │   │   │   ├── BuildService.kt
│   │   │   │   │   │   ├── FlashJobService.kt
│   │   │   │   │   │   ├── BinaryStorageService.kt
│   │   │   │   │   │   └── BuildLogService.kt
│   │   │   │   │   ├── security/
│   │   │   │   │   │   ├── JwtTokenProvider.kt
│   │   │   │   │   │   ├── JwtAuthenticationFilter.kt
│   │   │   │   │   │   └── SecurityConfig.kt
│   │   │   │   │   ├── config/
│   │   │   │   │   │   ├── DatabaseConfig.kt
│   │   │   │   │   │   └── WebSocketConfig.kt
│   │   │   │   │   ├── exception/
│   │   │   │   │   │   ├── CustomExceptions.kt
│   │   │   │   │   │   └── GlobalExceptionHandler.kt
│   │   │   │   │   └── util/
│   │   │   │   └── resources/
│   │   │   │       ├── graphql/
│   │   │   │       │   └── schema.graphqls
│   │   │   │       ├── db/migration/
│   │   │   │       │   ├── V1__initial_schema.sql
│   │   │   │       │   └── V2__add_indexes.sql
│   │   │   │       └── application.yml
│   │   │   └── test/
│   │   │       └── kotlin/com/star/flashmanager/
│   │   │           ├── services/
│   │   │           ├── repositories/
│   │   │           └── graphql/
│   │   ├── build.gradle.kts
│   │   ├── settings.gradle.kts
│   │   ├── gradlew
│   │   ├── gradlew.bat
│   │   └── README.md
│   │
│   ├── frontend/                        # React frontend
│   │   ├── src/
│   │   │   ├── App.tsx
│   │   │   ├── main.tsx
│   │   │   ├── pages/
│   │   │   │   ├── Login.tsx
│   │   │   │   ├── Register.tsx
│   │   │   │   ├── Dashboard.tsx
│   │   │   │   ├── Builds.tsx
│   │   │   │   ├── BuildLogViewer.tsx
│   │   │   │   ├── Flash.tsx
│   │   │   │   └── Devices.tsx
│   │   │   ├── components/
│   │   │   │   ├── layout/
│   │   │   │   │   ├── Header.tsx
│   │   │   │   │   ├── Sidebar.tsx
│   │   │   │   │   └── Layout.tsx
│   │   │   │   ├── common/
│   │   │   │   │   ├── Button.tsx
│   │   │   │   │   ├── Card.tsx
│   │   │   │   │   ├── Table.tsx
│   │   │   │   │   └── StatusBadge.tsx
│   │   │   │   ├── builds/
│   │   │   │   │   ├── BuildTrigger.tsx
│   │   │   │   │   ├── BuildHistory.tsx
│   │   │   │   │   └── BuildCard.tsx
│   │   │   │   ├── flash/
│   │   │   │   │   ├── FlashJobQueue.tsx
│   │   │   │   │   ├── FlashJobForm.tsx
│   │   │   │   │   └── FlashHistory.tsx
│   │   │   │   └── devices/
│   │   │   │       ├── DeviceList.tsx
│   │   │   │       ├── DeviceCard.tsx
│   │   │   │       └── DeviceRegistration.tsx
│   │   │   ├── graphql/
│   │   │   │   ├── queries.ts
│   │   │   │   ├── mutations.ts
│   │   │   │   └── subscriptions.ts
│   │   │   ├── services/
│   │   │   │   └── apollo/
│   │   │   │       └── client.ts
│   │   │   ├── store/
│   │   │   │   ├── slices/
│   │   │   │   │   └── authSlice.ts
│   │   │   │   └── store.ts
│   │   │   ├── hooks/
│   │   │   │   ├── useAuth.ts
│   │   │   │   └── useBuildLogs.ts
│   │   │   ├── types/
│   │   │   │   └── graphql.types.ts
│   │   │   └── utils/
│   │   │       └── formatters.ts
│   │   ├── public/
│   │   ├── index.html
│   │   ├── package.json
│   │   ├── vite.config.ts
│   │   ├── tsconfig.json
│   │   ├── tailwind.config.js
│   │   └── README.md
│   │
│   └── docker-compose.yml               # PostgreSQL for development
│
├── FlashAgent/                          # Rust local agent
│   ├── src/
│   │   ├── main.rs
│   │   ├── config.rs
│   │   ├── auth.rs
│   │   ├── graphql/
│   │   │   ├── mod.rs
│   │   │   ├── client.rs
│   │   │   ├── queries.rs
│   │   │   └── mutations.rs
│   │   ├── poller.rs
│   │   ├── downloader.rs
│   │   ├── flasher/
│   │   │   ├── mod.rs
│   │   │   ├── esp32.rs
│   │   │   ├── pi5.rs
│   │   │   └── android.rs
│   │   ├── devices.rs
│   │   └── error.rs
│   ├── Cargo.toml
│   ├── Cargo.lock
│   ├── config.example.toml
│   ├── build.rs
│   └── README.md
│
├── .coder/                              # Coder template
│   ├── coder.yaml
│   └── images/
│       └── Dockerfile
│
├── docs/
│   ├── FLASH_MANAGER.md                 # This file
│   ├── AGENT_SETUP.md                   # Agent installation guide
│   ├── DEVELOPMENT.md                   # Development guide
│   ├── API.md                           # GraphQL API documentation
│   └── TROUBLESHOOTING.md               # Common issues
│
└── scripts/
    ├── setup-dev.sh                     # Development environment setup
    ├── build-all.sh                     # Build all components
    └── release-agent.sh                 # Build agent for all platforms
```

---

## Development Workflow

### Setting Up the Coder Workspace

1. **Create Coder workspace from template**
   ```bash
   coder create my-star-workspace --template star-embedded-dev
   ```

2. **Access workspace**
   - Frontend: `http://localhost:5174`
   - Backend: `http://localhost:8081`
   - GraphQL Playground: `http://localhost:8081/api/v1/graphql`

3. **Clone STAR repo** (if not already present)
   ```bash
   git clone <star-repo-url>
   cd STAR
   ```

### Setting Up the Local Flash Agent

1. **Download agent binary** from FlashManager web UI or GitHub releases

2. **Make executable** (Mac/Linux)
   ```bash
   chmod +x flash-agent
   sudo mv flash-agent /usr/local/bin/
   ```

3. **Register device**
   ```bash
   flash-agent register --name "johns-macbook" --platform MAC --capabilities ESP32,PI5
   ```

   This will:
   - Prompt for FlashManager URL
   - Create account or login
   - Register device
   - Generate API key
   - Save config to `~/.star-flash-agent.toml`

4. **Start agent**
   ```bash
   flash-agent start
   ```

### Building and Flashing Workflow

#### Option 1: Via Web UI

1. **Trigger Build**
   - Navigate to `/builds`
   - Select component (ESP32, Pi5, Android, etc.)
   - Select branch (or use default)
   - Click "Trigger Build"

2. **Monitor Build**
   - View real-time logs at `/builds/:id/logs`
   - Wait for completion

3. **Create Flash Job**
   - Navigate to `/flash`
   - Select completed build
   - Select target device (registered agent)
   - Specify target path (e.g., `/dev/ttyUSB0`)
   - Click "Create Flash Job"

4. **Agent Flashes Hardware**
   - Agent polls and detects new job
   - Downloads binary
   - Flashes local hardware
   - Reports status back

#### Option 2: Via CLI (in workspace)

```bash
# Build component
cd STAR/esp32-firmware
source ~/esp/esp-idf/export.sh
idf.py build

# Trigger flash via GraphQL mutation (using curl or graphql client)
# Or use the web UI
```

---

## Estimated Effort

| Phase | Tasks | Hours |
|-------|-------|-------|
| Phase 1: Backend Foundation | 1-5 | 8-10 |
| Phase 2: GraphQL API | 6-10 | 6-8 |
| Phase 3: Build System Integration | 11-17 | 8-10 |
| Phase 4: Frontend | 18-25 | 10-12 |
| Phase 5: Flash Agent | 26-38 | 12-15 |
| Phase 6: Coder Template | 39-42 | 4-6 |
| Phase 7: Testing & Documentation | 43-48 | 6-8 |
| **Total** | **48 tasks** | **54-69 hours** |

---

## Key Features Summary

✅ **Full-Stack Web Application** - Spring Boot + React (mirrors Projectum)
✅ **GraphQL API** - Queries, mutations, and real-time subscriptions
✅ **Real-Time Build Logs** - Stream build output via WebSocket
✅ **JWT Authentication** - For both web users and devices
✅ **Device Management** - Register and manage multiple local machines
✅ **Flash Job Queue** - Prioritized queue with status tracking
✅ **Cross-Platform Agent** - Rust binary for Mac, Windows, Linux, WSL2
✅ **Poll-Based Security** - No incoming connections needed
✅ **Multi-Component Support** - ESP32, Pi5, Android, Spring Boot backends
✅ **PostgreSQL Database** - With Flyway migrations
✅ **Professional UI** - Tailwind CSS with responsive design
✅ **Coder Template** - Complete dev environment with all toolchains

---

## Security Considerations

1. **JWT Tokens**
   - 24-hour expiration for web users
   - 30-day expiration for devices
   - Refresh token mechanism

2. **API Key Storage**
   - Hashed in database (bcrypt)
   - Never logged or displayed after initial registration

3. **Binary Downloads**
   - Authenticated endpoint
   - Only agents can download binaries
   - Checksum verification

4. **Poll-Based Architecture**
   - No incoming connections to local machine
   - Firewall-friendly
   - NAT-traversal not required

5. **Sudo Access** (for Pi5 SD flashing)
   - Agent must run with sudo privileges for `dd` command
   - Document security implications
   - Provide alternative: manual flashing

---

## Future Enhancements

### Phase 8: Advanced Features (Optional)

- **Build Caching** - Speed up subsequent builds
- **Parallel Builds** - Multiple builds simultaneously
- **Build Artifacts** - Store and manage build outputs
- **Flash Templates** - Predefined flash configurations
- **Device Groups** - Flash to multiple devices
- **Scheduled Builds** - Cron-style build triggers
- **Build Notifications** - Email/Slack on completion
- **Build Analytics** - Build time trends, failure rates
- **Device Metrics** - Track flash history per device
- **Web-Based Serial Monitor** - View ESP32 serial output in browser
- **OTA Updates** - Over-the-air firmware updates for ESP32
- **Device Logs** - Stream logs from devices to backend
- **Multi-User Support** - Team collaboration features
- **Role-Based Access Control** - Admin, developer, viewer roles

---

## Questions & Next Steps

Before implementation, please confirm:

1. ✅ Architecture approved? (Spring Boot + React + Rust agent)
2. ✅ GraphQL API design approved?
3. ✅ Database schema approved?
4. ✅ Agent architecture approved? (poll-based, Rust)
5. ⏳ Any additional requirements or constraints?
6. ⏳ Preferred implementation order?
7. ⏳ Timeline expectations?

Once approved, we can begin Phase 1: Backend Foundation.
