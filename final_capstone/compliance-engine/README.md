# STAR ADA Compliance Engine

Python ROS2 package that subscribes to the STAR platform's SLAM,
odometry, and IMU topics and evaluates geometric ADA 2010 Standards
checks on the mapped environment.

## Status

| Check | Status | File |
|---|---|---|
| Ramp slope > 1:12 (ADA 405.2) | IMPLEMENTED | `star_compliance/nodes/ramp_slope_node.py` |
| Trip hazard > 0.25 in (ADA 303) | STRETCH, stub | `star_compliance/nodes/trip_hazard_node.py` |
| Accessible path width < 36 in (ADA 403.5) | STRETCH, stub | `star_compliance/nodes/path_width_node.py` |
| Ramp width < 36 in (ADA 405.5) | ARCHITECTED | `star_compliance/nodes/ramp_width_node.py` |
| Ramp landing < 60 x 60 in (ADA 405.7) | ARCHITECTED | `star_compliance/nodes/ramp_landing_node.py` |
| Door clear width < 32 in (ADA 404.2.3) | ARCHITECTED | `star_compliance/nodes/door_clear_width_node.py` |
| Door threshold > 0.5 in (ADA 404.2.5) | ARCHITECTED | `star_compliance/nodes/door_threshold_node.py` |

## Dependencies

Runs inside the STAR ROS2 workspace. Python deps:

```
rclpy
numpy
open3d
scikit-image
scipy
reportlab
pillow
```

ROS2 topics consumed:

- `/scan` - sensor_msgs/LaserScan from the RPLiDAR C1
- `/imu/data` - sensor_msgs/Imu from the BNO055 (published by
  `star_spi_bridge`)
- `/odom` - nav_msgs/Odometry from robot_localization EKF
- `/map` - nav_msgs/OccupancyGrid from slam_toolbox async

## Ground-truth protocol

1. Select an indoor ramp.
2. Record Wixey WR300 digital-angle-gauge readings (n = 5 minimum) at
   five positions along the ramp surface. Mean = ground truth.
3. Drive STAR onto the ramp using Nav2 goal or teleop.
4. Observe the PDF audit report and compare STAR's reported slope
   against the Wixey mean.
5. Write the session into `../extras/validation_log.csv`.

## Acceptance criteria

- LiDAR plane normal agrees with BNO055 pitch within 0.5 deg on at
  least 80% of sessions.
- Reported slope agrees with Wixey ground truth within 1 deg (stretch:
  0.5 deg).
- Correct flag outcome (violation or compliant) on 100% of sessions
  where the ramp is clearly one side or the other of the 4.76 deg ADA
  threshold.
