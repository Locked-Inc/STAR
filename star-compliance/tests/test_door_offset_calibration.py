"""Unit tests for the door-offset calibration module."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from star_compliance.engines.door_offset_calibration import (
    DEFAULT_DOOR_THICKNESS_M,
    DEFAULT_STOP_DEPTH_M,
    DEFAULT_HINGE_OFFSET_M,
    DoorOffset,
    OffsetCalibration,
    apply_offset,
)


def test_default_offset_is_2_point_5_inches():
    # 1.75 + 0.5 + 0.25 inches = 2.5 inches = 0.0635 m
    total = DoorOffset().total()
    assert abs(total - 0.0635) < 1e-9


def test_apply_offset_is_straight_subtraction():
    frame_width = 0.9144   # 36 in
    offset = DoorOffset()
    ada = apply_offset(frame_width, offset)
    assert abs(ada - (0.9144 - 0.0635)) < 1e-9


def test_default_constants_match_inch_math():
    inch = 0.0254
    assert abs(DEFAULT_DOOR_THICKNESS_M - 1.75 * inch) < 1e-12
    assert abs(DEFAULT_STOP_DEPTH_M - 0.50 * inch) < 1e-12
    assert abs(DEFAULT_HINGE_OFFSET_M - 0.25 * inch) < 1e-12


def test_offset_calibration_default_lookup():
    cal = OffsetCalibration()
    assert cal.for_style(None).source == "default"
    assert cal.for_style("does-not-exist").source == "default"


def test_offset_calibration_style_override():
    custom = DoorOffset(
        door_thickness_m=0.04,
        stop_depth_m=0.013,
        hinge_offset_m=0.005,
        source="thin-interior",
    )
    cal = OffsetCalibration(styles={"thin-interior": custom})
    got = cal.for_style("thin-interior")
    assert got is custom


def test_from_json_loads_defaults_and_styles(tmp_path: Path):
    data = {
        "default": {
            "door_thickness_m": 0.045,
            "stop_depth_m": 0.012,
            "hinge_offset_m": 0.006,
        },
        "styles": {
            "metal-commercial": {
                "door_thickness_m": 0.055,
                "stop_depth_m": 0.015,
                "hinge_offset_m": 0.008,
            },
        },
    }
    path = tmp_path / "doors.json"
    path.write_text(json.dumps(data))
    cal = OffsetCalibration.from_json(path)
    assert cal.default_offset.door_thickness_m == 0.045
    assert cal.for_style("metal-commercial").stop_depth_m == 0.015
    assert cal.for_style("metal-commercial").total() == (0.055 + 0.015 + 0.008)


def test_from_json_falls_back_on_missing_keys(tmp_path: Path):
    data = {
        "default": {"door_thickness_m": 0.0445},
        "styles": {"bare": {}},
    }
    path = tmp_path / "partial.json"
    path.write_text(json.dumps(data))
    cal = OffsetCalibration.from_json(path)
    assert cal.default_offset.door_thickness_m == 0.0445
    assert cal.default_offset.stop_depth_m == DEFAULT_STOP_DEPTH_M
    assert cal.for_style("bare").door_thickness_m == DEFAULT_DOOR_THICKNESS_M
