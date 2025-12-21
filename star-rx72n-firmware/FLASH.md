# Flashing RX72N Firmware

Guide to programming the compiled firmware onto RX72N hardware.

## Hardware Options

### 1. Renesas E2 Lite (Recommended - $25)

**Most cost-effective official debugger:**
- Part number: R0E000200xxx (region-specific)
- Supports JTAG and FINE (Renesas proprietary)
- Works with Renesas Flash Programmer and e² studio
- Purchase: [Renesas Store](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rx-32-bit-performance-efficiency-mcus/rte0t00020kce00000r-e2-emulator-lite-renesas-flash-programmer)

### 2. Renesas E2 Emulator (~$200)

**Full-featured debugger:**
- Part number: R0E000200xxx
- Faster programming, trace capability
- Better for active development

### 3. Segger J-Link (If Available)

**Third-party option:**
- J-Link EDU ($60), J-Link BASE (~$400)
- Check RX72N support: [J-Link Device List](https://www.segger.com/products/debug-probes/j-link/technology/cpus-and-devices/renesas-rx/)
- Not all RX devices supported

### 4. Serial Bootloader (No Debugger)

**UART-based flashing:**
- Requires bootloader pre-programmed in ROM
- Uses SCI (UART) + RESET/BOOT pins
- Slower, no debugging capability

## Method 1: Renesas Flash Programmer (GUI)

**Best for: First-time setup, infrequent flashing**

### Installation

1. Download [Renesas Flash Programmer (RFP)](https://www.renesas.com/us/en/software-tool/renesas-flash-programmer-programming-gui)
2. Install for your platform (Windows/Mac/Linux)
3. Connect E2 Lite to your PC (USB) and RX72N board (14-pin debug connector)

### Flashing Steps

1. Launch Renesas Flash Programmer
2. **Select microcontroller:**
   - Device: RX72N
   - Part number: R5F572NNHGFP
3. **Select tool:**
   - Tool: E2 Lite (on-board)
   - Interface: FINE (or JTAG if FINE unavailable)
4. **Connect:**
   - Click "Connect"
   - Should show "Connected" status
5. **Load firmware:**
   - Browse → Select `build/star-rx72n-firmware.hex`
6. **Program:**
   - Click "Start" to erase, program, and verify
   - Progress bar shows status
7. **Reset:**
   - Click "Reset" to run firmware

### Pinout (E2 Lite 14-pin)

```
E2 Lite Connector → RX72N Board
═══════════════════════════════
Pin 1  (VCC)       → 3.3V
Pin 2  (MD/FINED)  → MD (Mode pin)
Pin 4  (TMS/FINEC) → P67 or JTAG TMS
Pin 5  (GND)       → GND
Pin 6  (TCK)       → JTAG TCK (if using JTAG)
Pin 7  (TDO)       → JTAG TDO (if using JTAG)
Pin 9  (TDI)       → JTAG TDI (if using JTAG)
Pin 10 (RESET#)    → RES# (Reset)
Pin 12 (EMLE)      → GND (for serial programming mode)
```

**Mode Selection:**
- **FINE:** MD = High (3.3V) via 4.7kΩ pull-up
- **JTAG:** MD = Low (GND)

## Method 2: Command Line (Automation/CI)

**Best for: Automated builds, scripting**

### Renesas Flash Programmer CLI

```bash
# Flash firmware (Windows example)
rfp-cli.exe \
  --device RX72N \
  --tool E2Lite \
  --interface FINE \
  --file build/star-rx72n-firmware.hex \
  --erase \
  --program \
  --verify \
  --reset

# Flash firmware (Linux/Mac)
rfp-cli \
  --device RX72N \
  --tool E2Lite \
  --interface FINE \
  --file build/star-rx72n-firmware.hex \
  --erase --program --verify --reset
```

### Batch Script (Windows)

Create `flash.bat`:
```batch
@echo off
rfp-cli.exe ^
  --device RX72N ^
  --tool E2Lite ^
  --interface FINE ^
  --file build\star-rx72n-firmware.hex ^
  --erase --program --verify --reset

if %errorlevel% equ 0 (
  echo Flashing successful!
) else (
  echo Flashing failed with error %errorlevel%
)
```

### Shell Script (Mac/Linux)

Create `flash.sh`:
```bash
#!/bin/bash
rfp-cli \
  --device RX72N \
  --tool E2Lite \
  --interface FINE \
  --file build/star-rx72n-firmware.hex \
  --erase --program --verify --reset

if [ $? -eq 0 ]; then
  echo "Flashing successful!"
else
  echo "Flashing failed with error $?"
fi
```

## Method 3: J-Link (If Supported)

**Best for: If you already have J-Link**

### Using J-Link Commander

```bash
# Create flash script
cat > flash.jlink << 'EOF'
device R5F572NN
si JTAG
speed 4000
r
erase
loadfile build/star-rx72n-firmware.hex
r
g
q
EOF

# Flash
JLinkExe -CommandFile flash.jlink
```

### Using J-Flash

1. Open J-Flash
2. File → Open Project → Create new project
3. Device: R5F572NN (if listed)
4. Interface: JTAG, Speed: 4000 kHz
5. File → Open data file → `build/star-rx72n-firmware.hex`
6. Target → Connect
7. Target → Production Programming (Erase, Program, Verify)

## Method 4: Serial Bootloader (UART)

**Best for: Field updates without debugger**

### Prerequisites

- RX72N bootloader firmware (usually pre-programmed or via initial flash)
- USB-to-UART adapter (FTDI, CP2102, etc.)

### Connections

```
USB-UART → RX72N Board
═════════════════════
TX  → RXD (P30/SCI1)
RX  → TXD (P26/SCI1)
GND → GND
```

### Boot Mode Entry

1. Connect UART
2. Set boot mode: MD = Low (GND), PC7 = Low
3. Press RESET
4. RX72N enters bootloader mode (SCI)

### Using Renesas Flash Programmer

1. Tool: Select "Serial Interface"
2. Port: Select COM port (e.g., COM3, /dev/ttyUSB0)
3. Baud rate: 115200 (or bootloader default)
4. Load HEX file
5. Program

### Using RX Programmer CLI (Third-party)

```bash
# Install rxprog (if available)
pip install rxprog

# Flash via UART
rxprog -d /dev/ttyUSB0 -b 115200 build/star-rx72n-firmware.hex
```

## Verification

After flashing, verify the firmware is running:

### 1. LED Blink Test

- LED on PORT0, Pin 0 should blink at 500ms intervals
- Indicates ThreadX scheduler is running

### 2. UART Debug Output

Connect to SCI1 (115200 baud, 8N1):

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Windows (PuTTY, TeraTerm, etc.)
# Port: COM3, Baud: 115200, 8N1
```

Expected output:
```
===========================================
STAR RX72N Firmware v1.0.0
ThreadX RTOS Starting...
===========================================

[Demo Task] Count: 1
[Demo Task] Count: 2
[Demo Task] Count: 3
...
```

### 3. Check Memory Map

After programming, RFP shows:
- Code Flash: ~7 KB used / 4 MB total
- RAM: ~53 KB used / 1 MB total

## Troubleshooting

### "Cannot connect to E2 Lite"

**Check:**
1. E2 Lite USB cable connected
2. E2 Lite power LED on
3. 14-pin cable to RX72N properly seated
4. RX72N board powered (3.3V)
5. Mode pin (MD) set correctly (High for FINE, Low for JTAG)
6. E2 Lite drivers installed (Device Manager on Windows)

**Fix:**
```bash
# Windows: Reinstall E2 Lite drivers
# Download from Renesas website

# Linux: Add udev rules
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="045b", MODE="0666"' | sudo tee /etc/udev/rules.d/99-renesas.rules
sudo udevadm control --reload-rules
```

### "ID check error"

**Cause:** Wrong device selected or connection issue

**Fix:**
1. Verify R5F572NNHGFP#30 selected in RFP
2. Check connections (especially GND and VCC)
3. Try "Auto" device detection in RFP

### "Program error at address 0x..."

**Cause:** Code protection, flash corruption, or bad HEX file

**Fix:**
1. Full chip erase before programming
2. Check HEX file integrity
3. Try lowering programming speed
4. Reset code protection bits

### "Verify error"

**Cause:** Flashing succeeded but verification failed

**Fix:**
1. Re-flash with slower clock speed
2. Check power supply stability
3. Inspect debug cable for loose connections

### UART Bootloader Not Responding

**Check:**
1. Boot mode pins set correctly (MD=Low, PC7=Low)
2. UART TX/RX not swapped
3. Correct baud rate (usually 115200)
4. RESET pressed after setting boot pins
5. Bootloader actually present in ROM

## Production Flashing

For manufacturing/production:

### Batch Programming

1. Use RFP CLI in batch script
2. Store HEX files with version numbers
3. Log results to file

Example:
```bash
#!/bin/bash
VERSION="1.0.0"
HEX="build/star-rx72n-firmware-${VERSION}.hex"
LOG="flash_log_$(date +%Y%m%d_%H%M%S).txt"

echo "Flashing firmware v${VERSION}..." | tee -a $LOG
rfp-cli \
  --device RX72N \
  --file $HEX \
  --erase --program --verify | tee -a $LOG

if [ $? -eq 0 ]; then
  echo "SUCCESS: Firmware v${VERSION} programmed" | tee -a $LOG
else
  echo "FAILED: Flashing error" | tee -a $LOG
  exit 1
fi
```

### Gang Programmer

For high-volume production:
- Use Renesas UART Gang Programmer (up to 8 devices simultaneously)
- Or parallel E2 emulators with automation

## Debugging with GDB

### Prerequisites

1. **E2 Lite or J-Link** connected to RX72N
2. **rx-elf-gdb** installed (part of GNURX toolchain)
3. **GDB server** running:
   - For E2 Lite: Use e² studio or OpenOCD
   - For J-Link: Use JLinkGDBServer

### Quick Debug (Make Target)

```bash
# Terminal 1: Start GDB server (choose one)
# E2 Lite (example - actual command depends on your setup)
e2-server-cli --device RX72N --port 2331

# J-Link
JLinkGDBServer -device R5F572NN -if JTAG -speed 4000 -port 2331

# Terminal 2: Run debug target (auto-connects, loads, resets)
cd build
make debug
```

This launches `rx-elf-gdb` and automatically:
1. Connects to GDB server on `localhost:2331`
2. Resets the target
3. Loads the ELF file
4. Resets again and halts at `main()`

### Manual GDB Session

For more control:

```bash
# Start GDB
rx-elf-gdb build/star-rx72n-firmware.elf

# In GDB prompt:
(gdb) target remote localhost:2331    # Connect to server
(gdb) monitor reset                   # Reset MCU
(gdb) load                            # Flash firmware
(gdb) monitor reset                   # Reset again
(gdb) break main                      # Breakpoint at main()
(gdb) continue                        # Run to main()
```

### Common GDB Commands

```gdb
# Execution control
(gdb) continue                # Run until breakpoint
(gdb) next                    # Step over (source line)
(gdb) step                    # Step into function
(gdb) finish                  # Run until function returns
(gdb) until                   # Run until line number

# Breakpoints
(gdb) break main              # Break at main()
(gdb) break timer.c:32        # Break at timer.c line 32
(gdb) break cmt0_isr          # Break at interrupt handler
(gdb) info breakpoints        # List all breakpoints
(gdb) delete 1                # Delete breakpoint #1
(gdb) disable 2               # Disable breakpoint #2

# Inspection
(gdb) print led_thread        # Print variable
(gdb) print/x SYSTEM.PRCR     # Print in hex
(gdb) x/16xw 0xFFF80000       # Examine 16 words at address
(gdb) info registers          # Show all registers
(gdb) info locals             # Show local variables
(gdb) backtrace               # Show call stack

# Memory
(gdb) x/16xw 0x00000000       # Examine RAM
(gdb) x/16xw 0xFFFF0000       # Examine Flash
(gdb) set var counter = 0     # Modify variable
(gdb) set *(uint32_t*)0x87000 = 0x12345678  # Write memory

# Threading (ThreadX)
(gdb) info threads            # List all ThreadX threads
(gdb) thread 2                # Switch to thread 2
(gdb) where                   # Show current thread's stack
```

### Debugging ThreadX Tasks

```gdb
# Break in specific task
(gdb) break led_task_entry
(gdb) continue

# When breakpoint hits:
(gdb) print led_thread        # Show thread control block
(gdb) info threads            # Show all tasks
(gdb) backtrace               # Show task call stack
```

### CLion Integrated Debugging

#### 1. Start GDB Server

```bash
# J-Link (most common)
JLinkGDBServer -device R5F572NN -if JTAG -speed 4000 -port 2331

# Or E2 Lite (via e² studio or OpenOCD)
openocd -f interface/renesas_e2.cfg -f target/renesas_rx72n.cfg
```

#### 2. Configure CLion

1. **Run → Edit Configurations → Add → Embedded GDB Server**

2. **Configuration:**
   - **Name:** RX72N Debug
   - **Target:** star-rx72n-firmware.elf
   - **'target remote' args:** `localhost:2331`
   - **GDB:** `/opt/gnurx/bin/rx-elf-gdb` (or your GNURX path)
   - **Download executable:** ✅ Always
   - **Reset after download:** `monitor reset`

3. **Advanced Settings:**
   ```
   Commands before:
   set mem inaccessible-by-default off
   monitor reset

   Commands after:
   monitor reset
   break main
   ```

#### 3. Debug Session

1. Click **Debug** button (or Shift+F9)
2. CLion loads firmware and breaks at `main()`
3. Use normal CLion debugging:
   - Click gutter to set breakpoints
   - F8 = Step Over, F7 = Step Into
   - Hover variables to inspect
   - Watches, Call Stack, Variables windows

### Debugging Tips

#### 1. Stack Overflow Detection

ThreadX can detect stack overflow:

```c
// In tx_user.h (already enabled)
#define TX_ENABLE_STACK_CHECKING
```

If task crashes, check stack:
```gdb
(gdb) info locals
(gdb) backtrace full
(gdb) x/128xw $sp           # Examine stack
```

#### 2. Interrupt Debugging

Break in ISR:
```gdb
(gdb) break cmt0_isr
(gdb) continue
# When it hits:
(gdb) backtrace             # May show <signal handler>
(gdb) info registers        # Check interrupt state
```

**Note:** Some debuggers pause the MCU, preventing interrupts from firing naturally.

#### 3. Hard Fault Debugging

If firmware crashes (hard fault):

```gdb
# Check fault status registers
(gdb) print/x *(uint32_t*)0x00087000    # ICU.IR (interrupt request)
(gdb) info registers                    # Check PC, SP

# Check reset vector
(gdb) x/x 0xFFFFFFFC                    # Should point to _PowerON_Reset
```

#### 4. Live Variable Modification

Useful for testing without reflashing:

```gdb
# Change LED blink rate without recompiling
(gdb) break led_task_entry
(gdb) continue
# Inside task:
(gdb) set var input = 10    # Change task parameter
(gdb) continue
```

### Debugging Without Debugger

If you don't have E2 Lite or J-Link, use **printf debugging:**

```c
// In src/main.c or task
void led_task_entry(ULONG input) {
    uart_puts("[LED Task] Starting\r\n");

    while (1) {
        uart_puts("[LED Task] Toggle\r\n");
        led_toggle();

        tx_thread_sleep(50);
    }
}
```

Monitor via UART:
```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Windows (PuTTY)
# COM3, 115200, 8N1
```

### Common Debug Scenarios

#### Scenario 1: Code Not Running

**Symptom:** Firmware flashes OK, but LED doesn't blink

**Debug:**
```gdb
(gdb) break main
(gdb) continue
# If it breaks at main:
(gdb) next
(gdb) next
... # Step through to find where it stops
```

**Common causes:**
- Clock not configured (stuck in `clock_init()`)
- Stack overflow (task crashes immediately)
- ThreadX scheduler not starting (`tx_kernel_enter()` issue)

#### Scenario 2: Interrupt Not Firing

**Symptom:** System tick not working, tasks not switching

**Debug:**
```gdb
(gdb) break cmt0_isr
(gdb) continue
# Wait 10ms (should hit breakpoint)

# If it doesn't hit:
(gdb) print/x CMT0.CMCR       # Check timer config
(gdb) print/x ICU.IER[3]      # Check interrupt enable
```

**Common causes:**
- Interrupt not enabled in ICU
- Timer not started (CMSTR0 bit not set)
- Wrong interrupt vector number

#### Scenario 3: Task Crashes

**Symptom:** ThreadX starts but task crashes

**Debug:**
```gdb
(gdb) break led_task_entry
(gdb) continue
# Step through task:
(gdb) next
(gdb) next
# When it crashes, check:
(gdb) backtrace full
(gdb) info locals
```

**Common causes:**
- Stack too small (increase `LED_TASK_STACK_SIZE`)
- Null pointer dereference
- Accessing invalid peripheral register

## Next Steps

Once firmware is running:

1. ✅ Verify LED blink (confirms ThreadX + GPIO working)
2. ✅ Check UART output (confirms SCI + printf working)
3. **Debug with GDB:** Set breakpoints, step through code
4. **Add motor control:** Test MTU3a PWM output
5. **Test encoders:** Verify PCNT counting
6. **Test ADC:** Read current sensors
7. **Test SPI:** Communicate with RPi5

## References

- [Renesas Flash Programmer User Manual](https://www.renesas.com/us/en/document/mat/renesas-flash-programmer-v3-users-manual)
- [E2 Lite User Manual](https://www.renesas.com/us/en/document/man/e2-emulator-lite-users-manual-rej06b0417)
- [RX72N Hardware Manual](https://www.renesas.com/us/en/document/man/rx72n-group-users-manual-hardware)
- [GDB Manual](https://sourceware.org/gdb/current/onlinedocs/gdb/)
- [J-Link GDB Server](https://www.segger.com/products/debug-probes/j-link/tools/j-link-gdb-server/)
