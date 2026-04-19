# Pi Deployment Guide

Setup and GitHub integration reference for the STAR Raspberry Pi 5 (native aarch64).

---

## Pi Native Environment

### What's Installed

| Package | Version / Notes |
|---------|----------------|
| ROS2 Jazzy Desktop | via apt (packages.ros.org) |
| `ros-jazzy-nav2-lifecycle-manager` | required by `safety_monitor.launch.py` |
| `ros-jazzy-rmw-cyclonedds-cpp` | RMW middleware |
| Go | 1.25.7 at `/usr/local/go` |
| golangci-lint | v2.10.1 at `/usr/local/bin/golangci-lint` |
| buf CLI | v1.50.0 at `/usr/local/bin/buf` |
| libprotobuf-dev, libgrpc++-dev, protobuf-compiler-grpc | system packages |

### Required `.bashrc` Additions

```bash
source /opt/ros/jazzy/setup.bash
if [ -f /workspaces/STAR/star-ros2/install/local_setup.bash ]; then
    source /workspaces/STAR/star-ros2/install/local_setup.bash
fi
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export PATH="$PATH:/usr/local/go/bin"
export GOPATH="$HOME/go"
export PATH="$PATH:$GOPATH/bin"
```

### SPI Setup (for `star_spi_bridge` with hardware)

```bash
sudo groupadd -f spi
sudo usermod -aG spi $USER
echo 'SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"' | sudo tee /etc/udev/rules.d/99-spi.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
# Reboot or re-login for group membership to take effect
```

> **Note:** Use `99-spi.rules` (not `50-spi.rules`) -- the higher priority is needed for
> the rule to apply correctly on Raspberry Pi OS / Ubuntu.

### Build

```bash
cd /workspaces/STAR
git pull
./build-ros2.sh   # handles proto gen + colcon build
```

After any apt package additions, update the "What's Installed" table above so the setup
is reproducible if the Pi is reimaged.

---

## Pi to GitHub: SSH Access

Use a scoped deploy key -- one key per Pi, scoped to this repo only.

### Generate the Key

```bash
ssh-keygen -t ed25519 -C "star-pi5@star-desktop" -f ~/.ssh/id_ed25519_star
cat ~/.ssh/id_ed25519_star.pub   # copy this output
```

Use a strong passphrase when prompted. On the Pi, use `ssh-agent` so you only unlock once
per session:

```bash
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519_star
```

### Add to GitHub

1. Go to the repo: **Settings -> Deploy Keys -> Add deploy key**
2. Title: `Star Pi 5 - star-desktop`
3. Paste the public key
4. **Write access:** only enable if the Pi needs to push commits or branches. If the Pi
   is used only to pull and build, leave this unchecked (read-only is safer on embedded
   hardware). This Pi is used for development (creating PRs from the Pi), so write
   access is enabled here.

### Configure SSH

```ssh-config
# ~/.ssh/config
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_star
    IdentitiesOnly yes
```

Test: `ssh -T git@github.com` -- should print "Hi Locked-Inc/STAR! You've authenticated..."

### Why Deploy Keys Over Personal Account Keys

- Scoped to one repo only -- no access to other repos
- Revokable per device without affecting other developers
- No personal credentials on shared or embedded hardware

---

## What Belongs in the Repo vs Stays Local

| Belongs in repo (commit) | Stays local on Pi (never commit) |
|--------------------------|----------------------------------|
| `src/` package source code | `build/`, `install/`, `log/` |
| `config/*.yaml` params | `compile_commands.json` (machine-specific) |
| `launch/*.py` files | `.bashrc` / shell environment additions |
| `.gitignore`, CI configs | SSH private keys (`~/.ssh/id_ed25519_star`) |
| Scripts in `scripts/` | Baseline run result files (use CI artifacts) |
| `star-ros2/.gitignore` | WiFi passwords, network topology, IP addresses |
| Proto schemas and gen output | Device paths if they vary per machine |

---

## Secrets and Environment Variables

Set these in `.bashrc` only -- document the variable name here, never its value in the repo:

| Variable | Purpose |
|----------|---------|
| `ROS_DOMAIN_ID` | Isolates DDS traffic. `0` = default (single machine). Use a non-zero value (e.g. `42`) to isolate from other ROS2 systems on the same LAN. |
| `RMW_IMPLEMENTATION` | Already in `.bashrc`; also set in launch files for portability. |
| Hardware serials, API keys, network credentials | Store in `.bashrc` or a local `.env` (gitignored). |

The repo's `.gitignore` already excludes `.env` and `.local` files at the root level.

---

## Self-Hosted GitHub Actions Runner (Native ARM64 CI)

The existing `ros2.yml` runs in `osrf/ros:jazzy-desktop` on x86 GitHub-hosted runners.
To get native aarch64 test results from this Pi:

