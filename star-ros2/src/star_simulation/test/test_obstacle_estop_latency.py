#!/usr/bin/env python3
# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
"""Test: Measure e-stop latency from sonar trigger to emergency_stop publish."""

import time
import unittest

import launch
from launch.actions import (
    ExecuteProcess, IncludeLaunchDescription, TimerAction,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
import launch_testing
import launch_testing.actions
from launch_testing.ready_to_test_action_timeout import ready_to_test_action_timeout
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Range
from std_msgs.msg import Bool


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
            'use_ekf': 'false',
        }.items(),
    )

    safety_monitor = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'star_safety_monitor', 'safety_monitor_node',
            '--ros-args',
            '-p', 'use_sim_time:=true',
            '-p', 'enable_auto_estop:=true',
            '-p', 'obstacle_estop_distance:=0.10',
            '-p', 'heartbeat_timeout_ms:=60000',
            '-p', 'publish_rate:=50.0',
        ],
        output='screen',
    )

    configure = ExecuteProcess(
        cmd=['ros2', 'lifecycle', 'set', '/safety_monitor', 'configure'],
    )
    activate = ExecuteProcess(
        cmd=['ros2', 'lifecycle', 'set', '/safety_monitor', 'activate'],
    )

    return launch.LaunchDescription([
        sim_launch,
        TimerAction(period=20.0, actions=[safety_monitor]),
        TimerAction(period=25.0, actions=[configure]),
        TimerAction(period=28.0, actions=[activate]),
        TimerAction(period=32.0, actions=[
            launch_testing.actions.ReadyToTest(),
        ]),
    ]), {}


class TestObstacleEstopLatency(unittest.TestCase):
    """Measure and assert e-stop latency from sonar trigger."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_estop_latency')
        cls.sonar_pub = cls.node.create_publisher(
            Range, '/star/obstacle/front_left', 10)
        cls.estop_time = None
        cls.estop_sub = cls.node.create_subscription(
            Bool, '/emergency_stop', cls._estop_cb, 10)

    @classmethod
    def _estop_cb(cls, msg):
        if msg.data and cls.estop_time is None:
            cls.estop_time = time.monotonic()

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def test_estop_latency_under_500ms(self):
        """Publish close-range sonar, measure time to e-stop."""
        range_msg = Range()
        range_msg.header.frame_id = 'front_left_sonar_link'
        range_msg.radiation_type = Range.ULTRASOUND
        range_msg.field_of_view = 0.26
        range_msg.min_range = 0.02
        range_msg.max_range = 4.0
        range_msg.range = 0.05

        publish_time = time.monotonic()
        self.estop_time = None

        timeout = time.time() + 5.0
        while time.time() < timeout:
            range_msg.header.stamp = self.node.get_clock().now().to_msg()
            self.sonar_pub.publish(range_msg)
            rclpy.spin_once(self.node, timeout_sec=0.01)
            if self.estop_time is not None:
                break

        self.assertIsNotNone(self.estop_time,
                             'E-stop should have been triggered')

        latency_ms = (self.estop_time - publish_time) * 1000
        self.assertLess(latency_ms, 500.0,
                        f'E-stop latency {latency_ms:.1f}ms exceeds 500ms limit')

        print(f'\n  E-stop latency: {latency_ms:.1f} ms\n')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
