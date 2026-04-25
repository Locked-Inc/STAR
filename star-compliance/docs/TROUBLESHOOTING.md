# Troubleshooting Guide

Symptom -> likely cause -> diagnostic -> fix. Ordered from most-common
to least-common in each section.

---

## Boot-time failures

### `./start.sh` exits with "star_compliance package not found"

- **Cause:** `bootstrap_pi.sh` hasn't been run on this Pi, or
  `colcon build` failed silently.
- **Diagnose:** `ros2 pkg list | grep star_compliance`
- **Fix:**

  ```bash
  ./bootstrap_pi.sh
  source star-ros2/install/setup.bash
  ./start.sh
  ```

### `compliance engine may have exited -- check /tmp/star-logs/compliance.log`

- **Cause:** a launch-level failure in `compliance.launch.py`.
- **Diagnose:** `tail -100 /tmp/star-logs/compliance.log`
- **Common fixes:**
  - `ModuleNotFoundError: open3d` -> `pip install --user open3d`
    or launch with `use_ada_307:=false`
  - `No executable found: ramp_slope_node` -> `colcon build` the
    compliance package again
  - `Parameter 'enabled' already declared` -> restart the whole
    stack (`./stop.sh && ./start.sh`)

### `ros2 interface show star_compliance_msgs/msg/DoorwayMeasurement` fails

- **Cause:** `star_compliance_msgs` wasn't built or the workspace
  isn't sourced.
- **Diagnose:** `ls star-ros2/install/star_compliance_msgs/` - should
  contain `share/` and `lib/`.
- **Fix:**

  ```bash
  cd star-ros2
  colcon build --packages-select star_compliance_msgs
  source install/setup.bash
  ```

---

## Runtime failures

### No messages on `/compliance/door_clear_width`

- **Cause (likely):** LiDAR corridor-width minima aren't firing. Robot
  hasn't encountered a doorway narrow enough.
- **Diagnose:**

  ```bash
  ros2 topic echo /scan --qos-reliability best_effort --once
  ros2 node info /star_door_clear_width_node
  ros2 param get /star_door_clear_width_node enabled
  ```

- **Fix:** drive the robot into a hallway narrower than 1.1 m, or
  lower the `DOORWAY_CANDIDATE_MAX_M` constant in
  `detectors/doorway_lidar_detector.py`.

- **Cause (secondary):** `enabled` parameter was set to false by a
  prior `ros2 param set` call.
- **Fix:** `ros2 param set /star_door_clear_width_node enabled true`

### Stereo jamb fitter always returns None

- **Cause:** Open3D isn't installed on the Pi.
- **Diagnose:** `python3 -c "import open3d; print(open3d.__version__)"`
- **Fix:** `pip install --user open3d` (5-15 min on arm64)

- **Cause:** the /stereo/points2 topic is empty.
- **Diagnose:** `ros2 topic hz /stereo/points2`
- **Fix:** check `/tmp/star-logs/slam.log` for gscam / stereo_image_proc
  errors. Common: libpisp ABI conflict - re-source with the
  LD_LIBRARY_PATH fix from `stereo_camera.launch.py`.

### Threshold node never fires

- **Cause:** the `DoorwayMeasurement` event isn't coming through, so
  no watch window opens.
- **Diagnose:** `ros2 topic echo /compliance/door_clear_width` during
  a doorway traversal. If empty, fix door_clear_width first.
- **Fix:** see "No messages on /compliance/door_clear_width".

- **Cause:** jolt threshold too high for the actual event magnitude.
- **Diagnose:**

  ```bash
  ros2 topic echo /imu/data | grep linear_acceleration -A 3
  ```

  Watch the `z` value while crossing a threshold. It should briefly
  exceed `threshold_ms2 + 9.8`.
- **Fix:** lower `threshold_ms2` incrementally from 2.0 -> 1.5 -> 1.0,
  retesting after each step. Below 1.0 you'll get false positives on
  floor transitions.

### Protruding-objects node reports zero candidates

- **Cause:** the input cloud topic is empty or the BNO055 hasn't
  published yet.
- **Diagnose:**

  ```bash
  ros2 topic hz /cloud_map
  ros2 topic hz /imu/data
  ```

- **Fix:** switch input topic to `/stereo/points2` if `/cloud_map`
  is stale:

  ```bash
  ros2 param set /star_protruding_objects_node input_cloud_topic /stereo/points2
  ```

  Note: 3-4x CPU cost. Monitor via `/compliance/monitor/status`.

