#!/bin/sh
# ROS2 environment setup

# Set ROS2 environment variables
export ROS_VERSION=2
export ROS_DISTRO=jazzy
export ROS_PYTHON_VERSION=3

# Add ROS2 paths if installed
if [ -d "/opt/ros/jazzy" ]; then
    export ROS_ROOT=/opt/ros/jazzy
    export PATH="$ROS_ROOT/bin:$PATH"
    export LD_LIBRARY_PATH="$ROS_ROOT/lib:$LD_LIBRARY_PATH"
    export PYTHONPATH="$ROS_ROOT/lib/python3.11/site-packages:$PYTHONPATH"
    export CMAKE_PREFIX_PATH="$ROS_ROOT:$CMAKE_PREFIX_PATH"
    export AMENT_PREFIX_PATH="$ROS_ROOT"
fi

# Set Java environment
if [ -d "/usr/lib/jvm/java-17-openjdk" ]; then
    export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
    export PATH="$JAVA_HOME/bin:$PATH"
fi
