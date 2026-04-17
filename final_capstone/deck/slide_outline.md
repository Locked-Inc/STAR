# STAR 12-Slide Deck Outline

Four presenters; speaker rotates so everyone appears in both early
high-stakes slides and later technical slides. Each slide budgets ~25
seconds of speaking time, fitting a 5-7 minute formal window plus Q&A.

Aligned to the real hardware (Raspberry Pi 5 + custom RX72N PCB,
IMX219-83 stereo, two IMUs, HC-SR04 ultrasonic, slam_toolbox async +
Nav2 + m-explore-ros2) and the honest compliance-engine scope (ramp
slope IMPLEMENTED; trip hazard + path width STRETCH; four remaining
ADA checks ARCHITECTED).

| # | Title | Talking points | Visual | Speaker |
|---|---|---|---|---|
| 1 | **$236,451 per door** | "8,800 ADA lawsuits in 2024, +7% YoY. Max federal penalty $236,451 per repeat violation (90 FR 29445). 28.7% of adults - 70M - live with a disability. The only way to find violations today is one person, a tape measure, and a clipboard." | Hero stats on dark bg | Team Lead |
| 2 | **The problem: who even measures this?** | ~73% of U.S. commercial buildings fail >=1 ADA standard (industry est.). Only 32% of buyers ask at commercial inspection (Focus Bldg. Insp.). CASp audit: $800-$50,000+ depending on scope, 2-5 days on-site. | Chart (b) donut + chart (d) disability breakdown | Team Lead |
| 3 | **Why now: enforcement is accelerating** | Title III filings +7% to 8,800 in 2024. CA 3,252, NY 2,220, FL 1,627, TX 224. So Cal Equal Access Group filed 2,598 of CA's cases. DOJ Title II rule forces WCAG 2.1 AA by April 2026 for public entities. | Chart (a) lawsuit trend bar | Member 2 |
| 4 | **STAR: platform + compliance layer** | Platform: RPi5 + custom RX72N PCB + SPI 10 Mbps + HARQ/FEC + nanopb; ROS2 Jazzy with slam_toolbox + Nav2 + frontier exploration; 143 tests passing. Compliance: ramp slope IMPLEMENTED end-to-end; trip hazard + path width STRETCH; four more ARCHITECTED. | Two-column breakdown | Member 2 |
| 5 | **Tech stack** | Hardware: RPLiDAR C1, Waveshare IMX219-83 stereo, BNO055 IMU on RX72N + ICM20948 IMU on camera, 4x HC-SR04, DS18B20, BMP280, 4x DFRobot FIT0520, 4x TI DRV8263H, GPTW 32-bit PWM @ 20 kHz, MTU/TPU encoders. Software: ROS2 Jazzy, slam_toolbox + robot_localization EKF, Nav2, m-explore-ros2, Python/Open3D compliance engine, Go gRPC-Web gateway, React UI. Open source: Locked-Inc/STAR. | Two-column HW/SW list | Hardware Lead |
| 6 | **System architecture** | Three sensor groups -> RX72N real-time + RPi5 ROS2 -> SPI with HARQ/FEC -> SLAM + EKF + Nav2 -> compliance engine -> PDF report. | Chart (f) Sankey | Hardware Lead |
| 7 | **Seven ADA checks: status vs. sensor** | Ramp slope IMPLEMENTED (LiDAR + BNO055 cross-val). Trip hazard + path width STRETCH. Ramp width, ramp landing, door clear width, door threshold ARCHITECTED. Each cell: primary sensor, secondary sensor, not used. | Chart (e) sensor responsibilities matrix | Software Lead |
| 8 | **Demo: autonomous explore + live ramp-slope check** | Robot autonomously explores (no floor plan). slam_toolbox builds map live. Ramp encountered; LiDAR plane normal + BNO055 pitch agree within 0.5 deg -> engine accepts reading. If > 4.76 deg, flag violation to CSV + PDF. | Live RViz + camera feed | Software Lead |
| 9 | **Validation: what we can defend** | Platform: 143/143 tests, slam_toolbox on RPi5 at 0.5 Hz, full TF chain, Nav2 goals executed, frontier exploration to completion, SPI 1.6% utilization. Compliance: Wixey WR300 ground truth (+/- 0.1 deg) on on-campus ramp, n=5-30, live validation_log.csv. | Chart (e) + platform bullets | Software Lead |
| 10 | **Broader impact** | TAMU: 5,200 acres, hundreds of buildings. Manual audit = multi-million-dollar cost. Continuous monthly coverage at amortized hardware cost. Stakeholders: Disability Resources (471 Houston Street, Student Services Bldg, Suite 122), OREC (orec.tamu.edu/ada), Facilities. Open source under Locked-Inc. | Stakeholder block | Team Lead |
| 11 | **Future work (architected, not vaporware)** | Complete architected ADA checks. RTAB-Map LiDAR + RGB-D fusion (in-progress on main). Multi-floor via elevator. ML fixture recognition for ADAS Chapter 6. Outdoor GPS+RTK mode. Fleet/campus-as-a-service pilot. | Roadmap list | Member 2 |
| 12 | **Q&A and team** | Names, ESET roles, GitHub URL, contact. Ask: "Pilot building partners." | Team photo with STAR; QR to GitHub | All four |
