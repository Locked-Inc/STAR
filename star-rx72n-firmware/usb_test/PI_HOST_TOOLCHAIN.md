# Raspberry Pi 5 Host Toolchain for RX72N USB0 Bring-up

This document is the actual setup that worked on a Raspberry Pi 5 running
Ubuntu 24.04 LTS (Noble), aarch64, kernel `6.8.0-1051-raspi`, on
**2026-04-16**. It supersedes the planning doc `PI_SETUP_PROMPT.md`,
which assumed plain `qemu-user-static` would carry the entire
x86-64-only Renesas toolchain. It does not.

The short version: **two emulators are required**, one for the GNU RX
build chain and a *different* one for `rfp-cli`, and they fight over the
same binfmt entry. The middle two-thirds of this document is about
making them coexist.

## Why two emulators

| Tool                                 | Emulator that works | Why the other one fails                                                                                                                                                                                                                                                                                                          |
|--------------------------------------|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `rx-elf-gcc`/`as`/`ld` at `/opt/gnurx/` | `qemu-user-static` 8.2 | `box64` 0.4.1 reliably segfaults the GNU assembler (`as`) the moment `gcc` forks it, even with `BOX64_DYNAREC=0`. The error is `Internal error (Segmentation fault). Please report this bug.` from `as`. The C front-end and linker run fine; only the assembler is sensitive. We ran out of patience figuring out exactly why. |
| `rfp-cli` (`/opt/rfp/linux-x64/rfp-cli`) | `box64` 0.4.1+ from ryanfortner's repo | `qemu-x86_64-static` 8.2 instantly SIGSEGVs inside QEMU itself (`x86_64-binfmt-P: QEMU internal SIGSEGV {code=MAPERR, addr=0x20}`) during the .NET CLR's address-space probing. The Renesas Flash Programmer is a self-contained .NET 8 single-file deployment (~29 MB), and qemu-user 8.x is known to be brittle with the .NET runtime. |

Both register for the same x86-64 ELF magic in `binfmt_misc`, so only one
can be the active interpreter at a time. We toggle.

## Hardware assumed connected

| Device                | USB ID         | Role                                                            |
|-----------------------|----------------|-----------------------------------------------------------------|
| Renesas E2 Lite       | `045b:82a0`    | The only path to flash the RX72N (FINE protocol over USB)       |
| Digilent Analog Discovery 2 (FT232H) | `0403:6014` (SN `210321A2AE49`) | Diagnostic GPIO sniffer. AD2 IO7 is wired to RX72N pin 82 (PB3). |
| Cypress USB-UART (CY7C65213) | `04b4:0003`    | Appears as `/dev/ttyACM0` *only when the RX72N is out of reset* -- confirms the chip booted after `make flash`. |
| RX72N USB0 USB-C port | `1209:0001` (after bring-up) | The device under test. Empty until enumeration succeeds. |

The Pi5 has its own internal RP1 hub. `lsusb -t` shows everything hanging off
`Bus 001`. The RX72N USB0 sits at `1-1.2.1`, the Cypress UART at `1-1.2.2`,
the E2 Lite at `1-1.2.3`, and the AD2 at `1-1.2.4`.

---

## Step 1 -- Switch to `feat/multi-led-breathe`

The `hoco_pid_fix.c` and `capture_dvsq.py` referenced below live on
`feat/multi-led-breathe`.

```bash
cd /workspaces/STAR
git fetch origin feat/multi-led-breathe
git checkout feat/multi-led-breathe
git pull
```

The repo uses Git LFS for the GNURX installer and the rfp-cli archive --
both will appear as 134-byte ASCII pointer files until you run:

```bash
sudo apt-get install -y git-lfs
git lfs install
git lfs pull --include "gcc-14.2.0.202511-GNURX-ELF.run,RFP_CLI_Linux_V32200_x64.tgz"
```

After `git lfs pull` the installer should be `~215 MB` and the rfp-cli
tarball `~53 MB`. If they are still 134 bytes, LFS is not initialised
or you do not have access to the LFS server.

