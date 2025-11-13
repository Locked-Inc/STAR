# FlashManager Coder Template

Complete development environment for STAR FlashManager - a remote build system for embedded devices with local hardware flashing capabilities.

## Overview

This Coder template provides a fully-configured workspace with all the tools needed for embedded systems development across multiple platforms:

- **ESP32 Firmware** - ESP-IDF v5.1.2 toolchain
- **Raspberry Pi 5 OS** - Buildroot and flashing tools
- **Android Applications** - Android SDK 33 with build tools
- **Spring Boot Backend** - Kotlin/Java 17 with Gradle 8.5
- **React Frontend** - Node.js 18 with Vite
- **Rust Flash Agent** - Local device flashing service
- **PostgreSQL Database** - Isolated database per workspace

## Features

✅ **Complete Build Environment** - All toolchains pre-configured
✅ **Isolated PostgreSQL** - Dedicated database per workspace
✅ **Auto-Setup** - GitHub authentication and repository cloning
✅ **Persistent Storage** - Workspace data survives restarts
✅ **Coder Apps** - Access services via web UI
✅ **JetBrains Gateway** - Open in IntelliJ or WebStorm

## Quick Start

### 1. Create Workspace

1. Go to Coder Dashboard → **Create Workspace**
2. Select **"flashmanager-dev"** template
3. Configure parameters:
   - **GitHub Token** (optional): For automatic HTTPS authentication
   - **GitHub Username** (optional): Your GitHub username
   - **Auto-clone**: ✅ Enable to automatically clone repositories
   - **Auto-start**: ⬜ Enable to start services automatically
4. Click **Create Workspace**

### 2. Wait for Setup

First-time workspace creation takes **~10-15 minutes** to install:
- ESP-IDF toolchain
- Android SDK
- Rust compiler
- Gradle build system
- Node.js and npm
- All system dependencies

**Subsequent starts** take only **~30 seconds** (tools persist in workspace volume)

### 3. Access Your Workspace

**Via Coder Apps (recommended):**
- 🌐 **FlashManager Frontend** - Click to open web UI
- 🔧 **FlashManager Backend** - API server
- 📊 **GraphQL API** - Interactive GraphQL playground
- 💻 **JetBrains Gateway** - Open in IDE

**Via Terminal:**
```bash
# SSH into workspace
coder ssh flashmanager-workspace

# Or use VS Code with Coder extension
```

## Workspace Structure

```
/workspace/STAR/
├── FlashManager/          # Main application
│   ├── backend/          # Spring Boot API (Kotlin)
│   ├── frontend/         # React UI (TypeScript)
│   ├── agent/            # Flash agent (Rust)
│   └── artifacts/        # Build output storage
├── ESP32/                # ESP32 firmware repos (clone manually)
├── Pi5/                  # Raspberry Pi 5 repos (clone manually)
├── Android/              # Android app repos (clone manually)
└── Backends/             # Backend service repos (clone manually)
```

## Development Workflow

### Start Backend & Frontend

```bash
# Terminal 1: Start backend
flashmanager-backend

# Terminal 2: Start frontend
flashmanager-frontend
```

Access the application:
- **Frontend**: http://localhost:5174
- **Backend API**: http://localhost:8081/api/v1
- **GraphQL**: http://localhost:8081/api/v1/graphql

### Register Device & Start Agent

1. Open frontend at http://localhost:5174
2. Register your device and copy the API key
3. Start the flash agent:

```bash
flashmanager-agent YOUR_API_KEY
```

The agent connects to the backend and enables remote flashing to local hardware.

## Development Commands

### Quick Commands

| Command | Description |
|---------|-------------|
| `flashmanager-backend` | Start Spring Boot backend server |
| `flashmanager-frontend` | Start React dev server with Vite |
| `flashmanager-agent <KEY>` | Start flash agent (requires API key) |
| `flash-esp32 <binary> <port>` | Flash ESP32 device |
| `flash-pi5 <image> <device>` | Flash Raspberry Pi 5 SD card |
| `flash-android <apk>` | Install Android APK via adb |

### Shell Aliases

Convenient shortcuts (available after terminal restart):

