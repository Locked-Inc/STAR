SUMMARY = "Network configuration for STAR robot"
DESCRIPTION = "Static IP on eth0 for SSH access, DHCP on wlan0 for WiFi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://10-eth0.network \
    file://20-wlan0.network \
"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/10-eth0.network ${D}${sysconfdir}/systemd/network/
    install -m 0644 ${WORKDIR}/20-wlan0.network ${D}${sysconfdir}/systemd/network/
}

FILES:${PN} = "${sysconfdir}/systemd/network/*.network"

RDEPENDS:${PN} = "systemd"
