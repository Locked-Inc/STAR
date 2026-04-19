# Waveshare IMX219-83 Stereo Camera on Raspberry Pi 5

This document covers the full setup, driver stack, and ROS2 integration for the
Waveshare IMX219-83 stereo camera on the STAR platform (Raspberry Pi 5, Ubuntu 24.04).

## Hardware

| Item | Detail |
|------|--------|
| Module | Waveshare IMX219-83 stereo camera |
| Sensors | 2x Sony IMX219, 8 MP each |
| Baseline | 83 mm |
| Interface | 2x 15-pin MIPI CSI-2 flex cable |
| Pi connector | CAM0 (left) and CAM1 (right) |
| Power | Supplied by Pi via flex cable (3.3 V, regulated on-board) |

The module has on-board voltage regulators and XCLR drive; no external power or
reset wiring is required.

## Wiring

Connect the two 15-pin FFC cables from the Waveshare board to the Pi 5:

- **Left sensor** -> CAM0 (the connector closer to the HDMI ports)
- **Right sensor** -> CAM1 (the connector closer to the USB ports)

The blue side of each cable faces **up** on the Pi 5 connectors and **toward the
lens** on the Waveshare board.

## Boot Configuration

Edit `/boot/firmware/config.txt` (requires `sudo`):

```ini
# Disable auto-detect -- it does not work on Ubuntu 24.04
camera_auto_detect=0

# Waveshare IMX219-83 stereo camera
dtoverlay=imx219,cam0
dtoverlay=imx219,cam1
```

Reboot after editing.

## Verification

After reboot, confirm both sensors are enumerated:

```bash
v4l2-ctl --list-devices
```

Expected output (partial):

```
rp1-cfe (platform:1f00110000.csi):
    /dev/video0
    ...

rp1-cfe (platform:1f00128000.csi):
    /dev/video8
    ...
```

## Required Packages

### System packages

```bash
# Raspberry Pi apt repository (add once)
curl -fsSL https://archive.raspberrypi.com/debian/raspberrypi.gpg.key \
  | sudo gpg --dearmor -o /usr/share/keyrings/raspberrypi-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/raspberrypi-archive-keyring.gpg] \
  https://archive.raspberrypi.com/debian/ bookworm main" \
  | sudo tee /etc/apt/sources.list.d/raspberrypi.list
sudo apt-get update

# Pi-specific libcamera with IPA modules
sudo apt-get install -y libpisp1 libcamera0.5 libcamera-ipa

# GStreamer plugins
sudo apt-get install -y \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-libcamera

# V4L2 utilities (optional, for diagnostics)
sudo apt-get install -y v4l-utils i2c-tools
```

### User group

Add your user to the `video` group (log out and back in after):

```bash
sudo usermod -aG video $USER
```

Or apply immediately in the current shell:

```bash
newgrp video
```

### ROS2 package

```bash
sudo apt-get install -y ros-jazzy-gscam
```

## Known Issue: libpisp ABI Conflict

`ros-jazzy-libcamera` ships libpisp **1.3.0** in `/opt/ros/jazzy/lib/`.
The Pi IPA module (`ipa_rpi_pisp.so`) from `libcamera-ipa` 0.5.2 was built
against libpisp **1.2.1** (system, in `/usr/lib/aarch64-linux-gnu/`).

When the ROS workspace is sourced, `/opt/ros/jazzy/lib` precedes the system
path in `LD_LIBRARY_PATH`, causing an `undefined symbol` error in the IPA.

**Fix:** Prepend the system library path before starting any camera process:

```bash
export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
```

The `stereo_camera.launch.py` launch file applies this fix automatically.

## Raw GStreamer Test (no ROS)

Verify the cameras work at the GStreamer level before launching ROS2:

