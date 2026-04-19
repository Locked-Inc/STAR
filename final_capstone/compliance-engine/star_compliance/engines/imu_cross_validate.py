"""Cross-validation helper: compare an IMU pitch window to a LiDAR-
plane-normal slope estimate.
"""

from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass

import numpy as np


@dataclass
class ImuWindow:
    """Summarized pitch over a rolling IMU window."""
    pitch_deg: float
    n: int
    age_sec: float


class ImuPitchWindow:
    """Rolling window of IMU pitch samples.

    Intended to be fed by the BNO055 stream published at 200 Hz on the
    RX72N side and forwarded into ROS2 as /imu/data by
    star_spi_bridge.
    """

    def __init__(self, window_sec: float = 0.5):
        self.window_sec = window_sec
        self._samples: deque[tuple[float, float]] = deque()  # (t, pitch_deg)

    def push(self, t: float, pitch_deg: float) -> None:
        self._samples.append((t, pitch_deg))
        cutoff = t - self.window_sec
        while self._samples and self._samples[0][0] < cutoff:
            self._samples.popleft()

    def summary(self, now: float) -> ImuWindow | None:
        if not self._samples:
            return None
        values = np.array([v for _, v in self._samples])
        newest = self._samples[-1][0]
        oldest = self._samples[0][0]
        return ImuWindow(
            pitch_deg=float(values.mean()),
            n=int(values.size),
            age_sec=float(now - oldest),
        )


def quaternion_to_pitch_deg(qx: float, qy: float, qz: float, qw: float) -> float:
    """Extract pitch (rotation about the y-axis, ENU convention) from a
    unit quaternion and return it in degrees.
    """
    sin_pitch = 2.0 * (qw * qy - qz * qx)
    sin_pitch = max(-1.0, min(1.0, sin_pitch))
    return math.degrees(math.asin(sin_pitch))


def agreement_within(deg_a: float, deg_b: float, gate_deg: float) -> bool:
    """Return True if |a - b| <= gate. Defined here so it's trivially
    unit-testable and the gate value is never scattered through code.
    """
    return abs(deg_a - deg_b) <= gate_deg
