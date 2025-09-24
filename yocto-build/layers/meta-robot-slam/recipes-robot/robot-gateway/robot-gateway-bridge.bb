# Robot Gateway Bridge Recipe
# Bridge between existing Java Spring Boot gateway and ROS2 robot system

DESCRIPTION = "Bridge service connecting Java Robot Gateway with ROS2 robot nodes"
SECTION = "robot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Source information - will integrate with existing RobotGateway module
SRC_URI = " \
    file://robot_gateway_bridge.py \
    file://ros2_bridge_node.py \
    file://robot-gateway-bridge.service \
    file://gateway_config.yaml \
    file://LICENSE \
"

# Dependencies
DEPENDS = "python3 python3-setuptools"
RDEPENDS:${PN} = " \
    python3 \
    python3-requests \
    python3-flask \
    python3-zmq \
    python3-yaml \
    ros2-rclpy \
    ros2-std-msgs \
    ros2-geometry-msgs \
    ros2-sensor-msgs \
    ros2-nav-msgs \
"

# Installation
do_install() {
    # Install bridge application
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/robot_gateway_bridge.py ${D}${bindir}/
    install -m 0755 ${WORKDIR}/ros2_bridge_node.py ${D}${bindir}/
    
    # Install configuration
    install -d ${D}${sysconfdir}/robot
    install -m 0644 ${WORKDIR}/gateway_config.yaml ${D}${sysconfdir}/robot/
    
    # Install systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/robot-gateway-bridge.service ${D}${systemd_system_unitdir}/
}

# Package files
FILES:${PN} = " \
    ${bindir}/robot_gateway_bridge.py \
    ${bindir}/ros2_bridge_node.py \
    ${sysconfdir}/robot/gateway_config.yaml \
    ${systemd_system_unitdir}/robot-gateway-bridge.service \
"

# Systemd integration
inherit systemd
SYSTEMD_SERVICE:${PN} = "robot-gateway-bridge.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"