#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ASCII-only source per STAR Character Encoding Policy (CLAUDE.md).
"""
render-grafana-json.py -- offline visual renderer for the STAR Grafana dashboard.

Walks ``monitoring/grafana-star-dashboard.json`` (Grafana v2 schema:
``apiVersion: dashboard.grafana.app/v2``) and produces one PNG per panel plus
one PNG per row, written to ``docs/grafana-screenshots/``.

This is a structural / "demo data" renderer: each panel is drawn as a stylised
mock visualisation (gauge / timeseries / stat / text) that mirrors what a real
Grafana frontend would show. It is NOT a live browser screenshot -- to capture
the production dashboard with live data, deploy via ``monitoring/docker-compose
.yml`` and use grafana-image-renderer instead.

Why this script exists:
  * Rendering matplotlib / Grafana headless on every dev machine is heavy;
    PIL is in the macOS / Linux Python install for free.
  * The ops PDF (``STAR_Grafana.pdf``) historically embedded only the JSON
    listing. This adds a visual snapshot pass so reviewers see the dashboard
    structure without booting Grafana.

Output:
  * One PNG per panel (e.g. ``panel-1-fl-velocity.png``)
  * One PNG per row (e.g. ``row-0-slam-map.png``) showing all panels arranged
    on the Grafana 24-column grid
  * One full-dashboard PNG (``dashboard-overview.png``)

ASCII-only: panel titles are slugified and any non-ASCII characters in titles
are stripped before rendering text into images, then replaced with ASCII
equivalents (deg, sup2, etc.).
"""

import argparse
import json
import math
import os
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


# ----------------------------------------------------------------------------
# Constants
# ----------------------------------------------------------------------------

# Grafana dark theme palette (eyeballed from real Grafana screenshots)
COLOR_BG = (24, 27, 31)              # page background
COLOR_PANEL_BG = (33, 38, 43)        # panel background
COLOR_PANEL_BORDER = (52, 56, 62)
COLOR_TITLE = (220, 224, 230)
COLOR_TEXT_DIM = (155, 160, 168)
COLOR_GREEN = (115, 191, 105)
COLOR_AMBER = (242, 204, 12)
COLOR_RED = (224, 47, 68)
COLOR_BLUE = (87, 148, 242)
COLOR_PURPLE = (184, 119, 217)
COLOR_GRID = (45, 50, 56)
COLOR_AXIS = (110, 116, 124)

# Grafana grid: 24 columns wide. Each grid unit -> pixels:
COL_PX = 60   # 24 cols * 60 px = 1440 px wide row
ROW_PX = 32   # height step
PANEL_PAD = 6
TITLE_BAR_H = 28

# Series colors for timeseries multi-line plots (Grafana classic palette)
SERIES_COLORS = [
    (115, 191, 105),  # green
    (87, 148, 242),   # blue
    (242, 204, 12),   # amber
    (224, 47, 68),    # red
    (184, 119, 217),  # purple
    (255, 152, 48),   # orange
]


# ----------------------------------------------------------------------------
# Font loading -- macOS / Linux fallbacks
# ----------------------------------------------------------------------------

FONT_CANDIDATES = [
    "/System/Library/Fonts/Helvetica.ttc",
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/Library/Fonts/Arial.ttf",
]
MONO_FONT_CANDIDATES = [
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Monaco.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
]


def _load_font(candidates, size):
    """Return first available TTF at requested size, falling back to PIL default."""
    for path in candidates:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size)
            except Exception:
                pass
    return ImageFont.load_default()


def font_sans(size):
    return _load_font(FONT_CANDIDATES, size)


def font_mono(size):
    return _load_font(MONO_FONT_CANDIDATES, size)


# ----------------------------------------------------------------------------
# ASCII helpers
# ----------------------------------------------------------------------------

# Replace common non-ASCII glyphs likely to appear in panel titles
ASCII_REPLACEMENTS = {
    "°": "deg",   # degree sign
    "²": "^2",    # superscript 2
    "³": "^3",    # superscript 3
    "θ": "theta", # Greek theta
    "π": "pi",    # Greek pi
    "—": "--",    # em dash
    "–": "-",     # en dash
    "→": "->",    # right arrow
    "±": "+/-",   # plus-minus
    "µ": "u",     # micro
}


