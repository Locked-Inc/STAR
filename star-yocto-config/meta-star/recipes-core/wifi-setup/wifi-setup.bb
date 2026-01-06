SUMMARY = "Simple WiFi setup script"
DESCRIPTION = "Easy WiFi configuration for STAR robot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://wifi-setup"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/wifi-setup ${D}${bindir}/
}

FILES:${PN} = "${bindir}/wifi-setup"
RDEPENDS:${PN} = "wpa-supplicant"
