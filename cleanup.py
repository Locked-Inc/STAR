import shutil
import os

dirs = [
    'star-ros2/src/star_gateway_bridge',
    'star-ros2/src/star_safety_monitor'
]

for d in dirs:
    if os.path.exists(d):
        shutil.rmtree(d)
        print(f"Removed {d}")
