#!/usr/bin/env python3
# Copyright 2026 STAR Team
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Launch file for STAR safety monitor node."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode
from launch_ros.actions import Node


def generate_launch_description():
    """
    Generate launch description for safety monitor.

    Returns
    -------
    LaunchDescription
        Launch description.

    """
    # Get package directory
    pkg_dir = get_package_share_directory('star_safety_monitor')

    # Declare launch arguments
    config_file_arg = DeclareLaunchArgument(
        'config_file',
        default_value=os.path.join(pkg_dir, 'config', 'safety_monitor.yaml'),
        description='Path to safety monitor configuration file'
    )

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation time'
    )

    # Safety monitor lifecycle node
    safety_monitor_node = LifecycleNode(
        package='star_safety_monitor',
        executable='safety_monitor_node',
        name='safety_monitor',
        namespace='',
        output='screen',
        parameters=[
            LaunchConfiguration('config_file'),
            {'use_sim_time': LaunchConfiguration('use_sim_time')}
        ],
        remappings=[
            ('/diagnostics', '/diagnostics'),
            ('/odom', '/odom'),
            ('/emergency_stop', '/emergency_stop'),
        ]
    )

    # Lifecycle manager to automatically configure and activate the safety monitor
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='safety_monitor_lifecycle_manager',
        output='screen',
        parameters=[
            {'autostart': True},
            {'node_names': ['safety_monitor']}
        ]
    )

    return LaunchDescription([
        config_file_arg,
        use_sim_time_arg,
        safety_monitor_node,
        lifecycle_manager,
    ])
