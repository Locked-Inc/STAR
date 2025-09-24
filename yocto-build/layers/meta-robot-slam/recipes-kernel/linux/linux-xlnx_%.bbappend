# Linux kernel append for PYNQ-Z2 robot system
# Adds device tree and kernel configuration for robot-specific hardware

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Add robot-specific device tree
SRC_URI += " \
    file://pynqz2-robot.dts \
    file://robot-kernel.cfg \
"

# Additional device tree files
KERNEL_DEVICETREE:append = " pynqz2-robot.dtb"

# Kernel configuration fragments for robot system
do_configure:append() {
    # Merge robot-specific kernel configuration
    if [ -f "${WORKDIR}/robot-kernel.cfg" ]; then
        cat ${WORKDIR}/robot-kernel.cfg >> ${B}/.config
    fi
}

# Copy device tree source to kernel source
do_compile:prepend() {
    if [ -f "${WORKDIR}/pynqz2-robot.dts" ]; then
        cp ${WORKDIR}/pynqz2-robot.dts ${S}/arch/arm/boot/dts/
    fi
}