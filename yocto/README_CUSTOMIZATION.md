# TDA4VM Image Customization Guide

This guide explains how to customize your TDA4VM Yocto build to add packages, applications, and configurations to your final image.

## 🛠️ Adding Packages to Your Image

### Method 1: Using IMAGE_INSTALL (Quick & Easy)

Edit `build/conf/local.conf` and add packages:

```bash
# Add specific packages
IMAGE_INSTALL:append = " python3 openssh vim htop"

# Add development tools
IMAGE_INSTALL:append = " gcc g++ make cmake git"

# Add multimedia tools
IMAGE_INSTALL:append = " gstreamer1.0-plugins-good v4l-utils"

# Add AI/ML packages (already in tisdk-edgeai-image)
IMAGE_INSTALL:append = " tensorflow-lite onnxruntime opencv"

# Add networking tools
IMAGE_INSTALL:append = " curl wget netcat-openbsd tcpdump"

# Add system utilities
IMAGE_INSTALL:append = " nano screen tmux tree htop iotop"
```

**Apply changes:**
```bash
make shell
cd build && . conf/setenv
MACHINE=j721e-evm bitbake tisdk-edgeai-image
```

### Method 2: Create a Custom Image Recipe

Create your own image that inherits from the EdgeAI image:

**1. Create custom layer structure:**
```bash
mkdir -p sources/meta-custom/conf
mkdir -p sources/meta-custom/recipes-core/images
```

**2. Create layer configuration** `sources/meta-custom/conf/layer.conf`:
```bash
# We have a conf and classes directory, add to BBPATH
BBPATH .= ":${LAYERDIR}"

# We have recipes-* directories, add to BBFILES
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*.bbappend"

BBFILE_COLLECTIONS += "meta-custom"
BBFILE_PATTERN_meta-custom = "^${LAYERDIR}/"
BBFILE_PRIORITY_meta-custom = "10"

LAYERDEPENDS_meta-custom = "core"
LAYERSERIES_COMPAT_meta-custom = "scarthgap"
```

**3. Create custom image** `sources/meta-custom/recipes-core/images/my-tda4vm-image.bb`:
```bash
# Inherit from EdgeAI image
require recipes-core/images/tisdk-edgeai-image.bb

SUMMARY = "Custom TDA4VM EdgeAI Image"
DESCRIPTION = "Custom TDA4VM image with additional packages and configurations"

# Add your packages
IMAGE_INSTALL += " \
    python3-pip \
    python3-numpy \
    python3-opencv \
    nodejs \
    nginx \
    can-utils \
    i2c-tools \
    gpio-utils \
    wireless-tools \
    bluez5 \
    docker \
    "

# Add custom applications (define these in your layer)
IMAGE_INSTALL += " \
    my-custom-app \
    my-startup-scripts \
    "

# Add package groups
IMAGE_INSTALL += " \
    packagegroup-core-buildessential \
    packagegroup-base-wifi \
    "

# Set root password and enable SSH (development only!)
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-openssh"

# Custom image features
IMAGE_FEATURES += "package-management"

# Set image size (MB)
IMAGE_ROOTFS_SIZE = "8192"
```

**4. Add layer to build** - Edit `build/conf/bblayers.conf`:
```bash
BBLAYERS += "/home/yocto/yocto-workspace/sources/meta-custom"
```

**5. Build custom image:**
```bash
make shell
cd build && . conf/setenv
MACHINE=j721e-evm bitbake my-tda4vm-image
```

### Method 3: Package Groups (Categories)

Add entire categories of software by editing `build/conf/local.conf`:

```bash
# Development tools
IMAGE_INSTALL:append = " packagegroup-core-buildessential"

# Debug and profiling tools  
IMAGE_INSTALL:append = " packagegroup-core-tools-debug"

# WiFi support
IMAGE_INSTALL:append = " packagegroup-base-wifi"

# Bluetooth support
IMAGE_INSTALL:append = " packagegroup-base-bluetooth"

# TI-specific multimedia packages
IMAGE_INSTALL:append = " packagegroup-arago-tisdk-multimedia"

# TI SDK add-ons
IMAGE_INSTALL:append = " packagegroup-arago-tisdk-addons"
```

## 📁 Custom Applications and Scripts

### Adding a Custom Application

**1. Create recipe directory:**
```bash
mkdir -p sources/meta-custom/recipes-custom/my-app/files
```

**2. Create your application** `sources/meta-custom/recipes-custom/my-app/files/my-app.py`:
```python
#!/usr/bin/env python3
"""
Custom TDA4VM Application
"""
import time
import sys

def main():
    print("TDA4VM Custom Application Starting...")
    while True:
        print(f"Running at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        time.sleep(60)

if __name__ == "__main__":
    main()
```

