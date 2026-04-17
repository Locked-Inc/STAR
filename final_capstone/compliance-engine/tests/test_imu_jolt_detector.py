"""Unit tests for the BNO055 z-accel jolt detector."""

from __future__ import annotations

import math

import numpy as np
import pytest

from star_compliance.detectors.imu_jolt_detector import (
    GRAVITY_MS2,
    ImuJoltDetector,
    gravity_compensated_z,
)


def test_gravity_compensated_z_on_level_robot():
    # Identity quaternion, measured az = 9.80665 (pure gravity)
    # -> compensated z should be ~0.
    compensated = gravity_compensated_z(0.0, 0.0, 0.0, 1.0,
                                        0.0, 0.0, GRAVITY_MS2)
    assert abs(compensated) < 1e-6


def test_jolt_below_threshold_produces_no_event():
    det = ImuJoltDetector(threshold_ms2=2.0, min_burst_samples=5)
    det.start_watch(now_sec=0.0)
    for i in range(20):
        event = det.feed(
            t_sec=0.01 * i,
            qx=0.0, qy=0.0, qz=0.0, qw=1.0,
            ax=0.0, ay=0.0, az=GRAVITY_MS2 + 1.0,  # constant 1.0 ms^2 offset
            robot_xy=(0.1 * i, 0.0),
            velocity_mps=0.3,
        )
        assert event is None


def test_spike_above_threshold_triggers_event():
    det = ImuJoltDetector(threshold_ms2=2.0, min_burst_samples=3)
    det.start_watch(now_sec=0.0)

    # Feed 20 quiet samples to establish a baseline
    for i in range(20):
        det.feed(
            t_sec=0.005 * i,
            qx=0.0, qy=0.0, qz=0.0, qw=1.0,
            ax=0.0, ay=0.0, az=GRAVITY_MS2,  # exactly gravity
            robot_xy=(0.1 * i, 0.0),
            velocity_mps=0.3,
        )

    # Inject 5 spike samples, each 3 ms^2 above gravity
    last_event = None
    for i in range(5):
        last_event = det.feed(
            t_sec=0.005 * (20 + i),
            qx=0.0, qy=0.0, qz=0.0, qw=1.0,
            ax=0.0, ay=0.0, az=GRAVITY_MS2 + 3.0,
            robot_xy=(2.0 + 0.1 * i, 0.0),
            velocity_mps=0.3,
        )
        if last_event:
            break

    assert last_event is not None
    assert last_event.jolt_magnitude_ms2 >= 2.5
    assert 2.0 <= last_event.robot_x_m <= 2.5


def test_watch_window_expires():
    det = ImuJoltDetector(watch_duration_sec=0.5)
    det.start_watch(now_sec=0.0)
    assert det.in_watch_window(0.4) is True
    assert det.in_watch_window(0.6) is False


def test_no_event_outside_watch_window():
    det = ImuJoltDetector(watch_duration_sec=0.5)
    det.start_watch(now_sec=0.0)
    # Spike at t=1.0 - well past the 0.5 s watch window
    event = det.feed(
        t_sec=1.0,
        qx=0.0, qy=0.0, qz=0.0, qw=1.0,
        ax=0.0, ay=0.0, az=GRAVITY_MS2 + 10.0,
        robot_xy=(1.0, 0.0),
        velocity_mps=0.3,
    )
    assert event is None


def test_event_consumes_watch_to_prevent_double_fire():
    det = ImuJoltDetector(threshold_ms2=2.0, min_burst_samples=3)
    det.start_watch(now_sec=0.0)

    # Burn in some quiet data
    for i in range(10):
        det.feed(0.005 * i, 0, 0, 0, 1,
                 0.0, 0.0, GRAVITY_MS2, (0, 0), 0.3)

    # Spike to fire the first event
    for i in range(5):
        det.feed(0.005 * (10 + i), 0, 0, 0, 1,
                 0.0, 0.0, GRAVITY_MS2 + 3.0,
                 (1.0 + 0.1 * i, 0.0), 0.3)

    # After firing, the watch should be consumed; subsequent spikes
    # should NOT produce another event until start_watch is called again.
    post_event = det.feed(0.1, 0, 0, 0, 1,
                          0.0, 0.0, GRAVITY_MS2 + 5.0, (2, 0), 0.3)
    assert post_event is None
