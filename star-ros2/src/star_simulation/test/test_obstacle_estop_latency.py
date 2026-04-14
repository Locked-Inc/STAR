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

# Test parameters.

#: Close-range sonar reading used to trigger the e-stop. Must be below
#: the safety_monitor obstacle_estop_distance (0.10 m by default).
TRIGGER_RANGE_M = 0.05

#: Sonar field-of-view in radians. Matches the URDF/sim sensor model
#: for the front-left sonar.
SONAR_FIELD_OF_VIEW_RAD = 0.26

#: Minimum reportable range for the simulated sonar (meters).
SONAR_MIN_RANGE_M = 0.02

#: Maximum reportable range for the simulated sonar (meters).
SONAR_MAX_RANGE_M = 4.0

#: Acceptable upper bound on the publish-to-estop latency (milliseconds).
#: 500 ms is the safety SLA for the obstacle e-stop pipeline.
LATENCY_LIMIT_MS = 500.0

#: How long to wait for the e-stop to fire before declaring the test
#: failed (seconds). Generous to account for DDS discovery jitter.
ESTOP_WAIT_TIMEOUT_S = 15.0

#: How long to publish safe-range sonar before switching to the trigger
#: range, allowing DDS to discover the test publisher and the safety
#: monitor subscriber (seconds).
DDS_DISCOVERY_WINDOW_S = 5.0

#: Safe sonar range (meters) used during the discovery phase, well
#: above the obstacle_estop_distance threshold.
SAFE_RANGE_M = 2.0

#: rclpy.spin_once timeout (seconds) during DDS discovery. Coarse
#: enough to keep CPU low while the publisher warms up.
SPIN_TIMEOUT_DISCOVERY_S = 0.05

#: rclpy.spin_once timeout (seconds) during latency measurement.
#: Tighter for measurement accuracy.
SPIN_TIMEOUT_MEASUREMENT_S = 0.01


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
        TimerAction(period=35.0, actions=[configure]),
        TimerAction(period=50.0, actions=[activate]),
        TimerAction(period=55.0, actions=[
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
        # Allow DDS discovery between test node and safety monitor.
        end_discovery = time.time() + DDS_DISCOVERY_WINDOW_S
        safe_msg = Range()
        safe_msg.header.frame_id = 'front_left_sonar_link'
        safe_msg.radiation_type = Range.ULTRASOUND
        safe_msg.field_of_view = SONAR_FIELD_OF_VIEW_RAD
        safe_msg.min_range = SONAR_MIN_RANGE_M
        safe_msg.max_range = SONAR_MAX_RANGE_M
        safe_msg.range = SAFE_RANGE_M
        while time.time() < end_discovery:
            safe_msg.header.stamp = self.node.get_clock().now().to_msg()
            self.sonar_pub.publish(safe_msg)
            rclpy.spin_once(self.node, timeout_sec=SPIN_TIMEOUT_DISCOVERY_S)

        # Now publish close-range sonar and measure latency.
        range_msg = Range()
        range_msg.header.frame_id = 'front_left_sonar_link'
        range_msg.radiation_type = Range.ULTRASOUND
        range_msg.field_of_view = SONAR_FIELD_OF_VIEW_RAD
        range_msg.min_range = SONAR_MIN_RANGE_M
        range_msg.max_range = SONAR_MAX_RANGE_M
        range_msg.range = TRIGGER_RANGE_M

        # Reset on the class because _estop_cb writes to the class attribute.
        type(self).estop_time = None

        # Capture the timestamp of the FIRST trigger-range publish. The
        # e-stop cannot have been triggered before any trigger publish
        # arrived at the safety monitor, so (estop_time - first_publish)
        # is a true upper bound on the obstacle-to-estop latency. Earlier
        # versions of this test updated the timestamp on every iteration,
        # which produced a *lower* bound that could mask SLA violations.
        first_publish_time = time.monotonic()
        range_msg.header.stamp = self.node.get_clock().now().to_msg()
        self.sonar_pub.publish(range_msg)
        rclpy.spin_once(self.node, timeout_sec=SPIN_TIMEOUT_MEASUREMENT_S)

        timeout = time.time() + ESTOP_WAIT_TIMEOUT_S
        while time.time() < timeout and type(self).estop_time is None:
            range_msg.header.stamp = self.node.get_clock().now().to_msg()
            self.sonar_pub.publish(range_msg)
            rclpy.spin_once(self.node, timeout_sec=SPIN_TIMEOUT_MEASUREMENT_S)

        self.assertIsNotNone(type(self).estop_time,
                             'E-stop should have been triggered')

        latency_ms = (type(self).estop_time - first_publish_time) * 1000
        self.assertLess(latency_ms, LATENCY_LIMIT_MS,
                        f'E-stop latency {latency_ms:.1f}ms exceeds '
                        f'{LATENCY_LIMIT_MS:.0f}ms limit')

        print(f'\n  E-stop latency: {latency_ms:.1f} ms\n')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
