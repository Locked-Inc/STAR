from setuptools import find_packages, setup

package_name = "star_serial_bridge"

setup(
    name=package_name,
    version="0.0.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/launch", ["launch/star_bridge.launch.py"]),
        ("share/" + package_name + "/config", ["config/ekf.yaml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="STAR maintainers",
    maintainer_email="cesarmagana23@gmail.com",
    description="RX72N FIT sandbox <-> ROS2 Jazzy serial bridge.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "star_serial_bridge = star_serial_bridge.star_serial_bridge:main",
        ],
    },
)
