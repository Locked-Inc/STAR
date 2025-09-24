#!/usr/bin/env python3
"""
ROS2 Bridge Node
Interfaces between the Robot Gateway Bridge and ROS2 ecosystem
Handles ROS2 publishers, subscribers, and service calls
"""

import json
import logging
import threading
import time
from typing import Dict, Any, Optional

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
import zmq

# ROS2 message imports
from geometry_msgs.msg import Twist, PoseWithCovarianceStamped
from nav_msgs.msg import Odometry
from sensor_msgs.msg import LaserScan, Image, PointCloud2, Imu
from std_msgs.msg import String
from tf2_msgs.msg import TFMessage

# Navigation imports
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ROS2BridgeNode(Node):
    """ROS2 node that bridges with the Robot Gateway Bridge via ZMQ"""
    
    def __init__(self):
        super().__init__('robot_gateway_bridge_node')
        
        self.get_logger().info("Initializing ROS2 Bridge Node...")
        
        # ZMQ setup for communication with bridge
        self.context = zmq.Context()
        self.command_socket = None  # Receive commands from bridge
        self.data_socket = None     # Send robot data to bridge
        
        self.running = False
        
        # ROS2 Publishers
        self.cmd_vel_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        self.status_pub = self.create_publisher(String, '/robot/status', 10)
        
        # ROS2 Subscribers
        self.create_subscription(Odometry, '/odom', self.odom_callback, 10)
        self.create_subscription(LaserScan, '/scan', self.lidar_callback, 10)
        self.create_subscription(Image, '/stereo/left/image_raw', self.left_image_callback, 10)
        self.create_subscription(Image, '/stereo/right/image_raw', self.right_image_callback, 10)
        self.create_subscription(Imu, '/imu/data', self.imu_callback, 10)
        self.create_subscription(TFMessage, '/tf', self.tf_callback, 10)
        
        # Navigation action client
        self.nav_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        
        # Robot state tracking
        self.robot_data = {
            'position': {'x': 0.0, 'y': 0.0, 'z': 0.0},
            'orientation': {'x': 0.0, 'y': 0.0, 'z': 0.0, 'w': 1.0},
            'velocity': {'linear': 0.0, 'angular': 0.0},
            'sensors': {
                'lidar': {},
                'camera': {},
                'imu': {}
            }
        }
        
        # Setup ZMQ connections
        self.setup_zmq()
        
        # Start command processing thread
        self.command_thread = threading.Thread(target=self.process_commands, daemon=True)
        self.command_thread.start()
        
        self.get_logger().info("ROS2 Bridge Node initialized successfully")
    
    def setup_zmq(self):
        """Set up ZMQ sockets for bridge communication"""
        try:
            # Socket to receive commands from bridge
            self.command_socket = self.context.socket(zmq.PULL)
            self.command_socket.bind("tcp://*:5555")
            
            # Socket to send robot data to bridge
            self.data_socket = self.context.socket(zmq.PUSH)
            self.data_socket.bind("tcp://*:5556")
            
            self.get_logger().info("ZMQ sockets initialized")
            
        except Exception as e:
            self.get_logger().error(f"Failed to setup ZMQ: {e}")
    
    def process_commands(self):
        """Process incoming commands from bridge"""
        self.get_logger().info("Command processing thread started")
        
        # Set up poller for non-blocking receive
        poller = zmq.Poller()
        poller.register(self.command_socket, zmq.POLLIN)
        
        while rclpy.ok():
            try:
                socks = dict(poller.poll(timeout=1000))  # 1 second timeout
                
                if self.command_socket in socks:
                    message = self.command_socket.recv_string(zmq.NOBLOCK)
                    command_data = json.loads(message)
                    
                    self.get_logger().info(f"Received command: {command_data}")
                    self.handle_command(command_data)
                    
            except zmq.Again:
                continue  # Timeout, continue loop
            except Exception as e:
                self.get_logger().error(f"Error processing command: {e}")
    
    def handle_command(self, command_data: Dict[str, Any]):
        """Handle incoming commands from bridge"""
        command_type = command_data.get('type', 'unknown')
        
        if command_type == 'cmd_vel':
            self.handle_cmd_vel(command_data)
        elif command_type == 'nav_goal':
            self.handle_navigation_goal(command_data)
        elif command_type == 'status_request':
            self.send_robot_status()
        else:
            self.get_logger().warning(f"Unknown command type: {command_type}")
    
    def handle_cmd_vel(self, command_data: Dict[str, Any]):
        """Handle velocity command"""
        try:
            twist = Twist()
            
            linear = command_data.get('linear', {})
            angular = command_data.get('angular', {})
            
            twist.linear.x = float(linear.get('x', 0.0))
            twist.linear.y = float(linear.get('y', 0.0))
            twist.linear.z = float(linear.get('z', 0.0))
            
            twist.angular.x = float(angular.get('x', 0.0))
            twist.angular.y = float(angular.get('y', 0.0))
            twist.angular.z = float(angular.get('z', 0.0))
            
            self.cmd_vel_pub.publish(twist)
            
            self.get_logger().debug(f"Published cmd_vel: linear={twist.linear.x}, angular={twist.angular.z}")
            
        except Exception as e:
            self.get_logger().error(f"Error handling cmd_vel: {e}")
    
    def handle_navigation_goal(self, command_data: Dict[str, Any]):
        """Handle navigation goal"""
        try:
            if not self.nav_client.wait_for_server(timeout_sec=5.0):
                self.get_logger().error("Navigation server not available")
                return
            
            goal_msg = NavigateToPose.Goal()
            
            target_pose = command_data.get('target_pose', {})
            position = target_pose.get('position', {})
            orientation = target_pose.get('orientation', {})
            
            # Set target pose
            goal_msg.pose.header.frame_id = 'map'
            goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
            
            goal_msg.pose.pose.position.x = float(position.get('x', 0.0))
            goal_msg.pose.pose.position.y = float(position.get('y', 0.0))
            goal_msg.pose.pose.position.z = float(position.get('z', 0.0))
            
            goal_msg.pose.pose.orientation.x = float(orientation.get('x', 0.0))
            goal_msg.pose.pose.orientation.y = float(orientation.get('y', 0.0))
            goal_msg.pose.pose.orientation.z = float(orientation.get('z', 0.0))
            goal_msg.pose.pose.orientation.w = float(orientation.get('w', 1.0))
            
            # Send goal
            future = self.nav_client.send_goal_async(goal_msg)
            
            self.get_logger().info(f"Navigation goal sent: ({position.get('x')}, {position.get('y')})")
            
        except Exception as e:
            self.get_logger().error(f"Error handling navigation goal: {e}")
    
    def send_robot_status(self):
        """Send current robot status to bridge"""
        try:
            status_data = {
                'type': 'status',
                'status': 'active',
                'timestamp': time.time(),
                'robot_data': self.robot_data
            }
            
            self.send_to_bridge(status_data)
            
        except Exception as e:
            self.get_logger().error(f"Error sending robot status: {e}")
    
    def send_to_bridge(self, data: Dict[str, Any]):
        """Send data to bridge via ZMQ"""
        try:
            if self.data_socket:
                json_message = json.dumps(data)
                self.data_socket.send_string(json_message, zmq.NOBLOCK)
                self.get_logger().debug(f"Sent to bridge: {data['type']}")
        except zmq.Again:
            self.get_logger().warning("Failed to send to bridge (queue full)")
        except Exception as e:
            self.get_logger().error(f"Error sending to bridge: {e}")
    
    # ROS2 Callback functions
    def odom_callback(self, msg: Odometry):
        """Handle odometry updates"""
        try:
            self.robot_data['position'] = {
                'x': msg.pose.pose.position.x,
                'y': msg.pose.pose.position.y,
                'z': msg.pose.pose.position.z
            }
            
            self.robot_data['orientation'] = {
                'x': msg.pose.pose.orientation.x,
                'y': msg.pose.pose.orientation.y,
                'z': msg.pose.pose.orientation.z,
                'w': msg.pose.pose.orientation.w
            }
            
            self.robot_data['velocity'] = {
                'linear': msg.twist.twist.linear.x,
                'angular': msg.twist.twist.angular.z
            }
            
            # Send pose update to bridge
            pose_data = {
                'type': 'pose',
                'position': self.robot_data['position'],
                'orientation': self.robot_data['orientation'],
                'timestamp': time.time()
            }
            
            self.send_to_bridge(pose_data)
            
        except Exception as e:
            self.get_logger().error(f"Error in odom_callback: {e}")
    
    def lidar_callback(self, msg: LaserScan):
        """Handle LiDAR data"""
        try:
            # Process LiDAR data (simplified)
            min_range = min([r for r in msg.ranges if r > msg.range_min and r < msg.range_max])
            max_range = max([r for r in msg.ranges if r > msg.range_min and r < msg.range_max])
            
            self.robot_data['sensors']['lidar'] = {
                'min_range': min_range,
                'max_range': max_range,
                'angle_min': msg.angle_min,
                'angle_max': msg.angle_max,
                'range_count': len(msg.ranges)
            }
            
            # Send sensor data to bridge periodically (every 10th message)
            if hasattr(self, '_lidar_counter'):
                self._lidar_counter += 1
            else:
                self._lidar_counter = 1
            
            if self._lidar_counter % 10 == 0:
                sensor_data = {
                    'type': 'sensor_data',
                    'sensor_type': 'lidar',
                    'data': self.robot_data['sensors']['lidar'],
                    'timestamp': time.time()
                }
                self.send_to_bridge(sensor_data)
            
        except Exception as e:
            self.get_logger().error(f"Error in lidar_callback: {e}")
    
    def left_image_callback(self, msg: Image):
        """Handle left stereo camera image"""
        try:
            # Process camera data (metadata only for now)
            self.robot_data['sensors']['camera']['left'] = {
                'width': msg.width,
                'height': msg.height,
                'encoding': msg.encoding,
                'timestamp': msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
            }
            
        except Exception as e:
            self.get_logger().error(f"Error in left_image_callback: {e}")
    
    def right_image_callback(self, msg: Image):
        """Handle right stereo camera image"""
        try:
            # Process camera data (metadata only for now)
            self.robot_data['sensors']['camera']['right'] = {
                'width': msg.width,
                'height': msg.height,
                'encoding': msg.encoding,
                'timestamp': msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
            }
            
        except Exception as e:
            self.get_logger().error(f"Error in right_image_callback: {e}")
    
    def imu_callback(self, msg: Imu):
        """Handle IMU data"""
        try:
            self.robot_data['sensors']['imu'] = {
                'orientation': {
                    'x': msg.orientation.x,
                    'y': msg.orientation.y,
                    'z': msg.orientation.z,
                    'w': msg.orientation.w
                },
                'angular_velocity': {
                    'x': msg.angular_velocity.x,
                    'y': msg.angular_velocity.y,
                    'z': msg.angular_velocity.z
                },
                'linear_acceleration': {
                    'x': msg.linear_acceleration.x,
                    'y': msg.linear_acceleration.y,
                    'z': msg.linear_acceleration.z
                }
            }
            
            # Send IMU data periodically (every 50th message)
            if hasattr(self, '_imu_counter'):
                self._imu_counter += 1
            else:
                self._imu_counter = 1
            
            if self._imu_counter % 50 == 0:
                sensor_data = {
                    'type': 'sensor_data',
                    'sensor_type': 'imu',
                    'data': self.robot_data['sensors']['imu'],
                    'timestamp': time.time()
                }
                self.send_to_bridge(sensor_data)
            
        except Exception as e:
            self.get_logger().error(f"Error in imu_callback: {e}")
    
    def tf_callback(self, msg: TFMessage):
        """Handle TF transforms"""
        # Process coordinate transforms if needed
        pass
    
    def cleanup(self):
        """Clean up resources"""
        self.get_logger().info("Cleaning up ROS2 Bridge Node...")
        
        if self.command_socket:
            self.command_socket.close()
        if self.data_socket:
            self.data_socket.close()
        
        self.context.term()


def main():
    """Main entry point"""
    rclpy.init()
    
    try:
        node = ROS2BridgeNode()
        
        # Spin the node
        rclpy.spin(node)
        
    except KeyboardInterrupt:
        logger.info("Keyboard interrupt received")
    except Exception as e:
        logger.error(f"Error in main: {e}")
    finally:
        if 'node' in locals():
            node.cleanup()
        rclpy.shutdown()


if __name__ == '__main__':
    main()