```bash
# FlashManager shortcuts
fm-backend              # Start backend
fm-frontend             # Start frontend
fm-agent               # Start agent with key
fm-cd                  # cd to FlashManager directory
fm-logs-backend        # View backend logs
fm-logs-frontend       # View frontend logs

# STAR repository shortcuts
star-cd                # cd to STAR root
esp32-cd              # cd to ESP32 directory
pi5-cd                # cd to Pi5 directory
android-cd            # cd to Android directory
backends-cd           # cd to Backends directory
```

## Building Firmware

### ESP32 Firmware

```bash
esp32-cd
cd your-esp32-project

# Build firmware
idf.py build

# Flash to device (USB serial)
flash-esp32 build/firmware.bin /dev/ttyUSB0

# Or use idf.py directly
idf.py flash -p /dev/ttyUSB0
idf.py monitor  # View serial output
```

### Raspberry Pi 5 Images

```bash
pi5-cd
cd your-pi5-project

# Build image (example with Buildroot)
make

# Flash to SD card (⚠️ DANGEROUS - will erase device!)
flash-pi5 output/images/sdcard.img /dev/sdb

# Verify device first!
lsblk  # Check which device is your SD card
```

### Android APKs

```bash
android-cd
cd your-android-project

# Build debug APK
./gradlew assembleDebug

# Install to connected device
flash-android app/build/outputs/apk/debug/app-debug.apk

# Or use adb directly
adb devices  # List connected devices
adb install -r app-debug.apk
```

## Database Access

PostgreSQL is automatically configured for each workspace:

**Connection Details:**
- **Host**: `coder-<user>-<workspace>-postgres`
- **Port**: `5432`
- **Database**: `flashmanager`
- **User**: `flashmanager`
- **Password**: `flashmanager123`

**Connect via psql:**
```bash
psql -h $POSTGRES_HOST -U flashmanager -d flashmanager
```

The backend's `.env` file is auto-configured with correct database connection details.

## Environment Variables

### Auto-Configured

These are set automatically by the template:

```bash
WORKSPACE_ID           # Unique workspace identifier
POSTGRES_HOST          # PostgreSQL container name
DATABASE_URL           # JDBC connection string
DATABASE_USERNAME      # flashmanager
DATABASE_PASSWORD      # flashmanager123
WORKSPACE_DIR          # /workspace
STAR_DIR               # /workspace/STAR
FLASHMANAGER_DIR       # /workspace/STAR/FlashManager
```

### Development Tools

These are added to `~/.bashrc` during setup:

```bash
# ESP-IDF
export IDF_PATH=$HOME/esp-idf
export IDF_TOOLS_PATH=$HOME/.espressif

# Android SDK
export ANDROID_HOME=$HOME/android-sdk
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools

# Rust
source $HOME/.cargo/env

# Gradle
export GRADLE_HOME=/opt/gradle
export PATH=$PATH:$GRADLE_HOME/bin

# Node.js (via nvm)
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
```

## Coder Apps

The template exposes these applications in the Coder web UI:

### FlashManager Frontend
- **URL**: http://localhost:5174
- **Description**: React web interface for managing builds and devices
- **Health Check**: Automatic with 30s interval

### FlashManager Backend
- **URL**: http://localhost:8081
- **Description**: Spring Boot REST API and GraphQL server
- **Health Check**: GraphQL endpoint

### GraphQL API
- **URL**: http://localhost:8081/api/v1/graphql
- **Description**: Interactive GraphQL playground (GraphiQL)
- **WebSocket**: Enabled for GraphQL subscriptions

### JetBrains Gateway
- **Description**: Opens IntelliJ IDEA, WebStorm, or other JetBrains IDE
- **Working Directory**: `/workspace`

## Troubleshooting

### Services Won't Start

**Check if services are running:**
```bash
# Check backend
curl http://localhost:8081/api/v1/graphql

# Check frontend
curl http://localhost:5174

# View logs
fm-logs-backend
fm-logs-frontend
```

**Common issues:**
- Port already in use → Stop conflicting process
- Database not ready → Wait for PostgreSQL to start
- Build failed → Check logs for compilation errors

### PostgreSQL Connection Failed

