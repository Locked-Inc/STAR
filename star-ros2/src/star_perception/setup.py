"""ament_python setup for the STAR Hailo-8L perception package."""

from glob import glob
from pathlib import Path

from setuptools import find_packages, setup

PACKAGE_NAME = "star_perception"
HERE = Path(__file__).parent


setup(
    name=PACKAGE_NAME,
    version="0.1.0",
    packages=find_packages(include=[PACKAGE_NAME, f"{PACKAGE_NAME}.*"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + PACKAGE_NAME]),
        (f"share/{PACKAGE_NAME}", ["package.xml"]),
        (f"share/{PACKAGE_NAME}/launch", glob("launch/*.py")),
        (f"share/{PACKAGE_NAME}/config", glob("config/*.yaml")),
        (f"share/{PACKAGE_NAME}/scripts", glob("scripts/*.sh")),
        (f"share/{PACKAGE_NAME}/models",
         glob("models/*.hef") + glob("models/*.yaml")
         + glob("models/README.md")),
    ],
    install_requires=[
        "setuptools",
        "numpy",
    ],
    zip_safe=True,
    maintainer="Team Locked Inc.",
    maintainer_email="locked-inc@tamu.edu",
    description="Hailo-8L accelerated YOLO perception for STAR.",
    license="MIT",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "hailo_yolo_node = star_perception.hailo_yolo_node:main",
            "detection_3d_fusion_node = "
            "star_perception.detection_3d_fusion_node:main",
        ],
    },
)
