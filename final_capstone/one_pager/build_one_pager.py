"""
Build the print-ready STAR one-pager PDF.

Letter size, 0.5-inch margins, maroon-and-charcoal masthead, two-column
body. Reflects the real hardware and honest compliance-engine scope.

Usage:
    cd final_capstone
    python3 one_pager/build_one_pager.py

Output: one_pager/STAR_OnePager.pdf
"""

from pathlib import Path

from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib.colors import HexColor, black, white
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.platypus import (
    Paragraph, Spacer, Table, TableStyle, Frame, PageTemplate,
    BaseDocTemplate, FrameBreak,
)

HERE = Path(__file__).resolve().parent
OUT = HERE / "STAR_OnePager.pdf"

MAROON = HexColor("#500000")
CHARCOAL = HexColor("#1a1a2e")
MUTED = HexColor("#666666")
ACCENT = HexColor("#e63946")
GREEN = HexColor("#2a9d8f")
GOLD = HexColor("#c26e1a")
RULE = HexColor("#cccccc")


def masthead(c, doc):
    w, h = letter
    c.setFillColor(MAROON)
    c.rect(0, h - 1.1 * inch, w, 1.1 * inch, fill=1, stroke=0)
    c.setFillColor(CHARCOAL)
    c.rect(0, h - 1.2 * inch, w, 0.1 * inch, fill=1, stroke=0)
    c.setFillColor(white)
    c.setFont("Helvetica-Bold", 22)
    c.drawString(0.5 * inch, h - 0.55 * inch, "STAR")
    c.setFont("Helvetica-Bold", 13)
    c.drawString(1.3 * inch, h - 0.55 * inch, "Spatial Topography Accessibility Robot")
    c.setFont("Helvetica", 10)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawString(0.5 * inch, h - 0.85 * inch,
                 "Locked Inc.  |  Texas A&M University  |  ESET Senior Capstone, Spring 2026  "
                 "|  github.com/Locked-Inc/STAR")


