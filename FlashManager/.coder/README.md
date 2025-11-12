# FlashManager Coder Template

Complete Coder workspace template for STAR FlashManager development with all required toolchains.

## Overview

This Coder template provides a fully-configured development environment with:

- **ESP-IDF v5.1.2** for ESP32 firmware development
- **Android SDK 33** with command-line tools and platform-tools
- **Gradle 8.5** for Android and Spring Boot builds
- **Rust (stable)** for flash agent development
- **Node.js 18** with npm for frontend development
- **PostgreSQL 15** for database
- **Java 17** (OpenJDK) for Spring Boot
- **Python 3** with esptool for ESP32 flashing

## Architecture

This template uses **Terraform with Docker provider** (not Kubernetes). The template provisions:

1. **Docker Network**: Isolated network per workspace
2. **PostgreSQL Container**: Database sidecar for FlashManager
3. **Development Container**: Main workspace with all build tools
4. **Persistent Volume**: Workspace data persists across restarts
5. **Coder Agent**: Manages workspace lifecycle and apps

## Quick Start

### 1. Build Docker Image

**Important**: Build the development image BEFORE uploading the template.

```bash
cd /home/coder/Projectum/STAR/FlashManager/.coder

# Option A: Build locally (for testing)
./build-and-push.sh

# Option B: Build and push to registry (recommended for production)
REGISTRY=your-registry.com ./build-and-push.sh
```

This creates the `flashmanager-dev:latest` image with all build tools.

**If using a registry:**
Edit `main.tf` and update the image name:
```hcl
resource "docker_image" "flashmanager_dev" {
  name = "your-registry.com/flashmanager-dev:latest"
}
```

### 2. Upload Template to Coder

```bash
# From this directory
cd /home/coder/Projectum/STAR/FlashManager/.coder

# Option 1: Via Coder CLI
coder templates push flashmanager-dev

# Option 2: Via Coder UI
# 1. Go to Templates → Create Template
# 2. Upload main.tf file
# 3. Name it "flashmanager-dev"
```

### 3. Create Workspace from Template

1. Go to Coder dashboard → Create Workspace
2. Select "flashmanager-dev" template
3. Configure parameters:
   - **GitHub Token** (optional): For automatic HTTPS authentication
   - **GitHub Username** (optional): Your GitHub username
   - **Auto-clone**: Enable to auto-clone repositories
   - **Auto-start**: Enable to auto-start services
4. Click "Create Workspace"

### 4. Wait for Initialization

The workspace will:
- Pull the pre-built Docker image
- Start PostgreSQL container
- Run setup-flashmanager.sh script
- Clone repositories (if enabled)
- Install dependencies
- Build flash agent

This takes ~3-5 minutes (much faster with pre-built image!).

### 5. Access Your Workspace

**Via Coder Apps (in UI):**
- **Frontend**: Click "FlashManager Frontend" app
- **Backend**: Click "FlashManager Backend" app
- **GraphQL**: Click "GraphQL API" app
- **JetBrains Gateway**: Click to open in IntelliJ/WebStorm

**Via Terminal:**
```bash
# Via Coder CLI
coder ssh flashmanager-workspace

# Or use VS Code with Coder extension
# Or access via browser at workspace URL
```

## Workspace Structure

```
/workspace/
└── STAR/
    ├── FlashManager/       # Main FlashManager application
    │   ├── backend/        # Spring Boot backend
    │   ├── frontend/       # React frontend
    │   ├── agent/          # Rust flash agent
    │   └── artifacts/      # Build artifacts storage
    ├── ESP32/              # ESP32 firmware repository (clone manually)
    ├── Pi5/                # Raspberry Pi 5 OS repository (clone manually)
    ├── Android/            # Android app repository (clone manually)
    └── Backends/           # Backend services (clone manually)
```

## Development Commands

### Quick Start Commands

```bash
# Start backend
flashmanager-backend

# Start frontend (in new terminal)
flashmanager-frontend

# Start agent (after getting API key from UI)
flashmanager-agent YOUR_API_KEY
```

### Helper Scripts

All scripts available in `/usr/local/bin/`:

