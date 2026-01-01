SUMMARY = "ROS2 Jazzy environment auto-source script"
DESCRIPTION = "Adds /etc/profile.d script to auto-source ROS2 on login"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://ros2-jazzy.sh"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/profile.d
    install -m 0644 ${WORKDIR}/ros2-jazzy.sh ${D}${sysconfdir}/profile.d/
}

FILES:${PN} = "${sysconfdir}/profile.d/ros2-jazzy.sh"
