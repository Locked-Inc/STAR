SUMMARY = "Ultra-minimal headless image for STAR robot"
DESCRIPTION = "Minimal Raspberry Pi 5 image with runit, dropbear SSH, and basic networking. No bloat."
LICENSE = "MIT"

inherit core-image

# Image features - keep it minimal
IMAGE_FEATURES = ""

# Do not install recommended packages
BAD_RECOMMENDATIONS = "busybox-syslog busybox-udhcpc"

# Core packages - absolute minimum
IMAGE_INSTALL = " \
    packagegroup-core-boot \
    packagegroup-star-minimal \
    ${CORE_IMAGE_EXTRA_INSTALL} \
"

# Hostname
hostname:pn-base-files = "star-pi5"

# Root filesystem size (MB) - minimal
IMAGE_ROOTFS_SIZE ?= "512"

# Add extra free space (MB)
IMAGE_ROOTFS_EXTRA_SPACE = "256"

# Image overhead factor
IMAGE_OVERHEAD_FACTOR = "1.2"

# Compression
IMAGE_FSTYPES = "wic.bz2 wic"

# Remove unnecessary files to save space
ROOTFS_POSTPROCESS_COMMAND:append = " remove_unnecessary_files; "

remove_unnecessary_files() {
    # Remove documentation
    rm -rf ${IMAGE_ROOTFS}/usr/share/man
    rm -rf ${IMAGE_ROOTFS}/usr/share/doc
    rm -rf ${IMAGE_ROOTFS}/usr/share/info

    # Remove locale data except en_US
    if [ -d ${IMAGE_ROOTFS}/usr/share/locale ]; then
        cd ${IMAGE_ROOTFS}/usr/share/locale
        find . -maxdepth 1 -type d ! -name 'en*' ! -name '.' -exec rm -rf {} +
    fi

    # Remove build-time only files
    rm -rf ${IMAGE_ROOTFS}/usr/src
}

# License manifest
LICENSE_CREATE_PACKAGE = "1"
