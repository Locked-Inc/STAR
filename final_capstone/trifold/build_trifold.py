"""
Build the STAR trifold brochure (landscape letter, 3 panels per side).

Page 1 (outer):  back-cover | inside-cover pitch | front-panel hook
Page 2 (inner):  problem | solution | validation

Print: landscape letter (11 x 8.5 in), duplex, flip on short edge.

Corrected for the real stack: Raspberry Pi 5 + custom RX72N PCB
(not Jetson), Waveshare IMX219-83 stereo (not RealSense), honest
compliance-engine scope (ramp slope IMPLEMENTED, others stretch/
architected).

Usage:
    cd final_capstone
    python3 trifold/build_trifold.py

Output: trifold/STAR_Trifold.pdf
"""

from pathlib import Path

from reportlab.lib.pagesizes import letter, landscape
from reportlab.lib.units import inch
from reportlab.lib.colors import HexColor, black, white
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph, Frame
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OUT = HERE / "STAR_Trifold.pdf"

PAGE = landscape(letter)   # (11in, 8.5in)
MARGIN = 0.25 * inch
PANEL_W = (PAGE[0] - 2 * MARGIN) / 3
PANEL_H = PAGE[1] - 2 * MARGIN

MAROON = HexColor("#500000")
CHARCOAL = HexColor("#1a1a2e")
MUTED = HexColor("#9090a8")
ACCENT = HexColor("#e63946")
GREEN = HexColor("#2a9d8f")
GOLD = HexColor("#c26e1a")
WHITE = white
LIGHT = HexColor("#f5f5f7")


def styles(base_family="Helvetica"):
    return {
        "h1": ParagraphStyle("h1", fontName=f"{base_family}-Bold", fontSize=26,
                             leading=30, textColor=MAROON, alignment=TA_LEFT, spaceAfter=6),
        "h2": ParagraphStyle("h2", fontName=f"{base_family}-Bold", fontSize=12,
                             leading=15, textColor=MAROON, alignment=TA_LEFT, spaceBefore=6, spaceAfter=3),
        "body": ParagraphStyle("body", fontName=base_family, fontSize=9, leading=11.5,
                               textColor=black, alignment=TA_LEFT, spaceAfter=4),
        "lead": ParagraphStyle("lead", fontName=f"{base_family}-Bold", fontSize=10.5,
                               leading=13, textColor=CHARCOAL, spaceAfter=6),
        "stat": ParagraphStyle("stat", fontName=f"{base_family}-Bold", fontSize=34,
                               leading=36, textColor=ACCENT, alignment=TA_LEFT, spaceAfter=0),
        "statlabel": ParagraphStyle("statlabel", fontName=base_family, fontSize=9,
                                    textColor=MUTED, alignment=TA_LEFT, spaceAfter=8),
        "small": ParagraphStyle("small", fontName=base_family, fontSize=8,
                                textColor=MUTED, alignment=TA_LEFT, spaceAfter=3),
    }


def panel_frame(c, col, content, sty, fill=None):
    x = MARGIN + col * PANEL_W
    y = MARGIN
    if fill is not None:
        c.setFillColor(fill)
        c.rect(x - 1, y - 1, PANEL_W + 2, PANEL_H + 2, fill=1, stroke=0)
    inset = 0.3 * inch
    f = Frame(x + inset, y + inset, PANEL_W - 2 * inset, PANEL_H - 2 * inset,
              showBoundary=0, leftPadding=0, rightPadding=0,
              topPadding=0, bottomPadding=0)
    f.addFromList(list(content), c)


def front_panel(c, sty):
    col = 2
    x = MARGIN + col * PANEL_W
    y = MARGIN

    c.setFillColor(MAROON)
    c.rect(x - 1, y - 1, PANEL_W + 2, PANEL_H + 2, fill=1, stroke=0)

    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 68)
    c.drawCentredString(x + PANEL_W / 2, y + PANEL_H - 1.2 * inch, "STAR")
    c.setFont("Helvetica-Bold", 12)
    c.drawCentredString(x + PANEL_W / 2, y + PANEL_H - 1.55 * inch,
                        "Spatial Topography Accessibility Robot")

    c.setStrokeColor(CHARCOAL)
    c.setLineWidth(2)
    c.line(x + 0.75 * inch, y + PANEL_H - 1.85 * inch,
           x + PANEL_W - 0.75 * inch, y + PANEL_H - 1.85 * inch)

    c.setFont("Helvetica-Bold", 46)
    c.setFillColor(WHITE)
    c.drawCentredString(x + PANEL_W / 2, y + 4.4 * inch, "$236,451")
    c.setFont("Helvetica", 9)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawCentredString(x + PANEL_W / 2, y + 4.1 * inch,
                        "max DOJ penalty per ADA repeat violation")

    c.setFont("Helvetica-Bold", 46)
    c.setFillColor(WHITE)
    c.drawCentredString(x + PANEL_W / 2, y + 3.0 * inch, "8,800")
    c.setFont("Helvetica", 9)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawCentredString(x + PANEL_W / 2, y + 2.7 * inch,
                        "ADA Title III federal lawsuits in 2024")

    c.setFont("Helvetica-Bold", 46)
    c.setFillColor(WHITE)
    c.drawCentredString(x + PANEL_W / 2, y + 1.6 * inch, "1 in 4")
    c.setFont("Helvetica", 9)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawCentredString(x + PANEL_W / 2, y + 1.3 * inch,
                        "U.S. adults live with a disability")

    c.setFont("Helvetica", 8.5)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawCentredString(x + PANEL_W / 2, y + 0.4 * inch,
                        "Team Locked Inc.  |  TAMU ESET Capstone, Spring 2026")


