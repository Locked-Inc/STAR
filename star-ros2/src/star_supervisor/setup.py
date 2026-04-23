from setuptools import find_packages, setup

package_name = "star_supervisor"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="STAR Team",
    maintainer_email="cesarmagana23@gmail.com",
    description="Command gate + autonomy/e-stop arbiter for STAR",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "supervisor_node = star_supervisor.supervisor_node:main",
        ],
    },
)