**3. Create systemd service** `sources/meta-custom/recipes-custom/my-app/files/my-app.service`:
```ini
[Unit]
Description=My Custom TDA4VM Application
After=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/bin/my-app
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**4. Create recipe** `sources/meta-custom/recipes-custom/my-app/my-app.bb`:
```bash
SUMMARY = "My custom TDA4VM application"
DESCRIPTION = "Custom application that runs on TDA4VM at startup"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://my-app.py \
           file://my-app.service"

S = "${WORKDIR}"

inherit systemd

do_install() {
    # Install the application
    install -d ${D}${bindir}
    install -m 0755 my-app.py ${D}${bindir}/my-app
    
    # Install systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 my-app.service ${D}${systemd_system_unitdir}
}

FILES:${PN} = "${bindir}/my-app ${systemd_system_unitdir}/my-app.service"

SYSTEMD_SERVICE:${PN} = "my-app.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

RDEPENDS:${PN} = "python3"
```

### Adding Custom Configuration Files

**1. Create config recipe directory:**
```bash
mkdir -p sources/meta-custom/recipes-custom/my-configs/files
```

**2. Create config files** `sources/meta-custom/recipes-custom/my-configs/files/`:
```bash
# custom.conf - Custom application configuration
CUSTOM_SETTING=value
LOG_LEVEL=info

# network-setup.sh - Custom network setup script
#!/bin/bash
echo "Setting up custom network configuration..."
# Add your network setup here
```

**3. Create config recipe** `sources/meta-custom/recipes-custom/my-configs/my-configs.bb`:
```bash
SUMMARY = "Custom configuration files for TDA4VM"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://custom.conf \
           file://network-setup.sh"

S = "${WORKDIR}"

do_install() {
    # Install configuration files
    install -d ${D}${sysconfdir}/my-app
    install -m 0644 custom.conf ${D}${sysconfdir}/my-app/
    
    # Install scripts
    install -d ${D}${bindir}
    install -m 0755 network-setup.sh ${D}${bindir}/
}

FILES:${PN} = "${sysconfdir}/my-app/custom.conf ${bindir}/network-setup.sh"
```

## 🔧 Kernel and Device Tree Customization

### Adding Custom Kernel Module

**1. Create kernel module recipe:**
```bash
mkdir -p sources/meta-custom/recipes-kernel/my-driver/files
```

**2. Create simple kernel module** `sources/meta-custom/recipes-kernel/my-driver/files/my-driver.c`:
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init my_driver_init(void)
{
    printk(KERN_INFO "My TDA4VM Driver: Module loaded\n");
    return 0;
}

static void __exit my_driver_exit(void)
{
    printk(KERN_INFO "My TDA4VM Driver: Module unloaded\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Custom TDA4VM Driver");
MODULE_VERSION("1.0");
```

**3. Create Makefile** `sources/meta-custom/recipes-kernel/my-driver/files/Makefile`:
```makefile
obj-m := my-driver.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
```

**4. Create kernel module recipe** `sources/meta-custom/recipes-kernel/my-driver/my-driver.bb`:
```bash
SUMMARY = "Custom kernel module for TDA4VM"
LICENSE = "GPL-2.0"
LIC_FILES_CHKSUM = "file://COPYING;md5=d7810fab7487fb0aad327b76f1be7cd7"

inherit module

SRC_URI = "file://my-driver.c \
           file://Makefile \
           file://COPYING"

S = "${WORKDIR}"

RPROVIDES:${PN} += "my-driver"
```

### Modifying Device Tree

Create `sources/meta-custom/recipes-kernel/linux/linux-ti-staging_%.bbappend`:
```bash
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Add custom device tree overlays
SRC_URI += "file://my-custom-overlay.dts"

do_compile:append() {
    # Compile custom device tree overlay
    dtc -I dts -O dtb -o ${B}/arch/arm64/boot/dts/ti/my-custom-overlay.dtbo \
        ${WORKDIR}/my-custom-overlay.dts
}

do_deploy:append() {
    # Deploy the custom overlay
    install -m 644 ${B}/arch/arm64/boot/dts/ti/my-custom-overlay.dtbo \
        ${DEPLOYDIR}/
}
```

## 🔍 Finding Available Packages

### Search for Packages
```bash
make shell
cd build && . conf/setenv

# Search for packages by name
bitbake-layers show-recipes | grep python
bitbake-layers show-recipes | grep opencv
bitbake-layers show-recipes | grep gstreamer

# Show package information
bitbake -s | grep tensorflow
bitbake-layers show-recipes tensorflow-lite

# Show dependencies
bitbake -g tisdk-edgeai-image
```

