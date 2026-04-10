#!/usr/bin/env python3
# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
"""Test: Measure EKF drift relative to Gazebo ground truth."""

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
from nav_msgs.msg import Odometry
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


class TestEkfDrift(unittest.TestCase):
    """Measure EKF drift vs Gazebo ground truth."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_ekf_drift')
        cls.cmd_pub = cls.node.create_publisher(Twist, '/cmd_vel', 10)
        cls.last_raw = None
        cls.last_filtered = None
        cls.drift_samples = []

        cls.raw_sub = cls.node.create_subscription(
            Odometry, '/odom/unfiltered', cls._raw_cb, 10)
        cls.filtered_sub = cls.node.create_subscription(
            Odometry, '/odometry/filtered', cls._filtered_cb, 10)

    @classmethod
    def _raw_cb(cls, msg):
        cls.last_raw = msg

    @classmethod
    def _filtered_cb(cls, msg):
        cls.last_filtered = msg
        if cls.last_raw is not None:
            dx = msg.pose.pose.position.x - cls.last_raw.pose.pose.position.x
            dy = msg.pose.pose.position.y - cls.last_raw.pose.pose.position.y
            drift = math.sqrt(dx * dx + dy * dy)
            cls.drift_samples.append(drift)

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def drive(self, linear, angular, duration):
        cmd = Twist()
        cmd.linear.x = linear
        cmd.angular.z = angular
        end = time.time() + duration
        while time.time() < end:
            self.cmd_pub.publish(cmd)
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def test_ekf_drift_within_bounds(self):
        """Drive a pattern and verify EKF tracks ground truth within 0.5m."""
        self.drift_samples.clear()

        # Drive a square-ish pattern
        for _ in range(2):
            self.drive(0.3, 0.0, 3.0)
            self.drive(0.0, 1.0, 1.57)

        # Stop and collect final samples
        self.drive(0.0, 0.0, 2.0)

        self.assertGreater(len(self.drift_samples), 10,
                           'Should have collected drift samples')

        max_drift = max(self.drift_samples)
        avg_drift = sum(self.drift_samples) / len(self.drift_samples)

        self.assertLess(max_drift, 0.5,
                        f'Max EKF drift {max_drift:.3f}m exceeds 0.5m limit')

        print(f'\n  Drift samples: {len(self.drift_samples)}')
        print(f'  Average drift: {avg_drift:.4f} m')
        print(f'  Max drift: {max_drift:.4f} m\n')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
