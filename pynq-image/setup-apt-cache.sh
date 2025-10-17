#!/bin/bash
# Setup apt-cacher-ng for faster PYNQ image builds
# This caching proxy will cache all downloaded packages locally

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}apt-cacher-ng Setup${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}ERROR: Do not run this script as root!${NC}"
    echo "Run it as a regular user with sudo privileges."
    exit 1
fi

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

# Don't cache these patterns (security updates, etc)
# RequestAppendix: X-Apt-Cacher-Ng-PassThrough: 1

# Enable web-based statistics and reporting
ReportPage: acng-report.html

# Allow access to statistics (no authentication required)
# Uncomment and set AdminAuth if you want to require authentication:
# AdminAuth: username:password
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
echo -e "${YELLOW}Service status:${NC}"
if sudo systemctl is-active --quiet apt-cacher-ng; then
    echo -e "${GREEN}✓${NC} apt-cacher-ng is running"
else
    echo -e "${RED}✗${NC} apt-cacher-ng is not running"
    echo "Check logs: sudo journalctl -u apt-cacher-ng -n 50"
    exit 1
fi

echo ""

# Show cache location and size
CACHE_DIR="/var/cache/apt-cacher-ng"
if [ -d "$CACHE_DIR" ]; then
    CACHE_SIZE=$(du -sh "$CACHE_DIR" 2>/dev/null | cut -f1 || echo "unknown")
    echo -e "${YELLOW}Cache information:${NC}"
    echo "  Location: $CACHE_DIR"
    echo "  Size:     $CACHE_SIZE"
    echo ""
fi

# Configure host system to use the cache (optional)
echo -e "${YELLOW}Do you want to configure your host system to use the cache?${NC}"
echo "This will speed up apt operations on this machine as well."
read -p "Configure host? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo 'Acquire::http::Proxy "http://127.0.0.1:3142";' | sudo tee /etc/apt/apt.conf.d/01proxy > /dev/null
    echo -e "${GREEN}✓${NC} Host system configured to use cache"
    echo ""
    echo -e "${YELLOW}To disable later, run:${NC}"
    echo "  sudo rm /etc/apt/apt.conf.d/01proxy"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓ Setup Complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}Cache is now running on:${NC}"
echo "  http://localhost:3142"
echo ""
echo -e "${YELLOW}Web interface available at:${NC}"
echo "  http://localhost:3142/acng-report.html"
echo ""
echo -e "${GREEN}IMPORTANT:${NC}"
echo "  apt-cacher-ng is now REQUIRED for builds."
echo "  The build script will not run without it."
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. Run your first build - it will populate the cache"
echo "     ./build.sh"
echo "  2. Subsequent builds will be much faster (10-20 min saved)!"
echo ""
echo -e "${YELLOW}To clear the cache (if needed):${NC}"
echo "  sudo rm -rf /var/cache/apt-cacher-ng/*"
echo ""
echo -e "${YELLOW}To disable/stop the cache:${NC}"
echo "  sudo systemctl stop apt-cacher-ng"
echo "  (Note: build.sh will fail until you restart it)"
echo ""
