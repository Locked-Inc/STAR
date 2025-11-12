# FlashManager Development Template v1
# STAR Build & Flash System - Remote compilation with local hardware flashing
# Features:
# - Complete embedded development environment (ESP-IDF, Android SDK, Rust)
# - Isolated PostgreSQL database per workspace
# - Auto-setup with GitHub authentication
# - Spring Boot + React + Rust full-stack development
# - Build tools: ESP32, Pi5, Android, Backend services

terraform {
  required_providers {
    coder = {
      source  = "coder/coder"
      version = ">= 2.5"
    }
    docker = {
      source  = "kreuzwerker/docker"
      version = "~> 3.0"
    }
  }
}

provider "docker" {}

data "coder_provisioner" "me" {}
data "coder_workspace" "me" {}
data "coder_workspace_owner" "me" {}

# GitHub credentials for automatic Git setup
data "coder_parameter" "github_token" {
  name         = "github_token"
  display_name = "GitHub Personal Access Token"
  description  = "GitHub token for automatic repository access (optional - leave empty for SSH)"
  type         = "string"
  default      = ""
  mutable      = true
  order        = 1
}

data "coder_parameter" "github_username" {
  name         = "github_username"
  display_name = "GitHub Username"
  description  = "Your GitHub username (optional - will use workspace owner if empty)"
  type         = "string"
  default      = ""
  mutable      = true
  order        = 2
}

data "coder_parameter" "auto_clone" {
  name         = "auto_clone"
  display_name = "Auto-clone STAR Repositories"
  description  = "Automatically clone FlashManager and STAR repositories on workspace creation"
  type         = "bool"
  default      = true
  mutable      = false
  order        = 3
}

data "coder_parameter" "auto_start_services" {
  name         = "auto_start_services"
  display_name = "Auto-start Services"
  description  = "Automatically start backend and frontend services after setup"
  type         = "bool"
  default      = false
  mutable      = true
  order        = 4
}

# Unique network per workspace for complete isolation
resource "docker_network" "workspace" {
  name = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}"
}

# PostgreSQL - isolated per workspace for FlashManager
resource "docker_container" "postgres" {
  count = data.coder_workspace.me.start_count
  image = "postgres:15"
  name  = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-postgres"

  env = [
    "POSTGRES_DB=flashmanager",
    "POSTGRES_USER=flashmanager",
    "POSTGRES_PASSWORD=flashmanager123"
  ]

  networks_advanced {
    name = docker_network.workspace.name
  }

  restart = "unless-stopped"

  # Health check for PostgreSQL
  healthcheck {
    test = ["CMD-SHELL", "pg_isready -U flashmanager -d flashmanager"]
    interval = "30s"
    timeout = "10s"
    retries = 3
    start_period = "40s"
  }

  labels {
    label = "coder.workspace"
    value = data.coder_workspace.me.name
  }
  labels {
    label = "coder.owner"
    value = data.coder_workspace_owner.me.name
  }
}

# Development workspace - the main container with all build tools
resource "docker_container" "workspace" {
  count = data.coder_workspace.me.start_count
  image = docker_image.flashmanager_dev.image_id
  name  = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-workspace"

  entrypoint = ["sh", "-c", coder_agent.main.init_script]

  env = [
    "CODER_AGENT_TOKEN=${coder_agent.main.token}",
    "WORKSPACE_ID=${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}",
    "POSTGRES_HOST=coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-postgres",
    "POSTGRES_PORT=5432",
    "DATABASE_URL=jdbc:postgresql://coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-postgres:5432/flashmanager",
    "DATABASE_USERNAME=flashmanager",
    "DATABASE_PASSWORD=flashmanager123",
    # GitHub credentials for automatic authentication
    "GITHUB_TOKEN=${data.coder_parameter.github_token.value}",
    "GITHUB_USER=${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}",
    "GITHUB_EMAIL=${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}@users.noreply.github.com",
    "AUTO_CLONE=${data.coder_parameter.auto_clone.value}",
    "AUTO_START_SERVICES=${data.coder_parameter.auto_start_services.value}",
    # FlashManager specific
    "PORT=8081",
    "BUILD_ARTIFACTS_DIR=/workspace/STAR/FlashManager/artifacts"
  ]

  # Mount Docker socket for container management
  volumes {
    host_path      = "/var/run/docker.sock"
    container_path = "/var/run/docker.sock"
  }

  # Persistent storage for user files
  volumes {
    volume_name    = docker_volume.workspace_data[0].name
    container_path = "/workspace"
  }

  networks_advanced {
    name = docker_network.workspace.name
  }

  restart = "unless-stopped"

  labels {
    label = "coder.workspace"
    value = data.coder_workspace.me.name
  }
  labels {
    label = "coder.owner"
    value = data.coder_workspace_owner.me.name
  }
}

