SUMMARY = "ROS2 packages for STAR robot"
DESCRIPTION = "ROS2 Jazzy for robotics development"
LICENSE = "MIT"

inherit packagegroup

# Core ROS2 packages
RDEPENDS:${PN} = " \
    ros-base \
    ros-core \
    python3 \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    wget \
    ca-certificates \
    tar \
"

# Optional but recommended tools
RRECOMMENDS:${PN} = " \
    python3-argcomplete \
"
