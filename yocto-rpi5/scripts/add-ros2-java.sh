#!/bin/bash
# Script to add ROS2 and Java support to STAR Yocto build
# This adds meta-ros for ROS2 Jazzy support

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

echo "Adding ROS2 and Java support to STAR Yocto build..."
echo ""

# Clone meta-ros if not already present
if [ ! -d "meta-ros" ]; then
    echo "Cloning meta-ros repository..."
    git clone -b scarthgap https://github.com/ros/meta-ros.git
    echo "✓ meta-ros cloned"
else
    echo "✓ meta-ros already exists"
fi

echo ""
echo "Next steps:"
echo "1. Run: source setup-environment.sh"
echo "2. Add the following layers to build/conf/bblayers.conf:"
echo "   ${ROOT_DIR}/meta-ros/meta-ros-common"
echo "   ${ROOT_DIR}/meta-ros/meta-ros2"
echo "   ${ROOT_DIR}/meta-ros/meta-ros2-jazzy"
echo ""
echo "3. Update build/conf/local.conf with ROS2 settings (see docs/ROS2_JAVA_SETUP.md)"
echo "4. Update meta-star/recipes-core/packagegroups/packagegroup-star-minimal.bb"
echo "5. Rebuild: bitbake star-minimal-image"
echo ""
echo "Or use the automated setup script: ./scripts/configure-ros2-java.sh"
