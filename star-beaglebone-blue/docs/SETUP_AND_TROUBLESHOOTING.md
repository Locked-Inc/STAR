# BeagleBone Blue Setup and Troubleshooting

Guide for running the star-beaglebone-blue firmware on Debian 13 Trixie
with the 5.10-ti kernel. Covers eMMC setup, librobotcontrol patching,
cross-compilation, and common issues.

---

## 1. Trixie eMMC First-Boot Fixes

The stock BeagleBoard.org Trixie image needs three fixes before the BBB
will boot reliably from eMMC with USB gadget networking.

### 1.1 fstab: add nofail to boot and swap

Without `nofail`, systemd waits 90 seconds for the boot/swap partitions,
then cascades into `local-fs.target` failure -> `multi-user.target` failure
-> `bb-usb-gadgets.service` never starts -> USB gadget disappears.

```
/dev/mmcblk0p1  /boot/firmware vfat user,uid=1000,gid=1000,defaults,nofail 0 2
/dev/mmcblk0p2       none    swap    sw,nofail      0       0
```

### 1.2 Mask serial-getty@ttyGS0.service

There is a confirmed race condition in the Trixie image where
`serial-getty@ttyGS0.service` tries to open `/dev/ttyGS0` before the
libcomposite gadget stack has fully registered the ACM function.

```bash
sudo ln -sf /dev/null /etc/systemd/system/serial-getty@ttyGS0.service
```

### 1.3 Pre-generate MAC address config

If `/etc/default/bb-mac-addr` doesn't exist on first boot, the
`bb-start-acm-ncm-rndis-old-gadget` script (run with `set -e`) may fail
before setting up USB networking. Pre-generate it while mounted from
another OS (e.g. SD card boot):

```bash
sudo mount /dev/mmcblk1p3 /mnt/emmc
cat > /mnt/emmc/etc/default/bb-mac-addr << 'EOF'
MAC_ADDR=F4:5E:AB:35:27:BF
USB0_HOST_ADDR=F4:5E:AB:35:27:C1
USB0_DEV_ADDR=F4:5E:AB:35:27:C2
USB1_HOST_ADDR=F4:5E:AB:35:27:C3
USB1_DEV_ADDR=F4:5E:AB:35:27:C4
EOF
```

Get the base MAC from the running BBB:
```bash
hexdump -v -e '1/1 "%02X" ":"' /proc/device-tree/ocp/ethernet@4a100000/slave@4a100200/mac-address | sed 's/.$//'
```

---

## 2. librobotcontrol Patches for 5.10-ti Kernel

The stock librobotcontrol (v1.0.5 from GitHub master) has hardcoded sysfs
paths for the 4.14 kernel. Three patches are required for the 5.10-ti
kernel used in the Trixie image.

### Build from source

```bash
git clone https://github.com/beagleboard/librobotcontrol.git /tmp/librobotcontrol
cd /tmp/librobotcontrol
# Apply patches below, then:
make clean && make -j2
sudo make install && sudo ldconfig
```

### Patch 1: PWM paths and buffer size (pwm.c)

**File**: `library/src/io/pwm.c`

The 5.10 kernel restructured the OCP device tree with an `interconnect`
layer. PWM chip numbers also changed from 0/2/4 to 3/5/7.

Changes:
1. `OCP_DIR`: use exact 5.10 path with `%1$d` positional format args
2. `MAXBUF`: 128 -> 256 (new paths exceed 128 chars)
3. All `SYS_DIR` snprintf calls: `ss*2` -> `ssindex[ss]` (dynamic chip numbers)
4. `ssindex` extraction: `strstr(path, "pwmchip") + 7` instead of fixed `OCP_OFFSET`

```c
// Before (4.14):
#define OCP_DIR "/sys/devices/platform/ocp/4830%d000.epwmss/4830%d200.pwm/pwm"
#define MAXBUF 128
#define OCP_OFFSET 66

// After (5.10):
#define OCP_DIR "/sys/devices/platform/ocp/48000000.interconnect/48000000.interconnect:segment@300000/4830%1$d000.target-module/4830%1$d000.epwmss/4830%1$d200.pwm/pwm"
#define MAXBUF 256
// OCP_OFFSET removed, use strstr("pwmchip") instead
```

### Patch 2: Encoder eQEP -> counter subsystem (encoder_eqep.c)

**File**: `library/src/io/encoder_eqep.c`

Complete rewrite. The 5.10 kernel replaced the custom eQEP sysfs driver
with the kernel counter subsystem (`ti-eqep` counter driver).

| Old (4.14) | New (5.10) |
|---|---|
| `/sys/devices/platform/ocp/4830X000.epwmss/4830X180.eqep/position` | `/sys/bus/counter/devices/counterN/count0/count` |
| `/sys/devices/platform/ocp/4830X000.epwmss/4830X180.eqep/enabled` | `/sys/bus/counter/devices/counterN/count0/enable` |

The replacement opens `/sys/bus/counter/devices/counter{0,1,2}/count0/count`
for position reads and writes `enable` and `function` (set to `quadrature x4`).

Verify counter paths exist:
```bash
ls /sys/bus/counter/devices/counter0/count0/
# Should show: ceiling count enable function function_available ...
```

### Patch 3: I2C_SLAVE_FORCE for MPU-9250 (i2c.c)

**File**: `library/src/io/i2c.c`

The 5.10 kernel's `inv-mpu6050-i2c` IIO driver claims the MPU-9250 at
address 0x68 on I2C bus 2. This causes `ioctl(fd, I2C_SLAVE, addr)` to
fail with `EBUSY`.