```bash
# Test database connection
pg_isready -h $POSTGRES_HOST -U flashmanager

# View PostgreSQL logs
docker logs $(docker ps | grep postgres | awk '{print $1}')

# Verify environment variables
echo $POSTGRES_HOST
echo $DATABASE_URL
```

### ESP-IDF Not Found

```bash
# Manually source ESP-IDF
. $HOME/esp-idf/export.sh

# Or restart terminal (it's in .bashrc)
exec bash

# Verify installation
idf.py --version
```

### Build Tools Missing

All tools persist in the workspace volume at `/home/coder/`.

If tools are missing after a rebuild:
```bash
# Check tool installations
which gradle   # /opt/gradle/bin/gradle
which node     # ~/.nvm/versions/node/v18.x.x/bin/node
which rustc    # ~/.cargo/bin/rustc
which idf.py   # ~/esp-idf/tools/idf.py

# Reinstall if needed (rare)
# Restart workspace with --build flag
```

### GitHub Clone Failed

**With HTTPS:**
- Ensure GitHub token has `repo` scope
- Verify token in workspace parameters
- Try manual clone:
  ```bash
  cd /workspace/STAR
  git clone https://github.com/YOUR_ORG/FlashManager.git
  ```

**With SSH:**
- Add SSH key to GitHub:
  ```bash
  cat ~/.ssh/id_rsa.pub
  # Copy and add to GitHub Settings → SSH Keys
  ```
- Test SSH connection:
  ```bash
  ssh -T git@github.com
  ```

## Performance Tips

- **RAM**: Allocate at least 8GB (ESP-IDF and Android SDK are memory-intensive)
- **CPU**: 4 cores minimum, 8 recommended for parallel builds
- **Storage**: Use SSD-backed volumes for best performance
- **Network**: Good internet connection needed for first-time setup
- **Persistence**: Keep workspace volume to avoid reinstalling tools

## Template Configuration

### Workspace Parameters

| Parameter | Description | Default | Required |
|-----------|-------------|---------|----------|
| `github_token` | GitHub Personal Access Token | `""` | No |
| `github_username` | Your GitHub username | workspace owner | No |
| `auto_clone` | Auto-clone FlashManager repo | `true` | No |
| `auto_start_services` | Auto-start backend/frontend | `false` | No |

### Persistent Storage

Data persists across workspace stops/starts/rebuilds:

- **Workspace Volume** (`/workspace/`):
  - All code and project files
  - ESP-IDF installation (~500MB)
  - Android SDK (~2GB)
  - Rust toolchain (~1GB)
  - Node modules, Cargo builds, Gradle cache

- **PostgreSQL Volume**:
  - Database data survives workspace rebuilds
  - Automatic backups recommended for production

## Security Considerations

- 🔒 JWT secrets are auto-generated on first run
- 🔒 PostgreSQL password in `.env` (change for production)
- 🔒 GitHub tokens stored in Coder (encrypted)
- 🔒 Services not exposed externally by default
- 🔒 Workspace isolation via Docker networks

**Production Recommendations:**
- Use strong database passwords
- Enable TLS for backend API
- Restrict Coder workspace access
- Regular database backups
- Keep dependencies updated

## Tool Versions

| Tool | Version | Path |
|------|---------|------|
| ESP-IDF | v5.1.2 | `~/esp-idf` |
| Android SDK | API 33 | `~/android-sdk` |
| Gradle | 8.5 | `/opt/gradle` |
| Rust | stable | `~/.cargo` |
| Node.js | 18.x | `~/.nvm` |
| Java | OpenJDK 17 | `/usr/lib/jvm/java-17-openjdk-amd64` |
| PostgreSQL | 15 | Container |

## Support

**Documentation:**
- Main README: `/workspace/STAR/FlashManager/README.md`
- Architecture docs: `/workspace/STAR/FlashManager/ARCHITECTURE.md`
- Workspace info: `/workspace/workspace-info.md`

**Common Resources:**
- ESP-IDF docs: https://docs.espressif.com/projects/esp-idf/en/v5.1.2/
- Android SDK: https://developer.android.com/studio/command-line
- Rust book: https://doc.rust-lang.org/book/

**Coder Resources:**
- Coder docs: https://coder.com/docs
- Template docs: https://coder.com/docs/templates

## License

This template is part of the FlashManager project. See main LICENSE file.
