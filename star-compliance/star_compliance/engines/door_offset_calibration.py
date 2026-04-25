"""
Door-thickness + stop-height + hinge-offset calibration.

The ADA 2010 Standards for Accessible Design, Section 404.2.3,
requires door clear width to be measured "between the face of the
door and the stop" with the door open 90 degrees. STAR cannot
physically open a door, and the door stop (typically 3/8 to 1/2 inch
high) falls below every onboard sensor's effective resolution. We
therefore apply a calibrated offset to the frame-opening width to
estimate ADA clear width.

This module holds the calibration constants, provides a simple JSON
loader for per-deployment overrides, and documents the math for
auditors. The PDF generator must echo whatever offset was applied.

Defaults (a typical U.S. residential/commercial swing door):

- door_thickness_m  = 1.75 inch = 0.04445 m
- stop_depth_m      = 0.50 inch = 0.01270 m
- hinge_offset_m    = 0.25 inch = 0.00635 m
- total_offset_m    = 2.50 inch = 0.06350 m

So `ada_clear_width_m = frame_width_m - total_offset_m`.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path


INCH_TO_M = 0.0254

DEFAULT_DOOR_THICKNESS_M = 1.75 * INCH_TO_M
DEFAULT_STOP_DEPTH_M = 0.50 * INCH_TO_M
DEFAULT_HINGE_OFFSET_M = 0.25 * INCH_TO_M


@dataclass
class DoorOffset:
    """Per-door-style offset that gets subtracted from frame width."""
    door_thickness_m: float = DEFAULT_DOOR_THICKNESS_M
    stop_depth_m: float = DEFAULT_STOP_DEPTH_M
    hinge_offset_m: float = DEFAULT_HINGE_OFFSET_M
    source: str = "default"

    def total(self) -> float:
        return self.door_thickness_m + self.stop_depth_m + self.hinge_offset_m

    def as_dict(self) -> dict:
        return {
            "door_thickness_m": self.door_thickness_m,
            "stop_depth_m": self.stop_depth_m,
            "hinge_offset_m": self.hinge_offset_m,
            "total_offset_m": self.total(),
            "source": self.source,
        }


@dataclass
class OffsetCalibration:
    """A named collection of door offsets (per-style, per-deployment)."""
    default_offset: DoorOffset = field(default_factory=DoorOffset)
    styles: dict[str, DoorOffset] = field(default_factory=dict)

    def for_style(self, style_key: str | None) -> DoorOffset:
        if style_key is None:
            return self.default_offset
        return self.styles.get(style_key, self.default_offset)

    def as_dict(self) -> dict:
        return {
            "default": self.default_offset.as_dict(),
            "styles": {k: v.as_dict() for k, v in self.styles.items()},
        }

    @classmethod
    def from_json(cls, path: str | Path) -> "OffsetCalibration":
        """Load a calibration JSON. Schema:
        {
          "default": {"door_thickness_m": 0.04445, ...},
          "styles": {
             "narrow-residential": {...},
             "commercial-hollow-metal": {...}
          }
        }
        Missing keys fall back to DEFAULT_* constants.
        """
        data = json.loads(Path(path).read_text())
        default = _offset_from_dict(data.get("default", {}),
                                    source=f"json:{path}:default")
        styles_in = data.get("styles", {}) or {}
        styles = {
            key: _offset_from_dict(val, source=f"json:{path}:{key}")
            for key, val in styles_in.items()
        }
        return cls(default_offset=default, styles=styles)


def _offset_from_dict(d: dict, source: str) -> DoorOffset:
    return DoorOffset(
        door_thickness_m=d.get("door_thickness_m", DEFAULT_DOOR_THICKNESS_M),
        stop_depth_m=d.get("stop_depth_m", DEFAULT_STOP_DEPTH_M),
        hinge_offset_m=d.get("hinge_offset_m", DEFAULT_HINGE_OFFSET_M),
        source=source,
    )


def apply_offset(frame_width_m: float, offset: DoorOffset) -> float:
    """Compute ADA-adjusted clear width from a measured frame opening."""
    return frame_width_m - offset.total()
