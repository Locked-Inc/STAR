SUMMARY = "Eclipse Temurin JDK 21 - Prebuilt Java Development Kit"
DESCRIPTION = "Eclipse Temurin is the open source Java SE build based upon OpenJDK"
HOMEPAGE = "https://adoptium.net/"
LICENSE = "GPL-2.0-with-classpath-exception"
LIC_FILES_CHKSUM = "file://legal/java.base/LICENSE;md5=3e0b59f8fac05c3c03d4a26bbda13f8f"

# Eclipse Temurin JDK 21.0.5+11 for aarch64 Linux
PV = "21.0.5"
TEMURIN_UPDATE = "11"

SRC_URI = "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-${PV}%2B${TEMURIN_UPDATE}/OpenJDK21U-jdk_aarch64_linux_hotspot_${PV}_${TEMURIN_UPDATE}.tar.gz"
SRC_URI[sha256sum] = "6482639ed9fd22aa2e704cc366848b1b3e1586d2bf1213869c43e80bca58fe5c"

S = "${WORKDIR}/jdk-${PV}+${TEMURIN_UPDATE}"

# This is a prebuilt binary package
INSANE_SKIP:${PN} = "already-stripped libdir ldflags file-rdeps"

# Only for aarch64
COMPATIBLE_HOST = "aarch64.*-linux"

do_install() {
    # Install to /usr/lib/jvm/temurin-21
    install -d ${D}${libdir}/jvm/temurin-21
    cp -r --no-preserve=ownership ${S}/* ${D}${libdir}/jvm/temurin-21/

    # Create symlinks in /usr/bin for common tools
    install -d ${D}${bindir}
    for tool in java javac jar jshell keytool; do
        ln -sf ../lib/jvm/temurin-21/bin/${tool} ${D}${bindir}/${tool}
    done
}

# Package all JDK files
FILES:${PN} = "${libdir}/jvm ${bindir}"

# Runtime dependencies
RDEPENDS:${PN} = "zlib libgcc"

# This is an aarch64 prebuilt binary
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_SYSROOT_STRIP = "1"
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
