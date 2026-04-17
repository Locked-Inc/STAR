"""
BNO055 z-axis jolt detector for ADA 404.2.5 door-threshold presence.

Subscribes (via its caller) to `/imu/data` at 200 Hz and `/odom` at
100 Hz. Maintains a rolling 500 ms window of gravity-compensated
z-accel samples. A "jolt" is >=5 consecutive samples (25 ms burst)
whose z-accel magnitude exceeds `threshold_ms2` above the rolling
mean.

STAR reports threshold PRESENCE only; precise height is not measured.
The disclosure is mandatory on the PDF audit report.
"""

from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass

import numpy as np


DEFAULT_WINDOW_SEC = 0.5
DEFAULT_THRESHOLD_MS2 = 2.0
DEFAULT_MIN_BURST_SAMPLES = 5
DEFAULT_WATCH_DURATION_SEC = 2.0
GRAVITY_MS2 = 9.80665


@dataclass
class JoltEvent:
    """Payload emitted when a jolt is confirmed."""
    timestamp_sec: float
    robot_x_m: float
    robot_y_m: float
    jolt_magnitude_ms2: float
    traversal_velocity_mps: float


def gravity_compensated_z(qx: float, qy: float, qz: float, qw: float,
                          ax: float, ay: float, az: float) -> float:
    """Return the z-axis linear acceleration with gravity subtracted.

    Rotates the world-frame gravity vector (0, 0, g) into the sensor
    frame using the conjugate quaternion, then subtracts it from the
    measured accel vector's z-axis.
    """
    cx, cy, cz = -qx, -qy, -qz
    vx, vy, vz = 0.0, 0.0, GRAVITY_MS2

    tx = 2.0 * (cy * vz - cz * vy)
    ty = 2.0 * (cz * vx - cx * vz)
    tz = 2.0 * (cx * vy - cy * vx)

    gz_sensor = vz + qw * tz + (cx * ty - cy * tx)
    return az - gz_sensor


class ImuJoltDetector:
    """Streaming z-accel jolt detector with watch-window gating.

    Usage (driven by a ROS2 node):

        detector = ImuJoltDetector()
        detector.start_watch(now)
        for (t, qx,qy,qz,qw, ax,ay,az, odom_xy, velocity) in imu_stream:
            event = detector.feed(t, qx, qy, qz, qw, ax, ay, az,
                                  odom_xy, velocity)
            if event:
                # fire a ThresholdMeasurement
                ...
    """

    def __init__(self,
                 window_sec: float = DEFAULT_WINDOW_SEC,
                 threshold_ms2: float = DEFAULT_THRESHOLD_MS2,
                 min_burst_samples: int = DEFAULT_MIN_BURST_SAMPLES,
                 watch_duration_sec: float = DEFAULT_WATCH_DURATION_SEC):
        self.window_sec = window_sec
        self.threshold_ms2 = threshold_ms2
        self.min_burst_samples = min_burst_samples
        self.watch_duration_sec = watch_duration_sec
        self._samples: deque[tuple[float, float]] = deque()  # (t, gz)
        self._burst_count: int = 0
        self._burst_peak: float = 0.0
        self._watch_active_until: float = -math.inf

    # -----------------------------------------------------------------

    def start_watch(self, now_sec: float) -> None:
        self._watch_active_until = now_sec + self.watch_duration_sec
        self._burst_count = 0
        self._burst_peak = 0.0
        self._samples.clear()

    def in_watch_window(self, now_sec: float) -> bool:
        return now_sec <= self._watch_active_until

    def feed(self,
             t_sec: float,
             qx: float, qy: float, qz: float, qw: float,
             ax: float, ay: float, az: float,
             robot_xy: tuple[float, float],
             velocity_mps: float) -> JoltEvent | None:
        """Feed one IMU sample; return a JoltEvent when a burst completes."""
        if not self.in_watch_window(t_sec):
            return None

        gz = gravity_compensated_z(qx, qy, qz, qw, ax, ay, az)
        self._samples.append((t_sec, gz))
        cutoff = t_sec - self.window_sec
        while self._samples and self._samples[0][0] < cutoff:
            self._samples.popleft()

        if len(self._samples) < self.min_burst_samples:
            return None

        # Baseline as the mean of the window EXCLUDING the current sample
        # so a sustained spike does not silently raise the baseline.
        arr = np.fromiter((v for _, v in self._samples), dtype=np.float64)
        baseline = float(np.mean(arr[:-1])) if arr.size > 1 else 0.0

        deviation = abs(gz - baseline)
        if deviation > self.threshold_ms2:
            self._burst_count += 1
            self._burst_peak = max(self._burst_peak, deviation)
            if self._burst_count >= self.min_burst_samples:
                event = JoltEvent(
                    timestamp_sec=t_sec,
                    robot_x_m=robot_xy[0],
                    robot_y_m=robot_xy[1],
                    jolt_magnitude_ms2=self._burst_peak,
                    traversal_velocity_mps=velocity_mps,
                )
                # consume the watch so we do not double-fire on the
                # same threshold
                self._watch_active_until = -math.inf
                self._burst_count = 0
                self._burst_peak = 0.0
                return event
        else:
            self._burst_count = 0
            self._burst_peak = 0.0
        return None
