# Xilinx Tools Installation Guide for STAR-Z2

This guide covers the installation of Xilinx Vivado/Vitis and PetaLinux tools required for building custom PYNQ images.

## Required Tools

For PYNQ v3.0.1 (recommended), you need:
- **Vivado/Vitis**: 2022.1 (primary), 2021.2, or 2020.2
- **PetaLinux**: Same version as Vivado/Vitis

## System Requirements

### Hardware
- **CPU**: 8+ cores (16+ recommended)
- **RAM**: 32GB minimum (64GB recommended)
- **Storage**: 100GB+ for tools, 200GB+ for builds
- **OS**: Ubuntu 18.04 or 20.04 LTS (64-bit)

### Software Prerequisites
```bash
sudo apt-get install -y \
    libtool \
    autoconf \
    automake \
    libncurses5-dev \
    libglib2.0-dev \
    libssl-dev \
    flex \
    bison \
    libselinux1 \
    xterm \
    lib32z1 \
    lib32ncurses5 \
    libbz2-1.0 \
    lib32stdc++6
```

## Vivado/Vitis Installation

### 1. Download Vivado

Visit AMD/Xilinx download page:
https://www.xilinx.com/support/download.html

Download the **Vivado ML Edition** installer for your version (e.g., 2022.1)

Options:
- **Web Installer**: Small download, installs packages from internet
- **Full Installer**: Large download (~80GB), includes all packages

### 2. Extract and Run Installer

```bash
# Extract installer
tar -xvf Xilinx_Unified_2022.1_*.tar.gz
cd Xilinx_Unified_2022.1_*/

# Run installer (graphical)
sudo ./xsetup

# Or run in batch mode with config file
sudo ./xsetup --agree XilinxEULA,3rdPartyEULA --batch Install --config install_config.txt
```

### 3. Installation Configuration

In the installer GUI:
1. Select **Vivado**
2. Choose **Vivado ML Standard Edition**
3. In device support, select:
   - **SoCs** → Zynq-7000 (for PYNQ-Z2)
4. Installation directory: `/opt/Xilinx` (recommended)

Installation size: ~50GB

### 4. Install Cable Drivers

After installation:
```bash
cd /opt/Xilinx/Vivado/2022.1/data/xicom/cable_drivers/lin64/install_script/install_drivers/
sudo ./install_drivers
```

### 5. Source Vivado Settings

Add to `~/.bashrc`:
```bash
# Xilinx Vivado 2022.1
source /opt/Xilinx/Vivado/2022.1/settings64.sh
```

Apply changes:
```bash
source ~/.bashrc
```

Verify installation:
```bash
vivado -version
```

## PetaLinux Installation

### 1. Download PetaLinux

Download PetaLinux from:
https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html

**IMPORTANT**: PetaLinux version must match Vivado version exactly!

Download: `petalinux-v2022.1-final-installer.run` (~10GB)

### 2. Install Dependencies

```bash
sudo apt-get install -y \
    gawk \
    python3 \
    python \
    build-essential \
    gcc \
    git \
    make \
    net-tools \
    libncurses5-dev \
    tftpd \
    zlib1g-dev \
    libssl-dev \
    flex \
    bison \
    libselinux1 \
    gnupg \
    wget \
    diffstat \
    chrpath \
    socat \
    xterm \
    autoconf \
    libtool \
    tar \
    unzip \
    texinfo \
    zlib1g-dev \
    gcc-multilib \
    build-essential \
    screen \
    pax \
    gzip \
    python3-pexpect \
    python3-pip \
    python3-git \
    python3-jinja2 \
    xz-utils \
    debianutils \
    iputils-ping \
    libegl1-mesa \
    libsdl1.2-dev \
    pylint3 \
    cpio
```

### 3. Run PetaLinux Installer

