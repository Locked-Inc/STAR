"""Chart (a): ADA Title III federal lawsuit trend, 2018-2024."""

import plotly.graph_objects as go
from theme import (
    ACCENT_BLUE, ACCENT_RED, TEXT, MUTED, EXPORT_KW,
)

years = ["2018", "2019", "2020", "2021", "2022", "2023", "2024"]
filings = [10163, 11053, 10982, 11452, 8694, 8227, 8800]
colors = [ACCENT_BLUE] * 6 + [ACCENT_RED]
labels = [f"{v:,}" for v in filings]

fig = go.Figure(go.Bar(
    x=years, y=filings, marker_color=colors, text=labels,
    textposition="outside", textfont=dict(size=18, color=TEXT),
    hovertemplate="<b>%{x}</b><br>%{y:,} filings<extra></extra>",
))

fig.add_annotation(
    x="2024", y=8800, ax=40, ay=-90, showarrow=True, arrowhead=3,
    arrowcolor=ACCENT_RED, arrowwidth=2,
    text="<b>+7% YoY</b><br>rebound after 2-yr decline",
    font=dict(size=18, color=ACCENT_RED),
    bgcolor="rgba(230,57,70,0.12)", bordercolor=ACCENT_RED, borderwidth=1,
)

fig.update_layout(
    title="ADA Title III federal lawsuits surged back to 8,800 in 2024",
    xaxis_title="Year",
    yaxis_title="Federal lawsuit filings (total Title III)",
    yaxis=dict(range=[0, 13000]),
    margin=dict(l=80, r=60, t=90, b=160),  # roomy enough for y=-0.17 source footer
    annotations=[
        *fig.layout.annotations,  # preserve the +7% YoY callout added above
        dict(
            text="Source: Seyfarth Shaw ADA Title III Report, March 2025. "
                 "Figures are TOTAL Title III (physical + website), not website-only.",
            showarrow=False, x=0, y=-0.15, xref="paper", yref="paper",
            font=dict(size=12, color=MUTED),
        ),
    ],
)

fig.write_image("output/chart_a_lawsuit_trend.png", **EXPORT_KW)
print("wrote output/chart_a_lawsuit_trend.png")
