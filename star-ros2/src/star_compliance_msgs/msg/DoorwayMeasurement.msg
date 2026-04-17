# DoorwayMeasurement - ADA 2010 Standards 404.2.3 compliance record
#
# One measurement event produced by the door_clear_width_node in the
# STAR compliance engine. Emitted per doorway detected during an
# autonomous audit run. Carries the full sensor-to-compliance audit
# trail so a CASp inspector or judge can reproduce the reasoning.

# When and where the measurement was taken
std_msgs/Header header
geometry_msgs/PoseStamped robot_pose_at_measurement

# --- Measured values -------------------------------------------------

# Distance between the two parallel jambs, measured by the fused LiDAR /
# stereo pipeline at handle height.
float32 frame_width_m

# ada_clear_width_m = frame_width_m - offset_applied_m.
# Represents an estimate of ADA 404.2.3 clear width (door face to stop
# with door open 90 deg), applying the calibrated door-thickness +
# stop-height + hinge-offset constant.
float32 ada_clear_width_m

# Offset subtracted from frame_width_m. Default 0.063 m (1.75 in door +
# 0.5 in stop + 0.25 in hinge). Calibrated per door style if known.
float32 offset_applied_m

# --- Door state (from YOLOv8n classifier + geometric cross-check) ----

uint8 DOOR_STATE_UNKNOWN = 0
uint8 DOOR_STATE_OPEN = 1
uint8 DOOR_STATE_CLOSED = 2
uint8 DOOR_STATE_AJAR = 3
uint8 DOOR_STATE_NONE = 4
uint8 door_state

# True when stereo geometry confirms the YOLO classification (e.g.,
# state=OPEN plus a vertical plane at 90 deg +/- 15 deg from the
# doorway normal).
bool state_validated

# --- Confidence -------------------------------------------------------

uint8 CONFIDENCE_HIGH = 0
uint8 CONFIDENCE_MEDIUM = 1
uint8 CONFIDENCE_LOW = 2
uint8 confidence

# --- Compliance result -----------------------------------------------

# True if ada_clear_width_m < 0.8128 m (32 inches).
bool flagged_violation

# Always "404.2.3". Included so downstream report generators can group
# and cite violations without external config.
string ada_section

# --- Cross-validation audit trail ------------------------------------

# Independent frame-width from the LiDAR wall-line RANSAC. Typically
# equals frame_width_m when stereo drives the final value, but kept
# separately for audit.
float32 lidar_frame_width_m

# Independent frame-width from the stereo vertical-plane RANSAC.
float32 stereo_frame_width_m

# |lidar_frame_width_m - stereo_frame_width_m|. Used to downgrade
# confidence when the two modalities disagree by > 0.03 m.
float32 cross_validation_delta_m

# --- Evidence thumbnails ---------------------------------------------

# Absolute paths to PNG thumbnails the PDF generator embeds in the
# audit report. Empty strings when thumbnail capture failed.
string image_thumbnail_path
string cloud_snippet_path
