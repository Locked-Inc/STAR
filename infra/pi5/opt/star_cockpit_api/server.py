#!/usr/bin/env python3
"""HTTP API backing the Grafana cockpit controls.

  GET  /api/state            -> {"autonomy": bool|null, "estop": bool|null, "speed": float|null}
  POST /api/autonomy/toggle  -> flips /star/autonomy_enable, returns new state
  POST /api/estop/toggle     -> flips /star/estop, returns new state
  POST /api/speed  body: {"value": 0.75}  -> publishes /star/speed_scale, returns applied

Binds to 127.0.0.1:9102; Caddy fronts it as https://star.local/api/*
State is learned from the supervisor's latched /star/state/* topics.
"""
import json
import subprocess
import re
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, DurabilityPolicy, ReliabilityPolicy, HistoryPolicy
from std_msgs.msg import Bool, Float32
from geometry_msgs.msg import Twist

PORT = 9102
LATCHED = QoSProfile(
    history=HistoryPolicy.KEEP_LAST, depth=1,
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
)


class CockpitNode(Node):
    def __init__(self):
        super().__init__("star_cockpit_api")
        self._lock = threading.Lock()
        self._autonomy = None
        self._estop = None
        self._last_speed = None
        self._slam_state_cached = None
        self._autonomy_pub = self.create_publisher(Bool, "/star/autonomy_enable", 10)
        self._estop_pub = self.create_publisher(Bool, "/star/estop", 10)
        self._speed_pub = self.create_publisher(Float32, "/star/speed_scale", 10)
        self._cmd_vel_pub = self.create_publisher(Twist, "/cmd_vel", 10)
        self.create_subscription(Bool, "/star/state/autonomy_enable", self._on_auto, LATCHED)
        self.create_subscription(Bool, "/star/state/estop", self._on_estop, LATCHED)
        self.get_logger().info(f"cockpit HTTP on 127.0.0.1:{PORT}")

    def _on_auto(self, m):
        with self._lock: self._autonomy = bool(m.data)

    def _on_estop(self, m):
        with self._lock: self._estop = bool(m.data)

    def get_state(self):
        with self._lock:
            return {"autonomy": self._autonomy, "estop": self._estop, "speed": self._last_speed, "slam": self.slam_state()}

    def toggle_autonomy(self):
        with self._lock:
            new = not bool(self._autonomy)
        self._autonomy_pub.publish(Bool(data=new))
        return new

    def toggle_estop(self):
        with self._lock:
            new = not bool(self._estop)
        self._estop_pub.publish(Bool(data=new))
        return new

    def slam_lifecycle(self, transition_id):
        try:
            r = subprocess.run(
                ["ros2","service","call","/slam_toolbox/change_state",
                 "lifecycle_msgs/srv/ChangeState",
                 "{transition: {id: %d}}" % transition_id],
                capture_output=True, text=True, timeout=15)
            ok = "success=True" in r.stdout
            self._refresh_slam_state()
            return ok
        except Exception:
            return False

    def slam_state(self):
        return self._slam_state_cached

    def _refresh_slam_state(self):
        try:
            r = subprocess.run(
                ["ros2","service","call","/slam_toolbox/get_state",
                 "lifecycle_msgs/srv/GetState"],
                capture_output=True, text=True, timeout=5)
            m = re.search(r"label=\'([a-z_]+)\'", r.stdout)
            new = m.group(1) if m else None
            with self._lock:
                self._slam_state_cached = new
        except Exception:
            pass

    def set_cmd_vel(self, linear, angular):
        m = Twist(); m.linear.x = float(linear); m.angular.z = float(angular)
        self._cmd_vel_pub.publish(m)
        return {"linear": m.linear.x, "angular": m.angular.z}

    def set_speed(self, v):
        v = max(0.0, min(1.0, float(v)))
        with self._lock:
            self._last_speed = v
        self._speed_pub.publish(Float32(data=v))
        return v


NODE = None  # set in main


class H(BaseHTTPRequestHandler):
    def log_message(self, *a, **k): pass

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self._json(204, {})

    def do_GET(self):
        if self.path == "/api/state":
            return self._json(200, NODE.get_state())
        self._json(404, {"error": "not found"})

    def do_POST(self):
        if self.path == "/api/autonomy/toggle":
            return self._json(200, {"autonomy": NODE.toggle_autonomy()})
        if self.path == "/api/estop/toggle":
            return self._json(200, {"estop": NODE.toggle_estop()})
        if self.path == "/api/slam/start":
            ok = NODE.slam_lifecycle(3)
            if not ok:
                NODE.slam_lifecycle(1)
                ok = NODE.slam_lifecycle(3)
            return self._json(200 if ok else 500, {"slam": NODE.slam_state(), "ok": ok})
        if self.path == "/api/slam/stop":
            ok = NODE.slam_lifecycle(4)
            return self._json(200 if ok else 500, {"slam": NODE.slam_state(), "ok": ok})
        if self.path == "/api/cmd_vel":
            n = int(self.headers.get("Content-Length", 0) or 0)
            try:
                body = json.loads(self.rfile.read(n).decode() or "{}")
                lin = float(body.get("linear", 0))
                ang = float(body.get("angular", 0))
            except Exception as e:
                return self._json(400, {"error": f"bad body: {e}"})
            return self._json(200, NODE.set_cmd_vel(lin, ang))
        if self.path == "/api/speed":
            n = int(self.headers.get("Content-Length", 0) or 0)
            try:
                v = float(json.loads(self.rfile.read(n).decode() or "{}").get("value"))
            except Exception as e:
                return self._json(400, {"error": f"bad body: {e}"})
            return self._json(200, {"speed": NODE.set_speed(v)})
        self._json(404, {"error": "not found"})


def _slam_refresher_loop():
    import time
    while True:
        try:
            if NODE is not None:
                NODE._refresh_slam_state()
        except Exception:
            pass
        time.sleep(4)

def main():
    global NODE
    rclpy.init()
    NODE = CockpitNode()
    srv = ThreadingHTTPServer(("100.64.0.6", PORT), H)
    threading.Thread(target=srv.serve_forever, name="cockpit-http", daemon=True).start()
    threading.Thread(target=_slam_refresher_loop, name="slam-refresh", daemon=True).start()
    try:
        rclpy.spin(NODE)
    finally:
        NODE.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
