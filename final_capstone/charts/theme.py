"""
STAR capstone - Locked Inc. unified Plotly theme.

Import this at the top of every chart script:

    from theme import *

Run order for the full chart set:

    pip install -r ../requirements.txt
    mkdir -p output
    for f in chart_*.py; do python3 "$f"; done

Each script writes a 1920x1080 @ scale=2 PNG into ./output/.
"""

import plotly.graph_objects as go
import plotly.io as pio

BG            = "#1a1a2e"
PANEL         = "#16213e"
TEXT          = "#e8e8f0"
GRID          = "#2a2a4a"
ACCENT_RED    = "#e63946"
ACCENT_GREEN  = "#2a9d8f"
ACCENT_BLUE   = "#4cc9f0"
ACCENT_GOLD   = "#f4a261"
ACCENT_PURPLE = "#9d4edd"
MAROON        = "#500000"  # TAMU maroon, for accent highlights
MUTED         = "#9090a8"  # source-footer color

LOCKED_INC_TEMPLATE = go.layout.Template(
    layout=dict(
        paper_bgcolor=BG,
        plot_bgcolor=BG,
        font=dict(family="Inter, Helvetica, Arial, sans-serif",
                  color=TEXT, size=16),
        title=dict(font=dict(size=26, color=TEXT), x=0.02, xanchor="left"),
        xaxis=dict(gridcolor=GRID, zerolinecolor=GRID, linecolor=GRID),
        yaxis=dict(gridcolor=GRID, zerolinecolor=GRID, linecolor=GRID),
        legend=dict(bgcolor="rgba(0,0,0,0)", font=dict(color=TEXT)),
        margin=dict(l=80, r=60, t=90, b=70),
    )
)
pio.templates["locked_inc"] = LOCKED_INC_TEMPLATE
pio.templates.default = "locked_inc"

EXPORT_KW = dict(width=1920, height=1080, scale=2)  # 4K-equivalent PNG
