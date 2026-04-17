# STAR Q&A Preparation

Anticipated questions and rehearsed answers, grouped by questioner type.
Every numeric answer traces to `research/source_verification.md` or
`extras/validation_log.csv`. Memorize the first sentence; the rest is
backup.

---

## 1. Technical / judge (other engineers, ESET faculty)

### Q: Why SPI 10 Mbps plus HARQ and FEC, not CAN or UART?

We need deterministic latency and tight synchronization between the Pi 5
and the RX72N for 250 Hz closed-loop motor control. SPI gives us a
controller-timed 10 Mbps link with DMA double-buffering. We layered HARQ
Chase Combining and a convolutional rate-1/2 FEC on top because the
cable run is inside the chassis with motor-switching noise nearby;
measured utilization at full telemetry is 1.6%, so we had headroom for
the error-correction overhead. CAN's max practical rate on an RPi is
1 Mbps which would not support the 100 Hz command plus full telemetry
plus firmware-update streams.

### Q: Why RPi5, not a Jetson?

Three reasons. First, we already split real-time off onto the RX72N
custom PCB, so the Pi 5's job is ROS2 + SLAM + Nav2 + compliance engine,
none of which benefit meaningfully from a dedicated GPU at our scale.
Second, the Pi 5 has two dedicated MIPI CSI-2 ports, which let us run
the Waveshare IMX219-83 stereo camera natively without a USB intermediary.
Third, cost: the total BOM is under $2,000, and a Jetson Orin Nano would
have doubled our compute spend without solving a problem we had.

### Q: Why slam_toolbox async instead of Cartographer or RTAB-Map?

slam_toolbox's async mode gave us a stable `/map` at 0.5 Hz on the Pi 5
with low CPU and the full TF chain working out of the box. Cartographer
runs, but tuning it for our LiDAR-only front-end was more work than
slam_toolbox for the same quality. RTAB-Map is the path forward once we
light up the IMX219-83 stereo pipeline - the most recent commit on main
is "Fuse lidar into RTAB-Map for 12m range 3D mapping" and that's the
next integration phase. For the capstone demo, slam_toolbox async is
the proven choice.

### Q: Walk me through the compliance-engine code.

The implemented check is in `compliance-engine/star_compliance/nodes/ramp_slope_node.py`.
It subscribes to `/scan`, `/imu/data`, and `/odom`. When the robot body
is moving forward, it accumulates LiDAR returns within 2 m of the robot,
transforms them to the `base_link` frame, and runs Open3D RANSAC plane
fitting on the accumulated cloud. The surface normal's angle with
gravity gives one slope estimate. In parallel, we take the BNO055 pitch
trace from the last 500 ms and compute the mean - that's the second
estimate. If the two estimates agree within 0.5 degrees, we accept; if
the accepted slope exceeds 4.76 degrees, we flag a violation, write a
row to the validation CSV, and emit an entry for the PDF audit report.
The other six checks are stubbed or spec'd only in the same directory.

### Q: What's the ground-truth uncertainty?

Wixey WR300 digital angle gauge is +/- 0.1 degrees. Stanley FatMax 25 ft
tape is +/- 1/32 inch over the range we measure. Bosch GLM50 laser is
+/- 1/16 inch over 50 ft. For the ramp-slope measurements we're
publishing, 0.1 deg ground-truth accuracy is well below any target we're
making claims at, so the instrument is not the dominant error source.

### Q: Two IMUs - why?

The BNO055 sits on our RX72N PCB and is mechanically coupled to the
drive chassis, so its pitch reading directly corresponds to the robot
body's orientation - that's authoritative for ramp slope. The ICM20948
rides on the IMX219-83 camera board and is bolted to the stereo pair,
which gives us an inertial reference for the visual pipeline when we
light up visual-inertial odometry in the RTAB-Map integration phase. We
use the BNO055 as primary for ramp slope because it's physically on the
chassis; we don't currently use the ICM20948 for the one implemented
check.

### Q: Your stereo pipeline isn't live yet. How does door clear width
work?

Door clear width is in the ARCHITECTED set, not the IMPLEMENTED or
STRETCH set. We specified the algorithm (StereoSGBM disparity + door
frame edge detection at handle height) against the IMX219-83 that's
physically wired, and the pipeline lives in `compliance-engine/`, but
it's intentionally not implemented for the capstone window. We were
honest in the deliverable about what's coded vs. what's designed.

### Q: What's your biggest platform failure mode?

Two. One, SLAM failing to converge on reflective floors or glass walls -
slam_toolbox's scan-matching is not bulletproof, and we handle it by
not accepting ambiguous scans in the compliance engine. Two, the SPI
transport's HARQ retry window stalling motor-command throughput in a
sustained noise burst - we measure this via the `TransportHealthReport`
diagnostic stream and can surface it in the UI's TransportDiag panel.

### Q: Dynamic obstacles (people walking through)?

slam_toolbox rejects points that don't reappear across frames, so a
person walking through adds noise to one scan but doesn't produce a
permanent obstacle in the map. The compliance engine only operates on
the consolidated post-scan map, so a pedestrian walking through while we
measure a ramp doesn't pollute the ramp plane fit.