def back_panel(c, sty):
    col = 0
    x = MARGIN + col * PANEL_W
    y = MARGIN
    c.setFillColor(CHARCOAL)
    c.rect(x - 1, y - 1, PANEL_W + 2, PANEL_H + 2, fill=1, stroke=0)

    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 16)
    c.drawString(x + 0.4 * inch, y + PANEL_H - 0.7 * inch, "Team Locked Inc.")

    c.setFont("Helvetica", 9.5)
    c.setFillColor(HexColor("#e8e8f0"))
    lines = [
        "Texas A&M University",
        "Department of Engineering Technology",
        "& Industrial Distribution",
        "",
        "ESET Senior Capstone, Spring 2026",
        "",
        "Team Lead           Member 2",
        "Hardware Lead       Software Lead",
        "",
        "Project advisor: (TBD)",
        "Sponsor: (pending TAMU Disability",
        "Resources + OREC + Facilities)",
    ]
    ty = y + PANEL_H - 1.1 * inch
    for ln in lines:
        c.drawString(x + 0.4 * inch, ty, ln)
        ty -= 13

    try:
        import qrcode
        img = qrcode.make("https://github.com/Locked-Inc/STAR")
        tmp = HERE / ".qr_github.png"
        img.save(str(tmp))
        c.drawImage(str(tmp), x + 0.4 * inch, y + 1.7 * inch,
                    width=1.6 * inch, height=1.6 * inch, mask='auto')
    except Exception:
        c.setFillColor(WHITE)
        c.rect(x + 0.4 * inch, y + 1.7 * inch, 1.6 * inch, 1.6 * inch, fill=1, stroke=0)
        c.setFillColor(CHARCOAL)
        c.setFont("Helvetica-Bold", 9)
        c.drawString(x + 0.6 * inch, y + 2.4 * inch, "QR")

    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 10)
    c.drawString(x + 0.4 * inch, y + 1.5 * inch, "github.com/Locked-Inc/STAR")
    c.setFont("Helvetica", 8.5)
    c.setFillColor(HexColor("#9090a8"))
    c.drawString(x + 0.4 * inch, y + 1.3 * inch, "Open-source platform + compliance engine")

    c.setFillColor(HexColor("#f4a261"))
    c.setFont("Helvetica-Bold", 11)
    c.drawString(x + 0.4 * inch, y + 0.7 * inch, "We're seeking")
    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 13)
    c.drawString(x + 0.4 * inch, y + 0.45 * inch, "pilot building partners.")


def inside_cover_panel(c, sty):
    col = 1
    x = MARGIN + col * PANEL_W
    y = MARGIN
    c.setFillColor(LIGHT)
    c.rect(x - 1, y - 1, PANEL_W + 2, PANEL_H + 2, fill=1, stroke=0)
    content = [
        Paragraph("Why this matters", sty["h1"]),
        Paragraph(
            "The Americans with Disabilities Act has been federal law for "
            "35 years. Industry estimates that roughly <b>73% of U.S. "
            "commercial buildings</b> still fail at least one ADA standard.",
            sty["body"]),
        Paragraph("Only 32% of buyers ask at commercial inspection.", sty["lead"]),
        Paragraph(
            "A Certified Access Specialist audit runs <b>$800 to $50,000+</b> "
            "depending on scope and takes <b>2 to 5 days</b> of manual "
            "measurement. For a campus like TAMU's - 5,200 acres, hundreds "
            "of buildings - full manual coverage is effectively impossible.",
            sty["body"]),
        Paragraph("The bottleneck is measurement, not law.", sty["lead"]),
        Paragraph(
            "STAR is the measurement layer. A distributed robot - Raspberry "
            "Pi 5 plus a custom RX72N real-time motor controller, SPI with "
            "HARQ and FEC, RPLiDAR C1 plus IMX219-83 stereo plus two IMUs - "
            "runs ROS2 Jazzy with slam_toolbox and Nav2 and our new ADA "
            "compliance engine on top. One check (ramp slope) is "
            "implemented end-to-end; six more are architected against the "
            "same sensor stack.",
            sty["body"]),
        Paragraph(
            "Same economic shift cybersecurity scanners brought to IT "
            "compliance a decade ago.",
            sty["small"]),
    ]
    panel_frame(c, col, content, sty)


