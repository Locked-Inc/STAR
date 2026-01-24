#!/bin/bash
set -e

# Source ROS2 if it exists (assuming Linux/Devcontainer environment)
if [ -f /opt/ros/jazzy/setup.bash ]; then
    source /opt/ros/jazzy/setup.bash
fi

# Source the local install if it exists
if [ -f star-ros2/install/setup.bash ]; then
    source star-ros2/install/setup.bash
fi

# Run tests
echo "Running ROS 2 tests..."
cd star-ros2
colcon test --event-handlers console_direct+
colcon test-result --summary
