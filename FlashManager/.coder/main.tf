# FlashManager Development Template v2
# STAR Build & Flash System - Remote compilation with local hardware flashing
#
# Simplified approach using standard Coder base image (no custom Docker image needed)
#
# Features:
# - Complete embedded development environment (ESP-IDF, Android SDK, Rust)
# - Isolated PostgreSQL database per workspace
# - Auto-setup with GitHub authentication
# - Spring Boot + React + Rust full-stack development
# - Build tools: ESP32, Pi5, Android, Backend services
# - Tools installed during workspace startup (10-15 min first time, persists in volume)

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
  image = "codercom/enterprise-base:ubuntu"
  name  = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-workspace"

  entrypoint = ["sh", "-c", "${coder_agent.main.init_script}"]

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

    # Install essential development tools
    echo "📦 Installing development dependencies..."
    sudo apt-get update
    sudo apt-get install -y \
      build-essential \
      cmake \
      git \
      curl \
      wget \
      unzip \
      vim \
      nano \
      python3 \
      python3-pip \
      python3-venv \
      libncurses5-dev \
      bc \
      rsync \
      cpio \
      openjdk-17-jdk \
      postgresql-client \
      tree \
      jq \
      htop \
      tmux \
      lsof \
      netcat-openbsd

    # Install Node.js 18 via NVM
    echo "📦 Installing Node.js 18..."
    timeout 300 curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash || echo "⚠️ NVM installation timeout"
    export NVM_DIR="$HOME/.nvm"
    [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
    nvm install 18
    nvm use 18
    nvm alias default 18

    # Add Node to PATH in bashrc
    echo 'export NVM_DIR="$HOME/.nvm"' >> ~/.bashrc
    echo '[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"' >> ~/.bashrc

    # Install ESP-IDF (ESP32 toolchain)
    echo "🔧 Installing ESP-IDF for ESP32 development..."
    export IDF_PATH=/home/coder/esp-idf
    export IDF_TOOLS_PATH=/home/coder/.espressif

    git clone --recursive --depth 1 --branch v5.1.2 \
      https://github.com/espressif/esp-idf.git $IDF_PATH
    cd $IDF_PATH
    ./install.sh esp32
    # Note: esptool is installed by ESP-IDF's install.sh

    echo 'export IDF_PATH=$HOME/esp-idf' >> ~/.bashrc
    echo 'export IDF_TOOLS_PATH=$HOME/.espressif' >> ~/.bashrc
    echo '. $IDF_PATH/export.sh' >> ~/.bashrc

    # Install Android SDK Command Line Tools
    echo "🤖 Installing Android SDK..."
    export ANDROID_HOME=/home/coder/android-sdk
    mkdir -p $ANDROID_HOME/cmdline-tools
    cd $ANDROID_HOME/cmdline-tools
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
    unzip -q commandlinetools-linux-9477386_latest.zip
    mv cmdline-tools latest
    rm commandlinetools-linux-9477386_latest.zip
    yes | latest/bin/sdkmanager --licenses || true
    latest/bin/sdkmanager "platform-tools" "platforms;android-33" "build-tools;33.0.1"

    echo 'export ANDROID_HOME=$HOME/android-sdk' >> ~/.bashrc
    echo 'export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools' >> ~/.bashrc

    # Install Rust for flash agent
    echo "🦀 Installing Rust..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source $HOME/.cargo/env
    rustup default stable

    echo 'source $HOME/.cargo/env' >> ~/.bashrc

    # Install Gradle
    echo "🏗️ Installing Gradle..."
    cd /tmp
    wget -q https://services.gradle.org/distributions/gradle-8.5-bin.zip
    unzip -q gradle-8.5-bin.zip
    sudo mv gradle-8.5 /opt/gradle
    rm gradle-8.5-bin.zip

    echo 'export GRADLE_HOME=/opt/gradle' >> ~/.bashrc
    echo 'export PATH=$PATH:$GRADLE_HOME/bin' >> ~/.bashrc
    export PATH=$PATH:/opt/gradle/bin

    # Create STAR directory structure
    echo "📁 Creating workspace structure..."
    mkdir -p /workspace/STAR/{FlashManager,ESP32,Pi5,Android,Backends}
    mkdir -p /workspace/STAR/FlashManager/artifacts

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

    # Run setup script if it exists (after cloning repo)
    cd /workspace
    cat > /tmp/setup-flashmanager.sh << 'SETUP_EOF'
${file("${path.module}/setup-flashmanager.sh")}
SETUP_EOF
    chmod +x /tmp/setup-flashmanager.sh

    echo "🔧 Running FlashManager setup..."
    /tmp/setup-flashmanager.sh

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
