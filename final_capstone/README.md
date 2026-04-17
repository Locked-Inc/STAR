# STAR Capstone - Final Deliverable Package

Team Locked Inc. | TAMU ESET Senior Capstone, Spring 2026

All text is pure 7-bit ASCII. All figures regenerate from source. Every
numeric claim is traced to a primary source in
`research/source_verification.md`.

---

## Contents

```
final_capstone/
  README.md                                 you are here
  requirements.txt                          Python deps for every build script
  STAR_Capstone_Deliverable_Package.pdf     earlier research agent handoff (superseded; see writeup/ for current content)
  48x36_Poster_Template.pptx                TAMU-provided accessible template

  architecture.mmd                          NEW: Mermaid system architecture
  bom.md                                    NEW: bill of materials with part numbers + suppliers

  research/
    research_dossier.md                     verified-source research package
    source_verification.md                  NEW: every claim traced to a primary source

  writeup/                                  technical writeup, aligned to real hardware
    00_abstract.md
    01_introduction.md
    02_related_work.md
    03_system_design.md                     RPi5 + RX72N + SPI + HARQ/FEC + real sensors
    04_compliance_engine.md                 seven checks with honest [IMPLEMENTED]/[STRETCH]/[ARCHITECTED] labels
    05_validation.md                        platform + ramp-slope validation (real)
    06_broader_impact.md

  charts/                                   Plotly chart scripts (dark theme)
    theme.py
    chart_a_lawsuit_trend.py                Seyfarth Shaw 2018-2024
    chart_b_compliance_donut.py             73% industry estimate
    chart_c_cost_time.py                    manual vs automated audit
    chart_d_disability_breakdown.py         CDC 2022 BRFSS
    chart_e_sensor_responsibilities.py      REWRITTEN: sensor x ADA-check matrix (no invented MAE)
    chart_f_pipeline_sankey.py              REWRITTEN: real pipeline w/ status badges
    generate_all.sh
    output/                                 generated PNGs

  flowcharts/                               NEW: Mermaid flowcharts for judge-facing diagrams
    system_architecture.mmd
    compliance_pipeline.mmd
    ramp_slope_algorithm.mmd
    demo_timeline.mmd
    spi_harq_protocol.mmd

  deck/                                     12-slide deck
    slide_outline.md
    speaker_notes.md
    build_deck.py                           REWRITTEN against real stack
    STAR_Deck.pptx

  poster/                                   48x36 TAMU showcase poster
    build_poster.py                         REWRITTEN
    STAR_Poster_48x36.pptx

  trifold/                                  tri-fold brochure
    build_trifold.py                        REWRITTEN
    STAR_Trifold.pdf

  one_pager/                                print-ready one-pager
    build_one_pager.py                      REWRITTEN
    STAR_OnePager.pdf

  demo/                                     5-min demo script + handout
    demo_script_5min.md                     REWRITTEN for on-campus hallway + real sensors
    contingency.md                          REWRITTEN with real failure modes
    build_handout.py                        REWRITTEN
    STAR_DemoHandout.pdf

  extras/
    github_readme.md                        REWRITTEN drop-in README for github.com/Locked-Inc/STAR
    qa_prep.md                              REWRITTEN with real tech Q&A
    validation_log.csv                      REPLACED: template for live capture (no invented rows)
    build_validation_xlsx.py                formats CSV into Excel workbook
    STAR_ValidationLog.xlsx                 built workbook (after script)
    traceability.md                         NEW: ADA check -> sensor -> algorithm -> file path matrix
    references.bib                          NEW: BibTeX for every academic citation

  compliance-engine/                        NEW: the implementation contribution
    star_compliance/
      nodes/
        ramp_slope_node.py                  [IMPLEMENTED] ADA 405.2
        trip_hazard_node.py                 [STRETCH stub]
        path_width_node.py                  [STRETCH stub]
        ramp_width_node.py                  [ARCHITECTED spec]
        ramp_landing_node.py                [ARCHITECTED spec]
        door_clear_width_node.py            [ARCHITECTED spec]
        door_threshold_node.py              [ARCHITECTED spec]
      engines/
        plane_segmentation.py
        imu_cross_validate.py
      report/
        pdf_generator.py
      bringup/
        compliance.launch.py
    README.md
    tests/
```