Fix: replace `I2C_SLAVE` with `I2C_SLAVE_FORCE` in two locations:
- `rc_i2c_init()` (~line 72)
- `rc_i2c_set_device_address()` (~line 109)

Additionally, blacklist the IIO driver to also release the GPIO interrupt
pin needed for DMP mode:

```bash
echo "blacklist inv_mpu6050_i2c" | sudo tee /etc/modprobe.d/blacklist-inv-mpu.conf
echo "blacklist inv_mpu6050" | sudo tee -a /etc/modprobe.d/blacklist-inv-mpu.conf
```

If the driver is already loaded, unbind it at runtime:
```bash
echo "2-0068" | sudo tee /sys/bus/i2c/drivers/inv-mpu6050-i2c/unbind
```

---

## 3. Cross-Compile from Pi5

### Install cross-compiler

```bash
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

### Set up partial sysroot

Copy librobotcontrol headers and .so from the BBB:

```bash
sudo mkdir -p /opt/bbb-sysroot/usr/include
sudo mkdir -p /opt/bbb-sysroot/usr/lib/arm-linux-gnueabihf
scp -r debian@192.168.7.2:/usr/include/rc /opt/bbb-sysroot/usr/include/
scp debian@192.168.7.2:/usr/include/robotcontrol.h /opt/bbb-sysroot/usr/include/
scp debian@192.168.7.2:/usr/lib/librobotcontrol.so /opt/bbb-sysroot/usr/lib/arm-linux-gnueabihf/
sudo chmod -R a+rX /opt/bbb-sysroot
```

Patch `/opt/bbb-sysroot/usr/include/rc/i2c.h` to add missing includes:
```c
#include <stddef.h>
#include <stdint.h>
```

### Build

```bash
cd star-beaglebone-blue
cmake --preset cross-debug-pi5    # uses CMakeUserPresets.json
cmake --build build-cross-debug -j$(nproc)
```

### Deploy

```bash
scp build-cross-debug/star-beaglebone-blue debian@192.168.7.2:~/
ssh debian@192.168.7.2 "sudo ./star-beaglebone-blue"
```

---

## 4. GCC Compatibility

The firmware uses C23 typed enums (`enum : type { ... }`).

- GCC 14 (Trixie native): use `-std=gnu23`
- GCC 13 (Pi5 cross-compiler): use `-std=gnu2x` (same feature, older flag name)

The `CMakeLists.txt` uses `-std=gnu2x` which works with both.

---

## 5. Common Errors

| Symptom | Cause | Fix |
|---------|-------|-----|
| `can't find pwm unexport file` | PWM OCP paths wrong for 5.10 kernel | Apply PWM patch (Patch 1) |
| `failed to open device driver` (encoder) | Old eQEP sysfs paths, need counter subsystem | Apply encoder patch (Patch 2) |
| `ioctl slave address change failed` | IIO driver holds I2C address | Apply I2C_SLAVE_FORCE (Patch 3) + blacklist inv-mpu6050 |
| `failed to initialize GPIO` (DMP) | IIO driver holds interrupt GPIO | Unbind: `echo 2-0068 > /sys/bus/i2c/drivers/inv-mpu6050-i2c/unbind` |
| `rc_encoder_pru_read` errors | PRU encoder (ch4) not supported on 5.10 | Use only eQEP channels 1-3, or skip ch4 |
| USB gadget disappears after ~90s | fstab partition timeout cascades | Add `nofail` to fstab p1/p2 entries |
| SSH password expired on first boot | Trixie forces password change | Use `ssh -tt` with expect/pexpect to change interactively |
| `GLIBC_2.34 not found` | Cross-compiled binary vs old Debian (Stretch) | Must run on Trixie or Bookworm (glibc >= 2.34) |
| `-std=gnu23` unrecognized | GCC 13 uses `-std=gnu2x` for C23 draft | Already fixed in CMakeLists.txt |

---

## 6. Diagnostic Commands

```bash
# I2C - verify MPU-9250 responds
sudo apt install i2c-tools
i2cdetect -l                     # list buses
sudo i2cdetect -y 2              # probe bus 2
sudo i2cget -y 2 0x68 0x75       # WHO_AM_I (expect 0x71 for MPU-9250)

# Counter/eQEP - verify encoder hardware
ls /sys/bus/counter/devices/      # should show counter0, counter1, counter2
cat /sys/bus/counter/devices/counter0/count0/function  # should be "quadrature x4"

# PWM - verify motor driver
ls /sys/class/pwm/                # should show pwmchip0,1,2,3,5,7

# USB gadget
systemctl status bb-usb-gadgets   # should be "inactive (dead)" after successful setup
ls /sys/class/udc/                # should show musb-hdrc.0

# Network from Pi5
ping 192.168.7.2                  # USB ethernet to BBB
ls /dev/ttyACM0                   # USB CDC serial
```

---

## 7. NAT Forwarding (Pi5 -> BBB Internet Access)

The BBB only has USB network to the Pi5. To give it internet for apt:

```bash
# On Pi5:
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
sudo iptables -A FORWARD -i enxf45eab3527c1 -o wlan0 -j ACCEPT
sudo iptables -A FORWARD -i wlan0 -o enxf45eab3527c1 -m state --state RELATED,ESTABLISHED -j ACCEPT

# On BBB:
sudo ip route add default via 192.168.7.1
sudo resolvectl dns usb0 8.8.8.8
```
