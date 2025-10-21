SUMMARY = "Network configuration for STAR minimal system"
DESCRIPTION = "Basic network interfaces configuration"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://interfaces"

S = "${WORKDIR}"

do_install () {
    install -d ${D}${sysconfdir}/network
    install -m 0644 ${WORKDIR}/interfaces ${D}${sysconfdir}/network/interfaces
}

FILES:${PN} = "${sysconfdir}/network/interfaces"
