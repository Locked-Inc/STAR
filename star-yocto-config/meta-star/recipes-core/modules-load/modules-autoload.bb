SUMMARY = "Auto-load kernel modules at boot"
DESCRIPTION = "Configures modules to load automatically at boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://i2c-dev.conf"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/modules-load.d
    install -m 0644 ${WORKDIR}/i2c-dev.conf ${D}${sysconfdir}/modules-load.d/
}

FILES:${PN} = "${sysconfdir}/modules-load.d"
