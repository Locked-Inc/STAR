# PathBlockage - ADA 2010 Standards 403.5 compliance record
#
# Flagged when a corridor whose SLAM-map baseline width was >= 36 in
# has shrunk in the current /scan view to < 36 in by at least 0.15 m -
# i.e., a dynamic obstacle is blocking an accessible route.

std_msgs/Header header
geometry_msgs/PoseStamped blockage_pose_map_frame

# --- Measurement pair -------------------------------------------------

# Local clearance at this pose when the SLAM map was built. Baseline
# from the medial-axis-transform of the occupancy grid.
float32 map_free_width_m

# Local clearance right now, from the projected /scan. Should match
# map_free_width_m minus any transient obstacle.
float32 live_free_width_m

# map_free_width_m - live_free_width_m. Positive values mean the path
# is narrower than the map records.
float32 blockage_delta_m

# --- Compliance result -----------------------------------------------

# True when live_free_width_m < 0.9144 m (36 in) AND
# blockage_delta_m > 0.15 m for 3+ consecutive /scan frames.
bool flagged_violation

string ada_section

# PNG thumbnail of the occupancy-grid region showing the blockage.
string cloud_snippet_path
