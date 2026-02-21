# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """
    Standalone launch for autonomous frontier exploration.

    Prerequisites:
      1. slam.launch.py is running (SLAM + Nav2 must be active)
      2. m-explore-ros2 built from source in the workspace:
           cd /workspaces/STAR/star-ros2/src
           git clone https://github.com/robo-friends/m-explore-ros2.git
           cd /workspaces/STAR && ./build-ros2.sh

    Usage:
      ros2 launch star_bringup slam.launch.py
      ros2 launch star_bringup explore.launch.py  # in a second terminal
    """
    pkg_star_bringup = get_package_share_directory('star_bringup')

    explore = Node(
        package='explore_lite',
        executable='explore',
        name='explore',
        output='screen',
        parameters=[os.path.join(pkg_star_bringup, 'config', 'explore_params.yaml')],
    )

    return LaunchDescription([
        explore,
    ])