### Register the Runner

1. Go to: **Settings -> Actions -> Runners -> New self-hosted runner**
2. Select: **Linux / ARM64**
3. On the Pi, follow the commands GitHub shows (they include a one-time token):

    ```bash
    mkdir ~/actions-runner && cd ~/actions-runner
    curl -o actions-runner-linux-arm64.tar.gz -L <URL-from-GitHub>
    tar xzf ./actions-runner-linux-arm64.tar.gz
    ./config.sh --url https://github.com/Locked-Inc/STAR --token <ONE-TIME-TOKEN>
    ```

4. Install as a systemd service so it survives reboots:

    ```bash
    sudo ./svc.sh install
    sudo ./svc.sh start
    ```

The runner will appear in **Settings -> Actions -> Runners** as `star-desktop`.

> The registration token is one-time and short-lived. The Pi persists as a registered
> runner after setup -- only the initial `config.sh` step needs the token.

### Add a Pi Job to `ros2.yml`

To use the runner, add a second job to `.github/workflows/ros2.yml`:

```yaml
build-and-test-pi:
  name: Build ROS2 Packages (Pi5 native)
  runs-on: [self-hosted, linux, ARM64]
  steps:
    - uses: actions/checkout@v4
    - name: Build
      shell: bash
      run: |
        source /opt/ros/jazzy/setup.bash
        ./build-ros2.sh
    - name: Test
      shell: bash
      run: |
        source /opt/ros/jazzy/setup.bash
        cd star-ros2
        source install/local_setup.bash
        colcon test
        colcon test-result --verbose
```

Note: no `container:` directive -- this runs directly on the Pi's native environment.
The explicit `source /opt/ros/jazzy/setup.bash` is required because GitHub Actions uses
a non-interactive shell and does not source `.bashrc`.

---

## Keeping the Pi in Sync

Daily workflow:

```bash
cd /workspaces/STAR
git pull
./build-ros2.sh   # rebuild if star-ros2/ or star-proto/ changed
```

---

## Host-Level Robot Settings (one-time)

Run once on every Pi5 after flashing the OS:

```bash
sudo bash /workspaces/STAR/scripts/setup-pi5-host.sh
```

That installs:

| File | Purpose |
|------|---------|
| `/etc/udev/rules.d/70-star-usb-no-autosuspend.rules` | Disables USB autosuspend for BeagleBone Blue (`1d6b:0104`) and RPLiDAR C1 (`10c4:ea60`). Prevents `/dev/ttyACM0` and `/dev/ttyUSB0` from dropping under idle. |
| `/etc/systemd/system/star-cpu-governor.service` | Pins CPU governor to `schedutil` very early in boot (`DefaultDependencies=no`, runs before `base.target`). Enabled automatically. |
| `/etc/logrotate.d/star-robot` | Rotates `/tmp/star-logs/*.log` daily, 3 rotations, 20 MB cap. Prevents tmpfs OOM on long runs. |
| `/etc/systemd/system/star-robot.service` | Optional boot-time robot launch unit (`Type=oneshot + RemainAfterExit=yes` so `systemctl status` correctly reflects running state). **Installed but NOT enabled.** |

`start.sh` calls `require_host_setup` on launch and prints a single warning block listing any missing pieces.

## Pre-Field-Deployment Checklist

Do NOT do these during active development -- they interfere with dev workflows -- but they MUST be done before any real field deployment:

1. **Disable wifi power-save.** `wlan0 power_save` defaults to `on`; under ROS2 traffic on the AP this causes latency spikes and occasional drops. Drop this file and restart NetworkManager:

   ```ini
   # /etc/NetworkManager/conf.d/10-wifi-powersave.conf
   [connection]
   wifi.powersave = 2     ; 0=default 1=ignore 2=disable 3=enable
   ```

   Then re-enable the power-save check inside `require_host_setup` in `start.sh` (currently a `TODO(pre-field)` comment).

2. **Enable `star-robot.service`** for auto-boot on power-on:

   ```bash
   sudo systemctl enable --now star-robot.service
   ```

   Verify with `journalctl -u star-robot.service -n 50 --no-pager` and `systemctl status star-robot.service`; expect `active (exited)`.

3. **Review AP credentials.** Defaults `STAR_WIFI_SSID=STAR-Robot` / `STAR_WIFI_PASS=STAR2026!` are dev-grade; set a stronger password for field via env vars or by editing `start.sh`.

4. **(Optional) STA-fallback.** Current pure-AP mode is the standard field-robot pattern (Clearpath, Hello Robot Stretch, REV Control Hub all do it). Consider a fallback-STA-then-AP wrapper only if the robot operates in both lab and field.
