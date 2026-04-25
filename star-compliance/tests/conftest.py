"""Shared pytest fixtures for compliance-engine tests.

Keeps the import path working when running via `pytest` from the
compliance-engine directory without a full ROS2 workspace build.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Put the package root on sys.path so `from star_compliance import ...`
# works from within tests/ directly.
_HERE = Path(__file__).resolve().parent
_ROOT = _HERE.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))
