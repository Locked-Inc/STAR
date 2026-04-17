# Prompt for Claude Code on the Raspberry Pi

Paste this whole thing into a fresh Claude Code session on the Pi.

---

I'm setting up development for the STAR project (Renesas RX72N firmware) on a
fresh Raspberry Pi running Linux. The project is at `~/Documents/github/STAR`
or wherever I clone it.

Read these first:
- `star-rx72n-firmware/usb_test/USB_BRINGUP_STATUS.md` -- explains the USB0
  bring-up, the 9 bugs that were fixed, and the working firmware
- The memory file you have about RX72N USB bring-up (9 bugs, `hoco_pid_fix.c`
  is the working firmware)

**Hardware attached to the Pi:**
- E2 Lite JTAG debugger (Renesas `045B:82a0`) -- for flashing the RX72N
- Analog Discovery 2 (Digilent, SN `210321A2AE49`, shows as FTDI `0403:6014`)
  -- for logic analysis; IO7 is wired to board pin 82 (PB3)
- The RX72N board's USB-C USB0 port -> Pi USB (this is what we're trying to
  enumerate as VID `0x1209` / PID `0x0001`)

**Goal:** get the environment to the point where you can:
1. Build `star-rx72n-firmware/usb_test/hoco_pid_fix.c` to a `.mot` file
2. Flash it via rfp-cli
3. Capture PB3 via AD2 with pydwf to see DVSQ state (0=Powered, 1=Default,
   2=Address, 3=Configured)
4. Verify the device enumerates: `lsusb | grep 1209`

**Install everything needed:**

1. **GNU RX toolchain (rx-elf-gcc)** -- The KPIT/Renesas GNURX build is
   x86-64 Linux only. On ARM64 Pi, either:
   - Use `qemu-user-static` + binfmt to run x86-64 binaries transparently:
     `sudo apt install qemu-user-static binfmt-support`, extract the x86-64
     toolchain to `/opt/gnurx/`, and it'll Just Work via binfmt.
   - OR use a Docker image with the toolchain and run with
     `--platform linux/amd64`.
   - Check `/opt/gnurx` isn't already populated first.

2. **Renesas Flash Programmer CLI (rfp-cli)** -- Same issue: no native ARM
   Linux build. Renesas provides Linux x86-64. Use qemu-user-static to run
   it. Install to `/opt/rfp/` and verify with `rfp-cli --version`. Needs
   libusb access to talk to E2 Lite.

3. **Digilent Waveforms SDK + pydwf** -- Digilent publishes ARM builds for
   Raspberry Pi specifically. Download the `.deb` from
   https://digilent.com/reference/software/waveforms/waveforms-3/start
   (ARM64 for Pi 4/5). Then `pip3 install --break-system-packages pydwf`
   (or use a venv).

4. **udev rules** for non-root USB access:
   - E2 Lite: vendor `045B`
   - AD2: vendor `0403`, product `6014`
   - Board USB-C once it enumerates: vendor `1209`, product `0001`
   - Add user to `plugdev` group if not already.

**Verification steps in order:**

1. `lsusb` should show E2 Lite (`045B:82a0`), AD2 (`0403:6014`), and any
   hubs.
2. `rfp-cli --version` works.
3. `rx-elf-gcc --version` works.
4. `python3 -c "from pydwf import DwfLibrary; dwf = DwfLibrary(); print(dwf.deviceEnum.enumerateDevices())"`
   prints `1` (or more).
5. Build firmware:
   ```bash
   cd star-rx72n-firmware/usb_test
   rx-elf-gcc -mcpu=rx72t -misa=v3 -mlittle-endian-data -std=gnu23 \
     -O0 -g3 -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
     startup.S hoco_pid_fix.c -o hoco_pid_fix.elf
   rx-elf-objcopy -O srec hoco_pid_fix.elf hoco_pid_fix.mot
   ```
6. Flash:
   ```bash
   sudo rfp-cli -d RX72N -t e2l -if fine -run \
     -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
     -a hoco_pid_fix.mot
   ```
7. After ~5s: `lsusb | grep 1209` should show the device.
   `sudo dmesg | tail -20` should show successful enumeration (no `-71` or
   `-110` errors).

If the device doesn't enumerate, use the AD2 to capture PB3 (pin 82) and
decode DVSQ state per the `USB_BRINGUP_STATUS.md` writeup.

**Don't re-litigate the 9 fixed bugs** -- they're already applied in
`hoco_pid_fix.c`. If enumeration fails, something in the Pi environment is
broken (toolchain, rfp-cli, USB access), not the firmware.
