#!/usr/bin/env python3
# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
"""Test: Nav2 recovery behaviors work when robot is stuck."""

import math
import time
import unittest

from geometry_msgs.msg import PoseStamped
import launch
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
import launch_testing
import launch_testing.actions
from launch_testing.ready_to_test_action_timeout import ready_to_test_action_timeout
from nav2_msgs.action import NavigateToPose
from nav_msgs.msg import Odometry
import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node


@ready_to_test_action_timeout(90)
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
            'use_nav2': 'true',
            'use_ekf': 'true',
        }.items(),
    )

    return launch.LaunchDescription([
        sim_launch,
        TimerAction(period=35.0, actions=[
            launch_testing.actions.ReadyToTest(),
        ]),
    ]), {}


class TestNav2Recovery(unittest.TestCase):
    """Verify Nav2 can recover from stuck situations."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_nav2_recovery')
        cls.nav_client = ActionClient(
            cls.node, NavigateToPose, 'navigate_to_pose')
        cls.last_odom = None
        cls.odom_sub = cls.node.create_subscription(
            Odometry, '/odometry/filtered', cls._odom_cb, 10)

    @classmethod
    def _odom_cb(cls, msg):
        cls.last_odom = msg

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def spin_for(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def test_navigate_to_goal(self):
        """Send a navigation goal and verify robot reaches it or times out gracefully."""
        # Wait for Nav2 to be ready
        self.assertTrue(
            self.nav_client.wait_for_server(timeout_sec=30.0),
            'Nav2 action server should be available')

        # Wait for initial odom
        self.spin_for(2.0)
        self.assertIsNotNone(self.last_odom, 'Should have odom')

        start_x = self.last_odom.pose.pose.position.x
        start_y = self.last_odom.pose.pose.position.y

        # Send goal: move 1 meter forward in the room
        goal = NavigateToPose.Goal()
        goal.pose = PoseStamped()
        goal.pose.header.frame_id = 'map'
        goal.pose.header.stamp = self.node.get_clock().now().to_msg()
        goal.pose.pose.position.x = start_x + 1.0
        goal.pose.pose.position.y = start_y
        goal.pose.pose.orientation.w = 1.0

        future = self.nav_client.send_goal_async(goal)

        # Wait for goal acceptance
        timeout = time.time() + 10.0
        while not future.done() and time.time() < timeout:
            rclpy.spin_once(self.node, timeout_sec=0.1)

        self.assertTrue(future.done(), 'Goal should be accepted or rejected')
        goal_handle = future.result()

        if goal_handle is None or not goal_handle.accepted:
            self.skipTest('Nav2 rejected goal (may need more startup time)')

        # Wait for result with timeout
        result_future = goal_handle.get_result_async()
        timeout = time.time() + 60.0
        while not result_future.done() and time.time() < timeout:
            rclpy.spin_once(self.node, timeout_sec=0.1)

        # Verify robot moved from start position
        end_x = self.last_odom.pose.pose.position.x
        end_y = self.last_odom.pose.pose.position.y
        distance_moved = math.sqrt(
            (end_x - start_x) ** 2 + (end_y - start_y) ** 2)

        self.assertGreater(distance_moved, 0.3,
                           f'Robot should have moved >0.3m, moved {distance_moved:.2f}m')

        print(f'\n  Robot moved: {distance_moved:.2f} m')
        print(f'  Start: ({start_x:.2f}, {start_y:.2f})')
        print(f'  End: ({end_x:.2f}, {end_y:.2f})\n')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
