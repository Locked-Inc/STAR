"""Minimal PDF audit-report generator.

Reads validation_log.csv and emits a one-or-two-page PDF summarizing
every flagged violation and every passing measurement.
"""

from __future__ import annotations

import csv
import time
from pathlib import Path
from typing import Iterable

from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib.colors import HexColor, black, white
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
)

MAROON = HexColor("#500000")
ACCENT = HexColor("#e63946")
GREEN = HexColor("#2a9d8f")
MUTED = HexColor("#666666")
RULE = HexColor("#cccccc")


def load_rows(csv_path: Path) -> list[dict]:
    rows: list[dict] = []
    with csv_path.open() as f:
        for row in csv.DictReader(f):
            if row.get("run_id", "").startswith("#"):
                continue
            rows.append(row)
    return rows


def generate(csv_path: Path, out_pdf: Path) -> None:
    styles = getSampleStyleSheet()
    body = ParagraphStyle("body", parent=styles["Normal"],
                          fontSize=10, leading=13, textColor=black)
    h1 = ParagraphStyle("h1", parent=styles["Heading1"],
                        fontSize=18, leading=22, textColor=MAROON)
    h2 = ParagraphStyle("h2", parent=styles["Heading2"],
                        fontSize=13, leading=16, textColor=MAROON, spaceBefore=8)
    small = ParagraphStyle("small", parent=body, fontSize=8, textColor=MUTED)

    doc = SimpleDocTemplate(str(out_pdf), pagesize=letter,
                            leftMargin=0.6 * inch, rightMargin=0.6 * inch,
                            topMargin=0.6 * inch, bottomMargin=0.6 * inch)

    story = []

    story.append(Paragraph("STAR ADA Compliance Audit Report", h1))
    story.append(Paragraph(
        f"Generated {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}",
        small))
    story.append(Spacer(1, 8))
    story.append(Paragraph(
        "This report summarizes the ADA geometric compliance measurements "
        "taken during the most recent STAR scan. Each row is a single "
        "measurement event recorded by the compliance engine from the "
        "on-chassis BNO055 IMU and the RPLiDAR C1, with cross-validation "
        "enforced at plus/minus 0.5 degrees.",
        body))

    rows = load_rows(csv_path)

    flagged = [r for r in rows if r.get("flagged") == "yes"]
    passing = [r for r in rows if r.get("flagged") == "no"]

    story.append(Paragraph("Summary", h2))
    story.append(Paragraph(
        f"Measurements captured: {len(rows)}. Violations flagged: {len(flagged)}. "
        f"Passing: {len(passing)}.",
        body))

    if flagged:
        story.append(Paragraph("Violations (ADA 405.2 / ramp slope > 1:12)", h2))
        story.append(_rows_table(flagged, body))

    if passing:
        story.append(Paragraph("Passing measurements", h2))
        story.append(_rows_table(passing, body))

    story.append(Spacer(1, 10))
    story.append(Paragraph(
        "Measurements are observational. Final legal attestation of ADA "
        "compliance remains with the Certified Access Specialist "
        "reviewing this report.",
        small))
    story.append(Paragraph(
        "github.com/Locked-Inc/STAR  -  Locked Inc. Capstone Team",
        small))

    doc.build(story)


def _rows_table(rows: Iterable[dict], body_style) -> Table:
    header = [
        Paragraph("<b>run</b>", body_style),
        Paragraph("<b>time</b>", body_style),
        Paragraph("<b>location</b>", body_style),
        Paragraph("<b>slope (deg)</b>", body_style),
        Paragraph("<b>GT (deg)</b>", body_style),
        Paragraph("<b>agreement</b>", body_style),
        Paragraph("<b>notes</b>", body_style),
    ]
    data = [header]
    for r in rows:
        slope = r.get("ramp_slope_lidar_deg", "") or r.get("ramp_slope_bno055_deg", "")
        data.append([
            Paragraph(r.get("run_id", ""), body_style),
            Paragraph(f"{r.get('date','')} {r.get('time','')}", body_style),
            Paragraph(r.get("location", ""), body_style),
            Paragraph(slope, body_style),
            Paragraph(r.get("ramp_slope_gt_deg", ""), body_style),
            Paragraph(r.get("bno055_lidar_agreement_deg", ""), body_style),
            Paragraph(r.get("notes", ""), body_style),
        ])

    t = Table(data, colWidths=[0.9 * inch, 1.3 * inch, 1.3 * inch,
                                0.7 * inch, 0.7 * inch, 0.8 * inch, 1.4 * inch])
    t.setStyle(TableStyle([
        ("FONTSIZE", (0, 0), (-1, -1), 8.5),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.5, MAROON),
        ("LINEBELOW", (0, 1), (-1, -1), 0.25, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return t


def main() -> None:  # pragma: no cover
    here = Path(__file__).resolve()
    csv_path = here.parents[3] / "extras" / "validation_log.csv"
    out_pdf = here.parents[3] / "compliance-engine" / "star_audit_report_sample.pdf"
    generate(csv_path, out_pdf)
    print(f"wrote {out_pdf}")


if __name__ == "__main__":  # pragma: no cover
    main()
