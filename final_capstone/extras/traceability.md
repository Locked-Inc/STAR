# ADA Check Traceability Matrix

Each ADA check that STAR addresses, traced from the ADA 2010 Standards
for Accessible Design through sensor to algorithm to code (or design
document) and implementation status.

Judges can use this one table to see exactly what is built vs.
architected.

---

| # | Check | ADA 2010 section | Primary sensor(s) | Secondary / cross-validation | Algorithm | Status | File / spec |
|---|---|---|---|---|---|---|---|
| 1 | Ramp slope > 1:12 (4.76 deg) | **405.2** | RPLiDAR C1 | BNO055 pitch (+/- 0.5 deg agreement gate) | Open3D RANSAC plane fit + surface-normal angle vs. gravity; IMU pitch mean over 500 ms window | **[IMPLEMENTED]** | `compliance-engine/star_compliance/nodes/ramp_slope_node.py` |
| 2 | Trip hazard > 0.25 inch | **303** | RPLiDAR C1 | BNO055 vertical-acceleration spike correlation | Vertical delta between adjacent floor-plane LiDAR returns; cross-validated against IMU jolt during traversal | [STRETCH] | `compliance-engine/star_compliance/nodes/trip_hazard_node.py` (stub + spec in docstring) |
| 3 | Accessible path width < 36 inches | **403.5** | slam_toolbox occupancy grid | HC-SR04 short-range validation in tight passes | Medial-axis transform (skimage `skeletonize` + `distance_transform_edt`) on the `/map` occupancy grid; local clearance = 2x distance-transform value | [STRETCH] | `compliance-engine/star_compliance/nodes/path_width_node.py` (stub + spec in docstring) |
| 4 | Ramp width < 36 inches | **405.5** | RPLiDAR C1 | - | Plane-polygon bounding, principal-axis projection of the ramp plane from check #1 | [ARCHITECTED] | `writeup/04_compliance_engine.md` section 4 |
| 5 | Ramp landing area < 60 x 60 inches | **405.7** | RPLiDAR C1 | - | Inscribed-rectangle test on flat regions (surface normal within 1 deg of vertical) adjacent to ramp plane | [ARCHITECTED] | `writeup/04_compliance_engine.md` section 5 |
| 6 | Door clear width < 32 inches | **404.2.3** | IMX219-83 stereo (OpenCV SGBM) | ICM20948 (visual-inertial) | Disparity -> depth -> door-frame edge detection at handle height; minimum gap = clear width | [ARCHITECTED] | `writeup/04_compliance_engine.md` section 6 |
| 7 | Door threshold > 0.5 inch | **404.2.5** | RPLiDAR C1 + IMX219-83 stereo | BNO055 | Combined LiDAR horizontal + stereo vertical profile across door bottom; max vertical step = threshold height | [ARCHITECTED] | `writeup/04_compliance_engine.md` section 7 |

---

## Legend

- **[IMPLEMENTED]** - live code, runs on real sensor data, validated
  against ground truth on the on-campus ramp
- **[STRETCH]** - stub present in `compliance-engine/`, spec in
  docstring, planned for implementation during the 7-day capstone window
- **[ARCHITECTED]** - algorithm, sensor assignment, ADA citation
  specified in the writeup; code not yet present

## Validation data pointer

Every number claimed for the IMPLEMENTED check comes from
`extras/validation_log.csv` (raw rows) and
`extras/STAR_ValidationLog.xlsx` (formatted, with per-check MAE and
agreement-rate summaries).

## Why we are not claiming more

Seven checks in seven days, on top of the platform work, would have
forced fabricated numbers. The team chose to implement one check
honestly, instrument two more as stretch, and architect the remaining
four against the same sensors so a deployment-phase continuation has a
clear map.
