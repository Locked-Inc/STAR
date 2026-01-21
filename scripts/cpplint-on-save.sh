#!/bin/bash
# Auto-run cpplint on save for ROS2 C++ files
# Usage: Add this to your VS Code tasks and run on file save

set -e

FILE="$1"

# Check if file is in star-ros2 and is a C++ file
if [[ "$FILE" != *"star-ros2"* ]] || [[ ! "$FILE" =~ \.(cpp|hpp)$ ]]; then
    exit 0
fi

# Source ROS2 setup
if [ -f "/opt/ros/jazzy/setup.bash" ]; then
    source /opt/ros/jazzy/setup.bash 2>/dev/null
fi

# Check if ament_cpplint is available
if ! command -v ament_cpplint &> /dev/null; then
    echo "⚠️  ament_cpplint not found - skipping lint"
    exit 0
fi

# Run cpplint on the file
echo "🔍 Checking $FILE..."
if ament_cpplint "$FILE" 2>&1 | grep -v "^Done processing"; then
    echo "✅ Style check passed"
else
    echo "❌ Style violations found - see above"
    exit 1
fi
