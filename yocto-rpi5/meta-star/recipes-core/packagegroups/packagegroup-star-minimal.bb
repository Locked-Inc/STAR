SUMMARY = "Minimal package group for STAR robot"
DESCRIPTION = "Essential packages for minimal headless Raspberry Pi 5 system"
LICENSE = "MIT"

inherit packagegroup

# Runtime dependencies - these packages will be installed
RDEPENDS:${PN} = " \
    busybox \
    dropbear \
    network-config \
    star-users \
    packagegroup-base-wifi \
    iw \
    wpa-supplicant \
    wireless-regdb-static \
    linux-firmware-rpidistro-bcm43455 \
    linux-firmware-rpidistro-bcm43456 \
    opkg \
    opkg-utils \
    kernel-modules \
    i2c-tools \
    util-linux \
    tzdata \
    packagegroup-star-ros2 \
"

# Optional but useful for debugging
RRECOMMENDS:${PN} = " \
    kernel-module-i2c-dev \
    kernel-module-spi-dev \
"
