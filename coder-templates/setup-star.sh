#!/bin/bash
set -euo pipefail

echo "Setting up STAR ESP32 development environment..."

# Setup Git authentication
echo "🔑 Setting up Git authentication..."

# Debug: Show environment variables
echo "🐛 DEBUG: Environment variables:"
echo "  GITHUB_TOKEN: '${GITHUB_TOKEN:-<not set>}' (length: ${#GITHUB_TOKEN})"
echo "  GITHUB_USER: '${GITHUB_USER:-<not set>}'"
echo "  GITHUB_EMAIL: '${GITHUB_EMAIL:-<not set>}'"
echo "  AUTO_CLONE: '${AUTO_CLONE:-<not set>}'"
echo "  ESP32_TARGET: '${ESP32_TARGET:-<not set>}'"

# Check if we have GitHub token from template parameters
if [ -n "${GITHUB_TOKEN:-}" ] && [ "${GITHUB_TOKEN}" != "" ]; then
    echo "✅ GitHub Personal Access Token provided"
    # Configure git to use token for HTTPS
    git config --global credential.helper store
    git config --global user.name "${GITHUB_USER:-$(whoami)}"
    git config --global user.email "${GITHUB_EMAIL:-$(whoami)@example.com}"
    
    # Set up credential store
    echo "https://${GITHUB_TOKEN}@github.com" > ~/.git-credentials
    chmod 600 ~/.git-credentials
    
    # Use token directly in URL to bypass Coder's git interception
    GIT_CLONE_URL="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/Locked-Inc/STAR.git"
    echo "✅ Will use HTTPS with token authentication"
else
    echo "⚠️ No GitHub Personal Access Token provided"
    echo "📝 To use automatic HTTPS authentication, provide a GitHub token when creating workspace"
    
    # Generate SSH key if it doesn't exist
    if [ ! -f ~/.ssh/id_rsa ]; then
        echo "🔑 Generating SSH key for Git operations..."
        mkdir -p ~/.ssh
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N "" -C "coder@star-workspace"
        chmod 700 ~/.ssh
        chmod 600 ~/.ssh/id_rsa
        chmod 644 ~/.ssh/id_rsa.pub
        
        echo ""
        echo "🚨 SSH KEY SETUP REQUIRED 🚨"
        echo "================================================"
        echo "Copy the following SSH public key to your GitHub account:"
        echo "GitHub Settings → SSH and GPG keys → New SSH key"
        echo ""
        echo "SSH Public Key:"
        cat ~/.ssh/id_rsa.pub
        echo ""
        echo "================================================"
        echo ""
        echo "After adding the SSH key to GitHub, run:"
        echo "  star-clone-ssh"
        echo ""
        
        # Test SSH connection
        echo "Testing SSH connection to GitHub..."
        ssh -o StrictHostKeyChecking=no -T git@github.com || true
    fi
    
    GIT_CLONE_URL="git@github.com:Locked-Inc/STAR.git"
    echo "✅ Will use SSH authentication"
fi

# Configure Git globals if not set
git config --global init.defaultBranch main
git config --global pull.rebase false
git config --global core.autocrlf input

# Clone the repository if auto-clone is enabled and it doesn't exist
if [ "${AUTO_CLONE:-true}" = "true" ] && [ ! -d "/home/coder/STAR" ]; then
    echo "📦 Auto-cloning STAR repository..."
    cd /home/coder
    
    # Try to clone with available authentication method
    if git clone "$GIT_CLONE_URL" STAR; then
        cd STAR
        git checkout ${GIT_BRANCH:-main}
        echo "✅ Successfully cloned repository"
        
        # Create marker file to signal successful clone
        echo "$(date): Repository cloned successfully to $(pwd)" > /tmp/repo_cloned
        echo "🔄 Repository status updated"
    else
        echo "❌ Failed to clone repository"
        echo ""
        echo "Manual clone options:"
        echo "  SSH:   git clone git@github.com:Locked-Inc/STAR.git STAR"
        echo "  HTTPS: git clone https://github.com/Locked-Inc/STAR.git STAR"
        echo ""
        echo "For SSH: Make sure you've added your SSH key to GitHub"
        echo "For HTTPS: You'll be prompted for credentials"
        
        # Create placeholder directory so script doesn't fail completely
        mkdir -p /home/coder/STAR
        cd /home/coder/STAR
        echo "⚠️ Repository clone failed - manual setup required"
        exit 0
    fi
elif [ -d "/home/coder/STAR" ]; then
    echo "📦 STAR repository already exists, pulling latest changes..."
    cd /home/coder/STAR
    git pull origin ${GIT_BRANCH:-main} || echo "⚠️ Failed to pull latest changes - check authentication"
