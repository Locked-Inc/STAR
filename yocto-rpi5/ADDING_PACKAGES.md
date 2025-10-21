# Adding Packages to STAR Minimal Image

Guide for extending your minimal system with additional packages.

## Two Methods

### Method 1: Runtime Installation (opkg)
- **Pros**: Fast, no rebuild needed
- **Cons**: Requires network, not reproducible, packages might not be available
- **Use for**: Quick testing, temporary packages

### Method 2: Build-time (Yocto)
- **Pros**: Reproducible, always works, included in image
- **Cons**: Requires full rebuild
- **Use for**: Production, permanent packages

## Method 1: Runtime with opkg

On the Raspberry Pi:

```bash
# Update package list
opkg update

# Search for packages
opkg list | grep <search-term>

# Install package
opkg install <package-name>

# Remove package
opkg remove <package-name>

# List installed packages
opkg list-installed
```

### Example: Install Python3

```bash
opkg update
opkg install python3
```

## Method 2: Build-time with Yocto

Edit `meta-star/recipes-core/packagegroups/packagegroup-star-minimal.bb`:

```bitbake
RDEPENDS:${PN} = " \
    busybox \
    dropbear \
    runit \
    ...
    python3 \
    python3-pip \
    your-other-package \
"
```

Then rebuild:

```bash
cd /path/to/yocto-rpi5
source setup-environment.sh
bitbake star-minimal-image
```

## Common Packages to Add

### Development Tools

```bitbake
RDEPENDS:${PN} = " \
    ...
    python3 \
    python3-pip \
    python3-setuptools \
    gcc \
    make \
    git \
    vim \
"
```

### I2C/SPI Tools

```bitbake
RDEPENDS:${PN} = " \
    ...
    i2c-tools \
    spitools \
    kernel-module-i2c-dev \
    kernel-module-spi-dev \
"
```

### CAN Bus

```bitbake
RDEPENDS:${PN} = " \
    ...
    can-utils \
    kernel-module-can \
    kernel-module-can-raw \
"
```

### Networking

```bitbake
RDEPENDS:${PN} = " \
    ...
    iperf3 \
    tcpdump \
    ethtool \
    net-tools \
"
```

### Sensors/Robotics

```bitbake
RDEPENDS:${PN} = " \
    ...
    opencv \
    v4l-utils \
    lidar-specific-package \
"
```

## Creating Custom Recipes

For STAR-specific software that doesn't exist in Yocto:

### 1. Create Recipe Directory

```bash
mkdir -p meta-star/recipes-robot/your-package
```

### 2. Create Recipe File

`meta-star/recipes-robot/your-package/your-package_1.0.bb`:

```bitbake
SUMMARY = "Your package description"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=..."

SRC_URI = "git://github.com/yourrepo/yourpackage.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/your-binary ${D}${bindir}/
}

FILES:${PN} = "${bindir}/your-binary"
```

### 3. Add to Image

Edit `packagegroup-star-minimal.bb`:

```bitbake
RDEPENDS:${PN} = " \
    ...
    your-package \
"
```

### 4. Build

```bash
bitbake your-package
bitbake star-minimal-image
```

## Finding Package Names

### Search Yocto Layers

```bash
cd yocto-rpi5
find meta-* -name "*.bb" | grep <search-term>
```

### Search OpenEmbedded Index

https://layers.openembedded.org/layerindex/branch/kirkstone/recipes/

### Check What's Available

```bash
# In build directory
bitbake-layers show-recipes | grep <search-term>
```

## Example: Adding ROS2 (Advanced)

For ROS2, you'll need additional layers:

### 1. Clone meta-ros

```bash
cd yocto-rpi5
git clone -b kirkstone https://github.com/ros/meta-ros.git
```

### 2. Add to bblayers.conf

```bash
cd build
bitbake-layers add-layer ../meta-ros/meta-ros2
bitbake-layers add-layer ../meta-ros/meta-ros2-humble
```

### 3. Add ROS2 packages

```bitbake
RDEPENDS:${PN} = " \
    ...
    ros-base \
    ros-humble-navigation2 \
"
```

### 4. Build

```bash
bitbake star-minimal-image
```

**Note**: ROS2 will significantly increase image size and build time.

## Removing Packages

To slim down even further, remove packages from `packagegroup-star-minimal.bb`:

```bitbake
# Remove i2c-tools if you don't need I2C
RDEPENDS:${PN} = " \
    busybox \
    dropbear \
    runit \
    # i2c-tools \    <- commented out
"
```

## Package Groups

Instead of individual packages, use package groups:

```bitbake
RDEPENDS:${PN} = " \
    ...
    packagegroup-core-buildessential \    # GCC, make, etc.
    packagegroup-core-tools-debug \       # GDB, strace
"
```

## Debugging Package Issues

### Package not found

```bash
# Search for it
bitbake-layers show-recipes | grep package-name

# Check if layer is enabled
bitbake-layers show-layers
```

### Build fails

```bash
# Clean package
bitbake -c cleanall package-name

# Rebuild
bitbake package-name

# Check logs
cat build/tmp/work/.../package-name/*/temp/log.do_*
```

### Package in image but not working

```bash
# Check if files are actually installed
cd build/tmp/deploy/images/raspberrypi5
unsquashfs -ll star-minimal-image*.wic | grep your-file
```

## Best Practices

1. **Start minimal**: Add packages incrementally
2. **Test each addition**: Build and test after adding each package
3. **Document**: Comment why you added each package
4. **Version control**: Track changes to packagegroup files
5. **Use build-time**: Prefer Yocto recipes over runtime installation

## Common Pitfalls

### License Issues

If build fails with license errors, add to `local.conf`:

```bash
LICENSE_FLAGS_ACCEPTED += "commercial"
```

### Dependency Conflicts

Some packages conflict. Check error messages and remove conflicting packages.

### Disk Space

Monitor disk space:
```bash
df -h build/
```

Clean if needed:
```bash
bitbake -c cleansstate star-minimal-image
```

## Resources

- [Yocto Package Management](https://docs.yoctoproject.org/dev-manual/common-tasks.html#using-runtime-package-management)
- [Writing Recipes](https://docs.yoctoproject.org/dev-manual/common-tasks.html#writing-a-new-recipe)
- [OpenEmbedded Layer Index](https://layers.openembedded.org/)