# Persistent volume for workspace data
resource "docker_volume" "workspace_data" {
  count = data.coder_workspace.me.start_count
  name  = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-data"

  labels {
    label = "coder.workspace"
    value = data.coder_workspace.me.name
  }
  labels {
    label = "coder.owner"
    value = data.coder_workspace_owner.me.name
  }
}

# FlashManager dev image
# IMPORTANT: Build and push this image BEFORE using the template:
#   cd .coder && ./build-and-push.sh
# Or push to registry:
#   REGISTRY=your-registry.com ./build-and-push.sh
# Then update the image name below to match your registry
resource "docker_image" "flashmanager_dev" {
  name = "flashmanager-dev:latest"

  # For local development: pulls or uses locally built image
  keep_locally = false

  # To use a registry image, change to:
  # name = "your-registry.com/flashmanager-dev:latest"
}

# Local values for consistent naming
locals {
  postgres_name = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-postgres"
}

resource "coder_agent" "main" {
  arch = data.coder_provisioner.me.arch
  os   = "linux"

  startup_script_behavior = "blocking"

  startup_script = <<-EOT
    #!/bin/bash
    set -e

    echo "=================================================="
    echo "🚀 FlashManager Development Workspace"
    echo "=================================================="

    # Wait for PostgreSQL to be ready
    echo "⏳ Waiting for PostgreSQL..."
    timeout=60
    elapsed=0
    while ! pg_isready -h ${local.postgres_name} -U flashmanager > /dev/null 2>&1; do
      if [ $elapsed -ge $timeout ]; then
        echo "❌ PostgreSQL failed to start within $timeout seconds"
        exit 1
      fi
      echo "  Waiting for PostgreSQL... ($elapsed/$timeout seconds)"
      sleep 2
      elapsed=$((elapsed + 2))
    done
    echo "✅ PostgreSQL: Connected"

    # Run setup script
    if [ -f /workspace/STAR/FlashManager/.coder/setup-flashmanager.sh ]; then
      echo "🔧 Running FlashManager setup..."
      bash /workspace/STAR/FlashManager/.coder/setup-flashmanager.sh
    else
      echo "⚠️ Setup script not found, skipping..."
    fi

    # Create workspace info
    cat > /workspace/workspace-info.md << 'INFO_EOF'
# FlashManager Development Workspace

## Workspace: ${data.coder_workspace_owner.me.name}/${data.coder_workspace.me.name}

### Database Connection
- **PostgreSQL**: `${local.postgres_name}:5432`
  - Database: `flashmanager`
  - User: `flashmanager`
  - Password: `flashmanager123`

### Quick Start
1. Navigate to FlashManager: `cd /workspace/STAR/FlashManager`
2. Start PostgreSQL: `cd backend && ./docker-start.sh` (or it's already running!)
3. Start backend: `./gradlew bootRun`
4. Start frontend: `cd ../frontend && npm run dev`
5. Register device and get API key from UI
6. Start agent: `cd ../agent && cargo run -- --api-key YOUR_KEY`

### Development Tools Installed
- **ESP-IDF v5.1.2** - ESP32 firmware development
- **Android SDK 33** - Android app builds
- **Gradle 8.5** - Build automation
- **Rust** (stable) - Flash agent development
- **Node.js 18** - Frontend development
- **Java 17** - Spring Boot backend
- **PostgreSQL 15** - Database

### Services
- Backend API: http://localhost:8081/api/v1
- GraphQL: http://localhost:8081/api/v1/graphql
- Frontend: http://localhost:5174

### Commands
- `flashmanager-backend` - Start backend server
- `flashmanager-frontend` - Start frontend dev server
- `flashmanager-agent` - Start flash agent (requires API key)
- `flash-esp32 <binary> <port>` - Flash ESP32 device
- `flash-pi5 <image> <device>` - Flash Pi5 SD card
- `flash-android <apk>` - Install Android APK

### Repository Structure
```
/workspace/STAR/
├── FlashManager/     # Main FlashManager application
├── ESP32/            # ESP32 firmware repository (clone manually)
├── Pi5/              # Raspberry Pi 5 OS repository (clone manually)
├── Android/          # Android app repository (clone manually)
└── Backends/         # Backend services (clone manually)
```
INFO_EOF

    echo ""
    echo "=================================================="
    echo "✅ Workspace Ready!"
    echo "=================================================="
    echo ""
    echo "📖 View workspace info: cat /workspace/workspace-info.md"
    echo ""

    # Auto-start services if requested
    if [ "${data.coder_parameter.auto_start_services.value}" = "true" ]; then
      echo "🚀 Auto-starting services..."
      cd /workspace/STAR/FlashManager

      # Start backend in background
      if [ -d "backend" ]; then
        echo "  Starting backend..."
        cd backend
        ./gradlew bootRun > /tmp/flashmanager-backend.log 2>&1 &
        echo $! > /tmp/flashmanager-backend.pid
        cd ..
      fi

      # Start frontend in background
      if [ -d "frontend" ]; then
        echo "  Starting frontend..."
        cd frontend
        npm run dev > /tmp/flashmanager-frontend.log 2>&1 &
        echo $! > /tmp/flashmanager-frontend.pid
        cd ..
      fi

      echo "✅ Services started! Check logs:"
      echo "  Backend: tail -f /tmp/flashmanager-backend.log"
      echo "  Frontend: tail -f /tmp/flashmanager-frontend.log"
    fi
  EOT

  env = {
    WORKSPACE_DIR = "/workspace"
    STAR_DIR = "/workspace/STAR"
    FLASHMANAGER_DIR = "/workspace/STAR/FlashManager"
  }
}

# Application endpoints
resource "coder_app" "backend" {
  agent_id     = coder_agent.main.id
  slug         = "backend"
  display_name = "FlashManager Backend"
  url          = "http://localhost:8081"
  icon         = "/icon/spring.svg"
  subdomain    = false
  share        = "owner"

  healthcheck {
    url       = "http://localhost:8081/api/v1/graphql"
    interval  = 30
    threshold = 3
  }
}

resource "coder_app" "frontend" {
  agent_id     = coder_agent.main.id
  slug         = "frontend"
  display_name = "FlashManager Frontend"
  url          = "http://localhost:5174"
  icon         = "/icon/react.svg"
  subdomain    = false
  share        = "owner"

  healthcheck {
    url       = "http://localhost:5174"
    interval  = 30
    threshold = 3
  }
}

resource "coder_app" "graphql" {
  agent_id     = coder_agent.main.id
  slug         = "graphql"
  display_name = "GraphQL API"
  url          = "http://localhost:8081/api/v1/graphql"
  icon         = "/icon/graphql.svg"
  subdomain    = false
  share        = "owner"

  healthcheck {
    url       = "http://localhost:8081/api/v1/graphql"
    interval  = 30
    threshold = 3
  }
}

resource "coder_app" "jetbrains_gateway" {
  agent_id     = coder_agent.main.id
  slug         = "jetbrains-gateway"
  display_name = "JetBrains Gateway"
  external     = true
  icon         = "/icon/jetbrains.svg"
  url = join("", [
    "jetbrains-gateway://connect#type=coder",
    "&owner=",
    data.coder_workspace_owner.me.name,
    "&workspace=",
    data.coder_workspace.me.name,
    "&url=",
    data.coder_workspace.me.access_url,
    "&token=$SESSION_TOKEN",
    "&folder=/workspace"
  ])
}

# Metadata for template display
resource "coder_metadata" "workspace_info" {
  resource_id = coder_agent.main.id

  item {
    key   = "PostgreSQL"
    value = "${local.postgres_name}:5432"
  }

  item {
    key   = "Database"
    value = "flashmanager"
  }

  item {
    key   = "ESP-IDF Version"
    value = "v5.1.2"
  }

  item {
    key   = "Android SDK"
    value = "33"
  }

  item {
    key   = "Rust Toolchain"
    value = "stable"
  }
}
