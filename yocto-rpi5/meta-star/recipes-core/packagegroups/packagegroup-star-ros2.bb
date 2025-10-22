SUMMARY = "ROS2 and Java packages for STAR robot"
DESCRIPTION = "ROS2 Jazzy and OpenJDK 21 for robotics development with Kotlin support"
LICENSE = "MIT"

inherit packagegroup

# Core ROS2 and Java packages
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
    openjdk-21 \
    openjdk-21-jre \
"

# Optional but recommended tools
RRECOMMENDS:${PN} = " \
    python3-argcomplete \
"
