# Validation Protocol

How to validate STAR compliance-engine output against tape-measure
ground truth. Each check gets its own protocol, instrument list, pass
threshold, and data-logging template.

Compliance reports are **screening tools**. Final adjudication still
belongs to a CASp inspector. This doc defines how the team proves
STAR is accurate enough to be worth a CASp's time.

---

## General setup

### Instruments (own / borrow before running a validation pass)

| Instrument | Purpose | Accuracy |
| --- | --- | --- |
| Stanley FatMax 25 ft tape | Frame widths, path widths | +/- 1/32 in |
| Bosch GLM50 laser distance meter | Door clearances, threshold heights | +/- 1/16 in over 50 ft |
| Wixey WR300 digital angle gauge | Ramp slopes | +/- 0.1 deg |
| Leica DISTO D2 (preferred) | Sub-mm distance measurements | +/- 1.5 mm |
| Calipers + depth gauge | Threshold heights (if GLM50 is too coarse) | +/- 0.1 mm |

### Bookkeeping

Every validation session writes rows into one of these CSVs in
`final_capstone/extras/`:

- `validation_log.csv` - ramp slope + door clear width
- `threshold_log.csv` - threshold events
- `protrusion_log.csv` - ADA 307 candidates
- `blockage_log.csv` - path blockage events

Append a `ground_truth_*_m` column alongside STAR's reading so the
downstream `build_validation_xlsx.py` script can compute per-check
absolute error, p50, p95, and true/false-positive rates.

---

## Check-by-check protocols

### ADA 405.2 Ramp Slope

**Ground truth:** Wixey WR300 on the ramp surface. Five readings
spaced 0.5 m along the slope; mean = ground truth.

**Trials:** n >= 20 ramps (mix of compliant and non-compliant).

**STAR reading:** drive onto each ramp; observe
`/compliance/ramp_slope` or tail `validation_log.csv`. STAR reports
the mean of LiDAR plane normal and BNO055 pitch after the 0.5-deg
agreement gate passes.

**Pass thresholds:**

- Mean absolute error (MAE) <= 0.5 deg
- 95th percentile absolute error <= 1.0 deg
- Agreement between BNO055 and LiDAR within 0.5 deg on 80%+ of
  sessions
- Flag outcome (violation vs compliant) correct on 100% of ramps
  clearly one side of 4.76 deg

### ADA 404.2.3 Door Clear Width

**Ground truth:** Bosch GLM50 measured between the face of the door
and the door stop, door open 90 degrees. **Also** measure frame-to-
frame for comparison with STAR's mid-layer number.

**Trials:** n >= 20 doors across 3 buildings. Mix:

- 5 compliant doors (ADA clear width >= 32 in)
- 5 borderline doors (ADA clear width 30-33 in)
- 5 clearly non-compliant doors (ADA clear width < 30 in)
- 5 with unusual frame hardware (metal hollow-core, double doors, pocket doors)

**STAR reading:** drive up to each door and pause. Observe
`/compliance/door_clear_width`:

- `frame_width_m` - raw measurement at handle height
- `ada_clear_width_m` - after offset subtraction
- `confidence` - HIGH / MEDIUM / LOW

**Pass thresholds:**

- Frame-width MAE <= 2 cm, p95 <= 4 cm
- ADA-adjusted MAE <= 3 cm, p95 <= 5 cm
- False-positive rate <= 5% on doors with GLM50-measured
  clear width >= 32 in
- False-negative rate <= 5% on doors with GLM50-measured
  clear width < 32 in
- Confidence tagging sane: HIGH on clear-cut cases, LOW when the
  door is glass / partly open / jamb is irregular

### ADA 404.2.5 Door Threshold Presence

**Ground truth:** use a calibrated depth gauge (Mitutoyo 547-400 or
equivalent) on the threshold. Any threshold >= 0.25 in is a
"present-and-potentially-noncompliant" target.

**Trials:** n >= 10 thresholds across mixed flooring (vinyl, carpet
transition, tile, wooden):

- 5 compliant (< 0.25 in)
- 5 non-compliant (>= 0.5 in)

**STAR reading:** drive over each threshold at a known velocity (run
with Nav2 `max_vel_x:=0.3` for consistency). Log the BNO055 jolt
event from `threshold_log.csv`.

**Pass thresholds:**

- True-positive rate (detected) on thresholds >= 0.5 in: >= 90%
- False-positive rate on smooth floors: <= 10%
- STAR explicitly reports **presence only**; no claim is made about
  height accuracy. A future revision with a downward-facing sensor
  will add height measurement.

### ADA 307 Protruding Objects

**Ground truth:** tape measure from the nearest wall plane to the
protruding object's extremity, measured parallel to the floor. Also
note the object's center height above the floor.

**Trials:** n >= 10 protrusions:

- 3 clearly non-compliant (> 5 in stand-off, in the 27-80 in band)
- 3 borderline (~4 in stand-off, in the band)
- 2 compliant (< 3 in stand-off)
- 2 below the band (< 27 in, e.g., baseboard heaters) - should NOT flag

**STAR reading:** drive past each; observe
`/compliance/protruding_objects`. Compare flagged candidates to
ground truth.

**Pass thresholds:**

- True-positive rate on > 4 in protrusions in the cane zone: >= 85%
- False-positive rate on < 4 in protrusions in the cane zone: <= 15%
- Zero-FP guarantee on below-27-in clusters (these are not ADA 307
  violations by definition)

### ADA 403.5 Path Blockage

**Ground truth:** tape-measure the corridor at the narrowest live
width (cart present, sign in place, etc.) and the pre-blockage baseline.

**Trials:** n >= 5 controlled setups with planted obstacles in a
corridor that's >= 40 in wide empty:

- 3 obvious blockages (30 in remaining)
- 2 borderline (35 in remaining)

**STAR reading:** drive the route twice - first pass builds the
baseline map; second pass (with blockage in place) should flag.

**Pass thresholds:**

- TPR on blocked corridors < 36 in: >= 80%
- FPR on unblocked corridors >= 36 in: <= 10%

---

## Reporting

### Individual validation session

Row in the appropriate CSV with:

- `run_id`, `date`, `time`, `operator`, `location`
- STAR's reported values (all columns)
- Ground-truth values in a parallel `gt_*` column added by hand or
  a follow-up script
- `instrument` used for ground truth
- `n` trials in the session

### Cumulative report

Run `python3 extras/build_validation_xlsx.py` to regenerate
`extras/STAR_ValidationLog.xlsx`. It computes per-check MAE, p50,
p95, and TPR/FPR automatically from the CSV rows. Commit the xlsx
snapshot before demo day so judges have the data trail.

### Poster and PDF

Every number on the final-presentation poster and in the PDF audit
report comes from this validation log. **No number appears in a
deliverable that isn't in the CSV.** Traceable claims only.

---

## Validation scheduling

| Session | Checks | When |
| --- | --- | --- |
| V1 - bench | ramp slope, door threshold presence | dress rehearsal #1 (T-6) |
| V2 - campus (ETID hall) | ramp slope, door clear width, path blockage | T-5 |
| V3 - campus (multi-building) | door clear width, protruding objects | T-4 |
| V4 - final | all above, replaying V2 and V3 for consistency | T-2 |

Each session adds 20-50 rows to the CSVs. If V2 or V3 fails the pass
thresholds, the team retunes parameters (see TUNING.md) and re-runs
that session before moving on.

---

## See also

- [TUNING.md](TUNING.md) - what to change when a check is failing
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - "my compliance engine
  says X" symptom guide
- `extras/traceability.md` - full ADA-check to source-file mapping
