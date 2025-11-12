#!/bin/bash
set -euo pipefail

echo "Setting up FlashManager development environment..."

# Setup Git authentication
echo "🔑 Setting up Git authentication..."

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

    # Use token directly in URL
    GIT_CLONE_URL_FLASHMANAGER="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/YOUR_ORG/FlashManager.git"
    GIT_CLONE_URL_ESP32="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/YOUR_ORG/ESP32.git"
    GIT_CLONE_URL_PI5="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/YOUR_ORG/Pi5.git"
    GIT_CLONE_URL_ANDROID="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/YOUR_ORG/Android.git"
    GIT_CLONE_URL_BACKENDS="https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/YOUR_ORG/Backends.git"
    echo "✅ Will use HTTPS with token authentication"
else
    echo "⚠️ No GitHub Personal Access Token provided"
    echo "📝 To use automatic HTTPS authentication, provide a GitHub token when creating workspace"

    # Generate SSH key if it doesn't exist
    if [ ! -f ~/.ssh/id_rsa ]; then
        echo "🔑 Generating SSH key for Git operations..."
        mkdir -p ~/.ssh
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N "" -C "coder@flashmanager-workspace"
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
    fi

    GIT_CLONE_URL_FLASHMANAGER="git@github.com:YOUR_ORG/FlashManager.git"
    GIT_CLONE_URL_ESP32="git@github.com:YOUR_ORG/ESP32.git"
    GIT_CLONE_URL_PI5="git@github.com:YOUR_ORG/Pi5.git"
    GIT_CLONE_URL_ANDROID="git@github.com:YOUR_ORG/Android.git"
    GIT_CLONE_URL_BACKENDS="git@github.com:YOUR_ORG/Backends.git"
    echo "✅ Will use SSH authentication"
fi

# Configure Git globals
git config --global init.defaultBranch main
git config --global pull.rebase false
git config --global core.autocrlf input

# Create STAR directory structure
echo "📁 Creating STAR directory structure..."
mkdir -p /workspace/STAR/{FlashManager,ESP32,Pi5,Android,Backends}
mkdir -p /workspace/STAR/FlashManager/artifacts

