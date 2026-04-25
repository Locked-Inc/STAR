"""ament_python setup for the STAR ADA compliance engine."""

from pathlib import Path

from setuptools import find_packages, setup

PACKAGE_NAME = "star_compliance"

HERE = Path(__file__).parent


setup(
    name=PACKAGE_NAME,
    version="0.1.0",
    packages=find_packages(include=[PACKAGE_NAME, f"{PACKAGE_NAME}.*"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + PACKAGE_NAME]),
        (f"share/{PACKAGE_NAME}", ["package.xml"]),
        (f"share/{PACKAGE_NAME}/launch",
         ["star_compliance/bringup/compliance.launch.py"]),
        (f"share/{PACKAGE_NAME}/models",
         ["star_compliance/models/README.md"]),
    ],
    install_requires=[
        "setuptools",
        "numpy",
        "scikit-image",
        "scikit-learn",
        "reportlab",
    ],
    zip_safe=True,
    maintainer="Team Locked Inc.",
    maintainer_email="locked-inc@tamu.edu",
    description="STAR ADA compliance engine - ROS2 nodes and detectors.",
    license="MIT",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "ramp_slope_node = star_compliance.nodes.ramp_slope_node:main",
            "door_clear_width_node = star_compliance.nodes.door_clear_width_node:main",
            "door_threshold_node = star_compliance.nodes.door_threshold_node:main",
            "protruding_objects_node = star_compliance.nodes.protruding_objects_node:main",
            "path_blockage_node = star_compliance.nodes.path_blockage_node:main",
            "dynamic_obstacle_node = star_compliance.nodes.dynamic_obstacle_node:main",
            "compliance_monitor_node = star_compliance.nodes.compliance_monitor_node:main",
        ],
    },
)
