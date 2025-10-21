# Customize base-files for STAR robot

# Set hostname
hostname:pn-base-files = "star-pi5"

# Minimal /etc/issue
do_install:append() {
    echo "STAR Pi5 - Minimal Linux" > ${D}${sysconfdir}/issue
    echo "" >> ${D}${sysconfdir}/issue
}