- **Cause:** CPU safety monitor has auto-disabled the node.
- **Diagnose:** `ros2 topic echo /compliance/monitor/status`
  Look for "throttled ada_307".
- **Fix:** wait for the 1-min load to drop below 50%, or permanently
  free up headroom by:
  - Disabling RTAB-Map (`use_rtabmap:=false` on the stereo launch)
  - Disabling Foxglove bridge (`use_foxglove:=false`)
  - Launching with `use_dynamic_obstacles:=false`

### Path-blockage node fires on every scan

- **Cause:** SLAM map baseline was captured with the blockage already
  in place, so "live" looks the same as "baseline".
- **Fix:** run `./stop.sh`, clear the saved map (delete
  `/var/lib/star/map.yaml` or restart SLAM with a fresh map), then
  run `./start.sh` and rebuild the map empty before bringing in the
  blockage.

- **Cause:** LiDAR drift within a corridor is producing false
  narrowings.
- **Diagnose:** `ros2 topic echo /compliance/path_blockage` and check
  the `blockage_delta_m` values. If they're < 0.2 m, it's noise.
- **Fix:** raise the `BLOCKAGE_DELTA_M` constant in
  `nodes/path_blockage_node.py` from 0.15 to 0.20.

### CSV rows are written with timestamps in 1970

- **Cause:** no internet / NTP sync, `time.time()` returns 0-ish.
- **Fix:** `sudo systemctl restart systemd-timesyncd`
  Also verify the Pi's RTC battery. If stale, replace.

### Door offset is consistently wrong (clear width reads +5 cm too wide on every door)

- **Cause:** the default 2.5-in offset under-estimates for commercial
  doors with thicker stops.
- **Diagnose:** follow the "How to tune door_offset_m" recipe in
  TUNING.md.
- **Fix:**

  ```bash
  ros2 param set /star_door_clear_width_node door_offset_m 0.075
  ```

  or update the default in `compliance.launch.py` and rebuild.

---

## Visualization failures

### Foxglove shows the robot but no /compliance/* topics

- **Cause:** Foxglove subscribes via the bridge only to topics
  present at connect time.
- **Fix:** refresh the Foxglove browser tab. The compliance topics
  should appear in the left sidebar.

- **Cause:** the buffer limit in `slam.launch.py` / Foxglove bridge
  is too small for point-cloud messages.
- **Fix:** already set to 50 MB per recent commit. Verify with
  `grep send_buffer_limit star-ros2/src/star_bringup/launch/slam.launch.py`.

### RViz segfaults on startup

- **Cause:** corrupted cache.
- **Fix:** `rm -rf ~/.cache/rviz2 ~/.rviz2 && ros2 run rviz2 rviz2`

---

## Developer-side failures

### Unit tests fail with `ImportError: cannot import name 'medial_axis'`

- **Cause:** scikit-image isn't installed in the test virtualenv.
- **Fix:** `pip install scikit-image` (not required for the detector
  unit tests but is for the corridor medial-axis test).

### Node smoke tests fail with `ImportError: No module named 'star_compliance_msgs'`

- **Cause:** the ros_mocks.py harness fell out of date.
- **Fix:** make sure `tests/ros_mocks.py:install_ros_mocks()` is
  called BEFORE the first `from star_compliance...` import in the
  test file. Check the order in `tests/test_nodes_smoke.py`.

### Launch tests hang indefinitely

- **Cause:** the node under test never publishes on the expected
  topic.
- **Fix:** raise the `WAIT_SECONDS` constant and re-run; then
  inspect what the node is actually doing via `ros2 node list` and
  `ros2 topic list` inside the test timeout.

---

## Filing a new issue

If you can't resolve a symptom via this guide:

1. Capture the logs: `tar czf star-logs.tgz /tmp/star-logs/`
2. Run `ros2 doctor --report > ros2_report.txt`
3. Open an issue at <https://github.com/Locked-Inc/STAR> with:
   - Pi 5 model and RAM size
   - ROS 2 distro + date of `bootstrap_pi.sh` run
   - Which compliance node is misbehaving
   - First 100 lines of the node's log
   - Attach `star-logs.tgz` and `ros2_report.txt`

---

## See also

- [DEPLOYMENT.md](DEPLOYMENT.md) - first-time setup
- [TUNING.md](TUNING.md) - parameter adjustments
- [VALIDATION.md](VALIDATION.md) - ground-truth protocols
