#!/usr/bin/env python3
"""Test: Safety monitor detects motor stall and triggers e-stop."""

import time
import subprocess
import unittest

import launch
import launch_testing
import launch_testing.actions
from launch.actions import (
    IncludeLaunchDescription, TimerAction, ExecuteProcess,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from std_msgs.msg import Bool


def generate_test_description():
    """Launch sim + safety monitor for stall detection test."""
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
            '-p', 'stall_detection_threshold:=0.05',
            '-p', 'stall_samples_required:=3',
            '-p', 'enable_auto_estop:=true',
            '-p', 'heartbeat_timeout_ms:=60000',
            '-p', 'obstacle_estop_distance:=0.02',
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


class TestStallDetection(unittest.TestCase):
    """Verify safety monitor detects stall and fires e-stop."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_stall_detection')
        cls.cmd_pub = cls.node.create_publisher(Twist, '/cmd_vel', 10)
        cls.estop_received = False
        cls.last_odom = None

        cls.odom_sub = cls.node.create_subscription(
            Odometry, '/odom/unfiltered', cls._odom_cb, 10)
        cls.estop_sub = cls.node.create_subscription(
            Bool, '/emergency_stop', cls._estop_cb, 10)

    @classmethod
    def _odom_cb(cls, msg):
        cls.last_odom = msg

    @classmethod
    def _estop_cb(cls, msg):
        if msg.data:
            cls.estop_received = True

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def spin_for(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def test_stall_triggers_estop(self):
        """Drive robot into a wall, verify stall detected -> e-stop."""
        # Spawn a thick wall directly in front of the robot
        subprocess.run([
            'gz', 'service', '-s', '/world/indoor_slam_test/create',
            '--reqtype', 'gz.msgs.EntityFactory',
            '--reptype', 'gz.msgs.Boolean',
            '--timeout', '5000',
            '--req',
            "sdf: '<model name=\"test_wall\"><static>true</static>"
            "<pose>0.25 0 0.25 0 0 0</pose>"
            "<link name=\"link\"><collision name=\"col\">"
            "<geometry><box><size>0.5 2.0 0.5</size></box></geometry>"
            "</collision></link></model>'",
        ], capture_output=True, timeout=10)

        self.spin_for(1.0)

        # Command the robot forward into the wall
        cmd = Twist()
        cmd.linear.x = 0.4
        self.estop_received = False

        # Keep commanding for up to 10 seconds, check for e-stop
        timeout = time.time() + 10.0
        while time.time() < timeout:
            self.cmd_pub.publish(cmd)
            rclpy.spin_once(self.node, timeout_sec=0.05)
            if self.estop_received:
                break

        self.assertTrue(self.estop_received,
                        "Safety monitor should have detected stall and fired e-stop")


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
