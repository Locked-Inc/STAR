# Troubleshooting Guide - STAR Yocto Build

Common issues and solutions.

## Build Issues

### Error: "No space left on device"

**Problem**: Yocto builds are huge and filled up your disk.

**Solution**:
```bash
# Check disk space
df -h

# Clean build artifacts
cd build
bitbake -c clean star-minimal-image

# More aggressive cleaning
rm -rf build/tmp

# Clean shared state (will slow down next build)
rm -rf sstate-cache/*

# Clean downloads (will re-download everything)
rm -rf downloads/*
```

**Prevention**: Ensure 100GB+ free before starting.

---

### Error: "Fetcher failure for URL"

**Problem**: Can't download source packages.

**Solution**:
```bash
# Check internet connection
ping -c 3 google.com

# Clear download cache for that package
bitbake -c cleanall <package-name>

# Try again
bitbake star-minimal-image
```

**Common causes**:
- Temporary network issue (retry)
- Upstream source moved/deleted (update recipe)
- Firewall blocking download (check corporate proxy)

---

### Error: "Nothing PROVIDES ..."

**Problem**: Package or dependency not found.

**Solution**:
```bash
# Check if layer is added
bitbake-layers show-layers

# Add missing layer
bitbake-layers add-layer ../meta-something

# Search for package
bitbake-layers show-recipes | grep package-name
```

---

### Error: "Multiple .bb files are due to be built"

**Problem**: Two layers provide the same recipe with same version.

**Solution**:
Edit `build/conf/bblayers.conf` and adjust layer priorities, or remove conflicting layer.

---

### Error: "License ... is not listed in LICENSE_FLAGS_ACCEPTED"

**Problem**: Package has restricted license.

**Solution**:
Edit `build/conf/local.conf`:
```bash
LICENSE_FLAGS_ACCEPTED += "commercial"
```

---

### Build fails after system update

**Problem**: System Python or dependencies changed.

**Solution**:
```bash
# Re-setup environment
cd yocto-rpi5
source setup-environment.sh

# Clean and rebuild
bitbake -c cleanall star-minimal-image
bitbake star-minimal-image
```

---

## Runtime Issues (On Raspberry Pi)

### Can't SSH to Raspberry Pi

**Problem**: Can't connect via SSH.

**Diagnosis**:
1. Can you ping the Pi? `ping <ip-address>`
   - No: Network issue (check cable, DHCP)
   - Yes: Continue...

2. Is SSH running?
   - Connect via serial console
   - Check: `sv status dropbear`

**Solutions**:

Network not configured:
```bash
# Check IP
ip addr show

# Check routes
ip route

# Restart networking
sv restart networking
```

Dropbear not running:
```bash
# Start dropbear
sv start dropbear

# Check logs
cat /var/log/dropbear/current
```

---

### Can't find Raspberry Pi IP

**Problem**: Don't know the IP address.

**Solutions**:

From your computer:
```bash
# Scan network
nmap -sn 192.168.1.0/24

# Or use arp
arp -a | grep -i "b8:27:eb\|dc:a6:32\|e4:5f:01"  # RPi MAC prefixes
```

Use serial console:
- Connect USB-UART to GPIO pins
- Use `screen /dev/ttyUSB0 115200`
- Login and run `ip addr`

Check your router:
- Login to router web interface
- Check DHCP leases

---

### WiFi not working

**Problem**: Can't connect to WiFi.

**Diagnosis**:
```bash
# Check if interface exists
ip link show wlan0

# Check if driver loaded
dmesg | grep -i wifi

# Check firmware
ls /lib/firmware/brcm/
```

**Solution**:

Create WPA config:
```bash
wpa_passphrase "YourSSID" "YourPassword" > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
chmod 600 /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

Edit `/etc/network/interfaces`:
```
auto wlan0
iface wlan0 inet dhcp
    wpa-conf /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

Bring up interface:
```bash
ifup wlan0
```

Check status:
```bash
ip addr show wlan0
iwconfig wlan0
```

---

### System doesn't boot

**Problem**: Pi doesn't boot after flashing SD card.

