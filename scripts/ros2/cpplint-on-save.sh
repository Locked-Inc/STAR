#!/usr/bin/env bash
# Auto-run cpplint on save for ROS2 C++ files
# Usage: Add this to your VS Code tasks and run on file save

set -euo pipefail

file="${1:-}"

# Check if file is provided and exists
if [[ -z "$file" ]] || [[ ! -f "$file" ]]; then
    echo "Error: Input file missing or invalid: $file" >&2
    exit 1
fi

# Check if file is in star-ros2 and is a C++ file
if [[ "$file" != *"star-ros2"* ]] || [[ ! "$file" =~ \.(cpp|hpp)$ ]]; then
    exit 0
fi

# Source ROS2 setup
if [ -f "/opt/ros/jazzy/setup.bash" ]; then
    source /opt/ros/jazzy/setup.bash 2>/dev/null
fi

# Check if ament_cpplint is available
if ! command -v ament_cpplint &> /dev/null; then
    echo "[WARN]  ament_cpplint not found - skipping lint"
    exit 0
fi

# Run cpplint on the file
echo " Checking $file..."
set +e
output=$(ament_cpplint "$file" 2>&1)
exit_code=$?
set -e

echo "$output" | grep -v "^Done processing"

if [ $exit_code -eq 0 ]; then
    echo "[PASS] Style check passed"
else
    echo "[FAIL] Style violations found - see above"
    exit 1
fi