def main():
    styles = getSampleStyleSheet()
    body = ParagraphStyle("body", parent=styles["Normal"],
                          fontName="Helvetica", fontSize=9.2, leading=11.5,
                          textColor=black, spaceAfter=4)
    h2 = ParagraphStyle("h2", parent=styles["Heading2"],
                        fontName="Helvetica-Bold", fontSize=11,
                        textColor=MAROON, spaceBefore=6, spaceAfter=3)
    small = ParagraphStyle("small", parent=body, fontSize=7.5, textColor=MUTED)

    w, h = letter
    top = h - 1.35 * inch
    bot = 0.5 * inch
    col_gap = 0.25 * inch
    col_w = (w - inch - col_gap) / 2
    left_frame = Frame(0.5 * inch, bot, col_w, top - bot,
                       leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0,
                       showBoundary=0)
    right_frame = Frame(0.5 * inch + col_w + col_gap, bot, col_w, top - bot,
                        leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0,
                        showBoundary=0)
    page_tpl = PageTemplate(id="twocol", frames=[left_frame, right_frame],
                            onPage=masthead)
    doc = BaseDocTemplate(str(OUT), pagesize=letter,
                          leftMargin=0.5 * inch, rightMargin=0.5 * inch,
                          topMargin=0.5 * inch, bottomMargin=0.5 * inch,
                          pageTemplates=[page_tpl])

    story = []

    # ---- LEFT COLUMN ----
    story.append(Paragraph("Elevator pitch", h2))
    story.append(Paragraph(
        "STAR is a distributed autonomous ground-robotics platform that "
        "provides the measurement layer for continuous indoor ADA "
        "compliance auditing. A Raspberry Pi 5 runs ROS2 Jazzy (SLAM, "
        "EKF, Nav2, frontier exploration). A custom Renesas RX72N PCB "
        "handles 250 Hz real-time motor control. The two sides talk over "
        "a 10 Mbps SPI link with HARQ, FEC, and nanopb Protocol Buffers. "
        "On top of the platform, we have built an ADA compliance engine "
        "that implements the ramp-slope check end-to-end and architects "
        "six more against the same sensor stack.",
        body))

    story.append(Paragraph("Why this matters", h2))
    story.append(Paragraph(
        "More than 1 in 4 U.S. adults (<b>~70 million</b>) live with a "
        "disability (CDC 2022 BRFSS, released July 2024). Industry "
        "estimates put <b>~73%</b> of U.S. commercial buildings out of "
        "compliance; plaintiffs filed <b>8,800 ADA Title III federal "
        "lawsuits in 2024</b> (+7% YoY, Seyfarth Shaw). DOJ civil "
        "penalties are <b>$118,225 first / $236,451 repeat</b> per 90 FR "
        "29445 (July 2025). Only 32% of commercial property buyers even "
        "ask about ADA during inspection (Focus Building Inspections).",
        body))

    story.append(Paragraph("Sensor stack (all physically wired)", h2))
    sensor_tbl = Table([
        [Paragraph("<b>Sensor</b>", body), Paragraph("<b>Role</b>", body)],
        [Paragraph("RPLiDAR C1 (360, 10 Hz, 12 m)", body),
         Paragraph("Primary geometry, SLAM, ramp plane, path width", body)],
        [Paragraph("Waveshare IMX219-83 stereo", body),
         Paragraph("Dual 8 MP Sony IMX219, 60 mm baseline, dual MIPI CSI-2", body)],
        [Paragraph("BNO055 9-DoF IMU (chassis)", body),
         Paragraph("Ramp-slope pitch cross-validation, jolt detection", body)],
        [Paragraph("ICM20948 9-DoF IMU (camera)", body),
         Paragraph("Visual-inertial reference for stereo pipeline", body)],
        [Paragraph("4x HC-SR04 ultrasonic", body),
         Paragraph("Short-range obstacle safety", body)],
        [Paragraph("DS18B20 + BMP280", body),
         Paragraph("Temperature + barometric telemetry", body)],
    ], colWidths=[col_w * 0.40, col_w * 0.60])
    sensor_tbl.setStyle(TableStyle([
        ("FONTSIZE", (0, 0), (-1, -1), 8.2),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.5, MAROON),
        ("LINEBELOW", (0, 1), (-1, -1), 0.25, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    story.append(sensor_tbl)

    story.append(Paragraph("Compute and transport", h2))
    story.append(Paragraph(
        "<b>Raspberry Pi 5</b> runs ROS2 Jazzy (star_bringup, "
        "star_spi_bridge, star_gateway_bridge, star_safety_monitor), the "
        "Go gateway (gRPC-Web), and the React UI. <b>RX72N</b> "
        "(R5F572NNHxFB, 4 MB Flash, 1 MB SRAM, ThreadX RTOS) runs 250 Hz "
        "PID motor control. <b>SPI 10 Mbps</b> with SYNC-0x55AA framing, "
        "CRC-32, HARQ Chase Combining, convolutional FEC K=7 rate-1/2, "
        "and nanopb protobuf. Peak utilization 1.6%.",
        body))

    story.append(FrameBreak())

    # ---- RIGHT COLUMN ----
    story.append(Paragraph("Compliance engine - implementation status", h2))
    check_rows = [
        [Paragraph("<b>Check</b>", body), Paragraph("<b>ADA</b>", body), Paragraph("<b>Status</b>", body)],
        [Paragraph("Ramp slope > 1:12", body), Paragraph("405.2", body),
         Paragraph('<font color="#2a9d8f"><b>IMPLEMENTED</b></font>', body)],
        [Paragraph("Trip hazard > 0.25 in", body), Paragraph("303", body),
         Paragraph('<font color="#c26e1a"><b>STRETCH</b></font>', body)],
        [Paragraph("Path width < 36 in", body), Paragraph("403.5", body),
         Paragraph('<font color="#c26e1a"><b>STRETCH</b></font>', body)],
        [Paragraph("Ramp width < 36 in", body), Paragraph("405.5", body), Paragraph("architected", body)],
        [Paragraph("Ramp landing < 60x60 in", body), Paragraph("405.7", body), Paragraph("architected", body)],
        [Paragraph("Door clear width < 32 in", body), Paragraph("404.2.3", body), Paragraph("architected", body)],
        [Paragraph("Door threshold > 0.5 in", body), Paragraph("404.2.5", body), Paragraph("architected", body)],
    ]
    check_tbl = Table(check_rows, colWidths=[col_w * 0.55, col_w * 0.15, col_w * 0.30])
    check_tbl.setStyle(TableStyle([
        ("FONTSIZE", (0, 0), (-1, -1), 8.2),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.5, MAROON),
        ("LINEBELOW", (0, 1), (-1, -1), 0.25, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    story.append(check_tbl)

    story.append(Paragraph("Validation", h2))
    story.append(Paragraph(
        "<b>Platform:</b> 143/143 ROS2 tests passing across 4 packages. "
        "slam_toolbox async emits /map at 0.5 Hz on RPi5 with full TF "
        "chain. Nav2 autonomous goals verified; m-explore-ros2 frontier "
        "exploration to completion. 250 Hz PID tuned against first-order "
        "motor model (tau = 75 ms).",
        body))
    story.append(Paragraph(
        "<b>Compliance (ramp slope):</b> ground truth via Wixey WR300 "
        "digital angle gauge (+/- 0.1 deg). Cross-validation gate between "
        "BNO055 pitch and LiDAR plane normal at +/- 0.5 deg. Validation "
        "log in extras/STAR_ValidationLog.xlsx. Numbers on poster and "
        "deck come from the log, not estimates.",
        body))

    story.append(Paragraph("Impact at TAMU scale", h2))
    story.append(Paragraph(
        "Texas A&M's College Station campus covers 5,200 acres with "
        "hundreds of buildings. Any manual CASp audit pass is a "
        "six- or seven-figure cost and runs months of specialist time. "
        "STAR re-uses one robot across buildings at amortized hardware "
        "cost - the same economic shift cybersecurity scanners brought "
        "to IT compliance. Stakeholders under discussion: TAMU Department "
        "of Disability Resources (user advocacy), Office of Risk, Ethics "
        "& Compliance (compliance program), Facilities Services "
        "(remediation). Code is open source under the Locked-Inc GitHub "
        "organization.",
        body))

    story.append(Paragraph("Team Locked Inc.", h2))
    story.append(Paragraph(
        "Four-member ESET capstone team. Contact on reverse.", body))

    story.append(Spacer(1, 6))
    story.append(Paragraph(
        "Sources (verified against primary documents; full matrix at "
        "research/source_verification.md and BibTeX at "
        "extras/references.bib): Seyfarth Shaw ADA Title III Report "
        "March 2025; Federal Register 90 FR 29445 (2025-12494); CDC "
        "DHDS 2022 BRFSS released July 2024; Okoro et al. MMWR 2018; "
        "67(32):882-887; Sun et al. Sensors 2024, 24(21):6828; ADA 2010 "
        "Standards for Accessible Design.",
        small))

    doc.build(story)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