| Command | Description |
|---------|-------------|
| `flashmanager-backend` | Start Spring Boot backend |
| `flashmanager-frontend` | Start React dev server |
| `flashmanager-agent KEY` | Start flash agent |
| `flash-esp32 BIN PORT` | Flash ESP32 device |
| `flash-pi5 IMG DEV` | Flash Pi5 SD card |
| `flash-android APK` | Install Android APK |

### Shell Aliases

Added to `~/.bashrc`:

```bash
# FlashManager shortcuts
fm-backend         # Start backend
fm-frontend        # Start frontend
fm-agent          # Start agent
fm-cd             # cd to FlashManager
fm-logs-backend   # View backend logs
fm-logs-frontend  # View frontend logs

# STAR project shortcuts
star-cd           # cd to STAR root
esp32-cd          # cd to ESP32
pi5-cd            # cd to Pi5
android-cd        # cd to Android
backends-cd       # cd to Backends
```

## Environment Variables

### Workspace Variables

Set by template automatically:

```bash
WORKSPACE_ID          # Unique workspace identifier
POSTGRES_HOST         # PostgreSQL hostname
DATABASE_URL          # JDBC connection string
DATABASE_USERNAME     # Database user
DATABASE_PASSWORD     # Database password
WORKSPACE_DIR         # /workspace
STAR_DIR              # /workspace/STAR
FLASHMANAGER_DIR      # /workspace/STAR/FlashManager
```

### User-Provided Variables

Via template parameters:

```bash
GITHUB_TOKEN          # GitHub PAT for HTTPS auth
GITHUB_USER           # GitHub username
AUTO_CLONE            # Auto-clone repos flag
AUTO_START_SERVICES   # Auto-start backend/frontend
```

## Database Configuration

PostgreSQL is automatically configured with:

- **Host**: Container name (e.g., `coder-user-workspace-postgres`)
- **Port**: `5432`
- **Database**: `flashmanager`
- **User**: `flashmanager`
- **Password**: `flashmanager123`
- **Connection**: Accessible from workspace via Docker network

The backend's `.env` file is auto-generated with correct connection details.

## Build Tools

### ESP-IDF (ESP32)

```bash
# Source environment
. /opt/esp-idf/export.sh

# Or it's already in ~/.bashrc
esp32-cd
idf.py build
idf.py flash -p /dev/ttyUSB0
```

### Android SDK

```bash
android-cd
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

### Rust Agent

```bash
fm-cd
cd agent
cargo build --release
./target/release/flashmanager-agent --api-key YOUR_KEY
```

## Coder Apps

The template exposes these applications in the Coder UI:

### 1. FlashManager Backend
- **URL**: `http://localhost:8081`
- **Health Check**: GraphQL endpoint
- **Auto-start**: If enabled in parameters

### 2. FlashManager Frontend
- **URL**: `http://localhost:5174`
- **Health Check**: Root endpoint
- **Auto-start**: If enabled in parameters

### 3. GraphQL API
- **URL**: `http://localhost:8081/api/v1/graphql`
- **GraphiQL**: Interactive GraphQL playground
- **WebSocket**: For subscriptions

### 4. JetBrains Gateway
- Opens IntelliJ IDEA, WebStorm, or other JetBrains IDE
- Connects directly to workspace
- Opens at `/workspace` directory

## Persistent Storage

Data persists across workspace stops/starts:

- **Workspace volume**: `/workspace` directory
  - All code and project files
  - Node modules, Cargo builds
  - Gradle cache
- **PostgreSQL volume**: Database data
  - Database persists across restarts
  - Survives workspace rebuilds

## Template Parameters

| Parameter | Description | Default | Mutable |
|-----------|-------------|---------|---------|
| `github_token` | GitHub PAT for auto-auth | `""` | Yes |
| `github_username` | GitHub username | owner name | Yes |
| `auto_clone` | Auto-clone repositories | `true` | No |
| `auto_start_services` | Auto-start backend/frontend | `false` | Yes |

## Troubleshooting

### PostgreSQL not ready

```bash
# Check if PostgreSQL container is running
docker ps | grep postgres

# Check PostgreSQL logs
docker logs coder-USER-WORKSPACE-postgres

# Test connection manually
pg_isready -h coder-USER-WORKSPACE-postgres -U flashmanager
```

