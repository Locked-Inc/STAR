"""Chart (d): U.S. disability prevalence breakdown, 2022 BRFSS (released 2024)."""

import plotly.graph_objects as go
from theme import (
    ACCENT_RED, MAROON, ACCENT_GOLD, ACCENT_BLUE, ACCENT_PURPLE, ACCENT_GREEN,
    TEXT, MUTED, EXPORT_KW,
)

types = ["Cognition", "Mobility", "Independent Living",
         "Hearing", "Vision", "Self-care"]
pct = [13.9, 12.2, 7.7, 6.2, 5.5, 3.6]
colors = [ACCENT_RED, MAROON, ACCENT_GOLD, ACCENT_BLUE, ACCENT_PURPLE, ACCENT_GREEN]

fig = go.Figure(go.Bar(
    x=pct, y=types, orientation="h",
    marker_color=colors,
    text=[f"{v}%" for v in pct],
    textposition="outside",
    textfont=dict(color=TEXT, size=18),
    cliponaxis=False,  # fix: prevent label clipping on the right edge
))

fig.update_layout(
    title="More than 1 in 4 U.S. adults live with a disability (28.7%, ~70M)",
    xaxis_title="% of U.S. adults",
    yaxis=dict(autorange="reversed"),
    xaxis=dict(range=[0, 17]),
    margin=dict(l=210, r=80, t=90, b=170),
    annotations=[
        dict(
            text="Source: CDC Disability and Health Data System, 2022 BRFSS (released 2024). "
                 "Categories overlap; many adults report multiple disability types.",
            showarrow=False, x=0, y=-0.15, xref="paper", yref="paper",
            font=dict(size=12, color=MUTED),
        ),
    ],
)

fig.write_image("output/chart_d_disability_breakdown.png", **EXPORT_KW)
print("wrote output/chart_d_disability_breakdown.png")
