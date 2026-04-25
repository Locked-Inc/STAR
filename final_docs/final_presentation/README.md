# Final Presentation - ESET 420 Capstone Submission

## Canvas Submission Artifact

`STAR_Final_Presentation.pdf` -- 14-slide deck rendered to PDF
(16:9, 13.33 in x 7.5 in, 14 pages, 3.6 MB) built on the
TAMU-supplied dark-Arial template. This is the single PDF
uploaded for the Canvas Final Presentation assignment.

`STAR_Final_Presentation.pptx` -- editable PowerPoint source for
the same deck. Speaker notes are populated on slides 1 through 13.

## Slide Map

| # | Layout | Title |
|---|---|---|
| 1 | Title slide | STAR -- Spatial Topography Accessibility Robot |
| 2 | Two-column, stat callout | $236,451 per door |
| 3 | Two-column with chart | Who measures this? |
| 4 | Two-column with chart | Enforcement is accelerating |
| 5 | Three-column subheadings | Tech stack |
| 6 | Two-column with chart | System architecture |
| 7 | Three-column subheadings | Seven ADA checks |
| 8 | Two-column with screenshot | Live demo |
| 9 | Two-column with screenshot | Validation |
| 10 | Two-column with chart | Cost |
| 11 | Section breaker | Broader impact |
| 12 | Title + bullets | Future work |
| 13 | Three-column subheadings | Q&A and team |
| 14 | Closing | Thank You |

Status labels on slide 7 match the SDD and SSUM (see those
documents for the full taxonomy).

## Build Inputs

| Path | Role |
|---|---|
| `templates/24_Template_PowerPoint_Dark-ArialOnly.pptx` | TAMU-supplied template (build base) |
| `slide_outline.md` | Earlier 12-slide outline that informed the narrative |
| `speaker_notes.md` | Per-slide cue cards |
| `charts/output/chart_a..f_*.png` | Six pitch charts embedded on slides 3, 4, 6, 10 |
| `final_docs/system_software_user_manual/images/lichtblick-teleop.png` | Slide 8 screenshot |
| `final_docs/system_software_user_manual/images/thompson-hall-map.png` | Slide 9 screenshot |

## Regeneration

The deck is built programmatically from the template. If slide
content changes, edit the `SLIDES` table in the build script and
re-run:

```
python3 /tmp/build_deck.py    # rebuilds STAR_Final_Presentation.pptx
soffice --headless --convert-to pdf \
    final_docs/final_presentation/STAR_Final_Presentation.pptx \
    --outdir final_docs/final_presentation/
```

## Archive

`STAR_Deck.pptx` -- the prior 12-slide draft from earlier in the
semester. Kept for archival reference; superseded by
`STAR_Final_Presentation.pptx`.

## Team

Locked Inc. -- Texas A&M ESET Senior Capstone, Spring 2026.
See the SDD or SSUM title page for the full team roster.