**Diagnosis**:
1. Do you see rainbow screen? (GPU working)
2. Does green LED flash? (SD card being read)
3. Any output on HDMI? (even though it's headless, can help debug)

**Solutions**:

Re-flash SD card:
```bash
# Verify image integrity
bunzip2 -t star-minimal-image*.wic.bz2

# Re-flash
sudo ./scripts/flash-sd.sh /dev/sdX
```

Check SD card:
```bash
# Test SD card for errors
sudo badblocks -v /dev/sdX
```

Try different SD card:
- Some cheap SD cards are unreliable
- Use Class 10 or better

---

### Root filesystem full

**Problem**: No space left on device at runtime.

**Diagnosis**:
```bash
df -h
du -sh /* | sort -h
```

**Solutions**:

Clean logs:
```bash
rm -rf /var/log/*/old
sv restart <service>  # Recreates log files
```

Remove unnecessary files:
```bash
opkg remove <unused-package>
```

Increase image size (rebuild):
Edit `meta-star/recipes-core/images/star-minimal-image.bb`:
```bitbake
IMAGE_ROOTFS_SIZE ?= "1024"  # Increase from 512
IMAGE_ROOTFS_EXTRA_SPACE = "512"  # Increase from 256
```

---

### Service won't start

**Problem**: Runit service failing.

**Diagnosis**:
```bash
# Check service status
sv status <service-name>

# Check logs
cat /var/log/<service-name>/current

# Run service script manually
sh -x /etc/sv/<service-name>/run
```

**Solutions**:

Fix run script:
```bash
vi /etc/sv/<service-name>/run
# Fix errors
sv restart <service-name>
```

---

### Can't install packages with opkg

**Problem**: `opkg install` fails.

**Solutions**:

Update package list:
```bash
opkg update
```

Check network:
```bash
ping -c 3 8.8.8.8
```

Clear cache:
```bash
rm -rf /var/lib/opkg/lists/*
opkg update
```

Package doesn't exist:
- You'll need to add it via Yocto and rebuild
- See ADDING_PACKAGES.md

---

## Development Issues

### Changes not appearing in build

**Problem**: Modified recipe but changes don't show up.

**Solution**:
```bash
# Force rebuild of specific recipe
bitbake -c cleanall <package-name>
bitbake <package-name>

# Rebuild image
bitbake star-minimal-image
```

---

### Want to modify kernel

**Solution**:
```bash
# Configure kernel
bitbake -c menuconfig virtual/kernel

# Save config
bitbake -c savedefconfig virtual/kernel

# Rebuild
bitbake virtual/kernel
bitbake star-minimal-image
```

---

### Need to debug recipe

**Solution**:
```bash
# See what recipe does
bitbake -e <package-name> | less

# Open devshell (interactive build environment)
bitbake -c devshell <package-name>
```

---

## Performance Issues

### Build too slow

**Solutions**:

Increase parallelism in `build/conf/local.conf`:
```bash
BB_NUMBER_THREADS = "12"  # Match your CPU cores
PARALLEL_MAKE = "-j 12"
```

Use shared state cache:
```bash
SSTATE_MIRRORS ?= "\
    file://.* http://sstate.yoctoproject.org/all/PATH;downloadfilename=PATH"
```

Enable build history for analysis:
```bash
INHERIT += "buildhistory"
BUILDHISTORY_COMMIT = "1"
```

---

### Pi running slow

**Check CPU frequency**:
```bash
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
```

**Check temperature**:
```bash
cat /sys/class/thermal/thermal_zone0/temp
```

**Monitor resources**:
```bash
top
free -h
```

---

## Getting Help

### Where to look:

1. **Build logs**:
   ```bash
   build/tmp/work/*/temp/log.do_*
   ```

2. **Runtime logs**:
   ```bash
   /var/log/*/current
   ```

3. **Yocto documentation**:
   https://docs.yoctoproject.org/

4. **Search error message**:
   Include "yocto" and "kirkstone" in search

5. **Yocto mailing lists**:
   https://lists.yoctoproject.org/

### How to report issues:

Include:
- Full error message
- Relevant log files
- Your local.conf and bblayers.conf
- Steps to reproduce
- Yocto version (kirkstone)

---

## Nuclear Options

### Start completely fresh:

```bash
cd yocto-rpi5
rm -rf build/ downloads/ sstate-cache/
rm -rf poky/ meta-openembedded/ meta-raspberrypi/
./scripts/init-build.sh
source setup-environment.sh
./scripts/build-image.sh
```

**Warning**: This will re-download everything and take hours!

---

## Prevention

1. **Keep plenty of disk space** - 100GB minimum
2. **Use git** - Track changes to meta-star
3. **Test incrementally** - Build after each change
4. **Read error messages** - They're usually helpful
5. **Check documentation** - Yocto docs are comprehensive
6. **Backup working builds** - Save successful images

---

## Still Stuck?

1. Double-check this guide
2. Search Yocto documentation
3. Search online (include "yocto kirkstone")
4. Ask on Yocto mailing lists
5. File issue in STAR repo with full details