def page2_panels(c, sty):
    # Panel 0: Problem
    content0 = [
        Paragraph("The problem", sty["h1"]),
        Paragraph("$236,451", sty["stat"]),
        Paragraph("max DOJ penalty per repeat violation (90 FR 29445, July 2025)",
                  sty["statlabel"]),
        Paragraph("8,800", sty["stat"]),
        Paragraph("ADA Title III lawsuits in 2024 (Seyfarth Shaw, +7% YoY)",
                  sty["statlabel"]),
        Paragraph("~70 million", sty["stat"]),
        Paragraph("U.S. adults with a disability, 28.7% of adults (CDC 2022 BRFSS)",
                  sty["statlabel"]),
        Paragraph("Manual audit economics", sty["h2"]),
        Paragraph("$800-$50,000+ per building depending on scope.", sty["body"]),
        Paragraph("2-5 days on-site with tape measure, smart level, clipboard.",
                  sty["body"]),
        Paragraph("Only 32% of buyers ask about ADA at commercial inspection.",
                  sty["body"]),
        Paragraph("TAMU: 5,200 acres, hundreds of buildings - full manual pass is infeasible.",
                  sty["body"]),
    ]
    panel_frame(c, 0, content0, sty, fill=LIGHT)

    # Panel 1: Solution
    content1 = [
        Paragraph("Our solution: STAR", sty["h1"]),
        Paragraph("Distributed autonomous platform + ADA compliance engine.",
                  sty["lead"]),
        Paragraph("Platform (demo-ready)", sty["h2"]),
        Paragraph("Raspberry Pi 5 (ROS2 Jazzy) + custom Renesas RX72N PCB (ThreadX).",
                  sty["body"]),
        Paragraph("SPI 10 Mbps with HARQ Chase Combining + FEC K=7 + nanopb + CRC-32.",
                  sty["body"]),
        Paragraph("slam_toolbox async + Nav2 + m-explore-ros2 frontier.", sty["body"]),
        Paragraph("143 ROS2 tests passing, 0 failures.", sty["body"]),
        Paragraph("Sensors", sty["h2"]),
        Paragraph("<b>SLAMTEC RPLiDAR C1</b> - 360 deg, 10 Hz, 12 m, +/- 3 cm.", sty["body"]),
        Paragraph("<b>Waveshare IMX219-83</b> stereo - dual 8 MP, 60 mm baseline, dual CSI-2.",
                  sty["body"]),
        Paragraph("<b>BNO055</b> 9-DoF IMU on chassis + <b>ICM20948</b> 9-DoF IMU on camera.",
                  sty["body"]),
        Paragraph("<b>4x HC-SR04</b> ultrasonic + DS18B20 + BMP280.", sty["body"]),
        Paragraph("Compliance engine (7 ADA checks)", sty["h2"]),
        Paragraph(
            '<font color="#2a9d8f"><b>IMPLEMENTED:</b></font> Ramp slope 405.2<br/>'
            '<font color="#c26e1a"><b>STRETCH:</b></font> Trip hazard 303, Path width 403.5<br/>'
            '<font color="#6c6c82"><b>ARCHITECTED:</b></font> Ramp width, Ramp landing, Door clear, Door threshold',
            sty["body"]),
    ]
    panel_frame(c, 1, content1, sty)

    # Panel 2: Validation
    content2 = [
        Paragraph("Validation", sty["h1"]),
        Paragraph("Platform (complete)", sty["h2"]),
        Paragraph("143 / 143 ROS2 tests passing.", sty["body"]),
        Paragraph("slam_toolbox /map at 0.5 Hz on RPi5; full TF chain.", sty["body"]),
        Paragraph("Nav2 autonomous goals executed; frontier exploration verified.", sty["body"]),
        Paragraph("SPI transport 1.6% utilization at 10 Mbps.", sty["body"]),
        Paragraph("Ramp-slope compliance check", sty["h2"]),
        Paragraph("Ground truth: Wixey WR300 digital angle gauge (+/- 0.1 deg).", sty["body"]),
        Paragraph("LiDAR plane normal and BNO055 pitch, agreement gate +/- 0.5 deg.",
                  sty["body"]),
        Paragraph("On-campus ramp; n per dress-rehearsal session; live validation_log.csv.",
                  sty["body"]),
        Paragraph("Impact at TAMU scale", sty["h2"]),
        Paragraph("5,200 acres, hundreds of buildings.", sty["body"]),
        Paragraph("Manual audit = multi-million-dollar cost.", sty["body"]),
        Paragraph("STAR = amortized hardware cost, re-runnable monthly.", sty["body"]),
        Paragraph("Stakeholders: TAMU Disability Resources + OREC + Facilities.",
                  sty["small"]),
    ]
    panel_frame(c, 2, content2, sty, fill=LIGHT)


def main():
    c = canvas.Canvas(str(OUT), pagesize=PAGE)
    sty = styles()

    back_panel(c, sty)
    inside_cover_panel(c, sty)
    front_panel(c, sty)
    c.showPage()

    page2_panels(c, sty)
    c.showPage()

    c.save()
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
