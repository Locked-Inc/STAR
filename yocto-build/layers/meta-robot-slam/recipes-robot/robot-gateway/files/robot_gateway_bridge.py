#!/usr/bin/env python3
"""
Robot Gateway Bridge
Connects the existing Java Spring Boot Robot Gateway with ROS2 robot nodes
Provides seamless integration between REST API and ROS2 messaging
"""

import asyncio
import json
import logging
import signal
import sys
import time
import yaml
from typing import Dict, Any, Optional

import requests
import zmq
from flask import Flask, request, jsonify

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class RobotGatewayBridge:
    """Bridge between Java Robot Gateway and ROS2 robot system"""
    
    def __init__(self, config_path: str = "/etc/robot/gateway_config.yaml"):
        """Initialize the bridge with configuration"""
        self.config = self._load_config(config_path)
        self.running = False
        
        # Initialize Flask app for REST API
        self.app = Flask(__name__)
        self._setup_routes()
        
        # Initialize ZMQ for ROS2 communication
        self.context = zmq.Context()
        self.ros2_socket = None
        
        # Gateway connection settings
        self.gateway_url = self.config.get('gateway_url', 'http://localhost:8080')
        self.bridge_port = self.config.get('bridge_port', 9090)
        
        # Robot state
        self.robot_state = {
            'position': {'x': 0.0, 'y': 0.0, 'z': 0.0},
            'orientation': {'x': 0.0, 'y': 0.0, 'z': 0.0, 'w': 1.0},
            'velocity': {'linear': 0.0, 'angular': 0.0},
            'sensors': {},
            'status': 'idle'
        }
        
    def _load_config(self, config_path: str) -> Dict[str, Any]:
        """Load configuration from YAML file"""
        try:
            with open(config_path, 'r') as file:
                return yaml.safe_load(file)
        except FileNotFoundError:
            logger.warning(f"Config file not found: {config_path}, using defaults")
            return {}
        except Exception as e:
            logger.error(f"Error loading config: {e}")
            return {}
    
    def _setup_routes(self):
        """Set up Flask routes for REST API"""
        
        @self.app.route('/health', methods=['GET'])
        def health_check():
            """Health check endpoint"""
            return jsonify({
                'status': 'healthy',
                'bridge_version': '1.0.0',
                'ros2_connected': self.ros2_socket is not None,
                'timestamp': time.time()
            })
        
        @self.app.route('/robot/command', methods=['POST'])
        def robot_command():
            """Receive robot commands from gateway"""
            try:
                command_data = request.get_json()
                logger.info(f"Received command: {command_data}")
                
                # Process command and send to ROS2
                result = self._process_robot_command(command_data)
                
                return jsonify({
                    'status': 'success',
                    'result': result,
                    'timestamp': time.time()
                })
                
            except Exception as e:
                logger.error(f"Error processing command: {e}")
                return jsonify({
                    'status': 'error',
                    'message': str(e),
                    'timestamp': time.time()
                }), 500
        
        @self.app.route('/robot/state', methods=['GET'])
        def get_robot_state():
            """Get current robot state"""
            return jsonify({
                'status': 'success',
                'state': self.robot_state,
                'timestamp': time.time()
            })
        
        @self.app.route('/robot/sensors', methods=['GET'])
        def get_sensor_data():
            """Get current sensor data"""
            return jsonify({
                'status': 'success',
                'sensors': self.robot_state['sensors'],
                'timestamp': time.time()
            })
    
    def _process_robot_command(self, command_data: Dict[str, Any]) -> Dict[str, Any]:
        """Process robot command and send to ROS2"""
        command_type = command_data.get('type', 'unknown')
        
        if command_type == 'move':
            return self._handle_move_command(command_data)
        elif command_type == 'rotate':
            return self._handle_rotate_command(command_data)
        elif command_type == 'stop':
            return self._handle_stop_command(command_data)
        elif command_type == 'navigation':
            return self._handle_navigation_command(command_data)
        else:
            logger.warning(f"Unknown command type: {command_type}")
            return {'message': f'Unknown command type: {command_type}'}
    
    def _handle_move_command(self, command_data: Dict[str, Any]) -> Dict[str, Any]:
        """Handle movement command"""
        linear_vel = command_data.get('linear_velocity', 0.0)
        angular_vel = command_data.get('angular_velocity', 0.0)
        
        # Send to ROS2 via ZMQ
        ros2_message = {
            'type': 'cmd_vel',
            'linear': {'x': linear_vel, 'y': 0.0, 'z': 0.0},
            'angular': {'x': 0.0, 'y': 0.0, 'z': angular_vel}
        }
        
        self._send_to_ros2(ros2_message)
        
        # Update robot state
        self.robot_state['velocity'] = {
            'linear': linear_vel,
            'angular': angular_vel
        }
        self.robot_state['status'] = 'moving'
        
        return {'message': 'Move command sent successfully'}
    
    def _handle_rotate_command(self, command_data: Dict[str, Any]) -> Dict[str, Any]:
        """Handle rotation command"""
        angular_vel = command_data.get('angular_velocity', 0.0)
        
        ros2_message = {
            'type': 'cmd_vel',
            'linear': {'x': 0.0, 'y': 0.0, 'z': 0.0},
            'angular': {'x': 0.0, 'y': 0.0, 'z': angular_vel}
        }
        
        self._send_to_ros2(ros2_message)
        
        self.robot_state['velocity']['angular'] = angular_vel
        self.robot_state['status'] = 'rotating'
        
        return {'message': 'Rotate command sent successfully'}
    
    def _handle_stop_command(self, command_data: Dict[str, Any]) -> Dict[str, Any]:
        """Handle stop command"""
        ros2_message = {
            'type': 'cmd_vel',
            'linear': {'x': 0.0, 'y': 0.0, 'z': 0.0},
            'angular': {'x': 0.0, 'y': 0.0, 'z': 0.0}
        }
        
        self._send_to_ros2(ros2_message)
        
        self.robot_state['velocity'] = {'linear': 0.0, 'angular': 0.0}
        self.robot_state['status'] = 'stopped'
        
        return {'message': 'Stop command sent successfully'}
    
    def _handle_navigation_command(self, command_data: Dict[str, Any]) -> Dict[str, Any]:
        """Handle navigation command"""
        target_x = command_data.get('target_x', 0.0)
        target_y = command_data.get('target_y', 0.0)
        
        ros2_message = {
            'type': 'nav_goal',
            'target_pose': {
                'position': {'x': target_x, 'y': target_y, 'z': 0.0},
                'orientation': {'x': 0.0, 'y': 0.0, 'z': 0.0, 'w': 1.0}
            }
        }
        
        self._send_to_ros2(ros2_message)
        
        self.robot_state['status'] = 'navigating'
        
        return {'message': f'Navigation goal set to ({target_x}, {target_y})'}
    
    def _send_to_ros2(self, message: Dict[str, Any]):
        """Send message to ROS2 bridge node via ZMQ"""
        if self.ros2_socket:
            try:
                json_message = json.dumps(message)
                self.ros2_socket.send_string(json_message, zmq.NOBLOCK)
                logger.debug(f"Sent to ROS2: {json_message}")
            except zmq.Again:
                logger.warning("Failed to send message to ROS2 (queue full)")
            except Exception as e:
                logger.error(f"Error sending to ROS2: {e}")
    
    def _setup_ros2_connection(self):
        """Set up ZMQ connection to ROS2 bridge node"""
        try:
            self.ros2_socket = self.context.socket(zmq.PUSH)
            zmq_address = self.config.get('ros2_zmq_address', 'tcp://localhost:5555')
            self.ros2_socket.connect(zmq_address)
            logger.info(f"Connected to ROS2 bridge at {zmq_address}")
        except Exception as e:
            logger.error(f"Failed to connect to ROS2 bridge: {e}")
            self.ros2_socket = None
    
    def _forward_to_gateway(self, data: Dict[str, Any]):
        """Forward robot data to the Java gateway"""
        try:
            endpoint = f"{self.gateway_url}/api/robot/telemetry"
            response = requests.post(endpoint, json=data, timeout=5)
            
            if response.status_code == 200:
                logger.debug("Data forwarded to gateway successfully")
            else:
                logger.warning(f"Gateway responded with status {response.status_code}")
                
        except requests.exceptions.RequestException as e:
            logger.warning(f"Failed to forward data to gateway: {e}")
    
    async def _monitor_ros2_data(self):
        """Monitor incoming data from ROS2 nodes"""
        # Set up subscriber socket for ROS2 data
        sub_socket = self.context.socket(zmq.PULL)
        zmq_sub_address = self.config.get('ros2_sub_address', 'tcp://localhost:5556')
        
        try:
            sub_socket.bind(zmq_sub_address)
            logger.info(f"Listening for ROS2 data on {zmq_sub_address}")
            
            # Set up poller for non-blocking receive
            poller = zmq.Poller()
            poller.register(sub_socket, zmq.POLLIN)
            
            while self.running:
                try:
                    socks = dict(poller.poll(timeout=1000))  # 1 second timeout
                    
                    if sub_socket in socks:
                        message = sub_socket.recv_string(zmq.NOBLOCK)
                        data = json.loads(message)
                        
                        # Update robot state
                        self._update_robot_state(data)
                        
                        # Forward to Java gateway
                        self._forward_to_gateway(data)
                        
                except zmq.Again:
                    continue  # Timeout, continue loop
                except Exception as e:
                    logger.error(f"Error receiving ROS2 data: {e}")
                
                await asyncio.sleep(0.1)  # Small delay to prevent busy loop
                
        except Exception as e:
            logger.error(f"Error in ROS2 data monitor: {e}")
        finally:
            sub_socket.close()
    
    def _update_robot_state(self, data: Dict[str, Any]):
        """Update robot state with incoming ROS2 data"""
        data_type = data.get('type', 'unknown')
        
        if data_type == 'pose':
            if 'position' in data:
                self.robot_state['position'] = data['position']
            if 'orientation' in data:
                self.robot_state['orientation'] = data['orientation']
                
        elif data_type == 'velocity':
            self.robot_state['velocity'] = data.get('velocity', self.robot_state['velocity'])
            
        elif data_type == 'sensor_data':
            sensor_type = data.get('sensor_type', 'unknown')
            self.robot_state['sensors'][sensor_type] = data.get('data', {})
            
        elif data_type == 'status':
            self.robot_state['status'] = data.get('status', self.robot_state['status'])
    
    def start(self):
        """Start the bridge service"""
        logger.info("Starting Robot Gateway Bridge...")
        self.running = True
        
        # Set up ROS2 connection
        self._setup_ros2_connection()
        
        # Start ROS2 data monitoring in background
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
        # Run both the Flask app and ROS2 monitor
        import threading
        
        def run_flask():
            self.app.run(
                host='0.0.0.0',
                port=self.bridge_port,
                debug=False,
                threaded=True
            )
        
        def run_ros2_monitor():
            loop.run_until_complete(self._monitor_ros2_data())
        
        # Start threads
        flask_thread = threading.Thread(target=run_flask, daemon=True)
        ros2_thread = threading.Thread(target=run_ros2_monitor, daemon=True)
        
        flask_thread.start()
        ros2_thread.start()
        
        logger.info(f"Bridge started on port {self.bridge_port}")
        logger.info(f"Gateway URL: {self.gateway_url}")
        
        try:
            # Keep main thread alive
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("Shutting down bridge...")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the bridge service"""
        logger.info("Stopping Robot Gateway Bridge...")
        self.running = False
        
        if self.ros2_socket:
            self.ros2_socket.close()
        
        self.context.term()


def main():
    """Main entry point"""
    # Set up signal handling
    def signal_handler(signum, frame):
        logger.info("Received shutdown signal")
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create and start bridge
    bridge = RobotGatewayBridge()
    bridge.start()


if __name__ == "__main__":
    main()