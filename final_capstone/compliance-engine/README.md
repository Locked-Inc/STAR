# STAR ADA Compliance Engine

ROS 2 Python package that evaluates indoor environments against the
**ADA 2010 Standards for Accessible Design** using the STAR robot's
fused sensor stack.

Part of the Locked Inc. senior capstone (Texas A&M ESET, Spring 2026).

---

## At a glance

| Check | ADA section | Status | Node |
|---|---|---|---|
| Ramp slope | 405.2 | IMPLEMENTED | `ramp_slope_node` |
| Door clear width | 404.2.3 | IMPLEMENTED | `door_clear_width_node` |
| Door threshold presence | 404.2.5 | IMPLEMENTED (presence-only) | `door_threshold_node` |
| Path blockage | 403.5 | IMPLEMENTED | `path_blockage_node` |
| Protruding objects | 307 | IMPLEMENTED (runtime-disable) | `protruding_objects_node` |
| Trip hazard | 303 | STRETCH (primitives shipped) | `trip_hazard_node` (stub) |
| Ramp width | 405.5 | ARCHITECTED | - |
| Ramp landing | 405.7 | ARCHITECTED | - |

Plus:
- Dynamic obstacle clustering (`dynamic_obstacle_node`, DBSCAN)
- Compliance CPU safety-net monitor (`compliance_monitor_node`)
- Nav2 costmap fusion of 4 x HC-SR04 ultrasonic (via the
  `RangeSensorLayer` configured in
  `star-ros2/src/star_bringup/config/nav2_params.yaml`)

See **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** for the data-flow
diagram and the per-node rate table.

---

## Quickstart (Pi 5)

```bash
# One-shot bootstrap: apt deps, pip deps, colcon build, smoke tests.
./bootstrap_pi.sh          # ~15-20 min on a fresh Pi 5 arm64

# Bring up the full stack including compliance.
./start.sh

# Watch the ADA output topics.
ros2 topic echo /compliance/door_clear_width
ros2 topic echo /compliance/path_blockage
ros2 topic echo /compliance/protruding_objects
ros2 topic echo /compliance/door_threshold
```

The first launch takes ~10 s to initialize (LiDAR spinning, stereo
cameras warming, SLAM building the first map). Doorway candidates fire
as soon as the robot enters a corridor narrower than 1.1 m.

Full setup walk-through in **[docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)**.

---

## Package layout

```
compliance-engine/
  package.xml                   ament_python manifest
  setup.py                      colcon entry_points for each node
  setup.cfg                     ament install-paths
  resource/star_compliance      ROS 2 ament index marker

  docs/                         operator and developer docs
    ARCHITECTURE.md             system-level design, data flow, rates
    DEPLOYMENT.md               Pi setup step-by-step
    TUNING.md                   parameter-by-parameter tuning guide
    VALIDATION.md               ground-truth protocols per check
    TROUBLESHOOTING.md          symptom -> cause -> fix

  star_compliance/
    nodes/                      rclpy glue - one file per ADA check
      ramp_slope_node.py
      door_clear_width_node.py
      door_threshold_node.py
      protruding_objects_node.py
      path_blockage_node.py
      dynamic_obstacle_node.py
      compliance_monitor_node.py
    detectors/                  pure-Python algorithms (no rclpy)
      doorway_lidar_detector.py
      jamb_plane_fitter.py
      door_state_classifier.py
      imu_jolt_detector.py
      cane_zone_filter.py
      wall_plane_fitter.py
      corridor_medial_axis.py
      obstacle_clusterer.py
    engines/                    shared primitives
      floor_frame.py
      plane_segmentation.py
      imu_cross_validate.py
      door_offset_calibration.py
    report/
      pdf_generator.py          reportlab PDF audit report
    bringup/
      compliance.launch.py      all nodes with use_* gates
    models/
      README.md                 where the YOLO ONNX weights live

  tests/
    test_*.py                   unit tests for detectors + engines
    test_nodes_smoke.py         rclpy-mocked node smoke tests
    ros_mocks.py                sys.modules ROS 2 mock harness
    fixtures/                   synthetic LaserScan / PointCloud2 bags
```

---

## Running tests

### Off-robot (dev machine, no ROS install needed)

```bash
cd final_capstone/compliance-engine

# Just the detectors and engines
PYTHONPATH=. pytest tests/ -v

# Only the node smoke tests (no rclpy needed - uses ros_mocks.py)
PYTHONPATH=. pytest tests/test_nodes_smoke.py -v
```

Tests that require Open3D, scikit-image, or scikit-learn skip
gracefully with `pytest.skip(...)` if those libraries aren't installed.

### On-robot (full integration)

```bash
cd star-ros2
colcon test \
    --packages-select star_compliance star_compliance_msgs \
    --event-handlers console_direct+
```

Runs the pytest suite against the installed workspace + any launch
tests that require a live ROS 2 domain.

### CI

GitHub Actions runs the full pytest suite + colcon build of the msgs
package + the ASCII-encoding guard on every push touching
`compliance-engine/` or `star_compliance_msgs/`. See
[`.github/workflows/compliance-engine-ci.yml`](../../.github/workflows/compliance-engine-ci.yml).

---

## Honest claims and disclosures

STAR is a screening tool. Every audit report STAR emits carries these
disclosures per ADA section:

**404.2.3 (door clear width)** - STAR measures the frame opening with
LiDAR + stereo at handle height and applies a calibrated 2.5 in
door + stop + hinge offset to estimate ADA clear width. STAR does
**not** physically open doors and cannot directly sense the door stop
(3/8-1/2 in, below every onboard sensor's resolution). Reported
values are screening estimates; a Certified Access Specialist must
verify flagged violations with a tape measure with the door open 90
degrees.

**404.2.5 (door threshold)** - STAR reports threshold **presence**
via a BNO055 z-axis acceleration jolt signature during traversal.
Precise height is **not** measured. A CASp inspector must confirm
flagged thresholds with a depth gauge.

**303 (trip hazards)** - primitives ready, node glue pending.

**307 (protruding objects)** - STAR flags objects in the 27-80 in
cane-detectable zone that exceed 4 in of wall-stand-off. Runtime-
disableable via `use_ada_307:=false` if CPU headroom is tight.

**403.5 (accessible path width)** - live /scan width compared against
the SLAM map baseline; sustained narrowing to < 36 in flags a
blockage. Does NOT measure the door-transit allowance per 404.2.4.

**405.2 (ramp slope)** - LiDAR plane normal cross-validated against
BNO055 pitch with a +/- 0.5 deg agreement gate. Ground truth by
Wixey WR300 digital angle gauge (+/- 0.1 deg).

**405.5, 405.7 (ramp width, landing)** - ARCHITECTED only; not
claimed by this version of STAR.

---

## License

MIT. Accessibility is a civil right; this tool should not be gated
behind licensing rent.

---

## Contributing

Open a GitHub issue against the
[Locked-Inc/STAR](https://github.com/Locked-Inc/STAR) repository.
Pull requests welcome.
