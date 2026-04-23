"""ament_python setup for the STAR simple ASCII serial bridge.

Provides a minimal ROS2 node that bridges /cmd_vel (Twist) and
/odom/unfiltered (Odometry) to the RX72N motor controller over a
newline-delimited ASCII protocol on /dev/ttyACM0. Intended as a SLAM
MVP replacement for the C++ star_gateway_bridge during bring-up.
"""

from setuptools import find_packages, setup

PACKAGE_NAME = "star_simple_bridge"


setup(
    name=PACKAGE_NAME,
    version="0.1.0",
    packages=find_packages(include=[PACKAGE_NAME, f"{PACKAGE_NAME}.*"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + PACKAGE_NAME]),
        (f"share/{PACKAGE_NAME}", ["package.xml"]),
    ],
    install_requires=[
        "setuptools",
        "pyserial",
    ],
    zip_safe=True,
    maintainer="STAR Team",
    maintainer_email="cesarmagana23@gmail.com",
    description="Minimal ASCII serial bridge between ROS2 and the RX72N.",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "simple_bridge_node = "
            "star_simple_bridge.simple_bridge_node:main",
        ],
    },
)
