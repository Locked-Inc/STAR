# Broader impact

STAR is a distributed autonomous robotics platform that gives ADA
compliance a continuous, re-runnable measurement layer. The platform
itself is a capstone-scale exemplar of real-time embedded + high-level
ROS2 integration: four brushed DC gearmotors under 250 Hz PID on a
custom RX72N PCB, a 10 Mbps SPI link with HARQ and FEC, ROS2 Jazzy
running slam_toolbox and Nav2 on a Raspberry Pi 5, all published from
a gateway service into a React UI. The compliance layer on top is one
implemented ADA check (ramp slope, ADA 405.2), two stretch checks
(trip hazard, accessible path width), and four architected checks
(ramp width, ramp landing, door clear width, door threshold) - a
deliberately honest scope that demonstrates the measurement layer
without over-claiming what a one-semester capstone can finish.

At the institutional scale that owns most accessibility risk, the
economic shift matters. Texas A&M's College Station campus covers 5,200
acres with hundreds of buildings; a conservative CASp audit pass at
$5,000 per building per visit already runs into seven figures and
consumes specialist time for years. A robot platform like STAR, once
productionized, can run the same coverage in weeks at amortized hardware
cost and re-run monthly rather than once - converting a one-shot
legal-defense expense into a continuous public-health and civil-rights
instrument. That is the same economic transition cybersecurity scanners
brought to IT compliance a decade ago: from expert-bounded audit events
to continuously monitored telemetry.

STAR is released open-source under the **Locked-Inc** GitHub
organization, with the compliance engine, ROS2 packages, firmware, and
gateway all under permissive licensing. Accessibility is a civil right;
a tool that measures ADA compliance should not be gated behind
licensing rent, and open release accelerates adoption in the
public-institution market (universities, school districts, county
governments) where the physical-access liability actually sits.

The stakeholder model for a TAMU deployment is a three-way partnership:
the **Department of Disability Resources** (471 Houston Street, Student
Services Building, Suite 122) as user advocate; the **Office of Risk,
Ethics & Compliance** (orec.tamu.edu/ada) as the formal ADA-program
owner; and **Facilities Services / Office of the University Architect**
as the remediation-budget holder. The Department of Justice's April 2024
Title II final rule requires public entities including state
universities to meet WCAG 2.1 Level AA by April 24, 2026 - the A&M
System already operates under active digital-accessibility compliance
pressure. STAR puts the physical-access side onto the same continuous
measurement footing.

STAR is not designed to replace human judgment in remediation decisions
or to substitute for the expertise of CASp specialists and disability
advocates. It is designed to give those professionals a measurement
layer that finally scales to the size of the problem - and to give the
ESET capstone a concrete demonstration of end-to-end distributed
robotics on real hardware built, wired, programmed, and validated by
four undergraduates.
