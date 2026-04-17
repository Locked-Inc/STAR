# ProtrudingObject - one object flagged under ADA 2010 Standards 307
#
# "Protruding Objects" are objects that extend more than 4 inches into
# circulation paths between 27 inches and 80 inches above the floor
# (the cane-detectable zone). Examples: wall-mounted signs, drinking
# fountains, fire extinguishers in cabinets that stick out too far.

std_msgs/Header header

# Pose of the cluster centroid in the map frame.
geometry_msgs/PoseStamped object_pose_map_frame

# --- Geometric measurements ------------------------------------------

# Perpendicular distance from the cluster centroid to the nearest
# RANSAC wall plane. > 0.1016 m (4 in) triggers the violation flag.
float32 protrusion_depth_m

# Height of the cluster centroid above the map-frame floor plane.
# Must be within [0.6858, 2.032] m (27-80 in) for the protrusion to
# count under ADA 307.
float32 height_above_floor_m

# Bounding-box diagonal of the cluster, in m. Helps a human reviewer
# tell a sign (small) from a drinking fountain (large).
float32 cluster_extent_m

# --- Raw cluster data ------------------------------------------------

# Number of 3D points inside the cluster.
uint32 cluster_point_count

# Parametric offset d of the wall plane equation n . p + d = 0, in the
# map frame, for audit reproducibility.
float32 wall_plane_distance_m

# --- Compliance result -----------------------------------------------

# True when protrusion_depth_m > 0.1016 AND
# 0.6858 <= height_above_floor_m <= 2.032 AND
# cluster_point_count >= 5.
bool flagged_violation

string ada_section

# Absolute path to a colored-point-cloud PNG embedded in the audit PDF.
string cloud_snippet_path
