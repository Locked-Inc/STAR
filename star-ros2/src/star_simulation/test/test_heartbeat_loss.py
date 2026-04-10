#!/usr/bin/env python3
# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
"""Test: Heartbeat loss triggers e-stop and holds until recovery."""

import time
import unittest

from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
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
            '-p', 'heartbeat_timeout_ms:=500',
            '-p', 'enable_auto_estop:=true',
            '-p', 'estop_recovery_delay:=2.0',
            '-p', 'obstacle_estop_distance:=0.01',
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


class TestHeartbeatLoss(unittest.TestCase):
    """Verify heartbeat loss triggers and holds e-stop."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_heartbeat_loss')
        cls.diag_pub = cls.node.create_publisher(
            DiagnosticArray, '/diagnostics', 10)
        cls.estop_active = False
        cls.estop_sub = cls.node.create_subscription(
            Bool, '/emergency_stop', cls._estop_cb, 10)

    @classmethod
    def _estop_cb(cls, msg):
        cls.estop_active = msg.data

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def spin_for(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def publish_heartbeat(self):
        """Publish a diagnostic heartbeat message."""
        msg = DiagnosticArray()
        msg.header.stamp = self.node.get_clock().now().to_msg()
        status = DiagnosticStatus()
        status.name = 'test_heartbeat'
        status.level = DiagnosticStatus.OK
        status.message = 'alive'
        msg.status.append(status)
        self.diag_pub.publish(msg)

    def test_heartbeat_loss_triggers_estop(self):
        """Stop heartbeat, verify e-stop fires within 1.5 seconds."""
        # Phase 1: Send heartbeats to establish baseline
        for _ in range(20):
            self.publish_heartbeat()
            self.spin_for(0.1)

        self.assertFalse(self.estop_active,
                         'E-stop should NOT be active while heartbeats flowing')

        # Phase 2: Stop heartbeats, wait for timeout (500ms + margin)
        self.spin_for(1.5)

        self.assertTrue(self.estop_active,
                        'E-stop should be active after heartbeat timeout')

    def test_heartbeat_recovery(self):
        """After heartbeat loss, resume heartbeats and verify recovery."""
        # Trigger heartbeat timeout first
        self.spin_for(1.5)
        self.assertTrue(self.estop_active, 'E-stop should be active')

        # Resume heartbeats and wait for recovery delay (2.0s)
        end = time.time() + 4.0
        while time.time() < end:
            self.publish_heartbeat()
            rclpy.spin_once(self.node, timeout_sec=0.05)

        # After recovery delay, e-stop should clear
        self.assertFalse(self.estop_active,
                         'E-stop should recover after heartbeats resume + delay')


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
