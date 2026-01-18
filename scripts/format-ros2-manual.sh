#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'
# Manual ROS2 formatting instructions
# Use this when devcontainer is not available

echo "📋 Manual ROS2 Formatting Instructions"
echo "========================================"
echo ""
echo "Since ament_uncrustify requires ROS2, you need to run these commands"
echo "in an environment with ROS2 installed (devcontainer or ROS2 installation)."
echo ""
echo "Option 1: Using VS Code Devcontainer (Recommended)"
echo "---------------------------------------------------"
echo "1. Open VS Code"
echo "2. Click 'Reopen in Container' (bottom-right notification)"
echo "   Or: Cmd+Shift+P → 'Dev Containers: Reopen in Container'"
echo "3. Open a terminal in VS Code"
echo "4. Run: ./scripts/format-ros2.sh"
echo ""
echo "Option 2: Format Individual Packages Manually"
echo "----------------------------------------------"
echo "In the devcontainer terminal, run:"
echo ""

# Find all ROS2 packages
if [ -d "star-ros2/src" ]; then
    for pkg_xml in star-ros2/src/*/package.xml; do
        if [ -f "$pkg_xml" ]; then
            pkg_dir=$(dirname "$pkg_xml")
            echo "cd $pkg_dir && ament_uncrustify --reformat"
        fi
    done
fi

echo ""
echo "Option 3: Quick Docker Command (if you see vsc-star container running)"
echo "-----------------------------------------------------------------------"
echo "docker exec -it \$(docker ps --filter 'name=vsc-star' -q) /bin/bash -c 'cd /workspaces/STAR && ./scripts/format-ros2.sh'"
echo ""
