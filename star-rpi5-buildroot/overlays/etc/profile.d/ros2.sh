# ROS2 Jazzy environment setup
# Uses Python 3.11 from /opt/python3.11

# Set Python 3.11 for ROS2
export ROS2_PYTHON=/opt/python3.11/bin/python3.11

# Add ROS2 Python packages to PYTHONPATH
if [ -n "$PYTHONPATH" ]; then
    export PYTHONPATH=/opt/ros/jazzy/lib/python3.11/site-packages:$PYTHONPATH
else
    export PYTHONPATH=/opt/ros/jazzy/lib/python3.11/site-packages
fi

# Add ROS2 binaries to PATH
export PATH=/opt/ros/jazzy/bin:$PATH

# ROS2 environment variables
export ROS_DISTRO=jazzy
export AMENT_PREFIX_PATH=/opt/ros/jazzy
export COLCON_PREFIX_PATH=/opt/ros/jazzy
export CMAKE_PREFIX_PATH=/opt/ros/jazzy

# Add ROS2 library path
if [ -n "$LD_LIBRARY_PATH" ]; then
    export LD_LIBRARY_PATH=/opt/ros/jazzy/lib:$LD_LIBRARY_PATH
else
    export LD_LIBRARY_PATH=/opt/ros/jazzy/lib
fi

# Note: Don't source ROS2's setup.bash - it uses bash-specific syntax
# The environment variables above provide equivalent functionality
