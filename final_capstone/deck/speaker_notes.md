# Speaker Notes - STAR Final Presentation

Use these as cue cards underneath each slide in PowerPoint's presenter
view. Timings assume ~25 s/slide; total 5 min of content + 2 min Q&A.

Aligned to the real stack: Raspberry Pi 5 + custom RX72N PCB + SPI with
HARQ/FEC + slam_toolbox + Nav2 + IMX219-83 stereo + two IMUs + HC-SR04
+ an honestly-scoped compliance engine (ramp slope IMPLEMENTED; trip +
path STRETCH; four more ARCHITECTED).

---

## Slide 1 - $236,451 per door (Team Lead, 20 s)

"Last year, U.S. plaintiffs filed eight thousand eight hundred ADA
lawsuits. The federal penalty for a single repeat violation is now two
hundred thirty-six thousand four hundred fifty-one dollars, per the
July 2025 Federal Register adjustment. The only way to find those
violations today is one person with a tape measure. We built the
measurement layer for a better way."

Pause. Let the numbers land.

---

## Slide 2 - The problem (Team Lead, 25 s)

"Industry estimates put seventy-three percent of U.S. commercial
buildings in violation of at least one ADA standard. Only thirty-two
percent of commercial property buyers even ask about ADA during
inspection. A CASp audit runs from eight hundred dollars for small
sites to over fifty thousand dollars for multi-building campuses, and
takes two to five days on-site. That is why nobody checks."

---

## Slide 3 - Why now (Member 2, 25 s)

"Enforcement is accelerating. Title III lawsuits rebounded seven
percent in 2024 after a two-year decline. California alone filed three
thousand two hundred fifty-two cases; one firm - So Cal Equal Access
Group - drove two thousand five hundred ninety-eight of them. That
pattern is replicable in any state. The DOJ's April 2024 Title II rule
forces public entities - state universities included - to meet WCAG
2.1 AA by April 2026."

---

## Slide 4 - Our solution (Member 2, 25 s)

"STAR is two contributions in one: a distributed autonomous robotics
platform, and an ADA compliance engine on top. The platform pairs a
Raspberry Pi 5 running ROS2 Jazzy with a custom Renesas RX72N PCB
running two hundred fifty hertz PID, linked by a ten-megabit SPI
channel with HARQ and FEC reliability and Protocol Buffers on top. On
top of that, the compliance engine implements the ramp-slope check
end-to-end against ADA 405.2, with trip hazard and path width as
stretch implementations and four more checks architected against the
same sensor stack."

---

## Slide 5 - Tech stack (Hardware Lead, 25 s)

"Three sensor groups. On the high-level side, the RPLiDAR C1 gives us
360-degree twelve-meter DTOF, and the Waveshare IMX219-83 stereo camera
connects via both of the Pi 5's MIPI CSI-2 ports - two Sony IMX219
sensors at a sixty-millimeter baseline, with an ICM20948 IMU bolted to
the camera board. On the real-time side, the RX72N handles the BNO055
IMU on its PCB, four HC-SR04 ultrasonics, a DS18B20, a BMP280, and
four DRV8263H motor drivers running four DFRobot FIT0520 gearmotors
off GPTW thirty-two-bit PWM at twenty kilohertz with MTU and TPU
quadrature encoders. Everything is open-source under Locked-Inc on
GitHub."

---

## Slide 6 - System architecture (Hardware Lead, 25 s)

"Sensors flow into the appropriate compute host - chassis sensors into
the RX72N, camera and LiDAR into the Pi 5. The RX72N aggregates and
ships over SPI through our HARQ Chase Combining and K=seven rate-one-
half FEC with nanopb Protocol Buffers. On the Pi 5 side, slam_toolbox
async plus the robot_localization EKF produce a stable map and odom;
Nav2 takes care of autonomous navigation; the compliance engine reads
the outputs and emits a PDF audit report."

---

## Slide 7 - Seven ADA checks (Software Lead, 30 s)

"We architected seven geometric checks. Ramp slope - ADA 405.2 - is
IMPLEMENTED: RANSAC plane fit on the LiDAR cross-validated against the
BNO055 pitch trace. Trip hazard and accessible path width are STRETCH
implementations for this capstone window. Ramp width, ramp landing,
door clear width, and door threshold are ARCHITECTED: the algorithm,
sensor assignment, and ADA citation are specified and the node stubs
are in place, but the implementation is deployment-phase work."

---

## Slide 8 - Demo (Software Lead, live; ~90 s of narration)

See `demo/demo_script_5min.md`. Key cues:

- Button press: "STAR is running autonomous frontier exploration."
- Ramp approach: "BNO055 pitch and LiDAR plane normal agree within half
  a degree. The engine accepts the reading."
- If slope > 4.76 deg: "Violation flagged under ADA 405.2."
- If compliant: "The ramp is compliant - below four-point-seven-six
  degrees. Passing measurement logged."
- At scan end: "PDF audit report generated from the validation log."

Contingency line: "We've also recorded yesterday's full run for time."

---

## Slide 9 - Validation (Software Lead, 25 s)

"On the platform side, we have one hundred forty-three ROS2 tests
passing with zero failures, slam_toolbox async producing a map on the
Raspberry Pi 5 at half a hertz with the full TF chain active, Nav2
executing autonomous goals, and the SPI transport running at one-point-
six-percent utilization with HARQ protection. On the compliance side,
we measure ramp slope against a Wixey WR300 digital angle gauge
ground truth, accurate to plus-or-minus one-tenth of a degree, on an
on-campus ramp. Numbers we report come from the validation log, not
estimates."

---

## Slide 10 - Broader impact (Team Lead, 25 s)

"TAMU's College Station campus is five thousand two hundred acres and
hundreds of buildings. Any comprehensive manual audit is a
multi-million-dollar, multi-year commitment. STAR converts that into
continuous monthly monitoring at amortized hardware cost. The realistic
stakeholder fit is joint: Disability Resources at four-seven-one
Houston Street for user advocacy; the Office of Risk, Ethics, and
Compliance at orec.tamu.edu/ada as compliance owner; and Facilities
Services as remediation owner. Code is open source under the Locked-Inc
GitHub organization."

---

## Slide 11 - Future work (Member 2, 20 s)

"Four architected ADA checks to complete. RTAB-Map LiDAR plus RGB-D
fusion - already in progress on main. Multi-floor via elevator
integration. ML fixture recognition for ADAS Chapter 6 coverage.
Outdoor mode with GPS and RTK for curb ramps. Fleet management for a
campus-as-a-service deployment pilot. We're looking for pilot site
partnerships today."

---

## Slide 12 - Q&A and team (All four)

Introduce each presenter by name and ESET role. Read the GitHub URL
once clearly: "github dot com, slash Locked dash Inc, slash STAR."
Point at the QR code. One explicit ask: "We're looking for a pilot
building on this campus."

See `extras/qa_prep.md` for rehearsed answers.