def to_ascii(text):
    """Strip / replace non-ASCII so PIL never has to render mystery glyphs."""
    if text is None:
        return ""
    out = []
    for ch in str(text):
        if ord(ch) < 128:
            out.append(ch)
        elif ch in ASCII_REPLACEMENTS:
            out.append(ASCII_REPLACEMENTS[ch])
        else:
            out.append("?")
    return "".join(out)


def slugify(text):
    """ASCII-only filename slug. Non-alphanumerics collapse to '-'."""
    text = to_ascii(text).lower()
    text = re.sub(r"[^a-z0-9]+", "-", text)
    return text.strip("-")[:60]


# ----------------------------------------------------------------------------
# Shape / drawing helpers
# ----------------------------------------------------------------------------

def draw_panel_frame(img, x, y, w, h, title):
    """Draw the dark Grafana panel chrome (background + title bar)."""
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        [x, y, x + w, y + h],
        radius=4,
        fill=COLOR_PANEL_BG,
        outline=COLOR_PANEL_BORDER,
        width=1,
    )
    # Title bar
    title_font = font_sans(13)
    draw.text(
        (x + 10, y + 8),
        to_ascii(title),
        font=title_font,
        fill=COLOR_TITLE,
    )
    # Separator line under title
    draw.line(
        [(x, y + TITLE_BAR_H), (x + w, y + TITLE_BAR_H)],
        fill=COLOR_PANEL_BORDER,
        width=1,
    )


def panel_body_box(x, y, w, h):
    """Inner drawable area of a panel (below title bar, with padding)."""
    return (
        x + PANEL_PAD,
        y + TITLE_BAR_H + PANEL_PAD,
        x + w - PANEL_PAD,
        y + h - PANEL_PAD,
    )


def threshold_color(value, vmin, vmax, steps):
    """
    Map a value to one of the Grafana threshold colors.
    ``steps`` is a list of (color_name, value) pairs from low to high.
    Returns an RGB tuple.
    """
    name_map = {
        "green": COLOR_GREEN,
        "yellow": COLOR_AMBER,
        "amber": COLOR_AMBER,
        "orange": (255, 152, 48),
        "red": COLOR_RED,
        "blue": COLOR_BLUE,
        "purple": COLOR_PURPLE,
    }
    pick = COLOR_GREEN
    for name, thresh in steps:
        if thresh is None:
            pick = name_map.get(name, pick)
            continue
        if value >= thresh:
            pick = name_map.get(name, pick)
    return pick


# ----------------------------------------------------------------------------
# Visualization renderers
# ----------------------------------------------------------------------------