```bash
# Requires: video group membership (newgrp video) or sudo -g video

# Left sensor (CAM0) -- streams for 5 seconds then exits
gst-launch-1.0 \
  libcamerasrc camera-name="/base/axi/pcie@120000/rp1/i2c@88000/imx219@10" \
  ! "video/x-bayer,format=bggr16le,width=1640,height=1232,framerate=15/1" \
  ! bayer2rgb ! videoconvert ! video/x-raw,format=RGB \
  ! filesink location=/tmp/cam0.raw &
sleep 5 && kill %1

# Check data was captured (should be tens of MB)
ls -lh /tmp/cam0.raw
rm /tmp/cam0.raw

# Right sensor (CAM1)
gst-launch-1.0 \
  libcamerasrc camera-name="/base/axi/pcie@120000/rp1/i2c@80000/imx219@10" \
  ! "video/x-bayer,format=bggr16le,width=1640,height=1232,framerate=15/1" \
  ! bayer2rgb ! videoconvert ! video/x-raw,format=RGB \
  ! filesink location=/tmp/cam1.raw &
sleep 5 && kill %1
ls -lh /tmp/cam1.raw
rm /tmp/cam1.raw
```

## ROS2 Launch

```bash
source /opt/ros/jazzy/setup.bash
source /workspaces/STAR/star-ros2/install/local_setup.bash

ros2 launch star_bringup stereo_camera.launch.py
```

Optional arguments:

```bash
# Lower resolution for faster processing
ros2 launch star_bringup stereo_camera.launch.py width:=640 height:=480 fps:=30
```

## ROS2 Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/cam0/image_raw` | `sensor_msgs/Image` | Left camera, RGB8 |
| `/cam0/camera_info` | `sensor_msgs/CameraInfo` | Left camera intrinsics |
| `/cam1/image_raw` | `sensor_msgs/Image` | Right camera, RGB8 |
| `/cam1/camera_info` | `sensor_msgs/CameraInfo` | Right camera intrinsics |

## TF Frames

```
base_link
+-- cam0_link          (left sensor, at robot origin)
|   +-- cam0_optical_frame   (Z forward, X right, Y down)
+-- cam1_link          (right sensor, +83 mm in Y from base_link)
    +-- cam1_optical_frame   (Z forward, X right, Y down)
```

## Camera Intrinsics and Calibration

The `camera_info` topics will contain empty intrinsics until calibration is run.
Calibration is required for stereo depth computation.

Calibrate using the ROS2 camera calibration tool with a checkerboard:

```bash
sudo apt-get install -y ros-jazzy-camera-calibration

# Run stereo calibration (both cameras must be streaming)
ros2 run camera_calibration cameracalibrator \
  --size 8x6 --square 0.025 \
  --ros-args \
  -r left:=/cam0/image_raw \
  -r right:=/cam1/image_raw \
  -r left_camera:=/cam0 \
  -r right_camera:=/cam1
```

Save the calibration. Results go to:
- `~/.ros/camera_info/cam0.yaml`
- `~/.ros/camera_info/cam1.yaml`

## IMX219 Native Capture Modes

| Width | Height | FPS | Notes |
|-------|--------|-----|-------|
| 3280 | 2464 | 15 | Full resolution, high CPU load |
| 1640 | 1232 | 30 | 2x2 binned, recommended for stereo |
| 640 | 480 | 30 | 4x4 binned, lowest CPU, wide FOV aliasing |

## Viewing with Foxglove or RViz2

**Foxglove Studio** (connect to `ws://pi5-ip:8765`):
- Add a Raw Image panel, subscribe to `/cam0/image_raw`
- Add a second panel for `/cam1/image_raw`

**RViz2:**
- Add > Image display, set topic to `/cam0/image_raw`

## Kernel Driver Notes

The Pi 5 camera stack on Ubuntu 24.04 uses:

| Component | Description |
|-----------|-------------|
| `imx219` kernel module | Sensor subdevice driver |
| `rp1-cfe` kernel module | RP1 Camera Front End (CSI-2 receiver) |
| `pisp_be` kernel module | PiSP Back End (ISP) |
| `libcamera0.5` (Pi fork) | Camera framework with Pi IPA |
| `ipa_rpi_pisp.so` | Pi ISP image processing algorithms |
| `gstreamer1.0-libcamera` | GStreamer source element |
| `gscam` (ROS2) | ROS2 wrapper around GStreamer pipelines |

The modules `imx219` and `rp1-cfe` are loaded automatically on boot via the
`dtoverlay=imx219,cam0/cam1` entries in `/boot/firmware/config.txt`.
