# FlashManager Agent

Rust-based local hardware flashing agent for the FlashManager STAR Build & Flash System.

## Overview

The FlashManager Agent runs on your local machine with USB access to hardware devices. It polls the backend server for pending flash jobs, downloads compiled binaries, flashes local hardware, and reports results.

## Features

- **Multi-Platform Support**: Mac, Windows, Linux, WSL2
- **Multiple Hardware Types**:
  - ESP32 microcontrollers (via esptool.py)
  - Raspberry Pi 5 (via dd)
  - Android devices (via adb)
- **Secure Authentication**: JWT token-based device authentication
- **Automatic Polling**: Configurable polling interval (default: 5 seconds)
- **Heartbeat**: Regular status updates to server
- **Error Handling**: Comprehensive error reporting

## Prerequisites

### System Requirements

- Rust 1.70+ (install from https://rustup.rs)
- USB access to target hardware

### Tools Required

Install the appropriate tool(s) for your hardware:

#### ESP32 Flashing
```bash
# Install esptool
pip install esptool
```

#### Raspberry Pi 5 Flashing
```bash
# dd is pre-installed on most Unix systems
# Requires sudo access for writing to SD cards/USB drives
```

#### Android Flashing
```bash
# Install Android SDK Platform Tools
# Download from: https://developer.android.com/studio/releases/platform-tools

# Add adb to PATH
export PATH=$PATH:/path/to/platform-tools
```

## Installation

### 1. Build the Agent

```bash
cd agent
cargo build --release
```

The compiled binary will be at `target/release/flashmanager-agent`

### 2. Get Device API Key

Register your device through the FlashManager web UI:

1. Login to FlashManager frontend
2. Navigate to "Devices" page
3. Click "Register Device"
4. Copy the generated API key
5. Save it securely (you won't see it again)

## Usage

### Basic Usage

```bash
./flashmanager-agent --api-key YOUR_API_KEY_HERE
```

### All Options

```bash
flashmanager-agent [OPTIONS] --api-key <API_KEY>

Options:
  -a, --api-key <API_KEY>          API key for device authentication [required]
  -s, --server-url <SERVER_URL>    Backend server URL [default: http://localhost:8081]
  -p, --poll-interval <SECONDS>    Polling interval in seconds [default: 5]
  -n, --name <NAME>                Device name (for logging)
  -h, --help                       Print help
```

### Example with Custom Server

```bash
./flashmanager-agent \
  --api-key abc123... \
  --server-url https://flashmanager.example.com \
  --poll-interval 10 \
  --name my-workstation
```

## Running as a Service

### Linux (systemd)

Create `/etc/systemd/system/flashmanager-agent.service`:

```ini
[Unit]
Description=FlashManager Agent
After=network.target

[Service]
Type=simple
User=your-username
ExecStart=/path/to/flashmanager-agent --api-key YOUR_API_KEY --server-url http://server:8081
Restart=always
RestartSec=10
Environment="RUST_LOG=info"

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable flashmanager-agent
sudo systemctl start flashmanager-agent
sudo systemctl status flashmanager-agent
```

### macOS (launchd)

Create `~/Library/LaunchAgents/com.flashmanager.agent.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.flashmanager.agent</string>
    <key>ProgramArguments</key>
    <array>
        <string>/path/to/flashmanager-agent</string>
        <string>--api-key</string>
        <string>YOUR_API_KEY</string>
        <string>--server-url</string>
        <string>http://server:8081</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/tmp/flashmanager-agent.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/flashmanager-agent.error.log</string>
</dict>
</plist>
```

Load the agent:
```bash
launchctl load ~/Library/LaunchAgents/com.flashmanager.agent.plist
```

## Architecture

### Polling Flow

```
1. Agent authenticates with backend using API key
2. Receives JWT token for subsequent requests
3. Every N seconds:
   a. Poll backend for pending flash jobs
   b. For each job:
      - Claim the job (marks as IN_PROGRESS)
      - Download binary from backend
      - Flash hardware using appropriate tool
      - Report success/failure with duration
   c. Send heartbeat to update device status
4. Sleep and repeat
```

### Hardware Detection

- **ESP32**: Specify serial port (e.g., `/dev/ttyUSB0`, `COM3`)
- **Pi5**: Specify SD card device (e.g., `/dev/sdb`, `/dev/disk2`)
- **Android**: Use device ID or "any" for first connected device

### GraphQL Communication

The agent uses standard GraphQL queries/mutations:
- `deviceLogin`: Authenticate and get JWT token
- `pollFlashJobs`: Get list of pending jobs for this device
- `claimFlashJob`: Mark job as claimed
- `updateFlashStatus`: Update job progress
- `completeFlashJob`: Report final result with duration
- `updateDeviceHeartbeat`: Keep device status as ONLINE

## Troubleshooting

### "esptool.py not found"
```bash
pip install esptool
# Or on some systems:
pip3 install esptool
```

### "Permission denied" when flashing Pi5
```bash
# Run with sudo or add user to disk group
sudo usermod -a -G disk $USER
# Logout and login again
```

### "No Android devices connected"
```bash
# Check adb devices
adb devices

# Enable USB debugging on Android device
# Settings > Developer Options > USB Debugging

# Try different USB cable (some are charge-only)
```

### "Authentication failed"
- Verify API key is correct
- Check device hasn't been deleted from backend
- Ensure backend server URL is correct

### Enable Debug Logging
```bash
RUST_LOG=debug ./flashmanager-agent --api-key YOUR_KEY
```

## Development

### Building

```bash
# Debug build
cargo build

# Release build (optimized)
cargo build --release

# Run tests
cargo test

# Check code
cargo clippy
```

### Project Structure

```
src/
├── main.rs          # Entry point, polling loop
├── client.rs        # GraphQL client
├── config.rs        # Configuration
└── flasher/
    ├── mod.rs       # Flasher trait
    ├── esp32.rs     # ESP32 flasher (esptool)
    ├── pi5.rs       # Pi5 flasher (dd)
    └── android.rs   # Android flasher (adb)
```

## Security Considerations

- API key is stored in memory only
- JWT tokens expire and are renewed automatically
- All communication over HTTPS in production
- No sensitive data stored on disk
- Requires explicit hardware access permissions

## Performance

- Binary size: ~2MB (release build with optimizations)
- Memory usage: ~10MB typical
- CPU usage: <1% when idle, varies during flashing
- Network: Minimal (polling + binary downloads)

## Future Enhancements

- Binary caching to avoid re-downloading
- Progress reporting during large flashes
- Multiple device support per agent
- Web UI for agent status
- Automatic device capability detection
