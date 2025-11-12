# FlashManager

**STAR Build & Flash System** - Remote compilation server with local hardware flashing

Complete full-stack system for building embedded firmware/OS remotely and flashing to local USB hardware across different networks.

## Overview

FlashManager solves the problem of building embedded systems (ESP32, Raspberry Pi 5, Android) on a powerful remote server while flashing the binaries to local USB-connected hardware. Perfect for teams working with embedded systems where:

- Build machines are in the cloud or different network
- Hardware devices are connected to developer workstations
- Firewall rules prevent incoming connections
- Multiple developers share build infrastructure

## Architecture

```
┌─────────────────────────────────────────┐
│  Remote Coder Workspace / Cloud         │
│  ┌────────────┐  ┌──────────┐          │
│  │  Backend   │  │ Frontend │          │
│  │ Spring Boot│  │  React   │          │
│  │  GraphQL   │  │  Apollo  │          │
│  │   API      │  │  Client  │          │
│  └────┬───────┘  └──────────┘          │
│       │                                  │
│  ┌────▼──────────────┐                  │
│  │   PostgreSQL      │                  │
│  │    Database       │                  │
│  └───────────────────┘                  │
│  ┌───────────────────┐                  │
│  │  Build Services   │                  │
│  │  ESP-IDF,Buildroot│                  │
│  │  Gradle, Cargo    │                  │
│  └───────────────────┘                  │
└─────────────────────────────────────────┘
            │
            │ HTTPS/WSS
            │ (GraphQL)
            ▼
┌─────────────────────────────────────────┐
│  Local Developer Machine                │
│  ┌────────────────────────────┐         │
│  │  FlashManager Agent (Rust) │         │
│  │  - Polls for jobs          │         │
│  │  - Downloads binaries      │         │
│  │  - Flashes hardware        │         │
│  └──────────┬──────┬──────────┘         │
│             │      │                     │
│   ┌─────────▼───┐  │  ┌────────────┐    │
│   │ ESP32       │  └──► Pi5 SD Card│    │
│   │ /dev/ttyUSB0│     │ /dev/sdb   │    │
│   └─────────────┘     └────────────┘    │
│                                          │
│   ┌──────────────────┐                  │
│   │ Android Device   │                  │
│   │ via ADB          │                  │
│   └──────────────────┘                  │
└─────────────────────────────────────────┘
```

### Key Design Decisions

**Poll-Based Architecture**: Agent polls server every 5 seconds. No incoming connections needed - firewall friendly.

**Projectum-Style Full Stack**: Spring Boot + GraphQL backend, React + Apollo Client frontend, matching proven Projectum architecture.

**Rust Agent**: Cross-platform (Mac, Windows, Linux, WSL2), small binary (~2MB), low resource usage.

**JWT Authentication**: Separate tokens for web users and devices. API keys for device registration.

## Components

### 1. Backend (Spring Boot + Kotlin)

**Tech Stack:**
- Spring Boot 3.1.5
- Kotlin 1.8.22
- GraphQL (Spring GraphQL with WebSocket subscriptions)
- PostgreSQL 15 with Flyway migrations
- JWT authentication (jjwt 0.12.3)
- Gradle Kotlin DSL

**Features:**
- GraphQL API with 15 queries, 17 mutations, 6 subscriptions
- Dual authentication (web users + devices)
- Asynchronous build execution with Kotlin coroutines
- Build log streaming
- Flash job management
- Device heartbeat tracking

**Repository**: `/backend`

**Start:**
```bash
cd backend
./docker-start.sh      # Start PostgreSQL
./gradlew bootRun      # Start server on port 8081
```

### 2. Frontend (React + TypeScript)

**Tech Stack:**
- React 18.2 + TypeScript
- Vite 5.0 (build tool)
- Apollo Client 3.8 (GraphQL + subscriptions)
- React Router 6.20
- Tailwind CSS 3.3
- Playwright 1.40 (E2E testing)

**Features:**
- Authentication (login/register)
- Dashboard with real-time statistics
- Build management and triggering
- Device monitoring
- Flash job tracking
- 18 E2E tests with Playwright

**Repository**: `/frontend`

**Start:**
```bash
cd frontend
npm install
npm run dev           # Start on port 5174
npm run test:e2e      # Run E2E tests
```

### 3. Flash Agent (Rust)

**Tech Stack:**
- Rust (stable)
- reqwest (HTTP client)
- tokio (async runtime)
- anyhow (error handling)

