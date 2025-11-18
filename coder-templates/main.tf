# STAR ESP32 Development Template v1
# Features:
# - ESP-IDF (Official Espressif IoT Development Framework)
# - Git integration for STAR project repository
# - VS Code with ESP32 extensions pre-configured
# - Serial monitor and debugging support
# - Build and flash automation
# - Real-time compilation and upload

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
  display_name = "Auto-clone STAR Repository"
  description  = "Automatically clone the STAR repository on workspace creation"
  type         = "bool"
  default      = true
  mutable      = false
  order        = 3
}

data "coder_parameter" "git_branch" {
  name         = "git_branch"
  display_name = "Git Branch"
  description  = "Which branch to checkout after cloning the repository"
  type         = "string"
  default      = "main"
  mutable      = true
  order        = 4
  option {
    name  = "main"
    value = "main"
  }
  option {
    name  = "development"
    value = "development"
  }
  option {
    name  = "feature-branch"
    value = "feature-branch"
  }
}

data "coder_parameter" "esp32_target" {
  name         = "esp32_target"
  display_name = "ESP32 Target"
  description  = "Target ESP32 chip for development"
  type         = "string"
  default      = "esp32"
  mutable      = true
  order        = 5
  option {
    name  = "ESP32"
    value = "esp32"
  }
  option {
    name  = "ESP32-S3"
    value = "esp32s3"
  }
  option {
    name  = "ESP32-C3"
    value = "esp32c3"
  }
  option {
    name  = "ESP32-S2"
    value = "esp32s2"
  }
}

# Unique network per workspace for complete isolation
resource "docker_network" "workspace" {
  name = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}"
}