---

## 2. Stakeholder (TAMU Disability Resources, OREC, Facilities)

### Q: Who owns ADA compliance at TAMU today?

The Office of Risk, Ethics & Compliance runs the formal program at
orec.tamu.edu/ada. Facilities Services and the Office of the University
Architect own remediation budgets. The Department of Disability
Resources at 471 Houston Street, Student Services Building, Suite 122
advocates for end users. STAR's realistic sponsor model is joint: OREC
as compliance owner, Facilities as remediation owner, and Disability
Resources as user-advocate stakeholder.

### Q: Does STAR replace CASp inspectors?

No. STAR gives CASp inspectors a measurement layer that scales. A CASp
exercises professional judgment on remediation priorities, signage, and
user-experience issues STAR cannot measure. What STAR eliminates is the
tape-measure phase that precedes every informed judgment.

### Q: What's the liability if STAR misses a violation?

The PDF report says "measured values at the time of scan", not "building
is ADA compliant". STAR is an observational tool, like a thermal camera
or a structural survey. Legal attestation of compliance still comes
from a CASp inspector reviewing STAR's output.

### Q: Data privacy on mapped buildings?

The output is geometric - point clouds, occupancy grids, measurements.
We do not currently capture or persist RGB frames from the stereo
camera, only disparity / depth. If a future deployment captures RGB,
we'd add a post-scan redaction step (face blur, screen blur). That's
documented in the architected stereo-based checks.

---

## 3. Business / VC (pitch competitions, sponsor discussions)

### Q: Business model?

Three tiers: (1) open-source core compliance engine, free forever; (2)
hardware sales of the robot at cost plus fulfillment; (3) managed
service for institutional customers - campus-as-a-service - at recurring
monthly fee tied to square footage. Tier (3) is where unit economics
work; (1) and (2) are distribution strategy.

### Q: Competitors?

None doing end-to-end autonomous indoor ADA auditing. CloudPoint
Geospatial offers human-led LiDAR-augmented site assessments.
ROCK Robotic sells a handheld LiDAR. Matterport scans geometry but ships
no compliance engine. Oregon State's Turkan group works on outdoor
curb-ramp assessment from vehicle-mounted LiDAR. We're the first
autonomous platform that pairs the measurement layer with a compliance
engine.

### Q: Why open-source the engine?

Accessibility is a civil right; licensing fees on a compliance tool
would gate who benefits. Open source also accelerates adoption in the
public-institution market (universities, school districts,
municipalities) - which is exactly where the revenue is. Red Hat, not
Oracle.

---

## 4. Non-technical / general public

### Q: Can STAR go upstairs?

Not in this MVP. Multi-floor via elevator integration is in the
"architected, not vaporware" future-work list.

### Q: How much does it cost?

Under $2,000 in parts. The sensors (RPLiDAR, IMX219-83, IMUs) are most
of the cost; the custom PCB and motors are the rest.

### Q: Is it going to run over my foot?

Top speed is under walking pace. Nav2 maintains a 40 cm safety envelope;
the robot refuses paths inside that bubble. There's also an e-stop.

### Q: When can my business buy one?

We're still capstone students. After the final presentation, Locked Inc.
will evaluate next-step options, which include academic publication,
open-source maintenance, and potential commercialization. Talk to us
after the show.

---

## Stats to have memorized cold

- **8,800** ADA Title III lawsuits in 2024, **+7% YoY**
- **3,252 CA / 2,220 NY / 1,627 FL / 224 TX** (2024)
- **$118,225 first / $236,451 repeat** DOJ penalty (90 FR 29445, July 2025)
- **~73%** buildings fail (Building Principles, industry estimate)
- **32%** buyers ask (Focus Building Inspections)
- **28.7% / ~70M** U.S. adults with a disability (CDC 2022 BRFSS)
- Cognition **13.9%** > Mobility **12.2%** (2022 BRFSS ordering)
- CASp pricing: **$800-$2,000** small commercial; **$2,000-$7,500**
  typical commercial; **$50,000+** multi-building campuses
- **143 ROS2 tests, 0 failures**
- slam_toolbox map at **0.5 Hz** on RPi5
- SPI transport **1.6%** utilization at 10 Mbps
- BNO055 vs. LiDAR plane normal agreement gate: **+/- 0.5 deg**
- ADA 405.2 threshold: **4.76 deg** (1:12)
- TAMU CS campus: **5,200 acres, hundreds of buildings**

## Corrections you might be challenged on

- "I thought disability prevalence was 25.7%" -> "That's the 2016 BRFSS
  number. CDC's 2022 BRFSS released in July 2024 updates it to 28.7%
  and 70 million."
- "Aren't ADA penalties $75K / $150K?" -> "Those are 2014 figures. 90
  FR 29445 (July 2025) puts the current max at $118,225 and $236,451."
- "That 2018: 2,258 lawsuit number" -> "That's website-only. Total
  Title III in 2018 was 10,163. We cite total because STAR addresses
  physical access."
