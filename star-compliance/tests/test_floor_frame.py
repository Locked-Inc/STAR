"""Unit tests for the floor-plane estimator."""

from __future__ import annotations

import math

import numpy as np
import pytest

from star_compliance.engines.floor_frame import (
    FloorEstimate,
    floor_from_bno055_quaternion,
)


def test_level_robot_gives_vertical_normal():
    # Identity quaternion = no rotation. +Z stays +Z.
    est = floor_from_bno055_quaternion(0.0, 0.0, 0.0, 1.0,
                                       sensor_height_m=0.25,
                                       now_sec=1.0)
    assert abs(est.nx) < 1e-6
    assert abs(est.ny) < 1e-6
    assert abs(est.nz - 1.0) < 1e-6
    assert est.is_vertical_enough()


def test_height_above_floor_matches_sensor_height():
    est = floor_from_bno055_quaternion(0.0, 0.0, 0.0, 1.0,
                                       sensor_height_m=0.25,
                                       now_sec=0.0)
    # Point at z=0 on the robot floor origin is 0 m above floor
    assert abs(est.height_above_floor(0.0, 0.0, 0.0) - 0.25) < 1e-9
    # Point at z=1.5 is 1.75 above the floor (robot sensor is 25 cm up)
    assert abs(est.height_above_floor(0.0, 0.0, 1.5) - 1.75) < 1e-9


def test_tilted_robot_tilts_floor_normal():
    # Pitch the robot forward 10 degrees (rotation about Y-axis)
    pitch_rad = math.radians(10)
    qx = 0.0
    qy = math.sin(pitch_rad / 2)
    qz = 0.0
    qw = math.cos(pitch_rad / 2)
    est = floor_from_bno055_quaternion(qx, qy, qz, qw,
                                       sensor_height_m=0.25,
                                       now_sec=0.0)
    # Normal should have some X component (pitch forward tilts floor in x)
    assert abs(est.nx) > 0.15
    # nz should drop to about cos(10 deg)
    assert abs(est.nz - math.cos(pitch_rad)) < 0.01
    assert est.is_vertical_enough()


def test_heavily_tilted_robot_fails_vertical_check():
    # Pitch 30 degrees - past the 18 degree threshold
    pitch_rad = math.radians(30)
    qx = 0.0
    qy = math.sin(pitch_rad / 2)
    qz = 0.0
    qw = math.cos(pitch_rad / 2)
    est = floor_from_bno055_quaternion(qx, qy, qz, qw,
                                       sensor_height_m=0.25,
                                       now_sec=0.0)
    assert not est.is_vertical_enough()


def test_age_sec_is_propagated():
    est = floor_from_bno055_quaternion(0.0, 0.0, 0.0, 1.0,
                                       sensor_height_m=0.25,
                                       now_sec=42.5)
    assert est.age_sec == 42.5
