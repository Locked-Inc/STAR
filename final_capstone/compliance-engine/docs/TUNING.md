# Parameter Tuning Guide

Every compliance node exposes a handful of ROS 2 parameters. Defaults
are tuned against synthetic data and the first round of hardware
testing. This doc says what each one does, expected working range,
and how to measure the effect of changes.

All parameters can be set at launch time:

```bash
ros2 launch star_compliance compliance.launch.py \
    use_ada_307:=false \
    ada_307_input_topic:=/stereo/points2
```

or per-node:

```bash
ros2 param set /star_protruding_objects_node enabled false
```

---

## Global launch arguments

Defined in `star_compliance/bringup/compliance.launch.py`.

| Argument | Default | Effect |
|---|---|---|
| `use_stereo` | `true` | Door clear-width uses stereo. Set `false` for LiDAR-only operation when the IMX219-83 is disconnected. |
| `use_ada_307` | `true` | ADA 307 node starts. Set `false` to save ~25% CPU. |
| `use_path_blockage` | `true` | ADA 403.5 node starts. |
| `use_dynamic_obstacles` | `true` | DBSCAN clusterer starts. |
| `use_compliance_monitor` | `true` | CPU safety-net monitor starts. |
| `ada_307_input_topic` | `/cloud_map` | Source for the protruding-objects RANSAC. Switch to `/stereo/points2` for 5-10 Hz at 3-4x CPU cost; keep on `/cloud_map` for 1 Hz steady-state. |

---

## Per-node parameters

### ramp_slope_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| (none user-configurable today) | - | - | - |

Known-good constants defined in `detectors/plane_segmentation.py`:
RANSAC `distance_threshold=0.03`, `num_iterations=500`,
`min_inliers=50`.

### door_clear_width_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `door_offset_m` | `0.0635` | 0.04 - 0.08 | Total door+stop+hinge offset subtracted from frame width. Defaults to 2.5 in (1.75 in door + 0.5 in stop + 0.25 in hinge). Calibrate per deployment by measuring a few known doors and fitting the mean. |
| `sensor_height_m` | `0.25` | 0.15 - 0.35 | LiDAR scan-plane height above the floor. Used to derive the handle-height band. Measure once with a ruler; static per robot build. |
| `enabled` | `true` | bool | Toggle the whole node without killing its subscriptions. Useful for mid-run throttling. |
| `classifier_weights_path` | `models/door_state_yolov8n.onnx` | path | YOLOv8n ONNX weights. When missing, the classifier falls back to a geometric heuristic on the stereo point cloud. |

**How to tune door_offset_m on a new deployment:**

1. Drive STAR up to 5 known-compliant doors (tape-measured clear
   width >= 32 in).
2. For each, record `frame_width_m` and the Wixey-verified
   `ada_clear_width_m` ground truth.
3. Set `door_offset_m = mean(frame_width_m - ada_clear_width_m)`.
4. Commit the new default to `launch/compliance.launch.py`.

### door_threshold_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `threshold_ms2` | `2.0` | 1.0 - 5.0 | z-accel deviation (above baseline) that fires the jolt detector. Lower = more sensitive (more false positives on carpet transitions); higher = fewer reads but misses compliant thresholds. |
| `watch_duration_sec` | `2.0` | 1.0 - 4.0 | How long after a DoorwayMeasurement the jolt detector stays armed. Match to robot traversal speed: time to cross a 1.2 m doorway at 0.5 m/s is 2.4 s. |
| `enabled` | `true` | bool | Toggle at runtime. |

**How to tune threshold_ms2:**

1. Drive over 5 known-compliant smooth floors (no threshold).
   Expected: zero jolt events.
2. Drive over 5 known thresholds > 0.5 in. Expected: every traversal
   produces exactly one event.
3. If FP > 10%, raise `threshold_ms2` by 0.5.
4. If FN > 10%, lower `threshold_ms2` by 0.3.
5. Re-measure after each change.

### protruding_objects_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `enabled` | `true` | bool | Auto-toggled by `compliance_monitor_node` under CPU stress. |
| `input_cloud_topic` | `/cloud_map` | topic | `/cloud_map` is 1 Hz (cheap). `/stereo/points2` is 5-10 Hz (3-4x CPU). |
| `sensor_height_m` | `0.25` | 0.15 - 0.35 | Same as above; used to anchor the floor plane. |
| `min_cluster_points` | `5` | 3 - 20 | Minimum stereo points in a cluster before it counts as a protrusion. Lower = more FP from stereo noise; higher = misses small signs. |

**How to tune min_cluster_points:**

1. Set up 3 known protrusions: 4.5 in (borderline), 6 in (clear
   violation), 2 in (compliant).
2. Walk the robot past each one slowly.
3. All three should produce point clusters; only the first two should
   flag `flagged_violation=true`. If the 2 in cluster is flagging,
   raise `min_cluster_points` and re-test.

### path_blockage_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `enabled` | `true` | bool | Toggle at runtime. |

Constants in `detectors/corridor_medial_axis.py`:
`free_threshold=50` (occupancy probability above which a cell is
"obstacle"). `BLOCKAGE_DELTA_M=0.15` and
`BLOCKAGE_SUSTAIN_FRAMES=3` are defined in the node itself; tune by
editing the module if persistent false positives appear.

### dynamic_obstacle_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `enabled` | `true` | bool | Toggle at runtime. |
| `eps_m` | `0.15` | 0.08 - 0.30 | DBSCAN neighborhood radius. Smaller = more, smaller clusters; larger = fewer, larger clusters. |
| `min_samples` | `5` | 3 - 15 | DBSCAN core-point threshold. Lower = more FP from LiDAR noise; higher = misses small dynamic objects. |

**How to tune DBSCAN eps_m:**

1. Walk through the robot's view.
2. `ros2 topic echo /perception/dynamic_obstacles` should show
   exactly one cluster per person.
3. If a person produces 2+ clusters, raise `eps_m` by 0.05.
4. If two people standing 30 cm apart merge into one cluster, lower
   `eps_m` by 0.03.

### compliance_monitor_node

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `cpu_count` | auto-detect | 1-16 | Divisor for normalizing the 1-minute load average. Override only if `os.cpu_count()` misreports. |

Thresholds `HIGH_LOAD_THRESHOLD=0.80` and `LOW_LOAD_THRESHOLD=0.50`
are in the module. Adjust only if you're chasing a specific CPU
budget.

---

## Nav2 costmap (RangeSensorLayer)

`star-ros2/src/star_bringup/config/nav2_params.yaml`:

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `local_costmap.sonar_layer.phi` | 1.2 | 0.5 - 2.5 | Full cone angle (radians) for HC-SR04 inflation. 1.2 rad ~ 70 deg. The HC-SR04's datasheet is 15 deg effective, but inflating wider accounts for near-range spread. |
| `sonar_layer.clear_threshold` | 0.2 | 0.1 - 0.5 | Cells with cost probability below this are cleared. |
| `sonar_layer.mark_threshold` | 0.8 | 0.5 - 0.95 | Cells with cost above this are marked as obstacle. |
| `sonar_layer.no_readings_timeout` | 1.0 | 0.5 - 5.0 | Seconds without a reading before the layer assumes the sensor is offline. |

Don't lower `phi` below 1.0: the HC-SR04 is noisy and the wider cone
absorbs false negatives from off-axis targets.

---

## See also

- [DEPLOYMENT.md](DEPLOYMENT.md) - setup sequence
- [VALIDATION.md](VALIDATION.md) - how to validate changes against
  tape-measure ground truth
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - symptom -> fix
