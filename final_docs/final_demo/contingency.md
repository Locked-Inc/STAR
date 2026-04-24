# Demo Contingency Plan

Real-failure-mode coverage for the STAR capstone demo. Every pivot is
scripted so no presenter is standing on stage improvising under stress.

## Primary failure modes and scripted responses

| Failure | Who detects | Scripted line | Action |
|---|---|---|---|
| Robot fails to boot | P2 at 1:30 | "We've also recorded yesterday's full run for time." | Switch projector to backup MP4. P3 narration is identical. |
| SPI link drops frames beyond threshold | P3 during setup | "Let me show you yesterday's scan - same hallway, same ramp." | Switch to backup MP4. |
| slam_toolbox fails to initialize | P3 at 1:45 | "The map is already captured from yesterday - let me walk you through it." | Skip live exploration, jump to pre-captured map on RViz. |
| BNO055 or LiDAR plane-fit produce disagreement > 0.5 deg | P3 at ramp approach | "Our cross-validation gate caught a discrepancy - the engine is designed to re-scan rather than trust one sensor. Here's yesterday's agreement-passing run." | Play backup MP4 from ramp onward. |
| Nav2 oscillates in the hallway | P2 silently engages manual mode | - | Manual drive the robot along the ramp so ramp-slope is still measured. Do not narrate the Nav2 problem. |
| RViz freezes | P3 | "The robot is still scanning - let me show the completed output." | Skip to PDF audit report reveal at 3:00. |
| PDF audit report fails to generate | P4 at 3:00 | "Here's yesterday's report from the same run." | Open pre-generated PDF. |
| Projector input drops | P1 | "Let's pause for one second." | P2 re-seats HDMI; others cover with research stats. |

## Backup assets (pre-staged on demo laptop)

- `backup_run_full.mp4` - 90-second autonomous exploration run from the
  dress rehearsal, same hallway
- `backup_audit_report.pdf` - compliance engine output from the dress
  rehearsal, with real ramp-slope violation flag
- `backup_map.png` - static map + trajectory screenshot
- `backup_scan_cloud.png` - static LiDAR scan cloud screenshot
- `backup_slides.pdf` - full deck as PDF, in case PowerPoint crashes

## Scripted pivot lines (memorize)

**Any non-obvious failure:** "This is exactly why we implemented
cross-validation on every measurement - the engine refuses to accept
ambiguous readings. Let me show you yesterday's successful run."

**If the ramp reads below threshold (compliant ramp):** "This ramp is
compliant - the slope measures X degrees, below ADA's four-point-seven-six.
We log passing measurements too; compliance isn't only about finding
violations."

**If asked to show the code live:** "Happy to - `compliance-engine/
star_compliance/nodes/ramp_slope_node.py` is the implemented check, and
the other nodes in that folder are the architected stubs." Be ready to
open that file in a terminal or VS Code instance.

## Pre-demo checklist (T-30 minutes)

- [ ] Laptop at 100% battery; power adapter plugged in anyway
- [ ] Backup laptop mirroring the primary
- [ ] HDMI + USB-C adapters ready; spare HDMI cable
- [ ] Clicker tested; spare battery in P1's pocket
- [ ] Robot e-stop reachable by P2 throughout
- [ ] Wixey ground-truth reading on the demo ramp logged into
      `validation_log.csv` within 24 h
- [ ] MP4, PDF, and PNG backups on both laptops at known paths
- [ ] 30-second silent room test with everyone at their marks
- [ ] All four presenters read the 5-min script once within the last
      24 h