else
    echo "⚠️ Auto-clone disabled - repository not cloned automatically"
    echo "To clone manually, run:"
    echo "  git clone https://github.com/Locked-Inc/STAR.git ~/STAR"
    echo "  cd ~/STAR && git checkout ${GIT_BRANCH:-main}"
    mkdir -p /home/coder/STAR
fi

# Setup ESP-IDF project structure if needed
echo "🔧 Setting up ESP-IDF project structure..."
cd /home/coder/STAR

# Create basic ESP-IDF project structure if it doesn't exist
if [ ! -f "CMakeLists.txt" ]; then
    echo "📁 Creating ESP-IDF project structure..."
    
    # Create main CMakeLists.txt
    cat > CMakeLists.txt << 'CMAKE_EOF'
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(star_esp32)
CMAKE_EOF
    
    # Create main directory and component
    mkdir -p main
    
    cat > main/CMakeLists.txt << 'MAIN_CMAKE_EOF'
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS ".")
MAIN_CMAKE_EOF
    
    # Create basic main.c if it doesn't exist
    if [ ! -f "main/main.c" ]; then
        cat > main/main.c << 'MAIN_C_EOF'
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "STAR";

void app_main(void)
{
    ESP_LOGI(TAG, "STAR ESP32 Application Starting...");
    
    while (1) {
        ESP_LOGI(TAG, "STAR is running...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
MAIN_C_EOF
    fi
    
    # Create sdkconfig.defaults for ESP32 target
    cat > sdkconfig.defaults << 'SDKCONFIG_EOF'
# STAR ESP32 Configuration

# ESP32 Target Configuration
CONFIG_IDF_TARGET_ESP32=y

# Serial monitor configuration
CONFIG_ESPTOOLPY_MONITOR_BAUD_115200B=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
SDKCONFIG_EOF
    
    echo "✅ Created ESP-IDF project structure"
else
    echo "✅ ESP-IDF project structure already exists"
fi

# Create development scripts
echo "🔧 Creating development scripts..."

# Create ESP32 build script
cat > /tmp/star-build << 'BUILD_SCRIPT'
#!/bin/bash
set -e

echo "🔨 Building STAR ESP32 project..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Are you in the STAR project directory?"
    echo "Run: cd ~/STAR"
    exit 1
fi

# Activate ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
    source $HOME/esp-idf/export.sh
else
    echo "❌ Error: ESP-IDF not found. Run setup again."
    exit 1
fi

# Build the project
idf.py build
BUILD_SCRIPT

# Create ESP32 flash script
cat > /tmp/star-flash << 'FLASH_SCRIPT'
#!/bin/bash
set -e

echo "⚡ Flashing STAR to ESP32..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Are you in the STAR project directory?"
    echo "Run: cd ~/STAR"
    exit 1
fi

# Activate ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
    source $HOME/esp-idf/export.sh
else
    echo "❌ Error: ESP-IDF not found. Run setup again."
    exit 1
fi

# Flash the project
idf.py flash
FLASH_SCRIPT

# Create ESP32 monitor script
cat > /tmp/star-monitor << 'MONITOR_SCRIPT'
#!/bin/bash
set -e

echo "📺 Starting STAR serial monitor..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Are you in the STAR project directory?"
    echo "Run: cd ~/STAR"
    exit 1
fi

# Activate ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
    source $HOME/esp-idf/export.sh
else
    echo "❌ Error: ESP-IDF not found. Run setup again."
    exit 1
fi

# Start monitor
idf.py monitor
MONITOR_SCRIPT

# Create all-in-one script
cat > /tmp/star-dev << 'DEV_SCRIPT'
#!/bin/bash
set -e

echo "🚀 STAR ESP32 Development - Build, Flash, and Monitor"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Are you in the STAR project directory?"
    echo "Run: cd ~/STAR"
    exit 1
fi

# Activate ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
    source $HOME/esp-idf/export.sh
else
    echo "❌ Error: ESP-IDF not found. Run setup again."
    exit 1
fi

echo "🔨 Building project..."
idf.py build

echo "⚡ Flashing to ESP32..."
idf.py flash

echo "📺 Starting monitor (Ctrl+] to exit)..."
idf.py monitor
DEV_SCRIPT

# Create clone scripts
cat > /tmp/star-clone-ssh << 'CLONE_SSH_SCRIPT'
#!/bin/bash
echo "📦 Cloning STAR via SSH..."
cd /home/coder
if [ -d "STAR" ]; then
    echo "⚠️ STAR directory already exists"
    echo "Remove it first: rm -rf STAR"
    exit 1
fi

if git clone git@github.com:Locked-Inc/STAR.git STAR; then
    cd STAR
    git checkout ${GIT_BRANCH:-main}
    echo "✅ Repository cloned successfully via SSH"
else
    echo "❌ SSH clone failed. Make sure your SSH key is added to GitHub:"
    echo "1. Copy your public key: cat ~/.ssh/id_rsa.pub"
    echo "2. Add it to GitHub: Settings → SSH and GPG keys → New SSH key"
fi
CLONE_SSH_SCRIPT

cat > /tmp/star-clone-https << 'CLONE_HTTPS_SCRIPT'
#!/bin/bash
echo "📦 Cloning STAR via HTTPS..."
cd /home/coder
if [ -d "STAR" ]; then
    echo "⚠️ STAR directory already exists"
    echo "Remove it first: rm -rf STAR"
    exit 1
fi

if git clone https://github.com/Locked-Inc/STAR.git STAR; then
    cd STAR
    git checkout ${GIT_BRANCH:-main}
    echo "✅ Repository cloned successfully via HTTPS"
else
    echo "❌ HTTPS clone failed. You'll be prompted for GitHub credentials."
    echo "Use your GitHub username and personal access token as password."
fi
CLONE_HTTPS_SCRIPT

# Create help script
cat > /tmp/star-help << 'HELP_SCRIPT'
#!/bin/bash
echo "🌟 STAR ESP32 Development Environment - Command Reference"
echo "======================================================="
echo ""
echo "📊 ESP-IDF COMMANDS:"
echo "  get_idf           Activate ESP-IDF environment"
echo "  idf.py build      Build project"
echo "  idf.py flash      Flash to ESP32"
echo "  idf.py monitor    Serial monitor"
echo "  idf.py menuconfig Configure project"
echo "  idf.py clean      Clean build"
echo ""
echo "🚀 STAR SHORTCUTS:"
echo "  star-build        Build STAR project"
echo "  star-flash        Flash STAR to ESP32"
echo "  star-monitor      Start serial monitor"
echo "  star-dev          Build + Flash + Monitor (all-in-one)"
echo ""
echo "📦 REPOSITORY MANAGEMENT:"
echo "  star-clone-ssh    Clone STAR via SSH"
echo "  star-clone-https  Clone STAR via HTTPS"
echo ""
echo "🔌 HARDWARE SETUP:"
echo "  ls /dev/ttyUSB*   List USB devices"
echo "  sudo chmod 666 /dev/ttyUSB0  Fix permissions if needed"
echo ""
echo "🌐 TARGET: ${ESP32_TARGET:-esp32}"
echo "📁 Project: ~/STAR/"
echo "🔧 ESP-IDF: ~/esp-idf/"
echo ""
echo "💡 QUICK START:"
echo "1. Connect ESP32 via USB"
echo "2. cd ~/STAR"
echo "3. star-dev  (builds, flashes, and monitors)"
echo ""
echo "🆘 TROUBLESHOOTING:"
echo "  - Ensure ESP32 is connected via USB"
echo "  - Check USB permissions: sudo chmod 666 /dev/ttyUSB0"
echo "  - Hold BOOT button while pressing RESET to enter download mode"
echo "  - Use 'get_idf' before running ESP-IDF commands manually"
HELP_SCRIPT

# Install all scripts
sudo cp /tmp/star-build /usr/local/bin/star-build
sudo cp /tmp/star-flash /usr/local/bin/star-flash
sudo cp /tmp/star-monitor /usr/local/bin/star-monitor
sudo cp /tmp/star-dev /usr/local/bin/star-dev
sudo cp /tmp/star-clone-ssh /usr/local/bin/star-clone-ssh
sudo cp /tmp/star-clone-https /usr/local/bin/star-clone-https
sudo cp /tmp/star-help /usr/local/bin/star-help

sudo chmod +x /usr/local/bin/star-*

# Clean up temp files
rm /tmp/star-*

echo "✅ STAR ESP32 development environment setup completed!"
echo ""
echo "🌟 STAR ESP32 Development Ready!"
echo "================================="
echo ""
echo "🚀 QUICK START:"
echo "  cd ~/STAR"
echo "  star-dev        # Build + Flash + Monitor (all-in-one)"
echo ""
echo "🔧 INDIVIDUAL COMMANDS:"
echo "  star-build      # Build project"
echo "  star-flash      # Flash to ESP32"
echo "  star-monitor    # Serial monitor"
echo "  star-help       # Show all commands"
echo ""
echo "🔌 CONNECT ESP32:"
echo "1. Connect ESP32 development board via USB"
echo "2. Check connection: ls /dev/ttyUSB*"
echo "3. Start developing: cd ~/STAR && star-dev"
echo ""
echo "📖 For detailed help: star-help"