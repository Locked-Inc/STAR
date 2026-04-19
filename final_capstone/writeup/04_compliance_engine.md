# ADA compliance engine

The compliance engine is a Python ROS2 package (`star_compliance`, see
`compliance-engine/`) that runs on the Pi 5 alongside the SLAM and
navigation nodes. It evaluates geometric features of the mapped
environment against the **ADA 2010 Standards for Accessible Design**.

Seven ADA 2010 geometric checks are architected. As of the post-
door/obstacle work pass, **six of the seven checks are implemented**
end-to-end on the live ROS2 stack with rosbag-testable rclpy nodes,
unit tests, and validation-log CSV output. One remains as a stretch
goal whose primitives are in place; the two remaining are architected
against the same sensor stack for deployment-phase work.

A parallel "bonus perception" layer ships alongside the ADA nodes:
DBSCAN dynamic-obstacle clustering (B1), a Nav2 HC-SR04 costmap
fusion (B2), and a CPU safety-net monitor (B3) that auto-disables the
heaviest compliance check when the Pi 5's load average crosses 80%.

## Implementation status legend

- **[IMPLEMENTED]** - live rclpy node + detector modules + unit tests,
  runs on real sensor data, emits ROS2 messages and validation-log
  CSV rows
- **[STRETCH]** - detectors and engines in place; node glue pending
- **[ARCHITECTED]** - algorithm, sensor assignment, and ADA section
  specified; implementation held for deployment-phase work

See `extras/traceability.md` for the full check-to-file matrix and
`compliance-engine/` for the ROS2 package source.

## 1. Ramp slope > 1:12 (4.76 degrees) - ADA 405.2 - [IMPLEMENTED]

The headline check. STAR drives onto the ramp, collects LiDAR returns
within a 2 m window ahead of the robot, and runs RANSAC plane
segmentation (Open3D) on the accumulated points transformed into
`base_link`. The plane normal's angle with gravity gives the slope.

The same slope is independently estimated from the BNO055 pitch trace
recorded during traversal. If the two estimates agree within 0.5 degrees,
the reading is accepted and, if it exceeds 4.76 degrees, written to the
violation CSV with the `/odom`-reported pose as the georeferenced
location.

If the two estimates disagree beyond 0.5 degrees, the engine flags the
frame for a re-scan rather than silently trusting one modality. This
design decision directly addresses the failure mode where LiDAR sees a
reflective or transparent surface and computes a phantom plane.

### Validation protocol

Ground truth via **Wixey WR300** digital angle gauge on the physical
ramp. n in the range 5 to 30 depending on available bench time. See
`extras/validation_log.csv` for the live-capture template and
`compliance-engine/ramp_slope/README.md` for the full measurement
procedure.

## 2. Trip hazards > 0.25 inch - ADA 303 - [STRETCH]

Surface discontinuities detected by computing vertical delta between
adjacent LiDAR scan points on a floor plane segmentation, cross-validated
against BNO055 vertical-acceleration spikes (the "jolt" signature)
during traversal. Deltas greater than 0.25 inch violate ADA 303.

## 3. Accessible path width < 36 inches - ADA 403.5 - [IMPLEMENTED]

`path_blockage_node` subscribes to `/map`, `/scan`, and `/odom`. On
each map update it runs `skimage.morphology.medial_axis` + the
distance transform on the free-space region to compute a baseline
free width along every corridor segment. On each /scan it computes
an instantaneous clear width via `scan_min_width_along_line` and
flags a sustained (3-frame) narrowing to less than 36 inches as an
ADA 403.5 violation. Emits `PathBlockage` and writes
`extras/blockage_log.csv`.

## 4. Ramp width < 36 inches - ADA 405.5 - [ARCHITECTED]

With the ramp plane identified by the slope algorithm, its bounding
polygon is extracted and the minimum width perpendicular to the slope
direction is measured. Implementation plan: Open3D plane-to-polygon + a
bounding-box principal-axis projection.

## 5. Ramp landing area < 60 x 60 inches - ADA 405.7 - [ARCHITECTED]

Flat regions whose surface normal is within 1 degree of vertical are
extracted via the same plane-segmentation pass used for ramp detection.
The largest contiguous flat area at each ramp end is bounded; if the
inscribed rectangle is smaller than 60 x 60 inches, the engine flags an
inadequate landing.

## 6. Door clear width < 32 inches - ADA 404.2.3 - [IMPLEMENTED]

`door_clear_width_node` implements the research-backed LiDAR-first +
stereo-at-handle-height + classifier pipeline:

1. `doorway_lidar_detector` runs a rolling wall-line RANSAC on /scan,
   returns a `DoorwayCandidate` when corridor width drops below 1.1 m.
2. `jamb_plane_fitter` RANSAC-fits two parallel vertical planes on the
   stereo /stereo/points2 cloud within the 85-100 cm handle-height
   band (Open3D).