### Common Package Categories

**Development Tools:**
- `packagegroup-core-buildessential` - GCC, make, autotools
- `packagegroup-core-tools-debug` - gdb, strace, ltrace
- `cmake` - CMake build system
- `git` - Version control
- `python3-dev` - Python development headers

**System Utilities:**
- `htop iotop` - Process monitoring
- `tree file which` - File utilities  
- `screen tmux` - Terminal multiplexers
- `nano vim` - Text editors
- `curl wget` - Network utilities

**Networking:**
- `packagegroup-base-wifi` - WiFi support
- `packagegroup-base-bluetooth` - Bluetooth support
- `openssh` - SSH client/server
- `netcat-openbsd tcpdump` - Network debugging

**Multimedia:**
- `packagegroup-arago-tisdk-multimedia` - TI multimedia stack
- `gstreamer1.0-plugins-good` - GStreamer plugins
- `v4l-utils` - Video4Linux utilities
- `alsa-utils` - Audio utilities

**AI/EdgeAI (included in tisdk-edgeai-image):**
- `tensorflow-lite` - TensorFlow Lite
- `onnxruntime` - ONNX Runtime  
- `opencv` - Computer Vision
- `ti-vision-apps` - TI Vision Apps

## 🚀 Build Workflow

### 1. Quick Package Addition
```bash
# Edit build/conf/local.conf
IMAGE_INSTALL:append = " python3 git cmake"

# Rebuild
make build
```

### 2. Custom Image Development
```bash
# 1. Create custom layer and recipes
mkdir -p sources/meta-custom/...

# 2. Add layer to bblayers.conf  
echo 'BBLAYERS += "/home/yocto/yocto-workspace/sources/meta-custom"' >> build/conf/bblayers.conf

# 3. Build custom image
make shell
MACHINE=j721e-evm bitbake my-tda4vm-image
```

### 3. Incremental Development
```bash
# Only rebuild changed packages
MACHINE=j721e-evm bitbake my-app -c clean
MACHINE=j721e-evm bitbake my-app

# Force rebuild everything
MACHINE=j721e-evm bitbake my-tda4vm-image -c cleanall
MACHINE=j721e-evm bitbake my-tda4vm-image
```

## 💡 Pro Tips

### Image Size Management
```bash
# Check image size
ls -lh build/arago-tmp-*/deploy/images/j721e-evm/

# Set image size in recipe
IMAGE_ROOTFS_SIZE = "4096"  # 4GB in MB
IMAGE_OVERHEAD_FACTOR = "1.3"
```

### Development Features
```bash
# In your image recipe or local.conf
EXTRA_IMAGE_FEATURES += "debug-tweaks"     # Enable root login
EXTRA_IMAGE_FEATURES += "ssh-server-openssh"  # SSH server
EXTRA_IMAGE_FEATURES += "package-management"  # opkg package manager
```

### Build Optimization
```bash
# In build/conf/local.conf
BB_NUMBER_THREADS = "8"     # Parallel bitbake tasks
PARALLEL_MAKE = "-j 8"      # Parallel make jobs

# Use shared state cache
SSTATE_DIR = "/path/to/shared/sstate-cache"
```

### Debugging Builds
```bash
# Verbose build output
MACHINE=j721e-evm bitbake my-app -v

# Drop into build environment
MACHINE=j721e-evm bitbake my-app -c devshell

# See build log
tail -f build/arago-tmp-*/work/*/my-app/*/temp/log.do_compile
```

## 🎯 Final Build and Deploy

After customization, build and deploy your image:

```bash
# Build your custom image
make build  # or: MACHINE=j721e-evm bitbake my-tda4vm-image

# Find your image
ls -la build/arago-tmp-*/deploy/images/j721e-evm/*.wic

# Flash to SD card (Linux/macOS)
sudo dd if=my-tda4vm-image-j721e-evm.wic of=/dev/sdX bs=1M status=progress

# Or use balenaEtcher (GUI tool)
```

Your customized TDA4VM image is ready to boot with all your added packages and configurations! 🚀

## 📚 Additional Resources

- [Yocto Project Manual](https://docs.yoctoproject.org/brief-yoctoprojectqs/index.html)
- [BitBake User Manual](https://docs.yoctoproject.org/bitbake/)
- [TI Processor SDK Documentation](https://software-dl.ti.com/jacinto7/esd/processor-sdk-linux-jacinto7/)
- [Device Tree Documentation](https://docs.kernel.org/devicetree/)
- [SystemD Service Files](https://www.freedesktop.org/software/systemd/man/systemd.service.html)