---

## One-shot build

From `final_capstone/`:

```bash
python3 -m pip install -r requirements.txt

# PNG charts (build these first; everything else embeds them)
bash charts/generate_all.sh

# Deliverables that embed charts
python3 deck/build_deck.py
python3 poster/build_poster.py
python3 trifold/build_trifold.py
python3 one_pager/build_one_pager.py
python3 demo/build_handout.py
python3 extras/build_validation_xlsx.py

# Flowcharts (requires mmdc: npm install -g @mermaid-js/mermaid-cli)
for f in flowcharts/*.mmd; do mmdc -i "$f" -o "${f%.mmd}.svg" -t dark -b "#1a1a2e"; done
```

After this:
- 6 chart PNGs in `charts/output/`
- `deck/STAR_Deck.pptx` (12 slides, 16:9)
- `poster/STAR_Poster_48x36.pptx` (TAMU template, filled)
- `trifold/STAR_Trifold.pdf`, `one_pager/STAR_OnePager.pdf`,
  `demo/STAR_DemoHandout.pdf`, `extras/STAR_ValidationLog.xlsx`
- 5 SVG flowcharts in `flowcharts/` for slide and poster embedding

---

## The three data corrections to defend

1. **Disability prevalence is 28.7% / ~70M** (CDC 2022 BRFSS released
   July 2024) - not 25.7% / 61M from 2016 BRFSS. Cognition (13.9%) has
   overtaken mobility (12.2%) as the most prevalent type.
2. **DOJ penalty maximums are $118,225 / $236,451** (90 FR 29445, July
   2025 - 2024 base $115,231/$230,464 x 1.02598). The widely-cited $75K
   / $150K figures are 2014 numbers.
3. **Cite total Title III lawsuits** (8,800 in 2024; 11,452 in 2021),
   not website-only filings (2,452 in 2024). STAR addresses physical
   access.

See `research/source_verification.md` for the full 40+ row matrix.

---

## The one thing the capstone contributes

A **distributed autonomous robotics platform** (RPi5 + custom RX72N PCB
+ SPI with HARQ/FEC + ROS2 Jazzy + slam_toolbox + Nav2 + frontier
exploration) with **one ADA compliance check implemented end-to-end**
(ramp slope, ADA 405.2) and **six more architected against the same
sensor stack**. The platform is verified by 143 passing ROS2 tests and
demonstrated by an autonomous frontier-exploration run. The compliance
check is validated against Wixey-angle-gauge ground truth on an
on-campus ramp.

Not claimed: stereo-based door measurement, hand-tuned MAE numbers
across all seven checks, a curated test course with planted shims.

---

## Presenter assignments (see `deck/slide_outline.md`)

| Member | Slides |
|---|---|
| Team Lead | 1, 2, 10 |
| Member 2 | 3, 4, 11 |
| Hardware Lead | 5, 6 |
| Software Lead | 7, 8, 9 |
| All four | 12 (Q&A) |

---

## Seven-day assembly plan

- **T-7:** text assets finalized against real hardware; charts rendered;
  source-verification matrix complete; deck / poster / trifold /
  one-pager / handout compiled; compliance-engine package scaffolded.
- **T-6:** dress rehearsal #1. Identify the on-campus ramp. Capture
  Wixey ground-truth readings into `validation_log.csv`. Capture first
  rosbag.
- **T-5:** tune ramp-slope node parameters against the rosbag. Second /
  third validation rows. First stretch-check attempt (trip hazard) if
  bandwidth allows.
- **T-4:** dress rehearsal #2 in the actual demo hallway. Record backup
  MP4. Final validation rows. Second stretch-check attempt (path width).
- **T-3:** Q&A drills. Stakeholder outreach. Freeze the validation table.
- **T-2:** print 48x36 poster. Print 20 copies each of one-pager,
  trifold, handout.
- **T-1:** light-touch day. Battery check. Dress rehearsal #3.
- **T-0:** ship.

Build the demo. Win the room.
