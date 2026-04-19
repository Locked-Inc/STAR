# Abstract

The Americans with Disabilities Act has been federal law for 35 years,
yet an industry-consensus estimate suggests roughly 73% of U.S.
commercial buildings still fail at least one accessibility standard,
federal Title III lawsuit filings reached 8,800 in 2024 (a 7%
year-over-year rebound), and the current Department of Justice civil
penalty maximums are $118,225 for a first violation and $236,451 for
each subsequent violation (90 FR 29445, July 2025). The dominant audit
method remains a human with a tape measure and clipboard. Certified
Access Specialist inspections range from $800 to $50,000+ depending on
scope and take two to five days on-site.

STAR (Spatial Topography Accessibility Robot), developed by Locked Inc.
for the Texas A&M ESET senior capstone, is a distributed autonomous
ground-robotics platform that provides the measurement layer for
automated indoor ADA compliance auditing. A Raspberry Pi 5 runs ROS2
Jazzy (slam_toolbox async SLAM, robot_localization EKF, Nav2, and
m-explore-ros2 frontier exploration); a custom Renesas RX72N PCB handles
250 Hz PID motor control with Hall-encoder feedback on four DFRobot
FIT0520 gearmotors via DRV8263H H-bridges. The two halves communicate
over a 10 Mbps SPI link with nanopb Protocol Buffers, CRC-32 framing,
HARQ Chase Combining, and convolutional FEC.

Sensing is a SLAMTEC RPLiDAR C1 (360 deg, 10 Hz, 12 m), a Waveshare
IMX219-83 stereo camera (dual Sony IMX219 8 MP, 60 mm baseline) with
onboard ICM20948 IMU, a BNO055 9-DoF IMU on the chassis, and four
HC-SR04 ultrasonic rangefinders. The capstone delivers an autonomous
frontier-exploration demo with real-time SLAM mapping plus a new
**ADA compliance engine** that implements the ramp-slope check end-to-end
(ADA section 405.2): RANSAC plane segmentation on the LiDAR returns,
cross-validated against the BNO055 pitch trace, with a PDF audit report
generated at scan completion. Trip-hazard and accessible-path-width
checks are in-progress stretch goals; ramp width, ramp landing, door
clear width, and door threshold are fully architected against the same
sensor stack for post-capstone deployment.

The team reports platform validation (143 tests, 0 failures), and
live ramp-slope validation against a Wixey WR300 digital angle gauge on
an on-campus ramp. STAR turns the measurement bottleneck in ADA
compliance into a tractable, re-runnable, open-source layer that scales
with hardware cost rather than specialist hours.