# Clone FlashManager repository if auto-clone is enabled
if [ "${AUTO_CLONE:-true}" = "true" ] && [ ! -d "/workspace/STAR/FlashManager/.git" ]; then
    echo "📦 Auto-cloning FlashManager repository..."
    cd /workspace/STAR

    if git clone "$GIT_CLONE_URL_FLASHMANAGER" FlashManager_tmp; then
        # Move contents to FlashManager directory
        mv FlashManager_tmp/* FlashManager/ 2>/dev/null || true
        mv FlashManager_tmp/.* FlashManager/ 2>/dev/null || true
        rm -rf FlashManager_tmp
        cd FlashManager
        echo "✅ Successfully cloned FlashManager repository"
    else
        echo "❌ Failed to clone FlashManager repository"
        echo "Manual clone: git clone <your-flashmanager-repo> /workspace/STAR/FlashManager"
    fi
fi

# Navigate to FlashManager directory
cd /workspace/STAR/FlashManager || exit 1

# Setup backend
if [ -d "backend" ]; then
    echo "🔧 Setting up FlashManager backend..."
    cd backend

    # Create .env file if it doesn't exist
    if [ ! -f ".env" ]; then
        echo "📝 Creating backend .env file..."
        cat > .env << 'ENV_EOF'
# Database Configuration
DATABASE_URL=${DATABASE_URL:-jdbc:postgresql://localhost:5432/flashmanager}
DATABASE_USERNAME=flashmanager
DATABASE_PASSWORD=flashmanager123

# JWT Configuration (auto-generated)
JWT_SECRET=$(openssl rand -base64 32)
JWT_EXPIRATION=86400000
JWT_REFRESH_EXPIRATION=2592000000

# Server Configuration
PORT=8081

# Build Configuration
BUILD_ARTIFACTS_DIR=/workspace/STAR/FlashManager/artifacts
ESP32_REPO_PATH=/workspace/STAR/ESP32
PI5_REPO_PATH=/workspace/STAR/Pi5
ANDROID_REPO_PATH=/workspace/STAR/Android
BACKENDS_REPO_PATH=/workspace/STAR/Backends

# Logging
LOG_LEVEL=INFO
ENV_EOF
        echo "✅ Created backend .env file"
    fi

    # Make scripts executable
    chmod +x gradlew 2>/dev/null || true
    chmod +x docker-start.sh 2>/dev/null || true
    chmod +x docker-start-clean.sh 2>/dev/null || true
    chmod +x start-dev.sh 2>/dev/null || true

    cd ..
fi

# Setup frontend
if [ -d "frontend" ]; then
    echo "🔧 Setting up FlashManager frontend..."
    cd frontend

    # Install dependencies if node_modules doesn't exist
    if [ ! -d "node_modules" ]; then
        echo "📦 Installing frontend dependencies..."
        npm install
        echo "✅ Frontend dependencies installed"
    fi

    cd ..
fi

# Setup agent
if [ -d "agent" ]; then
    echo "🔧 Setting up FlashManager agent..."
    cd agent

    # Build agent in release mode
    if [ ! -f "target/release/flashmanager-agent" ]; then
        echo "🦀 Building flash agent..."
        cargo build --release
        echo "✅ Flash agent built"
    fi

    cd ..
fi

# Create convenient shell scripts for development
echo "📝 Creating development helper scripts..."

cat > /usr/local/bin/flashmanager-backend << 'BACKEND_EOF'
#!/bin/bash
cd /workspace/STAR/FlashManager/backend
exec ./gradlew bootRun
BACKEND_EOF

cat > /usr/local/bin/flashmanager-frontend << 'FRONTEND_EOF'
#!/bin/bash
cd /workspace/STAR/FlashManager/frontend
exec npm run dev
FRONTEND_EOF

cat > /usr/local/bin/flashmanager-agent << 'AGENT_EOF'
#!/bin/bash
if [ -z "$1" ]; then
    echo "Usage: flashmanager-agent <API_KEY>"
    echo ""
    echo "Get your API key from the FlashManager UI:"
    echo "1. Start backend: flashmanager-backend"
    echo "2. Start frontend: flashmanager-frontend"
    echo "3. Register device and copy API key"
    exit 1
fi
cd /workspace/STAR/FlashManager/agent
exec ./target/release/flashmanager-agent --api-key "$1" --server-url http://localhost:8081
AGENT_EOF

cat > /usr/local/bin/flash-esp32 << 'ESP32_EOF'
#!/bin/bash
if [ "$#" -lt 2 ]; then
    echo "Usage: flash-esp32 <binary_path> <device_port>"
    echo "Example: flash-esp32 /path/to/firmware.bin /dev/ttyUSB0"
    exit 1
fi
. /opt/esp-idf/export.sh
esptool.py --chip esp32 --port "$2" --baud 460800 write_flash -z 0x0 "$1"
ESP32_EOF

cat > /usr/local/bin/flash-pi5 << 'PI5_EOF'
#!/bin/bash
if [ "$#" -lt 2 ]; then
    echo "Usage: flash-pi5 <image_path> <device>"
    echo "Example: flash-pi5 /path/to/image.img /dev/sdb"
    echo ""
    echo "⚠️ WARNING: This will ERASE all data on the target device!"
    exit 1
fi
echo "⚠️ About to flash $1 to $2"
echo "This will ERASE all data on $2"
read -p "Continue? (yes/no): " confirm
if [ "$confirm" = "yes" ]; then
    sudo dd if="$1" of="$2" bs=4M status=progress conv=fsync
else
    echo "Cancelled"
fi
PI5_EOF

cat > /usr/local/bin/flash-android << 'ANDROID_EOF'
#!/bin/bash
if [ -z "$1" ]; then
    echo "Usage: flash-android <apk_path>"
    echo "Example: flash-android /path/to/app.apk"
    exit 1
fi
adb install -r "$1"
ANDROID_EOF

# Make all scripts executable
chmod +x /usr/local/bin/flashmanager-*
chmod +x /usr/local/bin/flash-*

# Source ESP-IDF environment in bashrc if not already there
if ! grep -q "IDF_PATH" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# ESP-IDF environment" >> ~/.bashrc
    echo ". /opt/esp-idf/export.sh > /dev/null 2>&1" >> ~/.bashrc
fi

# Add helpful aliases
cat >> ~/.bashrc << 'ALIASES_EOF'

# FlashManager aliases
alias fm-backend='flashmanager-backend'
alias fm-frontend='flashmanager-frontend'
alias fm-agent='flashmanager-agent'
alias fm-cd='cd /workspace/STAR/FlashManager'
alias fm-logs-backend='tail -f /tmp/flashmanager-backend.log'
alias fm-logs-frontend='tail -f /tmp/flashmanager-frontend.log'

# STAR project shortcuts
alias star-cd='cd /workspace/STAR'
alias esp32-cd='cd /workspace/STAR/ESP32'
alias pi5-cd='cd /workspace/STAR/Pi5'
alias android-cd='cd /workspace/STAR/Android'
alias backends-cd='cd /workspace/STAR/Backends'
ALIASES_EOF

echo ""
echo "=================================================="
echo "✅ FlashManager Setup Complete!"
echo "=================================================="
echo ""
echo "📖 Quick Start:"
echo ""
echo "1. Start backend:"
echo "   flashmanager-backend"
echo ""
echo "2. Start frontend (in new terminal):"
echo "   flashmanager-frontend"
echo ""
echo "3. Access the application:"
echo "   - Frontend: http://localhost:5174"
echo "   - Backend: http://localhost:8081/api/v1"
echo "   - GraphQL: http://localhost:8081/api/v1/graphql"
echo ""
echo "4. Register device and get API key from UI, then:"
echo "   flashmanager-agent YOUR_API_KEY"
echo ""
echo "📝 Helper commands:"
echo "   - flashmanager-backend     - Start backend server"
echo "   - flashmanager-frontend    - Start frontend dev server"
echo "   - flashmanager-agent KEY   - Start flash agent"
echo "   - flash-esp32 BIN PORT     - Flash ESP32 device"
echo "   - flash-pi5 IMG DEV        - Flash Pi5 SD card"
echo "   - flash-android APK        - Install Android APK"
echo ""
echo "📂 Directory shortcuts:"
echo "   - fm-cd        - cd to FlashManager"
echo "   - star-cd      - cd to STAR root"
echo "   - esp32-cd     - cd to ESP32"
echo "   - pi5-cd       - cd to Pi5"
echo "   - android-cd   - cd to Android"
echo "   - backends-cd  - cd to Backends"
echo ""
echo "=================================================="