```bash
# Create installation directory
mkdir -p /opt/petalinux/2022.1
chmod -R 755 /opt/petalinux

# Run installer
./petalinux-v2022.1-final-installer.run /opt/petalinux/2022.1

# Accept license agreements when prompted
```

Installation size: ~40GB

### 4. Source PetaLinux Settings

Add to `~/.bashrc` (after Vivado settings):
```bash
# PetaLinux 2022.1
source /opt/petalinux/2022.1/settings.sh
```

**IMPORTANT**: Source PetaLinux settings **after** Vivado settings!

Apply changes:
```bash
source ~/.bashrc
```

Verify installation:
```bash
echo $PETALINUX
petalinux-util --help
```

## Version Compatibility Matrix

| PYNQ Version | Vivado/Vitis | PetaLinux | Ubuntu      |
|--------------|--------------|-----------|-------------|
| v3.0.1       | 2022.1       | 2022.1    | 18.04/20.04 |
| v2.7.0       | 2020.2       | 2020.2    | 18.04/20.04 |
| v2.6.0       | 2020.1       | 2020.1    | 18.04       |

**Recommendation**: Use PYNQ v3.0.1 with Vivado/PetaLinux 2022.1 for best stability.

## Tool Configuration

### Increase inotify Limits

Required for large builds:
```bash
echo "fs.inotify.max_user_watches=524288" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### Configure Swap Space

Recommended for systems with less than 32GB RAM:
```bash
# Create 16GB swap file
sudo fallocate -l 16G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# Make permanent
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### Set Build Thread Counts

Add to `~/.bashrc`:
```bash
# Optimize for your CPU core count
export BB_NUMBER_THREADS=$(nproc)
export PARALLEL_MAKE="-j$(nproc)"
```

## License Configuration (If Required)

For some Xilinx tools/features, you may need licenses:

1. Get license from: https://www.xilinx.com/getlicense
2. Save license file (e.g., `Xilinx.lic`)
3. Set environment variable:
```bash
export XILINXD_LICENSE_FILE=/path/to/Xilinx.lic
```

## Verification Checklist

Before building PYNQ images, verify:

```bash
# Check Vivado
vivado -version
# Should show: Vivado v2022.1 (64-bit)

# Check Vitis
vitis -version
# Should show: Vitis v2022.1 (64-bit)

# Check PetaLinux
echo $PETALINUX
# Should show: /opt/petalinux/2022.1

# Check environment
env | grep XILINX
# Should show XILINX_VIVADO, XILINX_VITIS paths

# Check disk space
df -h /opt/Xilinx /opt/petalinux
# Ensure sufficient space available

# Check RAM
free -h
# Ensure 32GB+ total (including swap)
```

## Troubleshooting

### "libncurses5.so.5 not found"
```bash
sudo apt-get install -y libncurses5 lib32ncurses5
```

### "libtinfo.so.5 not found"
```bash
sudo ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfo.so.5
```

### PetaLinux build fails with "bash version error"
```bash
# Use bash instead of dash
sudo dpkg-reconfigure dash
# Select "No" when asked to use dash as default shell
```

### Vivado won't start
```bash
# Check cable drivers
cd /opt/Xilinx/Vivado/2022.1/data/xicom/cable_drivers/lin64/install_script/install_drivers/
sudo ./install_drivers

# Check display
export DISPLAY=:0
vivado &
```

## Next Steps

After installing tools:
1. Return to main README: `../README.md`
2. Run environment setup: `./scripts/setup-env.sh`
3. Start building: `./scripts/build.sh`

## References

- [AMD Xilinx Downloads](https://www.xilinx.com/support/download.html)
- [PetaLinux Tools Documentation](https://docs.xilinx.com/r/en-US/ug1144-petalinux-tools-reference-guide)
- [Vivado Design Suite User Guide](https://docs.xilinx.com/v/u/en-US/ug910-vivado-getting-started)
- [PYNQ Documentation](https://pynq.readthedocs.io/)
