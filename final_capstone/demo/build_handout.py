"""
Build the 1-page demo-day booth handout PDF (letter, landscape).

Four panels: numbers | what STAR is | compliance status | validation.

Usage:
    cd final_capstone
    python3 demo/build_handout.py

Output: demo/STAR_DemoHandout.pdf
"""

from pathlib import Path
from reportlab.lib.pagesizes import letter, landscape
from reportlab.lib.units import inch
from reportlab.lib.colors import HexColor, black, white
from reportlab.pdfgen import canvas

HERE = Path(__file__).resolve().parent
OUT = HERE / "STAR_DemoHandout.pdf"

MAROON = HexColor("#500000")
CHARCOAL = HexColor("#1a1a2e")
ACCENT = HexColor("#e63946")
GREEN = HexColor("#2a9d8f")
GOLD = HexColor("#c26e1a")
MUTED = HexColor("#666666")
LIGHT = HexColor("#f5f5f7")


def main():
    c = canvas.Canvas(str(OUT), pagesize=landscape(letter))
    w, h = landscape(letter)

    # Masthead
    c.setFillColor(MAROON)
    c.rect(0, h - 1.0 * inch, w, 1.0 * inch, fill=1, stroke=0)
    c.setFillColor(CHARCOAL)
    c.rect(0, h - 1.08 * inch, w, 0.08 * inch, fill=1, stroke=0)
    c.setFillColor(white)
    c.setFont("Helvetica-Bold", 28)
    c.drawString(0.5 * inch, h - 0.55 * inch, "STAR")
    c.setFont("Helvetica-Bold", 14)
    c.drawString(1.55 * inch, h - 0.55 * inch, "Spatial Topography Accessibility Robot")
    c.setFont("Helvetica", 10)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawString(0.5 * inch, h - 0.85 * inch,
                 "Locked Inc.  |  TAMU ESET Capstone, Spring 2026  "
                 "|  github.com/Locked-Inc/STAR")
    c.setFont("Helvetica-Bold", 11)
    c.setFillColor(white)
    c.drawRightString(w - 0.5 * inch, h - 0.45 * inch,
                      "RPi5 + custom RX72N PCB + ROS2 Jazzy")
    c.setFont("Helvetica", 9)
    c.setFillColor(HexColor("#e8e8f0"))
    c.drawRightString(w - 0.5 * inch, h - 0.7 * inch,
                      "Open-source platform for indoor ADA measurement")

    top = h - 1.2 * inch
    bot = 0.5 * inch
    panel_h = top - bot
    gap = 0.15 * inch
    panel_w = (w - inch - 3 * gap) / 4
    xs = [0.5 * inch + i * (panel_w + gap) for i in range(4)]

    # -- Panel 1: numbers
    x = xs[0]
    c.setFillColor(LIGHT)
    c.rect(x, bot, panel_w, panel_h, fill=1, stroke=0)
    c.setFillColor(MAROON)
    c.setFont("Helvetica-Bold", 13)
    c.drawString(x + 0.15 * inch, top - 0.3 * inch, "Scale of the problem")

    rows = [
        ("8,800",      "ADA Title III federal lawsuits, 2024"),
        ("$236,451",   "DOJ max penalty per repeat violation (2025)"),
        ("~73%",       "U.S. buildings fail >= 1 ADA standard (industry est.)"),
        ("~70M",       "U.S. adults with a disability (CDC 2022)"),
        ("32%",        "of buyers ask about ADA at inspection"),
        ("$800-$50K+", "per CASp audit; 2-5 days on-site"),
        ("Hundreds",   "of buildings on the TAMU College Station campus"),
    ]
    y = top - 0.7 * inch
    for big, small in rows:
        c.setFillColor(ACCENT)
        c.setFont("Helvetica-Bold", 16)
        c.drawString(x + 0.15 * inch, y, big)
        c.setFillColor(black)
        c.setFont("Helvetica", 9)
        c.drawString(x + 0.15 * inch, y - 0.16 * inch, small)
        y -= 0.5 * inch

    # -- Panel 2: what STAR is
    x = xs[1]
    c.setFillColor(white)
    c.rect(x, bot, panel_w, panel_h, fill=1, stroke=1)
    c.setFillColor(MAROON)
    c.setFont("Helvetica-Bold", 13)
    c.drawString(x + 0.15 * inch, top - 0.3 * inch, "What STAR is")

    text = c.beginText(x + 0.15 * inch, top - 0.6 * inch)
    text.setFillColor(black)
    text.setFont("Helvetica", 9.5)
    text.setLeading(12)
    for line in [
        "Distributed robot:",
        "- Raspberry Pi 5 + custom RX72N PCB",
        "- SPI 10 Mbps + HARQ + FEC + nanopb",
        "- 143 ROS2 tests passing",
        "",
        "Sensors (all wired, all tested):",
        "- RPLiDAR C1 (360, 10 Hz, 12 m)",
        "- Waveshare IMX219-83 stereo",
        "   (dual 8 MP, 60 mm baseline)",
        "- BNO055 IMU on chassis",
        "- ICM20948 IMU on camera",
        "- 4x HC-SR04 ultrasonic",
        "- DS18B20 + BMP280",
        "",
        "Motor drivetrain:",
        "- 4x DFRobot FIT0520 gearmotors",
        "- 4x TI DRV8263H H-bridges",
        "- GPTW 32-bit PWM @ 20 kHz",
        "- MTU/TPU quadrature encoders",
        "- 250 Hz PID on RX72N",
        "",
        "Software: slam_toolbox async +",
        "robot_localization EKF + Nav2 +",
        "m-explore-ros2 + compliance engine",
        "",
        "Total BOM: under $2,000.",
    ]:
        text.textLine(line)
    c.drawText(text)

    # -- Panel 3: compliance status
    x = xs[2]
    c.setFillColor(LIGHT)
    c.rect(x, bot, panel_w, panel_h, fill=1, stroke=0)
    c.setFillColor(MAROON)
    c.setFont("Helvetica-Bold", 13)
    c.drawString(x + 0.15 * inch, top - 0.3 * inch, "7 ADA checks - status")

    rows3 = [
        ("1. Ramp slope > 1:12", "ADA 405.2", "IMPLEMENTED", GREEN),
        ("2. Trip hazard > 0.25\"", "ADA 303", "STRETCH", GOLD),
        ("3. Path width < 36\"", "ADA 403.5", "STRETCH", GOLD),
        ("4. Ramp width < 36\"", "ADA 405.5", "architected", MUTED),
        ("5. Ramp landing < 5x5 ft", "ADA 405.7", "architected", MUTED),
        ("6. Door clear < 32\"", "ADA 404.2.3", "architected", MUTED),
        ("7. Door threshold > 0.5\"", "ADA 404.2.5", "architected", MUTED),
    ]
    y = top - 0.7 * inch
    for label, ada, status, color in rows3:
        c.setFillColor(black)
        c.setFont("Helvetica-Bold", 10)
        c.drawString(x + 0.15 * inch, y, label)
        c.setFillColor(MUTED)
        c.setFont("Helvetica", 8.5)
        c.drawString(x + 0.15 * inch, y - 0.15 * inch, ada)
        c.setFillColor(color)
        c.setFont("Helvetica-Bold", 9)
        c.drawRightString(x + panel_w - 0.15 * inch, y, status)
        y -= 0.42 * inch

    c.setFillColor(MUTED)
    c.setFont("Helvetica-Oblique", 8)
    c.drawString(x + 0.15 * inch, bot + 0.25 * inch,
                 "Implementation code in compliance-engine/")

    # -- Panel 4: validation + ask
    x = xs[3]
    c.setFillColor(white)
    c.rect(x, bot, panel_w, panel_h, fill=1, stroke=1)
    c.setFillColor(MAROON)
    c.setFont("Helvetica-Bold", 13)
    c.drawString(x + 0.15 * inch, top - 0.3 * inch, "Validation & ask")

    text = c.beginText(x + 0.15 * inch, top - 0.6 * inch)
    text.setFillColor(black)
    text.setFont("Helvetica", 9.5)
    text.setLeading(12)
    for line in [
        "Platform validation (done):",
        "  143 / 143 ROS2 tests passing",
        "  slam_toolbox /map at 0.5 Hz",
        "  Full TF chain active",
        "  Nav2 autonomous goals run",
        "  Frontier exploration verified",
        "  SPI 1.6% utilization at 10 Mbps",
        "",
        "Ramp-slope check validation:",
        "  Wixey WR300 (+/- 0.1 deg) truth",
        "  BNO055 vs. LiDAR normal, +/- 0.5 deg gate",
        "  On-campus ramp, n=5-30 sessions",
        "  Live numbers: validation_log.csv",
        "",
        "At TAMU scale:",
        "  5,200 acres, hundreds of buildings",
        "  Manual audit = multi-million-dollar cost",
        "  STAR = amortized hardware cost,",
        "  re-runnable monthly",
    ]:
        text.textLine(line)
    c.drawText(text)

    c.setFillColor(MAROON)
    c.rect(x + 0.1 * inch, bot + 0.15 * inch, panel_w - 0.2 * inch, 0.55 * inch,
           fill=1, stroke=0)
    c.setFillColor(white)
    c.setFont("Helvetica-Bold", 10)
    c.drawString(x + 0.25 * inch, bot + 0.45 * inch, "github.com/Locked-Inc/STAR")
    c.setFont("Helvetica", 8)
    c.drawString(x + 0.25 * inch, bot + 0.27 * inch,
                 "Pilot building partnerships welcome.")

    c.showPage()
    c.save()
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
