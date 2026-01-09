#!/bin/bash
# Demo script to run mock BMS device with CLI
# STAR Project - Texas A&M University

set -e

echo "=== STAR BMS Tool Mock Demo ==="
echo

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "Error: socat is not installed"
    echo "Install with: brew install socat"
    exit 1
fi

# Create PTY pair
echo "Creating PTY pair..."
socat -d -d pty,raw,echo=0,link=/tmp/bms_mock pty,raw,echo=0,link=/tmp/bms_client 2>&1 &
SOCAT_PID=$!
sleep 1

echo "PTY pair created:"
echo "  Mock device: /tmp/bms_mock"
echo "  Client:      /tmp/bms_client"
echo

# Start mock device in background
echo "Starting mock BMS device..."
./target/release/mock_device /tmp/bms_mock &
MOCK_PID=$!
sleep 1

echo "Mock device running (PID: $MOCK_PID)"
echo

# Run CLI commands
echo "=== Running CLI Commands ==="
echo

echo "1. Reading telemetry..."
./target/release/app --port /tmp/bms_client telemetry
echo

echo "2. Reading cell voltages (4 cells)..."
./target/release/app --port /tmp/bms_client cell-voltages --num-cells 4
echo

echo "3. Reading device info..."
./target/release/app --port /tmp/bms_client device-info
echo

echo "4. Reading register 0x00..."
./target/release/app --port /tmp/bms_client read-register 0x00
echo

echo "5. Writing to register 0x10..."
./target/release/app --port /tmp/bms_client write-register 0x10 0x42
echo

# Cleanup
echo "=== Cleaning up ==="
kill $MOCK_PID 2>/dev/null || true
kill $SOCAT_PID 2>/dev/null || true
rm -f /tmp/bms_mock /tmp/bms_client

echo "Demo complete!"
