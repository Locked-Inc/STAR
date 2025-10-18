#!/bin/bash
# Combined cache setup for PYNQ builds
# Sets up apt-cacher-ng, pip cache, and git cache all at once

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}PYNQ Build Cache Setup${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""
echo -e "${BLUE}This script will set up three caching layers:${NC}"
echo -e "  1. ${YELLOW}apt-cacher-ng${NC}  - System package cache (REQUIRED)"
echo -e "  2. ${YELLOW}pip cache${NC}      - Python package cache (recommended)"
echo -e "  3. ${YELLOW}git cache${NC}      - Git repository cache (recommended)"
echo ""
echo -e "${GREEN}Benefits:${NC}"
echo "  • Significantly faster builds (10-20+ minutes saved)"
echo "  • Reduced bandwidth usage"
echo "  • Packages downloaded once, reused forever"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}ERROR: Do not run this script as root!${NC}"
    echo "Run it as a regular user with sudo privileges."
    exit 1
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Confirm setup
echo -e "${YELLOW}Do you want to set up all caches?${NC}"
echo "This will install apt-cacher-ng and configure all caching."
read -p "Continue? (y/n) " -n 1 -r
echo ""
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Setup cancelled${NC}"
    echo ""
    echo "You can set up individual caches with:"
    echo "  ./setup-apt-cache.sh   - APT cache only"
    echo "  ./setup-pip-cache.sh   - Pip cache only"
    echo "  ./setup-git-cache.sh   - Git cache only"
    exit 0
fi

