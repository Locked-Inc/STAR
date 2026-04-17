# STAR Compliance Engine Architecture

System-level design of the ROS 2 compliance nodes that consume the
STAR platform's fused sensor stack and emit ADA 2010 Standards
compliance records.

---

## Big picture

```
                          STAR platform (pre-existing)
                          ----------------------------
  RPLiDAR C1 --USB-------> /scan  (LaserScan, 10 Hz)
  IMX219-83  --CSI x2----> /cam0/image_raw, /cam1/image_raw
                         + /stereo/disparity
                         + /stereo/points2  (PointCloud2, 5-10 Hz)
                         + /cloud_map       (PointCloud2, 1 Hz, RTAB-Map)
  BNO055     --I2C ---\
  HC-SR04 x4 --GPIO---/--> RX72N --SPI(HARQ/FEC/nanopb)-->
                                                /imu/data (Imu, 200 Hz)
                                                /star/obstacle/{fl,fr,bl,br}
                                                           (Range, ~10 Hz each)
  slam_toolbox async                 ->  /map         (OccupancyGrid, 0.5 Hz)
  robot_localization EKF            ->  /odom        (Odometry, 100 Hz)


                              Compliance engine (this package)
                              --------------------------------
                                 |
  /scan, /imu/data, /odom -------+--> ramp_slope_node
                                 |        `--> /compliance/ramp_slope
                                 |        `--> extras/validation_log.csv
                                 |
  /scan, /imu, /odom,            +--> door_clear_width_node
     /cam0/image_rect_color,     |        `--> /compliance/door_clear_width
     /stereo/points2             |        `--> extras/validation_log.csv
                                 |
  /imu, /odom, <doorway>---------+--> door_threshold_node
                                 |        `--> /compliance/door_threshold
                                 |        `--> extras/threshold_log.csv
                                 |
  /cloud_map (or /stereo/points2)
     + /imu --------------------+--> protruding_objects_node
                                 |        `--> /compliance/protruding_objects
                                 |        `--> extras/protrusion_log.csv
                                 |
  /map, /scan, /odom ------------+--> path_blockage_node
                                 |        `--> /compliance/path_blockage
                                 |        `--> extras/blockage_log.csv
                                 |
  /scan, /map, /odom ------------+--> dynamic_obstacle_node
                                 |        `--> /perception/dynamic_obstacles
                                 |
  /diagnostics ------------------+--> compliance_monitor_node
                                          `--> toggles protruding_objects on CPU > 80%
                                          `--> /compliance/monitor/status