# Development workspace - ESP32 development container
resource "docker_container" "workspace" {
  count = data.coder_workspace.me.start_count
  image = "codercom/enterprise-base:ubuntu"
  name  = "coder-${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}-workspace"
  
  entrypoint = ["sh", "-c", "${coder_agent.main.init_script}"]
  
  env = [
    "CODER_AGENT_TOKEN=${coder_agent.main.token}",
    "WORKSPACE_ID=${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}",
    # GitHub credentials for automatic authentication
    "GITHUB_TOKEN=${data.coder_parameter.github_token.value}",
    "GITHUB_USER=${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}",
    "GITHUB_EMAIL=${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}@users.noreply.github.com",
    "AUTO_CLONE=${data.coder_parameter.auto_clone.value}",
    "GIT_BRANCH=${data.coder_parameter.git_branch.value}",
    "ESP32_TARGET=${data.coder_parameter.esp32_target.value}"
  ]
  
  # Mount USB devices for ESP32 flashing (privileged mode required)
  privileged = true
  
  # Mount /dev for USB device access
  volumes {
    host_path      = "/dev"
    container_path = "/dev"
  }
  
  # Persistent storage for user files and ESP32 projects
  volumes {
    volume_name    = docker_volume.workspace_data[0].name
    container_path = "/home/coder"
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

resource "coder_agent" "main" {
  arch = data.coder_provisioner.me.arch
  os   = "linux"
  
  startup_script_behavior = "blocking"
  
  startup_script = <<-EOT
    #!/bin/bash
    set -e
    
    echo "🚀 Setting up STAR ESP32 development workspace for ${data.coder_workspace_owner.me.name}/${data.coder_workspace.me.name}"
    
    # Update system and install development tools
    sudo apt-get update
    sudo apt-get install -y \
      curl \
      git \
      build-essential \
      python3 \
      python3-pip \
      python3-venv \
      cmake \
      ninja-build \
      ccache \
      libffi-dev \
      libssl-dev \
      dfu-util \
      libusb-1.0-0-dev \
      libudev-dev \
      wget \
      unzip \
      vim \
      nano \
      tree \
      htop \
      tmux \
      screen \
      minicom \
      socat \
      netcat-openbsd
    
    # Install ESP-IDF (Espressif IoT Development Framework)
    echo "🔧 Installing ESP-IDF..."
    cd /home/coder
    git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
    cd esp-idf
    ./install.sh ${data.coder_parameter.esp32_target.value}
    
    # Add ESP-IDF to bashrc
    echo 'alias get_idf=". $HOME/esp-idf/export.sh"' >> ~/.bashrc
    echo 'export IDF_PATH="$HOME/esp-idf"' >> ~/.bashrc
    
    # Source ESP-IDF for current session
    . /home/coder/esp-idf/export.sh
    
    # Configure Git authentication
    echo "🔧 Configuring Git authentication..."
    git config --global user.name "${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}"
    git config --global user.email "${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}@users.noreply.github.com"
    git config --global init.defaultBranch main
    git config --global pull.rebase false
    
    # Setup GitHub authentication if token provided
    if [ -n "${data.coder_parameter.github_token.value}" ] && [ "${data.coder_parameter.github_token.value}" != "" ]; then
      echo "✅ GitHub Personal Access Token provided"
      git config --global credential.helper store
      echo "https://${data.coder_parameter.github_token.value}@github.com" > ~/.git-credentials
      chmod 600 ~/.git-credentials
      GIT_CLONE_URL="https://${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}:${data.coder_parameter.github_token.value}@github.com/Locked-Inc/STAR.git"
    else
      echo "⚠️ No GitHub token - will use SSH if available"
      # Generate SSH key if it doesn't exist
      if [ ! -f ~/.ssh/id_rsa ]; then
        echo "🔑 Generating SSH key..."
        mkdir -p ~/.ssh
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N "" -C "coder@star-workspace"
        chmod 700 ~/.ssh
        chmod 600 ~/.ssh/id_rsa
        chmod 644 ~/.ssh/id_rsa.pub
        echo "📋 Add this SSH key to your GitHub account:"
        cat ~/.ssh/id_rsa.pub
      fi
      GIT_CLONE_URL="git@github.com:Locked-Inc/STAR.git"
    fi
    
    # Clone STAR repository if auto-clone enabled
    if [ "${data.coder_parameter.auto_clone.value}" = "true" ] && [ ! -d "/home/coder/STAR" ]; then
      echo "📦 Cloning STAR repository..."
      cd /home/coder
      if git clone "$GIT_CLONE_URL" STAR; then
        cd STAR
        git checkout ${data.coder_parameter.git_branch.value}
        echo "✅ Successfully cloned STAR repository"
      else
        echo "❌ Failed to clone repository - manual setup required"
        mkdir -p /home/coder/STAR
      fi
    elif [ -d "/home/coder/STAR" ]; then
      echo "📦 STAR repository already exists"
      cd /home/coder/STAR
      git pull origin ${data.coder_parameter.git_branch.value} || echo "⚠️ Failed to pull latest changes"
    fi
    
    # Create ESP32 development info
    cat > /home/coder/esp32-info.md << 'INFO_EOF'
# STAR ESP32 Development Environment

## Workspace: ${data.coder_workspace_owner.me.name}/${data.coder_workspace.me.name}

### ESP32 Development Tools
- **ESP-IDF**: Official Espressif development framework
- **Target**: ${data.coder_parameter.esp32_target.value}

### Development Commands

#### ESP-IDF Commands
First activate ESP-IDF environment: `get_idf`
- `idf.py build` - Build project
- `idf.py flash` - Flash to ESP32
- `idf.py monitor` - Serial monitor
- `idf.py menuconfig` - Configure project
- `idf.py clean` - Clean build
- `idf.py fullclean` - Full clean including dependencies

### USB Device Access
Container runs in privileged mode for USB device access.
ESP32 devices should be automatically detected.

### Project Structure
- `~/STAR/` - Main project directory
- `~/esp-idf/` - ESP-IDF framework

### Serial Monitor
Use ESP-IDF monitor or alternative tools:
- `idf.py monitor` (ESP-IDF)
- `minicom -D /dev/ttyUSB0 -b 115200`
- `screen /dev/ttyUSB0 115200`

### Git Workflow
- Repository: bsikar/STAR
- Branch: ${data.coder_parameter.git_branch.value}
- Authentication: ${data.coder_parameter.github_token.value != "" ? "HTTPS Token" : "SSH Key"}

### Quick Start
1. Connect ESP32 device via USB
2. `get_idf` - Activate ESP-IDF environment
3. `cd ~/STAR`
4. `idf.py build` - Build project
5. `idf.py flash` - Upload to ESP32
6. `idf.py monitor` - View serial output

### Troubleshooting
- Check USB device: `ls /dev/ttyUSB*`
- Check permissions: `sudo chmod 666 /dev/ttyUSB0`
- Reset ESP32: Hold BOOT button while pressing RESET
INFO_EOF
    
    # Create ESP32-specific setup script
    cat > /tmp/setup-star.sh << 'SETUP_EOF'
${file("${path.module}/setup-star.sh")}
SETUP_EOF
    chmod +x /tmp/setup-star.sh
    
    echo "🔧 Setting up STAR-specific development environment..."
    /tmp/setup-star.sh
    
    echo "🎉 STAR ESP32 workspace setup complete!"
    echo "📖 See esp32-info.md for development commands"
    echo "🔌 Connect your ESP32 and start developing!"
  EOT
  
  env = {
    WORKSPACE_ID = "${data.coder_workspace_owner.me.name}-${lower(data.coder_workspace.me.name)}"
    GIT_REPO_URL = "https://github.com/Locked-Inc/STAR.git"
    GIT_BRANCH = data.coder_parameter.git_branch.value
    GITHUB_TOKEN = data.coder_parameter.github_token.value
    GITHUB_USER = data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name
    GITHUB_EMAIL = "${data.coder_parameter.github_username.value != "" ? data.coder_parameter.github_username.value : data.coder_workspace_owner.me.name}@users.noreply.github.com"
    AUTO_CLONE = data.coder_parameter.auto_clone.value ? "true" : "false"
    ESP32_TARGET = data.coder_parameter.esp32_target.value
  }

  metadata {
    display_name = "ESP-IDF Status"
    key          = "espidf"
    script       = "if [ -d \"$HOME/esp-idf\" ]; then if [ -f \"$HOME/esp-idf/version.txt\" ]; then cat \"$HOME/esp-idf/version.txt\"; else echo 'v5.1 Installed'; fi; else echo 'Not installed'; fi"
    interval     = 60
    timeout      = 10
  }
  
  metadata {
    display_name = "USB Devices"
    key          = "usb_devices"
    script       = "ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | wc -l | xargs -I {} echo '{} device(s)' || echo 'No devices'"
    interval     = 30
    timeout      = 5
  }
  
  metadata {
    display_name = "Git Repository"
    key          = "git_repo"
    script       = "if [ -d /home/coder/STAR/.git ]; then cd /home/coder/STAR && echo \"$(git rev-parse --abbrev-ref HEAD) - $(git log -1 --pretty=format:'%h %s' 2>/dev/null)\"; else echo 'Not cloned'; fi"
    interval     = 60
    timeout      = 10
  }
  
  metadata {
    display_name = "Project Status"
    key          = "project"
    script       = "if [ -f /home/coder/STAR/CMakeLists.txt ]; then echo 'ESP-IDF project ready'; elif [ -d /home/coder/STAR ]; then echo 'Directory exists'; else echo 'Not initialized'; fi"
    interval     = 60
    timeout      = 5
  }
}

# JetBrains Gateway integration for ESP32 development
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
    "&folder=/home/coder"
  ])
}