---

## Step 2 -- Enable amd64 multiarch (qemu-user-static path)

`qemu-x86_64-static` runs the x86-64 GNURX binaries via binfmt, but those
binaries are dynamically linked against `libc.so.6`, `libstdc++.so.6`, etc.
qemu-user does not provide its own x86-64 sysroot -- it dlopens the host's
libraries through binfmt's `P` (preserve-argv0) flag. So the host needs
*x86-64* copies of those libs, which on aarch64 Ubuntu means amd64 multiarch.

The default `noble` ports repo (`http://ports.ubuntu.com/ubuntu-ports`)
serves only the architectures of "ports" CPUs (arm64, riscv64, ppc64el, ...) --
not amd64. Trying to install `libc6:amd64` from it returns 404. The fix
is to pin the ports repo to arm64 only and add the regular Ubuntu archive
for amd64:

```bash
# /etc/apt/sources.list.d/ubuntu.sources -- add Architectures: arm64 to BOTH stanzas
sudo sed -i '/URIs: http:\/\/ports.ubuntu.com\/ubuntu-ports/a Architectures: arm64' \
  /etc/apt/sources.list.d/ubuntu.sources

# /etc/apt/sources.list.d/amd64.sources -- new file, amd64 only, archive.ubuntu.com
sudo tee /etc/apt/sources.list.d/amd64.sources > /dev/null <<'EOF'
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: noble-security
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

sudo dpkg --add-architecture amd64
sudo apt-get update
sudo apt-get install -y qemu-user-static binfmt-support \
  libc6:amd64 libstdc++6:amd64 libncurses6:amd64 libtinfo6:amd64 zlib1g:amd64
```

Without the multiarch libs you get:

```
x86_64-binfmt-P: Could not open '/lib64/ld-linux-x86-64.so.2': No such file or directory
```

---

## Step 3 -- Install GNU RX 14.2 to `/opt/gnurx`

```bash
sudo /workspaces/STAR/gcc-14.2.0.202511-GNURX-ELF.run -p /opt/gnurx -y
```

The installer is itself an x86-64 ELF (not the historical 32-bit Makeself
SFX from older Renesas releases), so the libs from Step 2 are sufficient
to run it under qemu binfmt -- no extra `lib32` packages needed. Verify:

```bash
/opt/gnurx/bin/rx-elf-gcc --version
# rx-elf-gcc (GCC_Build_e13a947a1) 14.2.0.202511-GNURX 20240801
```

---

## Step 4 -- Install `rfp-cli` to `/opt/rfp`

```bash
sudo mkdir -p /opt/rfp
sudo tar xzf /workspaces/STAR/RFP_CLI_Linux_V32200_x64.tgz -C /opt/rfp
ls /opt/rfp/linux-x64/rfp-cli   # ~29 MB single-file .NET deployment
```

Do not try to run it yet -- under qemu-user it crashes; under stock
Ubuntu's box64 0.2.6 it hangs. We need newer box64.

---

## Step 5 -- Install **box64 v0.4.1+** for `rfp-cli`

The Ubuntu 24.04 universe ships `box64` and `box64-rpi4` at version
`0.2.6+dfsg-3` (from January 2024). That version reliably hangs `rfp-cli`
forever during .NET JIT -- worker thread spins at 99% CPU, no syscalls,
no progress past the `Load: ...mot` line.

The `ryanfortner/box64-debs` apt repository
(<https://ryanfortner.github.io/box64-debs/>) ships nightly builds. As of
2026-04-16 the candidate is `0.4.1+20260416.55e8ebf-1`, which works for
this workload.

```bash
sudo wget -q https://ryanfortner.github.io/box64-debs/box64.list \
  -O /etc/apt/sources.list.d/box64.list
wget -qO- https://ryanfortner.github.io/box64-debs/KEY.gpg | \
  sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/box64-debs-archive-keyring.gpg
sudo apt-get update
```

### Pitfall A -- the Pi5 variant SIGILLs

```bash
sudo apt-get install -y box64-rpi5arm64    # DON'T
```

`box64-rpi5arm64` is built assuming the Pi5's Cortex-A76 features (LSE
atomics, AES, SHA, ...). The Ubuntu Pi5 kernel exposes only
`fp asimd evtstrm crc32 cpuid` to userspace -- box64 reports
"Running on Cortex-A72" and SIGILLs the instant the .NET CLR hits an
LSE atomic. **Use the generic `box64` package**, which targets the
ARMv8.0 baseline:

