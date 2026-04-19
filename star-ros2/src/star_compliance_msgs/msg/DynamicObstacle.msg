# DynamicObstacle - one cluster detected by the DBSCAN clusterer
#
# Candidate dynamic obstacles are LiDAR scan returns that do NOT
# coincide with occupied cells in the SLAM map (i.e., not walls or
# known static geometry). Each cluster becomes one DynamicObstacle.
# Not tied to any specific ADA section - feeds path-blockage and
# behavior-tree "pause for pedestrian" logic.

std_msgs/Header header

# Cluster centroid in the map frame.
geometry_msgs/PoseStamped cluster_pose_map_frame

# Max distance from any cluster point to the centroid, in m.
float32 cluster_radius_m

# Number of LiDAR points in the cluster.
uint32 point_count

# DBSCAN core-point density / cluster_radius_m. Higher is more
# confident. 0-1 normalized.
float32 confidence