**Features:**
- GraphQL client with JWT authentication
- Polling mechanism (configurable interval)
- ESP32 flasher (esptool.py wrapper)
- Pi5 flasher (dd wrapper)
- Android flasher (adb wrapper)
- Heartbeat mechanism
- Cross-platform (compiles to all major OSes)

**Repository**: `/agent`

**Build & Run:**
```bash
cd agent
cargo build --release
./target/release/flashmanager-agent \
  --api-key YOUR_API_KEY \
  --server-url http://localhost:8081
```

### 4. Coder Template

**Complete development environment with:**
- ESP-IDF v5.1.2 (ESP32)
- Android SDK 33 + Build Tools
- Gradle 8.5
- Rust (stable)
- Node.js 18
- PostgreSQL 15
- Java 17

**Repository**: `/.coder`

**Build:**
```bash
cd .coder
./build.sh            # Builds Docker image
```

## Quick Start

### Prerequisites

- Java 17+ (for backend)
- Node.js 18+ (for frontend)
- Rust 1.70+ (for agent)
- Docker (for PostgreSQL)
- PostgreSQL 15

### 1. Clone Repository

```bash
git clone <repository-url> FlashManager
cd FlashManager
```

### 2. Start Backend

```bash
cd backend

# Start PostgreSQL
./docker-start.sh

# Run backend
./gradlew bootRun
```

Backend will be available at:
- API: http://localhost:8081/api/v1
- GraphQL: http://localhost:8081/api/v1/graphql

### 3. Start Frontend

```bash
cd frontend
npm install
npm run dev
```

Frontend will be available at: http://localhost:5174

### 4. Register & Login

1. Open http://localhost:5174
2. Click "Register"
3. Create account
4. Login

### 5. Register a Device

