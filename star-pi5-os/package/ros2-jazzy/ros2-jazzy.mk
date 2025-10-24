################################################################################
#
# ros2-jazzy
#
################################################################################

ROS2_JAZZY_VERSION = 1.0
ROS2_JAZZY_SITE_METHOD = local
ROS2_JAZZY_SITE = $(BR2_EXTERNAL_RPI_MINIMAL_PATH)/package/ros2-jazzy
ROS2_JAZZY_LICENSE = Apache-2.0

# This package doesn't build anything, just sets up the environment
define ROS2_JAZZY_BUILD_CMDS
	# Create ROS2 directory structure
	mkdir -p $(@D)/opt/ros/jazzy/lib
	mkdir -p $(@D)/opt/ros/jazzy/bin
	mkdir -p $(@D)/opt/ros/jazzy/share
	mkdir -p $(@D)/opt/ros/jazzy/include
endef

# Install ROS2 to target filesystem at /opt/ros/jazzy
define ROS2_JAZZY_INSTALL_TARGET_CMDS
	# Copy ROS2 directory structure
	mkdir -p $(TARGET_DIR)/opt/ros/jazzy
	if [ -d $(@D)/opt/ros/jazzy ]; then \
		cp -a $(@D)/opt/ros/jazzy/* $(TARGET_DIR)/opt/ros/jazzy/ || true; \
	fi

	# Create environment setup script
	mkdir -p $(TARGET_DIR)/etc/profile.d
	(echo '# ROS2 Jazzy environment setup'; \
	 echo 'export ROS_DISTRO=jazzy'; \
	 echo 'export ROS_VERSION=2'; \
	 echo 'export ROS_PYTHON_VERSION=3'; \
	 echo 'export AMENT_PREFIX_PATH=/opt/ros/jazzy'; \
	 echo 'export COLCON_PREFIX_PATH=/opt/ros/jazzy'; \
	 echo 'export CMAKE_PREFIX_PATH=/opt/ros/jazzy'; \
	 echo 'export PYTHONPATH=/opt/ros/jazzy/lib/python3.11/site-packages:/opt/ros/jazzy/local/lib/python3.11/dist-packages:$$PYTHONPATH'; \
	 echo 'export LD_LIBRARY_PATH=/opt/ros/jazzy/lib:$$LD_LIBRARY_PATH'; \
	 echo 'export PATH=/opt/ros/jazzy/bin:$$PATH') > $(TARGET_DIR)/etc/profile.d/ros2.sh
	chmod +x $(TARGET_DIR)/etc/profile.d/ros2.sh

	# Create README with information about ROS2
	(echo 'ROS2 Jazzy ARM64 for Raspberry Pi'; \
	 echo ''; \
	 echo 'ROS2 environment is configured automatically.'; \
	 echo 'Run "source /etc/profile.d/ros2.sh" or login again to activate.'; \
	 echo ''; \
	 echo 'To install ROS2 binaries after boot:'; \
	 echo '  Add ROS2 repository and install ros-jazzy-ros-base'; \
	 echo ''; \
	 echo 'To build ROS2 packages from source:'; \
	 echo '  All development tools are available (gcc, make, cmake, python3)') > $(TARGET_DIR)/opt/ros/jazzy/README.txt
endef

$(eval $(generic-package))