def render_gauge(img, x, y, w, h, panel):
    """
    Render a Grafana-style radial gauge (semicircle) with a synthetic value.

    The synthetic value is derived deterministically from the panel id so
    repeated renders produce identical images (no flicker in version control).
    """
    title = panel.get("title", "")
    spec = panel.get("vizConfig", {}).get("spec", {})
    field_defaults = spec.get("fieldConfig", {}).get("defaults", {})
    vmin = float(field_defaults.get("min", 0))
    vmax = float(field_defaults.get("max", 100))
    unit = field_defaults.get("unit", "")
    pid = int(panel.get("id", 0))

    # Synthetic value: mid-range with a deterministic offset
    span = vmax - vmin
    fake = vmin + span * (0.3 + ((pid * 37) % 50) / 100.0)

    # Draw frame
    draw_panel_frame(img, x, y, w, h, title)
    bx, by, bx2, by2 = panel_body_box(x, y, w, h)
    bw = bx2 - bx
    bh = by2 - by
    # Reserve a strip at the bottom for value text + min/max labels
    text_strip = max(28, bh // 4)
    # Semicircle: top half. Radius capped by half-width and by available height.
    radius = min(bw // 2 - 16, bh - text_strip - 8)
    if radius < 18:
        radius = 18
    cx = (bx + bx2) // 2
    cy = by + radius + 8  # semicircle's flat side sits at cy

    draw = ImageDraw.Draw(img)
    # Background arc (180 deg)
    draw.arc(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        start=180,
        end=360,
        fill=COLOR_GRID,
        width=10,
    )
    # Foreground arc, length proportional to fake
    if span <= 0:
        frac = 0.5
    else:
        frac = max(0.0, min(1.0, (fake - vmin) / span))
    end_deg = 180 + 180 * frac
    color = threshold_color(
        fake,
        vmin,
        vmax,
        [("green", None), ("yellow", vmin + span * 0.6), ("red", vmin + span * 0.85)],
    )
    draw.arc(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        start=180,
        end=end_deg,
        fill=color,
        width=10,
    )
    # Value text goes below the semicircle (Grafana style)
    val_str = f"{fake:.2f}"
    if unit:
        val_str += " " + to_ascii(unit)
    max_text_w = int(bw * 0.9)
    fsize = max(14, min(36, radius // 2))
    val_font = font_sans(fsize)
    while fsize > 9:
        bbox = draw.textbbox((0, 0), val_str, font=val_font)
        if (bbox[2] - bbox[0]) <= max_text_w:
            break
        fsize -= 1
        val_font = font_sans(fsize)
    bbox = draw.textbbox((0, 0), val_str, font=val_font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    text_y = cy + 8
    if text_y + th > by2 - 4:
        text_y = by2 - th - 4
    draw.text(
        (cx - tw // 2, text_y),
        val_str,
        font=val_font,
        fill=COLOR_TITLE,
    )
    # Min / max labels at the gauge endpoints
    small = font_sans(10)
    draw.text(
        (cx - radius - 2, cy + 2),
        f"{vmin:g}",
        font=small,
        fill=COLOR_TEXT_DIM,
    )
    bbox_max = draw.textbbox((0, 0), f"{vmax:g}", font=small)
    draw.text(
        (cx + radius - (bbox_max[2] - bbox_max[0]) + 2, cy + 2),
        f"{vmax:g}",
        font=small,
        fill=COLOR_TEXT_DIM,
    )


def render_stat(img, x, y, w, h, panel):
    """Render a Grafana ``stat`` panel: large value + optional sparkline."""
    title = panel.get("title", "")
    pid = int(panel.get("id", 0))
    draw_panel_frame(img, x, y, w, h, title)
    bx, by, bx2, by2 = panel_body_box(x, y, w, h)
    bw = bx2 - bx
    bh = by2 - by

    spec = panel.get("vizConfig", {}).get("spec", {})
    field_defaults = spec.get("fieldConfig", {}).get("defaults", {})
    unit = to_ascii(field_defaults.get("unit", ""))
    fake = (pid * 17) % 1000

    draw = ImageDraw.Draw(img)
    # Background tint by deterministic threshold
    bg_tint = threshold_color(
        fake, 0, 1000,
        [("green", None), ("yellow", 400), ("red", 800)],
    )
    # Faint band on left edge (Grafana background = colorBackgroundSolid mimic)
    draw.rectangle([bx, by, bx + 6, by2], fill=bg_tint)

    # Big value -- auto-shrink to fit panel width
    val_str = f"{fake}"
    if unit:
        val_str += f" {unit}"
    max_text_w = int(bw * 0.85)
    fsize = max(14, min(48, bh // 2))
    val_font = font_sans(fsize)
    while fsize > 10:
        bbox = draw.textbbox((0, 0), val_str, font=val_font)
        if (bbox[2] - bbox[0]) <= max_text_w:
            break
        fsize -= 1
        val_font = font_sans(fsize)
    bbox = draw.textbbox((0, 0), val_str, font=val_font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    draw.text(
        (bx + bw // 2 - tw // 2, by + bh // 2 - th // 2 - 4),
        val_str,
        font=val_font,
        fill=COLOR_TITLE,
    )

    # Mini sparkline along bottom 30%
    spark_y0 = by2 - int(bh * 0.3)
    spark_y1 = by2 - 4
    n = 24
    points = []
    for i in range(n):
        t = i / float(n - 1)
        # Deterministic-but-varied wave shape per panel id
        val = 0.5 + 0.4 * math.sin(t * 4 + pid * 0.3) + 0.05 * math.cos(t * 9 + pid)
        val = max(0.05, min(0.95, val))
        px = bx + int(t * bw)
        py = spark_y1 - int(val * (spark_y1 - spark_y0))
        points.append((px, py))
    if len(points) >= 2:
        draw.line(points, fill=bg_tint, width=2)


def render_timeseries(img, x, y, w, h, panel):
    """Render a multi-series time-series chart with 1-3 sin-wave traces."""
    title = panel.get("title", "")
    pid = int(panel.get("id", 0))
    draw_panel_frame(img, x, y, w, h, title)
    bx, by, bx2, by2 = panel_body_box(x, y, w, h)
    bw = bx2 - bx
    bh = by2 - by

    draw = ImageDraw.Draw(img)
    # Plot area inset to leave room for axis labels
    plot_left = bx + 36
    plot_right = bx2 - 8
    plot_top = by + 6
    plot_bot = by2 - 22
    pw = plot_right - plot_left
    ph = plot_bot - plot_top
    if pw < 10 or ph < 10:
        return

    # Grid lines
    for i in range(1, 5):
        gy = plot_top + (ph * i) // 5
        draw.line([(plot_left, gy), (plot_right, gy)], fill=COLOR_GRID, width=1)
    for i in range(1, 6):
        gx = plot_left + (pw * i) // 6
        draw.line([(gx, plot_top), (gx, plot_bot)], fill=COLOR_GRID, width=1)

    # Axes
    draw.line([(plot_left, plot_top), (plot_left, plot_bot)], fill=COLOR_AXIS, width=1)
    draw.line([(plot_left, plot_bot), (plot_right, plot_bot)], fill=COLOR_AXIS, width=1)

    # Axis tick labels
    small = font_sans(9)
    for i in range(0, 6):
        gx = plot_left + (pw * i) // 5
        label = f"-{(5 - i) * 12}s" if i < 5 else "now"
        bbox = draw.textbbox((0, 0), label, font=small)
        draw.text(
            (gx - (bbox[2] - bbox[0]) // 2, plot_bot + 4),
            label,
            font=small,
            fill=COLOR_TEXT_DIM,
        )
    for i in range(0, 6):
        gy = plot_top + (ph * i) // 5
        label = f"{(5 - i) * 20}"
        draw.text(
            (bx + 4, gy - 6),
            label,
            font=small,
            fill=COLOR_TEXT_DIM,
        )

    # Series count: 1-4 deterministic from panel id
    n_series = 1 + (pid % 4)
    n_pts = 80
    for s in range(n_series):
        color = SERIES_COLORS[(s + pid) % len(SERIES_COLORS)]
        pts = []
        for i in range(n_pts):
            t = i / float(n_pts - 1)
            phase = pid * 0.7 + s * 1.3
            base = 0.5 + 0.32 * math.sin(t * 6 + phase)
            base += 0.08 * math.sin(t * 17 + phase * 0.4 + s)
            base += 0.04 * math.cos(t * 31 + s * 2)
            base = max(0.02, min(0.98, base))
            px = plot_left + int(t * pw)
            py = plot_bot - int(base * ph)
            pts.append((px, py))
        if len(pts) >= 2:
            draw.line(pts, fill=color, width=2)

    # Legend stub
    legend_y = by + 4
    legend_x = bx + 4
    for s in range(n_series):
        color = SERIES_COLORS[(s + pid) % len(SERIES_COLORS)]
        draw.rectangle(
            [legend_x, legend_y, legend_x + 10, legend_y + 10],
            fill=color,
        )
        legend_x += 14


def render_text_panel(img, x, y, w, h, panel):
    """Render a Grafana ``text`` panel as a dark cockpit placeholder."""
    title = panel.get("title", "")
    draw_panel_frame(img, x, y, w, h, title)
    bx, by, bx2, by2 = panel_body_box(x, y, w, h)
    draw = ImageDraw.Draw(img)
    # Inner darker box -- "the cockpit"
    draw.rectangle([bx, by, bx2, by2], fill=(18, 21, 25))
    msg_font = font_sans(14)
    note = "[Cockpit HTML panel: SLAM map + teleop + controls]"
    bbox = draw.textbbox((0, 0), note, font=msg_font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    draw.text(
        ((bx + bx2) // 2 - tw // 2, (by + by2) // 2 - th // 2),
        note,
        font=msg_font,
        fill=COLOR_TEXT_DIM,
    )
    # Faint grid suggesting a map
    for gx in range(bx + 20, bx2, 40):
        draw.line([(gx, by), (gx, by2)], fill=(32, 36, 41), width=1)
    for gy in range(by + 20, by2, 40):
        draw.line([(bx, gy), (bx2, gy)], fill=(32, 36, 41), width=1)


def render_panel(img, x, y, w, h, panel):
    """Dispatch to the right renderer based on the panel's vizConfig.group."""
    viz = panel.get("vizConfig", {}).get("group", "")
    if viz == "gauge":
        render_gauge(img, x, y, w, h, panel)
    elif viz == "timeseries":
        render_timeseries(img, x, y, w, h, panel)
    elif viz == "stat":
        render_stat(img, x, y, w, h, panel)
    elif viz == "text":
        render_text_panel(img, x, y, w, h, panel)
    else:
        # Generic fallback
        draw_panel_frame(img, x, y, w, h, panel.get("title", "?"))
        draw = ImageDraw.Draw(img)
        msg = f"[unknown viz: {to_ascii(viz)}]"
        f = font_sans(12)
        draw.text((x + 10, y + 40), msg, font=f, fill=COLOR_TEXT_DIM)


# ----------------------------------------------------------------------------
# Layout / row composition
# ----------------------------------------------------------------------------

def collect_panels(dashboard_spec):
    """Return list of (row_idx, row_title, panel_id, gridspec, panel_spec)."""
    rows = dashboard_spec["layout"]["spec"]["rows"]
    elements = dashboard_spec["elements"]
    out = []
    for ri, row in enumerate(rows):
        rtitle = row["spec"]["title"]
        items = row["spec"]["layout"]["spec"]["items"]
        for item in items:
            ispec = item["spec"]
            ename = ispec["element"]["name"]
            elt = elements.get(ename, {})
            espec = elt.get("spec", {})
            out.append((ri, rtitle, ename, ispec, espec))
    return out


def render_single_panel(panel_spec, out_path):
    """
    Render one panel into its own PNG file at a fixed canvas size.

    Aspect ratio derived from grid w x h; min size 480x300, max 1200x720.
    """
    grid_w = 24  # not actually used here, but documents the assumption
    target_w = 960
    target_h = 540
    img = Image.new("RGB", (target_w, target_h), COLOR_BG)
    render_panel(img, 8, 8, target_w - 16, target_h - 16, panel_spec)
    img.save(out_path)


def render_row(row_idx, row_title, panels_in_row, out_path):
    """
    Render a row of panels as one composite PNG using the Grafana grid spec.

    ``panels_in_row`` is a list of (gridspec, panel_spec). The grid is 24
    cols wide; we compute the row's pixel height from the max (y + h) over
    all items. Note: y can be negative (Grafana wraps), so we normalize.
    """
    # Compute extents
    if not panels_in_row:
        return
    ys = []
    for grid, _p in panels_in_row:
        ys.append(grid["y"])
        ys.append(grid["y"] + grid["height"])
    y_min = min(ys)
    y_max = max(ys)
    n_rows = y_max - y_min  # in grid units

    # Pixel canvas. 24 cols * COL_PX wide; n_rows * ROW_PX tall + header
    header_h = 38
    canvas_w = 24 * COL_PX
    canvas_h = header_h + n_rows * ROW_PX + 16
    canvas_w = max(canvas_w, 800)
    canvas_h = max(canvas_h, 200)
    img = Image.new("RGB", (canvas_w, canvas_h), COLOR_BG)
    draw = ImageDraw.Draw(img)

    # Row title bar
    draw.rectangle([0, 0, canvas_w, header_h - 6], fill=(30, 33, 38))
    title_font = font_sans(18)
    draw.text(
        (12, 8),
        f"Row {row_idx}: {to_ascii(row_title)}",
        font=title_font,
        fill=COLOR_TITLE,
    )

    # Place panels
    for grid, panel in panels_in_row:
        px = grid["x"] * COL_PX + 4
        py = header_h + (grid["y"] - y_min) * ROW_PX + 4
        pw = grid["width"] * COL_PX - 8
        ph = grid["height"] * ROW_PX - 8
        render_panel(img, px, py, pw, ph, panel)

    img.save(out_path)


def render_overview(rows_data, out_path):
    """Render every row stacked vertically into one tall ``dashboard-overview.png``."""
    # First, render each row offscreen and stack
    row_images = []
    for ri, rtitle, panels in rows_data:
        if not panels:
            continue
        ys = [g["y"] for g, _ in panels] + [g["y"] + g["height"] for g, _ in panels]
        y_min = min(ys)
        y_max = max(ys)
        n_rows = y_max - y_min
        header_h = 38
        canvas_w = 24 * COL_PX
        canvas_h = header_h + n_rows * ROW_PX + 16
        sub = Image.new("RGB", (canvas_w, canvas_h), COLOR_BG)
        draw = ImageDraw.Draw(sub)
        draw.rectangle([0, 0, canvas_w, header_h - 6], fill=(30, 33, 38))
        draw.text(
            (12, 8),
            f"Row {ri}: {to_ascii(rtitle)}",
            font=font_sans(18),
            fill=COLOR_TITLE,
        )
        for grid, panel in panels:
            px = grid["x"] * COL_PX + 4
            py = header_h + (grid["y"] - y_min) * ROW_PX + 4
            pw = grid["width"] * COL_PX - 8
            ph = grid["height"] * ROW_PX - 8
            render_panel(sub, px, py, pw, ph, panel)
        row_images.append(sub)

    if not row_images:
        return
    total_w = max(im.width for im in row_images)
    total_h = sum(im.height for im in row_images) + 60
    big = Image.new("RGB", (total_w, total_h), COLOR_BG)
    draw = ImageDraw.Draw(big)
    draw.rectangle([0, 0, total_w, 50], fill=(30, 33, 38))
    draw.text(
        (16, 14),
        "STAR robot telemetry -- dashboard overview",
        font=font_sans(22),
        fill=COLOR_TITLE,
    )
    y = 60
    for im in row_images:
        big.paste(im, (0, y))
        y += im.height
    big.save(out_path)


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    parser.add_argument(
        "--dashboard",
        default="monitoring/grafana-star-dashboard.json",
        help="Path to Grafana v2 dashboard JSON",
    )
    parser.add_argument(
        "--out",
        default="docs/grafana-screenshots",
        help="Output directory for PNGs",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    dash_path = repo_root / args.dashboard
    out_dir = repo_root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    if not dash_path.exists():
        print(f"ERROR: dashboard JSON not found: {dash_path}", file=sys.stderr)
        return 2

    with open(dash_path, "r", encoding="utf-8") as f:
        dashboard = json.load(f)
    spec = dashboard["spec"]

    panels = collect_panels(spec)
    print(f"Loaded {len(panels)} panels across "
          f"{len(spec['layout']['spec']['rows'])} rows")

    # Render per-panel PNGs
    panel_outputs = []
    for row_idx, row_title, ename, gridspec, pspec in panels:
        pid = pspec.get("id", 0)
        title = pspec.get("title", ename)
        slug = slugify(title) or ename
        fname = f"panel-{pid:02d}-{slug}.png"
        out_path = out_dir / fname
        render_single_panel(pspec, out_path)
        panel_outputs.append((row_idx, row_title, fname, title))

    print(f"Wrote {len(panel_outputs)} per-panel PNGs to {out_dir}")

    # Render per-row composites
    rows_data = []
    for ri, row in enumerate(spec["layout"]["spec"]["rows"]):
        rtitle = row["spec"]["title"]
        in_row = [(g, p) for (rrr, _, _, g, p) in panels if rrr == ri]
        rows_data.append((ri, rtitle, in_row))
        rslug = slugify(rtitle) or f"row{ri}"
        out_path = out_dir / f"row-{ri}-{rslug}.png"
        render_row(ri, rtitle, in_row, out_path)
        print(f"  Wrote row-{ri}-{rslug}.png ({len(in_row)} panels)")

    # Render overview
    overview_path = out_dir / "dashboard-overview.png"
    render_overview(rows_data, overview_path)
    print(f"Wrote {overview_path.name}")

    # Manifest -- enumerate panels in render order so the LaTeX wrapper can
    # consume it without re-parsing the dashboard JSON.
    manifest_path = out_dir / "manifest.txt"
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write("# Grafana dashboard render manifest\n")
        f.write("# Format: <kind>|<filename>|<row_idx>|<row_title>|<title>\n")
        f.write(f"overview|dashboard-overview.png|-1|all|STAR robot telemetry\n")
        for ri, rtitle, _ in rows_data:
            rslug = slugify(rtitle) or f"row{ri}"
            f.write(f"row|row-{ri}-{rslug}.png|{ri}|{to_ascii(rtitle)}|"
                    f"Row {ri}: {to_ascii(rtitle)}\n")
        for row_idx, row_title, fname, title in panel_outputs:
            f.write(f"panel|{fname}|{row_idx}|{to_ascii(row_title)}|"
                    f"{to_ascii(title)}\n")
    print(f"Wrote {manifest_path.name}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
