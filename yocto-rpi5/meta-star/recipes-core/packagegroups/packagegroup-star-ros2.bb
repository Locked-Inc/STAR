SUMMARY = "ROS2 and Java packages for STAR robot"
DESCRIPTION = "ROS2 Jazzy and OpenJDK 17 (LTS) for robotics development with Kotlin support"
LICENSE = "MIT"

inherit packagegroup

# Core ROS2 and Java packages
# Using meta-openjdk-temurin for modern Java (Eclipse Temurin binaries)
# OpenJDK 17 is an LTS release (supported until 2029)
# Also available: openjdk-8-jre, openjdk-11-jre, openjdk-21-jre
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
    openjdk-17-jre \
"

# Optional but recommended tools
RRECOMMENDS:${PN} = " \
    python3-argcomplete \
"
