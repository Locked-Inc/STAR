# STAR - 5-Minute Demo Script (with timing)

**Total runtime:** 4 min 40 s of script + ~20 s transition buffer = 5:00.

**Staging:** an on-campus hallway the team has pre-walked and measured.
The hallway should have (a) one indoor ramp where STAR can drive onto a
sloped surface, and (b) enough room for an autonomous frontier-exploration
run of 60-90 seconds before the scan completes. No planted shims, no
fabricated doorways - the demo narrates whatever STAR actually
encounters on the real route.

---

## 0:00 - 0:15  Opening hook (15 s)

*(Presenter 1 stands beside STAR. Robot powered on, stationary, status
LEDs pulsing blue.)*

> "Last year, U.S. plaintiffs filed eight thousand eight hundred ADA
> lawsuits. The federal penalty for a single repeat violation is two
> hundred thirty-six thousand four hundred fifty-one dollars. And the
> only way to find violations today is one person, with a tape measure,
> walking the building.
>
> We built the measurement layer for a better way."

---

## 0:15 - 0:45  Problem statement (30 s)

> "The Centers for Disease Control reports more than one in four
> American adults - about seventy million people - live with a
> disability. An industry-consensus estimate says seventy-three percent
> of U.S. commercial buildings fail at least one ADA standard. And a
> professional CASp audit costs anywhere from eight hundred to fifty
> thousand dollars and takes two to five days per building.
>
> On a campus the size of Texas A&M, auditing every building manually
> is effectively impossible. The bottleneck isn't the law. It's
> measurement."

---

## 0:45 - 1:30  Hardware walkthrough (45 s)

*(Presenter 2 takes over, walks around STAR pointing to components.)*

> "This is STAR - **S**patial **T**opography **A**ccessibility **R**obot.
>
> On top: a SLAMTEC RPLiDAR C1 - three-sixty degrees, ten hertz,
> twelve-meter range.
>
> Forward-facing: a Waveshare IMX219-83 stereo camera. Two eight-megapixel
> sensors, sixty-millimeter baseline, connected to both of the Raspberry
> Pi 5's camera ports. There's a nine-DoF IMU on this camera board too.
>
> Inside the chassis: a second nine-DoF IMU - a Bosch BNO055 - sitting
> on our custom PCB. That's where real-time motor control lives: a
> Renesas RX72N running ThreadX, driving four brushed DC gearmotors
> through DRV8263H H-bridges at two hundred fifty hertz PID. The RX72N
> talks to the Raspberry Pi 5 over a ten-megabit SPI link with HARQ and
> FEC reliability and Protocol Buffers on top.
>
> High-level software is ROS2 Jazzy on the Pi 5: slam_toolbox for
> mapping, robot_localization EKF, Nav2 for navigation, and our
> compliance engine. Total bill of materials: under two thousand dollars."

---

## 1:30 - 3:00  Live robot run narration (90 s)

*(Presenter 2 presses the button. Presenter 3 narrates while the RViz
feed shows on the projector and the robot autonomously explores the
hallway.)*

### 1:30 - 1:50 (first 20 s of robot run)

> "STAR is now running autonomous frontier exploration. No pre-loaded
> floor plan, no waypoints. On the projector, the left panel is
> slam_toolbox building the occupancy grid live; the right panel is
> the laser scan. The Pi 5 is doing all of this on-device - no cloud."

### 1:50 - 2:30 (seconds 20-60 of run)

> "It's approaching the ramp at the end of the hallway. Watch the IMU
> trace at the bottom - the BNO055 pitch is climbing as the robot
> drives onto the slope. At the same time, the compliance engine is
> doing RANSAC plane fitting on the LiDAR returns in front of the robot
> and computing the surface normal.
>
> Both estimates agree within half a degree. The engine accepts the
> reading as valid."

### 2:30 - 3:00 (seconds 60-90 of run)

> "The measured slope is flagged against the ADA 405.2 threshold of
> one-in-twelve - four-point-seven-six degrees. If it's over, the
> engine records the violation with the robot's current map-frame pose
> and writes a row to the validation log. You can see the pin appear
> on the map."

*(If the live ramp reading is below threshold, adjust: "This ramp is
compliant - the slope reads X degrees, below four-point-seven-six. The
engine logs it as a passing measurement.")*

---

## 3:00 - 4:00  Map and report reveal (60 s)

*(Presenter 4 takes over. Robot has parked. Projector switches to the
auto-generated PDF report.)*

> "Scan complete. What you're looking at now is the consolidated
> occupancy map from slam_toolbox, the EKF-filtered trajectory, and the
> PDF audit report STAR just generated. The report cites the ADA 2010
> section we measured against, shows the recorded slope and the
> cross-validation agreement, and pins the measurement to the map.
>
> We validated this against ground truth using a Wixey WR300 digital
> angle gauge, accurate to plus-or-minus one-tenth of a degree.
> Measurements logged into the validation spreadsheet under
> extras/STAR_ValidationLog.xlsx - those are the numbers we report, not
> estimates.
>
> On the back end, the ROS2 codebase has one hundred forty-three tests
> passing, zero failures. The platform is verified; ramp slope is the
> first check we've implemented end-to-end on top of it."

---

## 4:00 - 4:20  Closing impact (20 s)

*(Presenter 1 returns.)*

> "Manual ADA auditing doesn't scale. STAR is our proof that a sub-
> two-thousand-dollar student-built robot can replace the tape-measure
> layer. One check today, six architected for deployment phase. Code is
> open source under Locked-Inc on GitHub. We're looking for pilot
> building partners on this campus. Thank you."

*(4:20 - 5:00 reserved as Q&A buffer or transition.)*

---

## Contingency

If the live run fails, see `demo/contingency.md`. Scripted line:
"We've also recorded yesterday's full run for time" -> cut to
backup MP4. Narration script is identical because the backup is from
the same hallway and the same ramp.

---

## Pre-run checklist (T-30 minutes)

- [ ] Robot fully charged; battery voltage logged
- [ ] RPLiDAR C1 spinning, `/scan` publishing at ~10 Hz
- [ ] IMX219-83 stereo pair both streaming (both `/dev/video*` present)
- [ ] BNO055 `/imu/data` streaming via `star_spi_bridge`
- [ ] slam_toolbox launch OK, map visible in RViz
- [ ] Nav2 + frontier-exploration launch OK
- [ ] SPI transport diagnostic showing 0% frame loss over a 30-second
      test
- [ ] Projector mirrors the Pi 5's RViz feed at 1080p
- [ ] Pre-recorded backup MP4 on laptop desktop
- [ ] Pre-generated audit PDF from dress rehearsal on laptop desktop
- [ ] Wixey reading on the demo ramp logged in validation_log.csv within
      the last 24 h
- [ ] Hallway clear of spectators inside the robot's planned path
- [ ] Every presenter has mic check and laser pointer
