#!/usr/bin/env python3
"""Test: Robot stops when cmd_vel stops publishing (teleop timeout)."""

import time
import unittest

import launch
import launch_testing
import launch_testing.actions
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry

import math

# Generous timeouts for emulated x86 Gazebo on ARM Mac.
# Sim runs at ~0.1-0.3x real-time in Docker emulation.
STARTUP_WAIT_S = 20.0
DRIVE_WALL_CLOCK_S = 30.0
STOP_WAIT_S = 15.0
SPEED_THRESHOLD = 0.02


def generate_test_description():
    """Launch Gazebo sim headless for testing."""
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

    return launch.LaunchDescription([
        sim_launch,
        TimerAction(period=STARTUP_WAIT_S, actions=[
            launch_testing.actions.ReadyToTest(),
        ]),
    ]), {'sim_launch': sim_launch}


class TestTeleopTimeout(unittest.TestCase):
    """Verify robot stops when cmd_vel stops publishing."""

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node('test_teleop_timeout')
        cls.cmd_pub = cls.node.create_publisher(Twist, '/cmd_vel', 10)
        cls.last_odom = None
        cls.odom_sub = cls.node.create_subscription(
            Odometry, '/odom/unfiltered', cls._odom_cb, 10)

    @classmethod
    def _odom_cb(cls, msg):
        cls.last_odom = msg

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def spin_for(self, seconds):
        """Spin the node for a given wall-clock duration."""
        end = time.time() + seconds
        while time.time() < end:
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def get_linear_speed(self):
        """Get current linear speed from odom."""
        if self.last_odom is None:
            return 0.0
        vx = self.last_odom.twist.twist.linear.x
        vy = self.last_odom.twist.twist.linear.y
        return math.sqrt(vx * vx + vy * vy)

    def test_robot_stops_when_cmdvel_stops(self):
        """Drive robot, stop publishing, verify velocity drops to zero."""
        # Phase 1: Drive forward and wait until odom confirms movement
        cmd = Twist()
        cmd.linear.x = 0.3

        moving = False
        end = time.time() + DRIVE_WALL_CLOCK_S
        while time.time() < end:
            self.cmd_pub.publish(cmd)
            rclpy.spin_once(self.node, timeout_sec=0.05)
            if self.get_linear_speed() > SPEED_THRESHOLD:
                moving = True
                # Keep driving a bit more to establish steady state
                self.spin_for(2.0)
                # Re-publish during that wait
                break

        # Keep publishing for a couple more wall seconds
        end2 = time.time() + 5.0
        while time.time() < end2:
            self.cmd_pub.publish(cmd)
            rclpy.spin_once(self.node, timeout_sec=0.05)

        speed_while_driving = self.get_linear_speed()
        self.assertTrue(moving or speed_while_driving > SPEED_THRESHOLD,
                        f"Robot never started moving (speed={speed_while_driving})")

        # Phase 2: Stop publishing cmd_vel entirely
        self.spin_for(STOP_WAIT_S)

        # Verify robot has stopped
        speed_after_stop = self.get_linear_speed()
        self.assertLess(speed_after_stop, SPEED_THRESHOLD,
                        f"Robot should have stopped, got {speed_after_stop}")


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        # Gazebo exits with SIGTERM (-15) in headless mode -- that is normal
        launch_testing.asserts.assertExitCodes(
            proc_info, allowable_exit_codes=[0, -2, -15])
