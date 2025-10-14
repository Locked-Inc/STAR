"""
STAR-Z2 Base Overlay
Custom overlay for STAR robot with LiDAR SLAM and stereo vision capabilities
"""

from pynq import DefaultOverlay

class BaseOverlay(DefaultOverlay):
    """Custom overlay for STAR-Z2 robot platform

    This overlay provides interfaces for:
    - LiDAR sensor communication
    - Stereo camera processing
    - Motor control
    - Network communication
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def __repr__(self):
        return "STAR-Z2 Base Overlay"
