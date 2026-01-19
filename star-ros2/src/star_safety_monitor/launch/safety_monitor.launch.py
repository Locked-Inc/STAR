#!/usr/bin/env python3
# Copyright 2026 STAR Team
# Licensed under MIT License

"""Launch file for STAR safety monitor node."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    """Generate launch description for safety monitor."""
    
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