```

Also: the Nav2 local costmap consumes the 4 HC-SR04 topics via
`nav2_costmap_2d::RangeSensorLayer` (configured in
`star-ros2/src/star_bringup/config/nav2_params.yaml`). This is
independent of the compliance engine and runs inside the Nav2 node
graph.

---

## Topic / message table

| Topic | Type | Direction | Rate | Consumer |
|---|---|---|---|---|
| `/scan` | `sensor_msgs/LaserScan` | in | 10 Hz | ramp slope, door clear width, path blockage, dynamic obstacle |
| `/imu/data` | `sensor_msgs/Imu` | in | 200 Hz | ramp slope, door clear width, door threshold, protruding objects |
| `/odom` | `nav_msgs/Odometry` | in | 100 Hz | every compliance node |
| `/cam0/camera/image_rect_color` | `sensor_msgs/Image` | in | 15 Hz | door clear width |
| `/stereo/points2` | `sensor_msgs/PointCloud2` | in | 5-10 Hz | door clear width, protruding objects |
| `/cloud_map` | `sensor_msgs/PointCloud2` | in | 1 Hz | protruding objects (default) |
| `/map` | `nav_msgs/OccupancyGrid` | in | 0.5 Hz | path blockage, dynamic obstacle |
| `/star/obstacle/front_left` | `sensor_msgs/Range` | in | ~10 Hz | safety_monitor, Nav2 RangeSensorLayer |
| `/star/obstacle/front_right` | `sensor_msgs/Range` | in | ~10 Hz | same |
| `/star/obstacle/back_left` | `sensor_msgs/Range` | in | ~10 Hz | same |
| `/star/obstacle/back_right` | `sensor_msgs/Range` | in | ~10 Hz | same |
| `/compliance/door_clear_width` | `star_compliance_msgs/DoorwayMeasurement` | out | on encounter | audit PDF, UI |
| `/compliance/door_threshold` | `star_compliance_msgs/ThresholdMeasurement` | out | on jolt | audit PDF, UI |
| `/compliance/protruding_objects` | `star_compliance_msgs/ProtrudingObjectArray` | out | 1 Hz | audit PDF, UI |
| `/compliance/path_blockage` | `star_compliance_msgs/PathBlockage` | out | on sustained blockage | audit PDF, UI |
| `/perception/dynamic_obstacles` | `star_compliance_msgs/DynamicObstacleArray` | out | per /scan | Nav2 behavior tree (future) |
| `/compliance/monitor/status` | `std_msgs/String` | out | 0.2 Hz | UI |

---

## TF frame requirements

| Frame | Parent | Source |
|---|---|---|
| `map` | - | slam_toolbox async |
| `odom` | `map` | slam_toolbox |
| `base_link` | `odom` | robot_localization EKF |
| `laser_frame` | `base_link` | static_transforms |
| `cam0_link` | `base_link` | static_transforms |
| `cam0_optical_frame` | `cam0_link` | static_transforms |
| `cam1_link` | `base_link` | static_transforms (+0.061 m in Y for baseline) |
| `cam1_optical_frame` | `cam1_link` | static_transforms |

The compliance nodes read poses from `/odom` directly and never use
`tf` directly today. This avoids the common "missing TF tree" failure
mode but means all compliance outputs are map-frame only - not
transformed into `base_link` or any other frame.

---

## Thread and rate model

- Every compliance node is a single-threaded `rclpy.Node` spinning via
  `rclpy.spin()`. No multi-threaded callbacks today. Pi 5 CPU budget
  is the rate limiter.

- Protruding-objects is rate-limited to **1 Hz** via a ROS 2 timer
  because the 3D RANSAC over `/cloud_map` is the heaviest workload
  and the data source is already 1 Hz.

- Door clear-width runs per `/scan` frame (10 Hz) for the LiDAR
  detector but only invokes the stereo RANSAC + YOLO classifier when
  a doorway candidate fires, gated by a 0.5 m min-separation policy
  -> effective stereo rate ~0.3 Hz in a typical hallway run.

- Path blockage runs per `/scan` frame; medial-axis recomputes per
  `/map` update (0.5 Hz).

---

## CPU budget on Pi 5 (measured targets)

| Node | Expected load | Disable flag |
|---|---|---|
| ramp_slope_node | 5-8% | - |
| door_clear_width_node (idle) | 3% | `use_stereo:=false` |
| door_clear_width_node (burst, doorway measurement) | 35% for 200 ms | |
| door_threshold_node | 2-5% | - |
| protruding_objects_node | 15-30% | `use_ada_307:=false` |
| path_blockage_node | 8-12% | `use_path_blockage:=false` |
| dynamic_obstacle_node | 4-7% | `use_dynamic_obstacles:=false` |
| compliance_monitor_node | <1% | `use_compliance_monitor:=false` |

Total compliance load: roughly 35-70% of one core during active audit
runs. The safety-net monitor disables the heaviest node (ADA 307) if
1-minute load exceeds 80%. See [TUNING.md](TUNING.md) for throttling
knobs.

---

## Why Python (and where not)

Compliance-engine code is **Python**. The rationale:

- Open3D RANSAC, scikit-image medial-axis, scikit-learn DBSCAN,
  OpenCV DNN ONNX loader, and reportlab PDF all have first-class
  Python APIs. Rewriting in C++ would add weeks of boilerplate with
  no runtime benefit at our rates.
- Compliance checks run at **measurement rate** (1-10 Hz), not control
  rate. The GIL + numpy overhead is well below our latency budget.
- Safety-critical code stays in C++: `star_safety_monitor` (e-stop,
  heartbeat, HC-SR04 debounce), `star_spi_bridge` (250 Hz PID command
  path), and RX72N firmware (250 Hz motor loop). Those are lifecycle-
  managed, tight-latency, and explicitly scoped to the ROS 2 C++
  style guide in `CLAUDE.md`.

The compliance engine is offline-friendly measurement. The safety-
monitor is real-time watchdog. Both live on the Pi 5 but in different
language tiers.

---

## Error model

Every compliance node follows the same failure pattern:

1. **Import-time failures** (e.g., `star_compliance_msgs` not built):
   the node logs a warning, continues without a publisher, still
   processes callbacks internally (useful for dev-mode CSV logging).

2. **Runtime sensor-gap failures** (e.g., /imu/data not publishing):
   the node's callbacks short-circuit; no CSV row is written. No
   downstream compliance is fabricated.

3. **RANSAC / detector failures**: logged at WARN level; the affected
   candidate is dropped, not promoted to a violation flag.

4. **Cross-validation disagreement** (e.g., LiDAR vs stereo door
   width differ by > 3 cm): the measurement is still published but
   with `confidence=LOW`.

No node ever fabricates a measurement or writes a "default" violation
flag when the sensor data is missing.