```bash
cd /tmp
apt-get download box64                      # don't install via apt -- see Pitfall B
dpkg-deb -x box64_*.deb /tmp/box64_unpack/
sudo cp /tmp/box64_unpack/usr/local/bin/box64 /usr/local/bin/box64
sudo cp -r /tmp/box64_unpack/usr/lib/box64-i386-linux-gnu/. /usr/lib/box64-i386-linux-gnu/
sudo cp -r /tmp/box64_unpack/usr/lib/box64-x86_64-linux-gnu/. /usr/lib/box64-x86_64-linux-gnu/
/usr/local/bin/box64 --version
# Box64 arm64 v0.4.1 55e8ebf52 with Dynarec built on Apr 16 2026 07:52:20
```

### Pitfall B -- box64's `binfmt.d` and qemu's conflict

`box64-*.deb` ships `/etc/binfmt.d/box64.conf` and `/etc/binfmt.d/box32.conf`.
The qemu-user-static deb ships `/usr/lib/binfmt.d/qemu-x86_64.conf`. Both
register for the same x86-64 ELF magic. **They cannot both be installed via
apt** -- whichever is installed last triggers `dpkg` to remove the other one
(the conflict is registered in the box64 deb's `Conflicts:` field).

Workaround: install `qemu-user-static` via apt for tracked package
management, install `box64` by manually copying the binary out of the deb
(no apt install). Both `binfmt.d` entries register at boot; we toggle which
is active per command in Step 7.

---

## Step 6 -- udev rules and AD2 driver unbind

`/etc/udev/rules.d/99-star-rx72n.rules`:

```
# E2 Lite JTAG debugger (Renesas)
SUBSYSTEM=="usb", ATTRS{idVendor}=="045b", ATTRS{idProduct}=="82a0", \
  MODE="0666", GROUP="plugdev", TAG+="uaccess"

# Analog Discovery 2 (Digilent FT232H, vendored)
SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6014", \
  MODE="0666", GROUP="plugdev", TAG+="uaccess"

# Detach ftdi_sio so libdwf can claim the AD2 via libusb.
ACTION=="add", SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6014", \
  RUN+="/bin/sh -c 'echo $kernel:1.0 > /sys/bus/usb/drivers/ftdi_sio/unbind 2>/dev/null || true'"

# RX72N USB0 once enumerated (pid.codes test VID 0x1209 / PID 0x0001)
SUBSYSTEM=="usb", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="0001", \
  MODE="0666", GROUP="plugdev", TAG+="uaccess"
```

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger --action=add --subsystem-match=usb
```

The user must already be in `plugdev` (`groups star` should list it). If
not: `sudo gpasswd -a star plugdev` then re-login.

The `ftdi_sio` unbind is critical. Linux's stock USB-serial driver claims
any `0403:6014` device on plug-in, which makes it disappear from `libusb`'s
enumerable device list. With `ftdi_sio` unbound, `libdwf` can claim the
interface and `pydwf.DwfLibrary().deviceEnum.enumerateDevices()` returns
`1` instead of `0`.

---

## Step 7 -- Install Digilent Adept + Waveforms + pydwf

Digilent serves the .deb files from `files.digilent.com`. URLs use literal
spaces in the path that **must be `%20`-encoded** -- using `+` returns 404.

```bash
cd /tmp
curl -fL -o adept.deb \
  "https://files.digilent.com/Software/Adept2%20Runtime/2.27.9/digilent.adept.runtime_2.27.9-arm64.deb"
curl -fL -o waveforms.deb \
  "https://files.digilent.com/Software/Waveforms/3.24.3/digilent.waveforms_3.24.3_arm64.deb"
sudo apt-get install -y /tmp/adept.deb /tmp/waveforms.deb
```

**Do not use Waveforms 3.25.x on Ubuntu 24.04** -- it depends on
`libc6 (>= 2.41)` and Noble ships `2.39`. 3.24.3 is the latest version
that works.

```bash
pip3 install --break-system-packages pydwf       # for the user
sudo pip3 install --break-system-packages pydwf  # for sanity-checking as root
python3 -c "from pydwf import DwfLibrary; print(DwfLibrary().deviceEnum.enumerateDevices())"
# 1
```

---

## Step 8 -- The binfmt toggle dance

Both `qemu-x86_64` and `box64` are registered with the kernel for the same
ELF magic, both show up under `/proc/sys/fs/binfmt_misc/`. Only one is
enabled at a time -- writing `0` or `1` to the entry's procfs file flips
the per-entry `enabled` flag.

```bash
# Build the firmware -- uses qemu (box64 segfaults the assembler)
echo 0 | sudo tee /proc/sys/fs/binfmt_misc/box64        > /dev/null
echo 1 | sudo tee /proc/sys/fs/binfmt_misc/qemu-x86_64  > /dev/null

# ...build commands...

# Flash via rfp-cli -- needs box64 (qemu segfaults the .NET CLR)
echo 1 | sudo tee /proc/sys/fs/binfmt_misc/box64        > /dev/null
echo 0 | sudo tee /proc/sys/fs/binfmt_misc/qemu-x86_64  > /dev/null
```

Toggling is per-command, not persistent across reboot. `systemd-binfmt`
re-registers both at boot from the `binfmt.d` dir, with both *enabled*. At
that point whichever was registered later wins (in practice this is box64
because the package installs after qemu in our setup) -- so a fresh boot
defaults to box64-mode. Run the toggle commands explicitly before each step
so the state is unambiguous.

---

## Step 9 -- The rfp-cli wrapper

`/usr/local/bin/rfp-cli`:

```sh
#!/bin/sh
# rfp-cli wrapper: run via box64 with .NET diagnostics disabled.
# DOTNET_EnableDiagnostics=0 is REQUIRED -- without it the .NET CLR
# debug pipe deadlocks under box64 emulation and rfp-cli hangs at startup.
exec /usr/bin/env DOTNET_EnableDiagnostics=0 \
  /usr/local/bin/box64 /opt/rfp/linux-x64/rfp-cli "$@"
```

The `DOTNET_EnableDiagnostics=0` env var is non-negotiable. Without it the
.NET runtime spawns a "diagnostic IPC" thread that blocks on a named pipe
(`/tmp/clr-debug-pipe-<pid>-<startup-cookie>-in`) waiting for an external
debugger to attach. Under box64, the wakeup that should release that wait
never fires, the diagnostic thread holds the CLR startup mutex, and
`rfp-cli` hangs at 99% CPU forever in the JIT.

With the env var set, `rfp-cli` connects to the E2 Lite, erases, programs,
verifies, and disconnects in roughly 15 seconds.

---

## Step 10 -- Build, flash, verify

The full sequence, with binfmt flips inline:

```bash
cd /workspaces/STAR/star-rx72n-firmware/usb_test

# --- BUILD (binfmt = qemu-x86_64) ---
echo 0|sudo tee /proc/sys/fs/binfmt_misc/box64 >/dev/null
echo 1|sudo tee /proc/sys/fs/binfmt_misc/qemu-x86_64 >/dev/null

/opt/gnurx/bin/rx-elf-gcc -mcpu=rx72t -misa=v3 -mlittle-endian-data \
  -std=gnu23 -O0 -g3 -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
  startup.S hoco_pid_fix.c -o hoco_pid_fix.elf
/opt/gnurx/bin/rx-elf-objcopy -O srec hoco_pid_fix.elf hoco_pid_fix.mot

# --- FLASH (binfmt = box64) ---
echo 1|sudo tee /proc/sys/fs/binfmt_misc/box64 >/dev/null
echo 0|sudo tee /proc/sys/fs/binfmt_misc/qemu-x86_64 >/dev/null

rfp-cli -d RX72N -t e2l -if fine -run \
  -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
  -a hoco_pid_fix.mot -noprogress

# --- VERIFY ---
sleep 5
lsusb | grep 1209
# Bus 001 Device N: ID 1209:0001 Generic pid.codes Test PID

python3 capture_dvsq.py
# Samples: ~1390, avg PB3 = 1.000
#   -> DVSQ >= 3 (CONFIGURED) -- enumeration complete
```

If `lsusb` shows `1209:0001`, the firmware is enumerating cleanly and
nothing else needs to happen. If not, `dmesg | tail -30` will show the
host-side error code (`-71`, `-75`, `-110`, ...) and `capture_dvsq.py`
will show the device-side state, which together point at a specific bug
(see `USB_BRINGUP_STATUS.md`'s 10-bug index).

---

## Capturing host-side USB traffic (`usbmon`)

When the device-side state (PB3) and the host-side state (`dmesg`) disagree,
`usbmon` is the tiebreaker. It dumps every USB transaction on the bus in
ASCII text.

```bash
sudo modprobe usbmon

# Stream bus 1 to a file in the background
sudo bash -c 'cat /sys/kernel/debug/usb/usbmon/1u > /tmp/usbmon.log' &

# Trigger fresh enumeration (re-flashing toggles the D+ pull-up)
rfp-cli -d RX72N -t e2l -if fine -run \
  -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a hoco_pid_fix.mot

sleep 5
sudo pkill -f 'cat /sys/kernel/debug/usb/usbmon/1u'

# The new RX72N device address shows up in dmesg:
sudo dmesg | tail -30 | grep '1-1.2.1: new full-speed'

# Filter usbmon to that address
ADDR=...     # from dmesg
sudo grep -E ":1:0?$ADDR:0" /tmp/usbmon.log | head -30
```

The format (one line per submit `S` or complete `C`):

```
<urb>      <ts>      S Ci:1:029:0 s 80 06 0200 0000 0009 9 <
                                  ^^^^^^^^^^^^^^^^^^^^^^^^^
                                  setup packet: bmRequestType=80 (D2H, std, dev)
                                                bRequest=06 (GET_DESCRIPTOR)
                                                wValue=0x0200 (CONFIG, idx 0)
                                                wIndex=0x0000
                                                wLength=0x0009 (9 bytes!)
<urb>      <ts>      C Ci:1:029:0 -75 0
                                  ^^^^^
                                  status: -75 = EOVERFLOW (device sent more
                                                            than wLength)
```

This is exactly how bug 10 was nailed down: a `wLength = 9` SETUP completing
with status `-75` and zero data is the FIFO-padding signature.

---

## Things this Pi5 setup does NOT need

For future-self sanity, these were investigated and rejected:

- **Docker with `--platform linux/amd64`** -- the host's binfmt+qemu are
  what serve container x86-64 binaries, so we'd have the same qemu-vs-box64
  problem inside the container plus an extra layer.
- **Building qemu from source** -- newer qemu does have .NET fixes but
  we'd lose the apt-managed update path; box64 0.4.1 was the right escape
  hatch instead.
- **Registering box64 with higher binfmt priority than qemu** -- binfmt has
  no priority field. The order in `/proc/sys/fs/binfmt_misc/` is
  alphabetical; the kernel walks the list and uses the first *enabled*
  match. Toggling `enabled` is the only knob.
- **Running `rfp-cli` as root via `sudo`** -- the udev rule already gives
  `plugdev` access to the E2 Lite, so the wrapper script does not need
  `sudo`. Running .NET under sudo also creates a *root-owned*
  `/tmp/clr-debug-pipe-*` and a *root-owned* `/tmp/dotnet-diagnostic-*`
  socket which then prevents subsequent non-root runs from cleaning them
  up. Avoid.
- **`box64-bash`** -- ships with the box64 deb, not used here.
