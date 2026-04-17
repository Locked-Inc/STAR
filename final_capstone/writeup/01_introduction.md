# Introduction and motivation

Accessibility is the largest civil-rights compliance regime in U.S.
real estate and one of the most poorly measured. The Centers for Disease
Control's most recent Disability and Health Data System update, drawn
from the 2022 Behavioral Risk Factor Surveillance System and released in
July 2024, reports that **28.7% of U.S. adults - approximately
70 million people - live with a disability**. The most prevalent types
are cognition (13.9%), mobility (12.2%), independent living (7.7%),
hearing (6.2%), vision (5.5%), and self-care (3.6%). Mobility, vision,
and self-care disabilities together affect more than 60 million
Americans whose daily quality of life depends on whether the doorways,
ramps, and paths in their buildings actually meet the geometric
tolerances the ADA requires.

The legal exposure for non-compliance is no longer hypothetical.
Plaintiffs filed **8,800 ADA Title III federal lawsuits in 2024**, a
7% rebound from 2023's 8,227 (Seyfarth Shaw, March 2025). California led
with 3,252 filings, more than 80% of them driven by a single firm
(So Cal Equal Access Group, 2,598 cases). Texas placed fourth nationally
with 224 filings - small relative to coastal states, but rising. The
Department of Justice's annual civil-penalty inflation adjustment,
codified at 28 CFR 85.5 and most recently published at 90 FR 29445
(July 3, 2025), now sets statutory maximums at **$118,225 per first
violation and $236,451 per subsequent violation**, before plaintiff
attorney fees and remediation costs.

Yet the dominant audit method remains a human with a tape measure.
Certified Access Specialist (CASp) inspections in California range from
**$800 to $2,000** for small commercial sites through **$2,000 to
$7,500** for typical commercial buildings, up to **$50,000+ for
multi-building campuses**, and require two to five days on-site to
measure ramp slopes, door clearances, threshold heights, parking, and
the entire path of travel. An industry survey from Focus Building
Inspections reports that **only 32% of commercial property buyers**
ask about ADA compliance at all, meaning roughly two-thirds of building
transactions close with the accessibility status entirely unknown. At
the institutional scale that owns most U.S. accessibility risk -
universities, hospitals, K-12 districts, federal facilities -
comprehensive auditing is effectively unaffordable. Texas A&M's College
Station campus alone spans 5,200 acres with hundreds of buildings;
auditing every one at the midpoint of an industry CASp quote would run
into the multi-million-dollar range and tie up specialists for years.

STAR (Spatial Topography Accessibility Robot) addresses the measurement
bottleneck by building a distributed autonomous ground-robotics platform
that provides the sensing and mapping layer a compliance engine can
build on. The robot autonomously navigates a building, fuses LiDAR,
stereo depth, and inertial data on a Pi-5-plus-RX72N architecture,
produces a metrically accurate map, and runs a new Python ROS2
compliance-engine package over the resulting geometry.

The capstone contribution is twofold:
(1) a working distributed robotics platform - Raspberry Pi 5 running
ROS2 Jazzy for SLAM, EKF pose estimation, Nav2, and frontier exploration;
a custom RX72N PCB handling 250 Hz real-time motor control; and a
10 Mbps SPI link between them with HARQ/FEC reliability and nanopb
Protocol Buffers; and
(2) an end-to-end ADA compliance engine demonstrated on the ramp-slope
check, with two stretch checks in progress and the remaining four
architected against the same sensor stack for deployment-phase
implementation.

The platform side is validated by 143 passing ROS2 tests and an
end-to-end autonomous exploration demo already working on the Pi 5. The
compliance side is validated live against ground-truth Wixey-WR300
angle-gauge measurements on an on-campus ramp. This capstone demonstrates
that the sensing, real-time control, and software architecture required
to do continuous indoor ADA measurement exist on a student-built
platform, fit under $2,000 in hardware, and produce results defensible
enough to inform an ADA specialist's professional judgment.
