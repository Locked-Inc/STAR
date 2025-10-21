# STAR Yocto Build Checklist

Use this checklist to track your progress building the minimal Raspberry Pi 5 image.

## Pre-Build Checklist

### System Requirements
- [ ] Ubuntu 20.04 LTS or later installed
- [ ] At least 100GB free disk space (check: `df -h`)
- [ ] At least 8GB RAM (16GB recommended)
- [ ] Internet connection working

### Install Dependencies
```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm python3-subunit \
    mesa-common-dev zstd liblz4-tool file
```

- [ ] Dependencies installed successfully
- [ ] No error messages from apt-get

## Build Process

### Step 1: Initialize Yocto Layers
```bash
cd /home/bsikar/Documents/git/STAR/yocto-rpi5
./scripts/init-build.sh
```

- [ ] Poky cloned
- [ ] meta-openembedded cloned
- [ ] meta-raspberrypi cloned
- [ ] No errors during cloning

**Expected time**: 5-10 minutes

### Step 2: Setup Environment
```bash
source setup-environment.sh
```

- [ ] Environment variables set
- [ ] `build/conf/` directory created
- [ ] `local.conf` and `bblayers.conf` created
- [ ] No error messages

**Expected time**: < 1 minute

### Step 3: Build Image
```bash
./scripts/build-image.sh
```

- [ ] Build started successfully
- [ ] Downloads completed (~10GB)
- [ ] Build completed without errors
- [ ] Image file created in `build/tmp/deploy/images/raspberrypi5/`

**Expected time**: 4-8 hours (first build)

**⚠️ Note**: Build will consume significant CPU. Your computer may be slow during this time.

### Step 4: Verify Build Output
```bash
ls -lh build/tmp/deploy/images/raspberrypi5/star-minimal-image*.wic*
```

- [ ] `.wic` file exists
- [ ] `.wic.bz2` file exists (compressed)
- [ ] File size is reasonable (500MB-2GB range)

## Flashing to SD Card

### Prepare SD Card
- [ ] SD card is at least 4GB (8GB+ recommended)
- [ ] SD card is Class 10 or faster
- [ ] Important data backed up (will be erased!)

### Find SD Card Device
```bash
# Before inserting SD card
lsblk

# Insert SD card

# After inserting SD card
lsblk
```

- [ ] SD card device identified (e.g., `/dev/sdb`)
- [ ] Verified it's the correct device (check size!)
- [ ] **DOUBLE CHECKED** device name (wrong device = data loss!)

### Unmount SD Card
```bash
sudo umount /dev/sdX*  # Replace X with your device
```

- [ ] All partitions unmounted

### Flash Image
```bash
sudo ./scripts/flash-sd.sh /dev/sdX  # Replace X
```

- [ ] Confirmed with "yes"
- [ ] Flash completed successfully
- [ ] No errors during write

**Expected time**: 5-15 minutes depending on SD card speed

### Verify Flash
- [ ] Script completed with success message
- [ ] No error messages
- [ ] SD card still detected by system

## First Boot

### Physical Setup
- [ ] SD card inserted into Raspberry Pi 5
- [ ] Ethernet cable connected (or WiFi configured)
- [ ] Power supply connected (official 5V/5A recommended)
- [ ] Optional: HDMI monitor connected (for debugging)
- [ ] Optional: USB keyboard connected (for debugging)

### Power On
- [ ] Raspberry Pi powered on
- [ ] Green LED flashing (indicates SD card activity)
- [ ] System appears to boot (if monitor connected)

### Find IP Address

**Method 1: Router**
- [ ] Checked router's DHCP leases
- [ ] Found "star-robot" hostname
- [ ] IP address noted: `___________________`

**Method 2: Network Scan**
```bash
nmap -sn 192.168.1.0/24
```
- [ ] Scan completed
- [ ] Raspberry Pi found
- [ ] IP address noted: `___________________`

**Method 3: Serial Console** (if above methods fail)
- [ ] USB-UART connected to GPIO pins
- [ ] Serial console opened: `screen /dev/ttyUSB0 115200`
- [ ] Login prompt visible
- [ ] IP address checked: `ip addr`

## First Login

### SSH Connection
```bash
ssh root@<ip-address>
```

- [ ] SSH connection successful
- [ ] Login prompt displayed
- [ ] Warning about no password accepted
- [ ] Successfully logged in as root

### Set Root Password (CRITICAL!)
```bash
passwd
```

- [ ] New password entered
- [ ] Password confirmed
- [ ] Password set successfully
- [ ] Password recorded securely

### Verify System
```bash
# Check hostname
hostname
# Should show: star-robot

# Check running services
sv status dropbear
sv status networking
# Both should show "run"

# Check disk space
df -h

# Check memory
free -h

# Check network interfaces
ip addr
```

- [ ] Hostname is "star-robot"
- [ ] Services are running
- [ ] Disk space reasonable
- [ ] Network interfaces up

## Configure WiFi (Optional)

### Create WPA Configuration
```bash
wpa_passphrase "YourSSID" "YourPassword" > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
chmod 600 /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

- [ ] WPA config created
- [ ] Permissions set

### Edit Network Interfaces
```bash
vi /etc/network/interfaces
```

Uncomment WiFi section and save.

- [ ] WiFi section uncommented
- [ ] File saved

### Bring Up WiFi
```bash
ifup wlan0
ip addr show wlan0
```

- [ ] WiFi interface brought up
- [ ] IP address assigned
- [ ] Can ping internet: `ping -c 3 8.8.8.8`

## Post-Setup

### Security Hardening
- [ ] Root password set (if not already done)
- [ ] SSH keys configured (optional but recommended)
- [ ] Unnecessary users disabled (none by default)
- [ ] Firewall considered (none by default)

### Package Management
```bash
opkg update
opkg list | head
```

- [ ] Package list updated
- [ ] opkg working correctly

### Install Additional Packages (as needed)
```bash
opkg install <package-name>
```

- [ ] Packages installed as needed
- [ ] All packages working correctly

## Troubleshooting (if needed)

If you encounter issues:

- [ ] Checked TROUBLESHOOTING.md
- [ ] Reviewed build logs in `build/tmp/work/`
- [ ] Searched error messages online
- [ ] Asked for help in appropriate forum

## Final Verification

- [ ] System boots reliably
- [ ] SSH access works
- [ ] Network connectivity working
- [ ] Services running correctly
- [ ] Can add/remove packages
- [ ] System is stable

## Success! 🎉

Your minimal Raspberry Pi 5 system is now running!

**Next steps:**
1. Start adding packages for STAR robot (see ADDING_PACKAGES.md)
2. Configure robot-specific hardware
3. Deploy robot software
4. Test and iterate

---

**Build Date**: _______________
**Image Version**: _______________
**Notes**:
_______________________________________________
_______________________________________________
_______________________________________________