# Track what was set up
SETUP_COUNT=0
FAILED_COUNT=0

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}1/3: Setting up APT Cache (apt-cacher-ng)${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Check if apt-cacher-ng is already installed
if dpkg -l | grep -q apt-cacher-ng; then
    echo -e "${YELLOW}apt-cacher-ng is already installed${NC}"
else
    echo -e "${YELLOW}Installing apt-cacher-ng...${NC}"
    sudo apt-get update
    sudo apt-get install -y apt-cacher-ng
    echo -e "${GREEN}✓${NC} apt-cacher-ng installed"
fi

echo ""

# Configure apt-cacher-ng
echo -e "${YELLOW}Configuring apt-cacher-ng...${NC}"

# Backup original config if it exists and hasn't been backed up
if [ -f /etc/apt-cacher-ng/acng.conf ] && [ ! -f /etc/apt-cacher-ng/acng.conf.backup ]; then
    sudo cp /etc/apt-cacher-ng/acng.conf /etc/apt-cacher-ng/acng.conf.backup
    echo -e "${GREEN}✓${NC} Backed up original config"
fi

# Create optimized configuration
sudo tee /etc/apt-cacher-ng/acng.conf > /dev/null <<'EOF'
# apt-cacher-ng configuration optimized for PYNQ builds

# Cache directory - change if you want to use a different location
CacheDir: /var/cache/apt-cacher-ng

# Log directory
LogDir: /var/log/apt-cacher-ng

# Port to listen on
Port:3142

# Allow Ubuntu ports (for ARM packages)
Remap-ubports: http://ports.ubuntu.com/ubuntu-ports ; http://ports.ubuntu.com/ubuntu-ports

# Standard Ubuntu remapping
Remap-ubuntu: http://archive.ubuntu.com/ubuntu ; http://archive.ubuntu.com/ubuntu

# Keep downloaded data for 30 days (increase for slower connections)
ExTreshold: 30

# Enable verbose logging (useful for debugging)
VerboseLog: 1

# Don't cache volatile files like Release files for too long
# This ensures you get security updates promptly
FreshIndexMaxAge: 27

# Passthrough for packages that shouldn't be cached
PassThroughPattern: .*

# Enable web-based statistics and reporting
ReportPage: acng-report.html
EOF

echo -e "${GREEN}✓${NC} Configuration updated"
echo ""

# Restart apt-cacher-ng
echo -e "${YELLOW}Restarting apt-cacher-ng...${NC}"
sudo systemctl restart apt-cacher-ng
sudo systemctl enable apt-cacher-ng
echo -e "${GREEN}✓${NC} apt-cacher-ng restarted and enabled"
echo ""

# Check status
if sudo systemctl is-active --quiet apt-cacher-ng; then
    echo -e "${GREEN}✓${NC} apt-cacher-ng is running"
    SETUP_COUNT=$((SETUP_COUNT + 1))
else
    echo -e "${RED}✗${NC} apt-cacher-ng is not running"
    echo "Check logs: sudo journalctl -u apt-cacher-ng -n 50"
    FAILED_COUNT=$((FAILED_COUNT + 1))
fi

echo ""
echo ""

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}2/3: Setting up Pip Cache${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Default pip cache directory
PIP_CACHE_DIR="${HOME}/.cache/pynq-build/pip"

echo -e "${YELLOW}Setting up pip cache directory...${NC}"
echo ""

# Check if cache directory already exists
if [ -d "$PIP_CACHE_DIR" ]; then
    CACHE_SIZE=$(du -sh "$PIP_CACHE_DIR" 2>/dev/null | cut -f1 || echo "0B")
    echo -e "${YELLOW}Pip cache directory already exists:${NC}"
    echo "  Location: $PIP_CACHE_DIR"
    echo "  Size:     $CACHE_SIZE"
    echo ""

    echo -e "${YELLOW}Do you want to clear the existing pip cache?${NC}"
    read -p "(y/n) " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$PIP_CACHE_DIR"
        echo -e "${GREEN}✓${NC} Pip cache cleared"
    else
        echo -e "${YELLOW}Keeping existing pip cache${NC}"
    fi
fi

# Create cache directory
mkdir -p "$PIP_CACHE_DIR"
echo -e "${GREEN}✓${NC} Pip cache directory created: $PIP_CACHE_DIR"
echo ""

# Copy existing pre-built wheels if they exist
PREBUILT_WHEELS="$SCRIPT_DIR/PYNQ/sdbuild/packages/python_packages_jammy/pre-built/root/.cache/pip/wheels"

if [ -d "$PREBUILT_WHEELS" ]; then
    echo -e "${YELLOW}Copying pre-built wheels to cache...${NC}"
    mkdir -p "$PIP_CACHE_DIR/wheels"
    cp -rf "$PREBUILT_WHEELS"/* "$PIP_CACHE_DIR/wheels/" 2>/dev/null || true
    WHEEL_COUNT=$(find "$PIP_CACHE_DIR/wheels" -name "*.whl" 2>/dev/null | wc -l)
    echo -e "${GREEN}✓${NC} Copied $WHEEL_COUNT pre-built wheels"
    echo ""
fi

# Create pip configuration file
PIP_CONFIG_FILE="$SCRIPT_DIR/.pip-cache-config"
cat > "$PIP_CONFIG_FILE" <<EOF
# pip cache configuration for PYNQ builds
# Generated by setup-cache.sh on $(date)
PIP_CACHE_DIR=$PIP_CACHE_DIR
EOF

echo -e "${GREEN}✓${NC} Pip configuration saved: $PIP_CONFIG_FILE"
SETUP_COUNT=$((SETUP_COUNT + 1))

echo ""
echo ""

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}3/3: Setting up Git Cache${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Default git cache directory
GIT_CACHE_DIR="${HOME}/.cache/pynq-build/git"

echo -e "${YELLOW}Setting up git cache directory...${NC}"
echo ""

# Check if cache directory already exists
if [ -d "$GIT_CACHE_DIR" ]; then
    CACHE_SIZE=$(du -sh "$GIT_CACHE_DIR" 2>/dev/null | cut -f1 || echo "0B")
    REPO_COUNT=$(find "$GIT_CACHE_DIR" -name "config" -path "*/refs/*" -prune -o -type f -name "config" -print 2>/dev/null | wc -l)
    echo -e "${YELLOW}Git cache directory already exists:${NC}"
    echo "  Location:     $GIT_CACHE_DIR"
    echo "  Size:         $CACHE_SIZE"
    echo "  Repositories: $REPO_COUNT"
    echo ""

    echo -e "${YELLOW}Do you want to clear the existing git cache?${NC}"
    read -p "(y/n) " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$GIT_CACHE_DIR"
        echo -e "${GREEN}✓${NC} Git cache cleared"
    else
        echo -e "${YELLOW}Keeping existing git cache${NC}"
    fi
fi

# Create cache directory
mkdir -p "$GIT_CACHE_DIR"
echo -e "${GREEN}✓${NC} Git cache directory created: $GIT_CACHE_DIR"
echo ""

# Create git configuration file
GIT_CONFIG_FILE="$SCRIPT_DIR/.git-cache-config"
cat > "$GIT_CONFIG_FILE" <<EOF
# Git cache configuration for PYNQ builds
# Generated by setup-cache.sh on $(date)
GIT_CACHE_DIR=$GIT_CACHE_DIR
EOF

echo -e "${GREEN}✓${NC} Git configuration saved: $GIT_CONFIG_FILE"
SETUP_COUNT=$((SETUP_COUNT + 1))

echo ""

# Optional: Pre-populate git cache
echo -e "${YELLOW}Do you want to pre-populate the git cache with common repositories?${NC}"
echo "This will clone several repositories (may take 10-15 minutes)."
echo "You can skip this and let the cache populate during builds."
read -p "Pre-populate cache? (y/n) " -n 1 -r
echo ""
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Pre-populating git cache...${NC}"
    echo ""

    # Function to create a reference clone
    create_reference() {
        local url=$1
        local name=$2

        echo -e "${YELLOW}Caching: $name${NC}"
        echo "  URL: $url"

        # Create a safe directory name from the URL
        local cache_name=$(echo "$url" | sed 's|https\?://||' | sed 's|/|_|g' | sed 's|\.git$||')
        local cache_path="$GIT_CACHE_DIR/$cache_name"

        if [ -d "$cache_path" ]; then
            echo "  Already cached, updating..."
            (cd "$cache_path" && git fetch --all --prune) || true
        else
            git clone --mirror "$url" "$cache_path" || {
                echo -e "${RED}  Failed to clone $name${NC}"
                return 1
            }
        fi
        echo -e "${GREEN}✓${NC} Cached"
        echo ""
    }

    # Add repositories that are cloned DURING the build process here
    # Note: PYNQ itself doesn't need to be cached since it's already a submodule

    # Cache XRT which is cloned during the build
    create_reference "https://github.com/Xilinx/XRT.git" "Xilinx XRT"

    # Add more build-time dependencies here as discovered
    # create_reference "https://github.com/some/dependency.git" "Build dependency"

    echo -e "${GREEN}✓${NC} Git cache pre-population complete"
    echo ""
fi

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${GREEN}✓ Cache Setup Complete!${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Summary
echo -e "${BLUE}Setup Summary:${NC}"
echo "  Caches configured: $SETUP_COUNT/3"
if [ $FAILED_COUNT -gt 0 ]; then
    echo -e "  ${RED}Failed: $FAILED_COUNT${NC}"
fi
echo ""

# Cache locations and sizes
echo -e "${YELLOW}Cache Locations:${NC}"
echo ""

echo "1. APT Cache:"
if sudo systemctl is-active --quiet apt-cacher-ng; then
    APT_CACHE_SIZE=$(du -sh /var/cache/apt-cacher-ng 2>/dev/null | cut -f1 || echo "0B")
    echo "   Status:   Running"
    echo "   Location: /var/cache/apt-cacher-ng"
    echo "   Size:     $APT_CACHE_SIZE"
    echo "   Web UI:   http://localhost:3142/acng-report.html"
else
    echo -e "   Status:   ${RED}Not running${NC}"
fi
echo ""

echo "2. Pip Cache:"
if [ -d "$PIP_CACHE_DIR" ]; then
    PIP_CACHE_SIZE=$(du -sh "$PIP_CACHE_DIR" 2>/dev/null | cut -f1 || echo "0B")
    echo "   Status:   Configured"
    echo "   Location: $PIP_CACHE_DIR"
    echo "   Size:     $PIP_CACHE_SIZE"
else
    echo -e "   Status:   ${RED}Not found${NC}"
fi
echo ""

echo "3. Git Cache:"
if [ -d "$GIT_CACHE_DIR" ]; then
    GIT_CACHE_SIZE=$(du -sh "$GIT_CACHE_DIR" 2>/dev/null | cut -f1 || echo "0B")
    GIT_REPO_COUNT=$(find "$GIT_CACHE_DIR" -name "config" -path "*/refs/*" -prune -o -type f -name "config" -print 2>/dev/null | wc -l)
    echo "   Status:       Configured"
    echo "   Location:     $GIT_CACHE_DIR"
    echo "   Size:         $GIT_CACHE_SIZE"
    echo "   Repositories: $GIT_REPO_COUNT"
else
    echo -e "   Status:   ${RED}Not found${NC}"
fi
echo ""

# Next steps
echo -e "${YELLOW}Next Steps:${NC}"
echo ""
echo "1. Run your first build:"
echo -e "   ${GREEN}./build.sh${NC}"
echo ""
echo "   This will populate the caches with packages and repositories."
echo ""
echo "2. Subsequent builds will be MUCH faster!"
echo "   Expected time savings:"
echo "   • APT cache:  10-15 minutes"
echo "   • Pip cache:  3-5 minutes"
echo "   • Git cache:  2-5 minutes"
echo -e "   ${GREEN}Total: 15-25 minutes saved per build!${NC}"
echo ""

# Cache management
echo -e "${YELLOW}Cache Management:${NC}"
echo ""
echo "View cache sizes:"
echo "  du -sh /var/cache/apt-cacher-ng"
echo "  du -sh ~/.cache/pynq-build/pip"
echo "  du -sh ~/.cache/pynq-build/git"
echo ""
echo "Clear individual caches:"
echo "  sudo rm -rf /var/cache/apt-cacher-ng/*  # APT cache"
echo "  rm -rf ~/.cache/pynq-build/pip          # Pip cache"
echo "  rm -rf ~/.cache/pynq-build/git          # Git cache"
echo ""
echo "Re-run individual setup scripts:"
echo "  ./setup-apt-cache.sh   # APT cache only"
echo "  ./setup-pip-cache.sh   # Pip cache only"
echo "  ./setup-git-cache.sh   # Git cache only"
echo ""

# Optional: Configure host system to use apt cache
if sudo systemctl is-active --quiet apt-cacher-ng; then
    if [ ! -f /etc/apt/apt.conf.d/01proxy ]; then
        echo -e "${YELLOW}Do you want to configure your host system to use the APT cache?${NC}"
        echo "This will speed up apt operations on this machine as well."
        read -p "Configure host? (y/n) " -n 1 -r
        echo ""
        echo ""

        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo 'Acquire::http::Proxy "http://127.0.0.1:3142";' | sudo tee /etc/apt/apt.conf.d/01proxy > /dev/null
            echo -e "${GREEN}✓${NC} Host system configured to use APT cache"
            echo ""
            echo -e "${YELLOW}To disable later, run:${NC}"
            echo "  sudo rm /etc/apt/apt.conf.d/01proxy"
            echo ""
        fi
    fi
fi

echo -e "${CYAN}========================================${NC}"
echo -e "${GREEN}All caches are ready for use!${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""
