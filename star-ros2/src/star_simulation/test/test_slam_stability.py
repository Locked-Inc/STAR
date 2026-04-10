#!/usr/bin/env python3
# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
"""Test: SLAM map stays valid under aggressive driving."""

import math
import time
import unittest

from geometry_msgs.msg import Twist
import launch
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
import launch_testing
import launch_testing.actions
from launch_testing.ready_to_test_action_timeout import ready_to_test_action_timeout
from nav_msgs.msg import OccupancyGrid, Odometry
import rclpy
from rclpy.node import Node


@ready_to_test_action_timeout(60)
def generate_test_description():
    sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([
                FindPackageShare('star_simulation'), 'launch',
                'simulation.launch.py',
            ])
        ),
        launch_arguments={
            'headless': 'true',
            'use_rviz': 'false',
            'use_nav2': 'false',
            'use_ekf': 'true',
        }.items(),
    )

    return launch.LaunchDescription([
        sim_launch,
        TimerAction(period=25.0, actions=[
            launch_testing.actions.ReadyToTest(),
        ]),
    ]), {}


class TestSlamStability(unittest.TestCase):
    """Verify SLAM map integrity under aggressive driving."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_slam_stability')
        cls.cmd_pub = cls.node.create_publisher(Twist, '/cmd_vel', 10)
        cls.last_map = None
        cls.last_odom = None
        cls.map_sizes = []

        cls.map_sub = cls.node.create_subscription(
            OccupancyGrid, '/map', cls._map_cb, 10)
        cls.odom_sub = cls.node.create_subscription(
            Odometry, '/odometry/filtered', cls._odom_cb, 10)

    @classmethod
    def _map_cb(cls, msg):
        cls.last_map = msg
        cls.map_sizes.append(msg.info.width * msg.info.height)

    @classmethod
    def _odom_cb(cls, msg):
        cls.last_odom = msg

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def drive(self, linear, angular, duration):
        """Drive with given velocities for duration seconds."""
        cmd = Twist()
        cmd.linear.x = linear
        cmd.angular.z = angular
        end = time.time() + duration
        while time.time() < end:
            self.cmd_pub.publish(cmd)
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def test_slam_survives_aggressive_driving(self):
        """Execute aggressive maneuvers and verify map integrity."""
        # Record initial map state
        self.drive(0.0, 0.0, 1.0)
        initial_map_count = len(self.map_sizes)

        # Maneuver sequence: forward, sharp left, forward, sharp right,
        # spin in place, reverse, forward again
        maneuvers = [
            (0.4, 0.0, 2.0),
            (0.2, 1.5, 2.0),
            (0.4, 0.0, 2.0),
            (0.2, -1.5, 2.0),
            (0.0, 1.5, 3.0),
            (-0.3, 0.0, 1.5),
            (0.3, 0.8, 3.0),
        ]

        for linear, angular, duration in maneuvers:
            self.drive(linear, angular, duration)

        # Stop and let SLAM process final scans
        self.drive(0.0, 0.0, 2.0)

        # 1. Map should exist and have grown
        self.assertIsNotNone(self.last_map, 'SLAM should have produced a map')
        self.assertGreater(len(self.map_sizes), initial_map_count,
                           'Map should have been updated during driving')

        # 2. Map resolution should be valid
        res = self.last_map.info.resolution
        self.assertGreater(res, 0.0, 'Map resolution should be positive')
        self.assertFalse(math.isnan(res), 'Map resolution should not be NaN')

        # 3. Map origin should be valid
        ox = self.last_map.info.origin.position.x
        oy = self.last_map.info.origin.position.y
        self.assertFalse(math.isnan(ox), 'Map origin X should not be NaN')
        self.assertFalse(math.isnan(oy), 'Map origin Y should not be NaN')

        # 4. Robot position should be valid
        if self.last_odom:
            px = self.last_odom.pose.pose.position.x
            py = self.last_odom.pose.pose.position.y
            self.assertFalse(math.isnan(px), 'Robot X should not be NaN')
            self.assertFalse(math.isnan(py), 'Robot Y should not be NaN')

        # 5. Map data should not be all unknown (-1)
        known_cells = sum(1 for c in self.last_map.data if c >= 0)
        total_cells = len(self.last_map.data)
        known_pct = known_cells / total_cells * 100
        self.assertGreater(known_pct, 1.0,
                           f'Map should have >1% known cells, got {known_pct:.1f}%')

        print(f'\n  Map: {self.last_map.info.width}x{self.last_map.info.height}')
        print(f'  Known cells: {known_pct:.1f}%')
        print(f'  Map updates: {len(self.map_sizes)}\n')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
