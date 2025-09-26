# PYNQ-Z2 Robot Image Build Instructions

Complete guide to build the PYNQ-Z2 robot Linux image using an Ubuntu build server.

## Prerequisites

- Ubuntu build server with SSH access
- At least 100GB free disk space on server
- 16GB+ RAM recommended
- 4+ CPU cores for faster builds

## Step 1: SSH into Ubuntu Build Server

```bash
ssh your-build-server
```

## Step 2: Install Build Dependencies on Ubuntu

```bash
sudo apt update
sudo apt install -y \
    gawk wget git-core diffstat unzip texinfo gcc-multilib \
    build-essential chrpath socat cpio python3 python3-pip \
    python3-pexpect xz-utils debianutils iputils-ping python3-git \
    python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 xterm \
    python3-subunit mesa-common-dev zstd liblz4-tool
```

## Step 3: Copy Files to Build Server

**From your local machine**, copy the entire yocto-build directory:

```bash
rsync -avz --progress ./yocto-build/ your-server:/tmp/yocto-build/
```

This copies:
- `configs/` (build configurations)  
- `poky/` (Yocto build system)
- `flash-pynq-z2-macos.sh` and other files

## Step 4: Set Up Build Environment

**Back on the server:**

```bash
cd /tmp/yocto-build
source poky/oe-init-build-env build

# Copy configurations
cp ../configs/pynq-local.conf conf/local.conf
cp ../configs/pynq-bblayers.conf conf/bblayers.conf
```

## Step 5: Start Build

```bash
# This takes 3-6 hours depending on your hardware
time bitbake core-image-minimal 2>&1 | tee build.log
```

**Build Progress:**
The build will show progress like:
```
Currently 1532 running tasks (3582 of 3582)  85%
```

**Common build times:**
- 16 cores, 32GB RAM, NVMe SSD: ~2 hours
- 8 cores, 16GB RAM, SATA SSD: ~4 hours  
- 4 cores, 8GB RAM, HDD: ~6+ hours

## Step 6: Copy Results Back

**From your local machine**, copy the final images:

```bash
# Copy build outputs to local deploy directory
rsync -avz --progress your-server:/tmp/yocto-build/build/tmp/deploy/images/ ./deploy/images/

# Rename the main image to expected filename
mv ./deploy/images/zynq-generic/core-image-minimal-zynq-generic.wic ./deploy/images/pynq-z2-robot.img
```

## Step 7: Flash to SD Card (macOS)

```bash
# Find your SD card
diskutil list

# Flash the image (replace diskX with your SD card)
./flash-pynq-z2-macos.sh /dev/diskX
```

## Troubleshooting

### Build Fails with "No space left on device"
- Ensure at least 100GB free space on build server
- Clean previous builds: `bitbake -c cleanall core-image-minimal`

### Build Hangs on Downloads
- Check network connectivity on build server
- Restart the build (BitBake will resume from cache)

### Permission Errors
- Ensure your user can sudo on the build server
- Check SSH key authentication is working

### Out of Memory
- Add swap space on server: 
  ```bash
  sudo fallocate -l 8G /swapfile
  sudo mkswap /swapfile
  sudo swapon /swapfile
  ```
- Reduce parallel builds in `conf/local.conf`: `PARALLEL_MAKE = "-j 2"`

### Rsync Issues
- Ensure SSH access works: `ssh your-server`
- Check paths exist on both machines
- Use `-v` flag for verbose output to debug

## What You Get

The final `pynq-z2-robot.img` contains:
- ARM Linux kernel for Zynq-7020
- Root filesystem with Python 3, OpenCV
- SSH server (root login enabled for development)  
- Hardware drivers for PYNQ-Z2 interfaces
- Development tools and utilities

## Next Steps

1. Boot PYNQ-Z2 with flashed SD card
2. Connect Ethernet cable  
3. Find IP address and SSH in: `ssh root@<ip-address>`
4. Start developing your robot applications!

---

**Total build time:** 3-6 hours  
**Final image size:** ~8MB compressed, ~2GB uncompressed