# ThresholdMeasurement - ADA 2010 Standards 404.2.5 compliance record
#
# Emitted by the door_threshold_node when the robot traverses a
# doorway and the BNO055 jolt detector fires. STAR reports threshold
# PRESENCE only, not precise height - precise measurement requires a
# downward-facing sensor (deferred to stretch / post-capstone work).

std_msgs/Header header

# Robot pose in the map frame at the moment the jolt was detected.
# Derived from /odom (EKF-filtered) at the jolt timestamp.
geometry_msgs/PoseStamped step_pose_map_frame

# True when a z-accel jolt was detected within the threshold watch
# window (2 s after a doorway measurement). False means the robot
# traversed the doorway cleanly, which is ADA-compliant for 404.2.5.
bool detected

# Peak z-axis acceleration (gravity-compensated) above the rolling
# baseline, in m/s^2. Values above ~2.0 m/s^2 with a 25 ms duration
# indicate a wheel crossing a raised feature.
float32 jolt_magnitude_ms2

# Robot forward velocity at the moment of the jolt, in m/s. Used to
# contextualize the magnitude - the same step at a higher velocity
# produces a larger jolt.
float32 traversal_velocity_mps

# Rough height estimate from double-integrated accel minus gravity.
# **Not a certified measurement.** The PDF generator must include the
# disclosure string below.
float32 estimated_step_height_m

# --- Compliance result -----------------------------------------------

# True when detected==true. ADA 404.2.5 is violated only when a
# threshold > 0.5 in is present, so absence is compliant. STAR flags
# presence only and defers exact height to human follow-up.
bool flagged_violation

string ada_section
string detection_method
string disclosure
