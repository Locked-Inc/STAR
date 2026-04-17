# ADA compliance engine

The compliance engine is a Python ROS2 package (`star_compliance`, see
`compliance-engine/`) that runs on the Pi 5 alongside the SLAM and
navigation nodes. It evaluates geometric features of the mapped
environment against the **ADA 2010 Standards for Accessible Design**.

Seven checks are architected; one is implemented end-to-end with live
cross-validated redundancy, and two are in-progress stretch goals for the
seven-day window. The remaining four are fully specified against the
existing sensor stack and designed into the engine's module boundaries.

## Implementation status legend

- **[IMPLEMENTED]** - live code, runs on real sensor data, validated
  against ground truth
- **[STRETCH]** - implementation in progress, targeted for demo day
- **[ARCHITECTED]** - algorithm, sensor assignment, and ADA section are
  specified; implementation held for post-capstone deployment phase

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

## 3. Accessible path width < 36 inches - ADA 403.5 - [STRETCH]

On the slam_toolbox occupancy grid, extract medial-axis centerlines
(skimage `skeletonize` + `distance_transform_edt`) along accessible
segments. Local clearance at each centerline point gives path width.
Report the minimum width along each route; values below 36 inches
violate ADA 403.5.

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

## 6. Door clear width < 32 inches - ADA 404.2.3 - [ARCHITECTED]

Uses the Waveshare IMX219-83 stereo pipeline (OpenCV `StereoSGBM` +
calibration + door-frame edge detection). Vertical stripes of free space
between two parallel planes, measured at door-handle height, give the
clear width. Stereo is load-bearing here because the LiDAR's horizontal
scan plane cannot reliably measure door opening geometry.

## 7. Door threshold height > 0.5 inch - ADA 404.2.5 - [ARCHITECTED]

Combined LiDAR and stereo vertical profile sampled across the door's
bottom edge. The maximum vertical step within the door footprint is the
threshold height.

## Report generation

At scan completion, the engine's PDF generator (reportlab) compiles
each violation into a one-page entry: thumbnail of the offending region
from the colored point cloud, measured value, the ADA section cited, and
a georeferenced pin showing the violation's map-frame coordinates. The
output file is `star_audit_report_{timestamp}.pdf`.

## Module boundaries

```
compliance-engine/
  star_compliance/
    nodes/
      ramp_slope_node.py          [IMPLEMENTED]
      trip_hazard_node.py         [STRETCH, stub present]
      path_width_node.py          [STRETCH, stub present]
      ramp_width_node.py          [ARCHITECTED, spec only]
      ramp_landing_node.py        [ARCHITECTED, spec only]
      door_clear_width_node.py    [ARCHITECTED, spec only]
      door_threshold_node.py      [ARCHITECTED, spec only]
    engines/
      plane_segmentation.py       (Open3D RANSAC helpers, shared)
      imu_cross_validate.py       (BNO055 pitch / accel reader)
    report/
      pdf_generator.py            (reportlab, shared)
    bringup/
      compliance.launch.py
  tests/
```

This layout makes future completion of the architected checks a matter
of filling in node files against a stable shared API, not rearchitecting
the pipeline.