### Services not starting

```bash
# Check auto-start logs
cat /tmp/flashmanager-backend.log
cat /tmp/flashmanager-frontend.log

# Check if PIDs exist
cat /tmp/flashmanager-backend.pid
cat /tmp/flashmanager-frontend.pid

# Start manually
flashmanager-backend
flashmanager-frontend
```

### Build fails

```bash
# Backend build
fm-cd
cd backend
./gradlew clean build

# Frontend build
cd frontend
rm -rf node_modules
npm install

# Agent build
cd agent
cargo clean
cargo build --release
```

### ESP-IDF not found

```bash
# Manually source ESP-IDF
. /opt/esp-idf/export.sh

# Or restart terminal (already in .bashrc)
```

### Can't clone repositories

**With HTTPS:**
- Ensure GitHub token has `repo` scope
- Check token in template parameters
- Try manual clone: `git clone https://TOKEN@github.com/ORG/REPO.git`

**With SSH:**
- Add SSH key to GitHub: `cat ~/.ssh/id_rsa.pub`
- GitHub Settings → SSH and GPG keys → New SSH key
- Test: `ssh -T git@github.com`

## Customization

### Changing Repository URLs

Edit `setup-flashmanager.sh` and update:

```bash
GIT_CLONE_URL_FLASHMANAGER="https://github.com/YOUR_ORG/FlashManager.git"
GIT_CLONE_URL_ESP32="https://github.com/YOUR_ORG/ESP32.git"
# ... etc
```

### Adding More Tools

Edit `Dockerfile` in `build-context/`:

```dockerfile
RUN apt-get update && apt-get install -y \
    your-package-here
```

Then rebuild template:

```bash
coder templates push flashmanager-dev
```

### Changing Ports

Edit `main.tf` coder_app resources:

```hcl
resource "coder_app" "backend" {
  url = "http://localhost:YOUR_PORT"
}
```

### Adding Environment Variables

Edit `main.tf` workspace container env:

```hcl
env = [
  "YOUR_VAR=your_value",
  # ...
]
```

## Template Maintenance

### Updating Template

```bash
# Make changes to main.tf or setup scripts
# Push updated template
coder templates push flashmanager-dev

# Rebuild existing workspaces
coder restart YOUR_WORKSPACE --build
```

### Version Management

```bash
# List template versions
coder templates versions list flashmanager-dev

# Rollback to previous version
coder templates versions activate flashmanager-dev VERSION_ID
```

## CI/CD Integration

### Building Docker Image in CI

```yaml
# .github/workflows/build-coder-template.yml
name: Build Coder Template
on:
  push:
    paths:
      - '.coder/**'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build Docker image
        run: |
          cd .coder/build-context
          docker build -t flashmanager-dev:${{ github.sha }} .
      - name: Push to registry
        run: |
          docker tag flashmanager-dev:${{ github.sha }} registry.example.com/flashmanager-dev:latest
          docker push registry.example.com/flashmanager-dev:latest
```

### Auto-Deploy Template

```bash
# Update main.tf to use registry image
resource "docker_image" "flashmanager_dev" {
  name = "registry.example.com/flashmanager-dev:latest"
}
```

## Performance Tips

- Use SSD-backed persistent volumes for best performance
- Allocate at least 4GB RAM, 8GB recommended
- 2 CPU cores minimum, 4 recommended for builds
- First-time workspace creation takes ~10 minutes
- Subsequent starts take ~30 seconds

## Security Considerations

- JWT secrets are auto-generated on first run
- PostgreSQL password should be changed in production
- GitHub tokens stored in Coder secrets (encrypted)
- No services exposed to external network by default
- Docker socket access restricted to user

## Support & Additional Resources

- **Main README**: See `/workspace/STAR/FlashManager/README.md`
- **Architecture**: See `/workspace/STAR/FlashManager/ARCHITECTURE.md`
- **Deployment**: See `/workspace/STAR/FlashManager/DEPLOYMENT.md`
- **Workspace Info**: `cat /workspace/workspace-info.md`

## License

This template is part of the FlashManager project. See main LICENSE file.