3. `door_state_classifier` runs YOLOv8n via OpenCV DNN on the
   rectified left image (DoorDet fine-tune, pending training - falls
   back to a geometric point-cloud heuristic when weights are absent
   so the pipeline runs end-to-end in development).
4. `door_offset_calibration` applies a calibrated 2.5 in
   door+stop+hinge offset (disclosed on every PDF report and every
   ROS2 message); `ada_clear_width_m = frame_width_m - total_offset`.
5. LiDAR-derived and stereo-derived frame widths are cross-checked;
   agreement within 0.03 m stamps `confidence = HIGH`, disagreement
   drops it.

Emits `DoorwayMeasurement` and writes `extras/validation_log.csv`.
**Honest disclosure:** STAR does not physically open doors and cannot
directly sense the 3/8 to 1/2 inch door stop; reported values are
screening estimates and CASp-follow-up language is printed on every
audit PDF.

## 7. Door threshold > 0.5 inch - ADA 404.2.5 - [IMPLEMENTED - presence only]

`door_threshold_node` arms a 2-second "threshold watch" window on
every `DoorwayMeasurement` from node #6. During the watch,
`imu_jolt_detector` processes /imu/data at 200 Hz: quaternion-
rotated gravity subtracted from the z-accel, rolling 500 ms baseline
maintained. A sustained (25 ms, 5+ samples) deviation greater than
2.0 m/s^2 fires a `ThresholdMeasurement` with detected=True.

**Honest disclosure:** STAR reports threshold PRESENCE only; precise
height is not measured. The published message carries the mandatory
disclosure string and the PDF generator echoes it. A stereo-based
threshold measurement is future work.

## 8. (bonus) ADA 307 Protruding Objects - [IMPLEMENTED]

Not part of the original 7-check list; added during the door/obstacle
work. `protruding_objects_node` consumes /cloud_map (or
/stereo/points2 for faster rate), uses `cane_zone_filter` to restrict
to the ADA 27-80 inch band, `wall_plane_fitter` to iteratively RANSAC
vertical walls, and `is_protrusion` to flag clusters more than 4
inches from the nearest wall plane. Runtime-disableable via
`use_ada_307:=false` launch arg and the `/star_protruding_objects_node
/enabled` parameter; the `compliance_monitor_node` auto-disables when
Pi 5 1-minute load exceeds 80%.

## Report generation

At scan completion, the engine's PDF generator (reportlab) compiles
each violation into a one-page entry: thumbnail of the offending region
from the colored point cloud, measured value, the ADA section cited, and
a georeferenced pin showing the violation's map-frame coordinates. The
output file is `star_audit_report_{timestamp}.pdf`.

## Module boundaries

```
compliance-engine/
  package.xml                     (ament_python)
  setup.py                        (colcon entry_points per node)
  star_compliance/
    nodes/
      ramp_slope_node.py          [IMPLEMENTED] ADA 405.2
      door_clear_width_node.py    [IMPLEMENTED] ADA 404.2.3
      door_threshold_node.py      [IMPLEMENTED - presence] ADA 404.2.5
      path_blockage_node.py       [IMPLEMENTED] ADA 403.5
      protruding_objects_node.py  [IMPLEMENTED] ADA 307
      dynamic_obstacle_node.py    [IMPLEMENTED] bonus - DBSCAN
      compliance_monitor_node.py  [IMPLEMENTED] bonus - CPU safety net
      trip_hazard_node.py         [STRETCH]     ADA 303
      ramp_width_node.py          [ARCHITECTED] ADA 405.5
      ramp_landing_node.py        [ARCHITECTED] ADA 405.7
    detectors/
      doorway_lidar_detector.py   (rolling wall-line RANSAC on /scan)
      jamb_plane_fitter.py        (Open3D vertical-plane RANSAC at handle height)
      door_state_classifier.py    (YOLOv8n ONNX + heuristic fallback)
      imu_jolt_detector.py        (BNO055 gravity-compensated z-accel burst)
      cane_zone_filter.py         (27-80 inch band, ADA 307 limits)
      wall_plane_fitter.py        (iterative vertical-plane RANSAC)
      corridor_medial_axis.py     (skimage skeletonize + distance transform)
      obstacle_clusterer.py       (sklearn DBSCAN + map subtraction)
    engines/
      plane_segmentation.py       (Open3D RANSAC helpers, shared)
      imu_cross_validate.py       (BNO055 pitch reader)
      floor_frame.py              (floor-plane from BNO055 or LiDAR)
      door_offset_calibration.py  (2.5-in offset + per-style JSON)
    models/
      door_state_yolov8n.onnx     (PENDING: DoorDet fine-tune, see models/README.md)
    report/
      pdf_generator.py            (reportlab, shared)
    bringup/
      compliance.launch.py        (all nodes with use_* gates)
  tests/
```

This layout makes future completion of the architected checks a matter
of filling in node files against a stable shared API, not rearchitecting
the pipeline.
