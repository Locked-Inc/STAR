SUMMARY = "UTF-8 locale configuration"
DESCRIPTION = "Sets up en_US.UTF-8 locale for all users"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://locale.sh \
    file://locale-gen.sh \
    file://locale-gen.service \
"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "locale-gen.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    # Profile script for environment variables
    install -d ${D}${sysconfdir}/profile.d
    install -m 0644 ${WORKDIR}/locale.sh ${D}${sysconfdir}/profile.d/

    # Locale generation script
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/locale-gen.sh ${D}${bindir}/

    # Systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/locale-gen.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} = " \
    ${sysconfdir}/profile.d/locale.sh \
    ${bindir}/locale-gen.sh \
    ${systemd_system_unitdir}/locale-gen.service \
"

RDEPENDS:${PN} = "localedef"
