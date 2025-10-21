#!/bin/bash
# Automated script to configure ROS2 and Java in the Yocto build
# Run this after sourcing setup-environment.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

if [ ! -f "$BUILD_DIR/conf/bblayers.conf" ]; then
    echo "Error: build/conf/bblayers.conf not found"
    echo "Please run 'source setup-environment.sh' first"
    exit 1
fi

echo "Configuring ROS2 and Java for STAR Yocto build..."
echo ""

# Backup existing configs
echo "Creating backups..."
cp "$BUILD_DIR/conf/bblayers.conf" "$BUILD_DIR/conf/bblayers.conf.backup.$(date +%Y%m%d-%H%M%S)"
cp "$BUILD_DIR/conf/local.conf" "$BUILD_DIR/conf/local.conf.backup.$(date +%Y%m%d-%H%M%S)"

# Check if meta-ros layers already exist in bblayers.conf
if grep -q "meta-ros" "$BUILD_DIR/conf/bblayers.conf"; then
    echo "✓ meta-ros layers already configured in bblayers.conf"
else
    echo "Adding meta-ros layers to bblayers.conf..."

    # Add meta-ros layers before the closing quote
    sed -i '/^BBLAYERS ?= "/a \  '"$ROOT_DIR"'/meta-ros/meta-ros-common \\' "$BUILD_DIR/conf/bblayers.conf"
    sed -i '/meta-ros-common/a \  '"$ROOT_DIR"'/meta-ros/meta-ros2 \\' "$BUILD_DIR/conf/bblayers.conf"
    sed -i '/meta-ros2 \\/a \  '"$ROOT_DIR"'/meta-ros/meta-ros2-jazzy \\' "$BUILD_DIR/conf/bblayers.conf"

    echo "✓ Added meta-ros layers to bblayers.conf"
fi

# Update local.conf with ROS2 and Java settings
echo "Updating local.conf with ROS2 and Java settings..."

if ! grep -q "# ROS2 Configuration" "$BUILD_DIR/conf/local.conf"; then
    cat >> "$BUILD_DIR/conf/local.conf" << 'EOF'

#
# ROS2 Configuration
#

# ROS2 Distro - using Jazzy (compatible with Yocto Scarthgap)
ROS_DISTRO = "jazzy"

# Enable required features for ROS2
DISTRO_FEATURES:append = " systemd"

# ROS2 prefers Python 3
PREFERRED_VERSION_python3 = "3.12%"

#
# Java Configuration
#

# Add OpenJDK 21 (latest LTS)
PREFERRED_VERSION_openjdk-21 = "21%"

# Enable Java runtime
EOF
    echo "✓ Added ROS2 and Java configuration to local.conf"
else
    echo "✓ ROS2 configuration already exists in local.conf"
fi

echo ""
echo "Configuration complete!"
echo ""
echo "Next steps:"
echo "1. Update meta-star/recipes-core/packagegroups/packagegroup-star-minimal.bb"
echo "   to include ROS2 and Java packages (see docs/ROS2_JAVA_SETUP.md)"
echo "2. Rebuild: bitbake star-minimal-image"
echo ""
echo "Backups saved with timestamp in build/conf/"