1. Navigate to "Devices" page
2. Click "Register Device"
3. Enter device name and select platform/capabilities
4. **Copy the API key** (you won't see it again)

### 6. Start Agent

```bash
cd agent
cargo build --release

./target/release/flashmanager-agent \
  --api-key YOUR_API_KEY_FROM_STEP_5 \
  --server-url http://localhost:8081
```

Agent will:
- Authenticate with backend
- Start polling for flash jobs
- Send heartbeat every poll interval
- Process and flash any pending jobs

### 7. Trigger a Build

1. In frontend, go to "Builds" page
2. Click "Trigger Build"
3. Select component (ESP32, Pi5, Android, etc.)
4. Enter branch name
5. Click "Start Build"

Build will execute asynchronously on the server.

### 8. Create Flash Job

Once build completes:
1. Go to "Flash Jobs" page
2. Select completed build
3. Select target device
4. Create flash job

Agent will automatically:
- Detect the job
- Claim it
- Download binary
- Flash hardware
- Report result

## Development

### Backend Development

```bash
cd backend

# Clean build
./gradlew clean build

# Run tests
./gradlew test

# Code quality check
./gradlew detekt

# Run with hot reload
./gradlew bootRun
```

### Frontend Development

```bash
cd frontend

# Install dependencies
npm install

# Development server
npm run dev

# Build production
npm run build

# Lint
npm run lint

# Unit tests
npm run test

# E2E tests
npm run test:e2e
```

### Agent Development

```bash
cd agent

# Debug build
cargo build

# Release build (optimized)
cargo build --release

# Run tests
cargo test

# Check code
cargo clippy

# Enable debug logging
RUST_LOG=debug cargo run -- --api-key KEY
```

## Testing

### Backend Tests

```bash
cd backend
./gradlew test
```

**Test Coverage:**
- Service layer tests
- Repository tests
- GraphQL integration tests
- Security tests

### Frontend E2E Tests

```bash
cd frontend

# Start backend first
cd ../backend && ./gradlew bootRun &

# Run tests
npm run test:e2e

# Run with UI
npm run test:e2e:ui
```

**18 E2E Tests:**
- Authentication flows (6 tests)
- Dashboard navigation (6 tests)
- Build management (6 tests)

## Configuration

### Backend (.env)

```bash
# Database
DATABASE_URL=jdbc:postgresql://localhost:5433/flashmanager
DATABASE_USERNAME=flashmanager
DATABASE_PASSWORD=flashmanager123

# JWT
JWT_SECRET=your-256-bit-secret
JWT_EXPIRATION=86400000
JWT_REFRESH_EXPIRATION=2592000000

# Server
PORT=8081

# Build Paths
BUILD_ARTIFACTS_DIR=/workspace/STAR/FlashManager/artifacts
ESP32_REPO_PATH=/workspace/STAR/ESP32
PI5_REPO_PATH=/workspace/STAR/Pi5
ANDROID_REPO_PATH=/workspace/STAR/Android
BACKENDS_REPO_PATH=/workspace/STAR/Backends
```

### Agent Configuration

Via command-line arguments:
```bash
--api-key <KEY>           # Device API key (required)
--server-url <URL>        # Backend URL (default: http://localhost:8081)
--poll-interval <SECS>    # Polling interval (default: 5)
--name <NAME>             # Device name for logging
```

## Deployment

### Docker Compose

```yaml
version: '3.8'

services:
  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: flashmanager
      POSTGRES_USER: flashmanager
      POSTGRES_PASSWORD: flashmanager123
    volumes:
      - postgres-data:/var/lib/postgresql/data
    ports:
      - "5433:5432"

  backend:
    build: ./backend
    ports:
      - "8081:8081"
    environment:
      DATABASE_URL: jdbc:postgresql://postgres:5432/flashmanager
      DATABASE_USERNAME: flashmanager
      DATABASE_PASSWORD: flashmanager123
      JWT_SECRET: ${JWT_SECRET}
    depends_on:
      - postgres

  frontend:
    build: ./frontend
    ports:
      - "5174:80"
    environment:
      VITE_GRAPHQL_URL: http://backend:8081/api/v1/graphql
      VITE_GRAPHQL_WS_URL: ws://backend:8081/api/v1/graphql

volumes:
  postgres-data:
```

### Coder Workspace

Use the provided Coder template in `/.coder`:

1. Build Docker image: `cd .coder && ./build.sh`
2. Upload `coder.yaml` to Coder instance
3. Create workspace from template
4. Startup script runs automatically

## API Documentation

### GraphQL API

**Endpoint**: `http://localhost:8081/api/v1/graphql`

**GraphiQL IDE**: http://localhost:8081/api/v1/graphql (GET request)

**Schema**: 161 lines with complete type definitions

**Key Operations:**

**Queries:**
- `me`: Get current user
- `devices`: List all devices
- `buildJobs`: List build jobs with filtering
- `flashJobs`: List flash jobs
- `pollFlashJobs`: Get pending jobs for device (agent)

**Mutations:**
- `register`, `login`: Authentication
- `registerDevice`: Register new device, get API key
- `triggerBuild`: Start new build
- `createFlashJob`: Create flash job
- `deviceLogin`: Authenticate device
- `claimFlashJob`, `completeFlashJob`: Agent operations

**Subscriptions:**
- `buildJobUpdated`: Real-time build status
- `buildLogStream`: Live build logs
- `flashJobUpdated`: Real-time flash status

## Security

- **JWT Tokens**: Separate tokens for users and devices
- **API Keys**: One-time display, hashed storage
- **HTTPS**: Required for production
- **CORS**: Configurable origins
- **Authentication**: All endpoints protected except auth
- **Authorization**: Role-based (@PreAuthorize)

## Performance

- **Backend**: Handles concurrent builds via coroutines
- **Frontend**: Apollo Client caching, optimistic updates
- **Agent**: ~2MB binary, <1% CPU idle, ~10MB RAM
- **Database**: Indexed queries, connection pooling

## Troubleshooting

### Backend won't start
```bash
# Check PostgreSQL
docker ps | grep postgres

# Restart database
cd backend && ./docker-start-clean.sh

# Check logs
./gradlew bootRun --stacktrace
```

### Frontend can't connect
```bash
# Verify backend is running
curl http://localhost:8081/api/v1/graphql

# Check CORS settings in application.yml
```

### Agent authentication failed
```bash
# Verify API key is correct
# Check device hasn't been deleted
# Ensure server URL is correct

# Enable debug logging
RUST_LOG=debug ./flashmanager-agent --api-key KEY
```

### ESP32 flash fails
```bash
# Install esptool
pip install esptool

# Check device connection
ls -l /dev/ttyUSB*

# Test manually
esptool.py --port /dev/ttyUSB0 flash_id
```

### Pi5 flash fails
```bash
# Check device
lsblk

# Unmount first
sudo umount /dev/sdb*

# Requires sudo for dd
sudo chown $USER /dev/sdb
```

### Android flash fails
```bash
# Check adb
adb devices

# Enable USB debugging on device
# Try different USB cable
```

## Contributing

1. Fork the repository
2. Create feature branch
3. Make changes
4. Run tests
5. Submit pull request

## License

[Your License Here]

## Acknowledgments

Built as part of the STAR (embedded systems) project for remote compilation and local hardware flashing workflows.

## Support

For issues or questions:
- Backend: See `backend/README.md`
- Frontend: See `frontend/README.md`
- Agent: See `agent/README.md`
- Coder: See `.coder/README.md`
