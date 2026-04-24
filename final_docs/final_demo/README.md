# Final Demo - ESET 420 Capstone Submission

## Primary Deliverable

`demo_script_5min.md` -- five-minute live demo script with per-
segment timing (opening hook, problem framing, hardware walkthrough,
autonomous exploration + live ramp-slope check, validation commentary,
Q&A hand-off). Hits a 4 minute 40 second speaking budget plus 20
seconds of transition buffer.

## Supporting Materials

- `STAR_DemoHandout.pdf` -- one-page handout distributed to the
  audience at the demo. Summarizes what the viewer is about to see
  and where to find the repository.
- `contingency.md` -- fallback scripts for common demo failure modes:
  LiDAR USB hotplug, motor stall on startup, SPI retransmit storm,
  unexpected ramp geometry. Each fallback is a pre-rehearsed
  narrative the presenters can drop into without breaking flow.

## Demo Topology

- **Staging**: an on-campus hallway with (a) one indoor ramp where
  STAR can drive onto a sloped surface and (b) enough open run for a
  60-90 second autonomous frontier-exploration pass.
- **No planted shims or fabricated doorways.** The demo narrates
  whatever STAR actually encounters on the real route. This keeps
  the ramp-slope check honest and cross-validated against a Wixey
  WR300 inclinometer ground truth.

## Dashboards During the Demo

Live audience view comes from the Grafana cockpit and Lichtblick
layout described in the STAR System and Software User Manual
(SSUM). The ramp-slope result publishes to
`/compliance/ramp_slope` and appears in the compliance panel in
real time.

## Source of Truth

The authoritative editable copies live under `final_capstone/demo/`.
The files here are the submission snapshot.

## Team

Locked Inc. (Texas A&M ESET Senior Capstone, Spring 2026).
See the SDD or SSUM title page for the full team roster.
