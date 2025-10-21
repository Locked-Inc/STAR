#!/bin/bash
# Check all available UART/serial devices on the system

echo "============================================================"
echo "UART/Serial Device Discovery"
echo "============================================================"
echo ""

echo "📋 All serial devices:"
ls -l /dev/tty{PS,S,USB,ACM,AMA}* 2>/dev/null || echo "  None found"
echo ""

echo "🔍 Device details:"
echo ""

for pattern in "/dev/ttyAMA*" "/dev/ttyS*" "/dev/ttyUSB*" "/dev/ttyACM*"; do
    for dev in $pattern; do
    if [ -e "$dev" ]; then
        echo "Device: $dev"
        ls -l $dev

        # Check if it's the console
        if grep -q "$(basename $dev)" /proc/cmdline 2>/dev/null; then
            echo "  ⚠️  This is the system console"
        fi

        # Try to get more info
        if command -v udevadm &> /dev/null; then
            udevadm info $dev 2>/dev/null | grep -E "ID_MODEL|ID_VENDOR|DEVPATH" | head -3
        fi

        echo ""
    fi
    done
done

echo "============================================================"
echo "💡 Key Info:"
echo "============================================================"
echo ""
echo "ttyAMA* = Raspberry Pi UART (ARM processor built-in UART)"
echo "  ttyAMA0 = Usually system console"
echo "  ttyAMA1 = May be available for 40-pin header"
echo ""
echo "ttyUSB* = USB-UART adapters (FTDI, etc.)"
echo "  ttyUSB0 = Your FTDI adapter"
echo ""
echo "For 40-pin header (pins 8, 10):"
echo "  These are typically mapped to ttyAMA1 or ttyS1"
echo "  But may not be enabled in device tree"
echo ""

echo "============================================================"
echo "🔍 Checking device tree for UART configuration"
echo "============================================================"
echo ""

if [ -d /proc/device-tree/amba/serial* ]; then
    echo "Found UART entries in device tree:"
    ls -d /proc/device-tree/amba/serial* 2>/dev/null
    echo ""

    for uart in /proc/device-tree/amba/serial*; do
        if [ -f "$uart/status" ]; then
            status=$(cat "$uart/status" 2>/dev/null)
            echo "$(basename $uart): status = $status"
        fi
    done
else
    echo "No device tree entries found for UART"
fi

echo ""
echo "============================================================"
echo "📝 Summary:"
echo "============================================================"
echo ""
echo "If ttyAMA1 exists and is NOT console:"
echo "  -> Use this for 40-pin header UART"
echo ""
echo "If ttyAMA1 does NOT exist:"
echo "  -> UART1 not enabled in device tree"
echo "  -> Need custom device tree or FPGA overlay"
echo "  -> Use GPIO software UART as fallback"
echo ""
