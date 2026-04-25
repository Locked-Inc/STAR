"""Chart (b): 73% of U.S. commercial buildings fail ADA compliance."""

import plotly.graph_objects as go
from theme import (
    ACCENT_RED, ACCENT_GREEN, TEXT, BG, MUTED, EXPORT_KW,
)

fig = go.Figure(go.Pie(
    labels=["Fail at least one ADA standard", "Fully compliant"],
    values=[73, 27],
    hole=0.62,
    marker=dict(colors=[ACCENT_RED, ACCENT_GREEN], line=dict(color=BG, width=4)),
    # fix: keep percents inside the slices, push category labels into the legend
    textinfo="percent",
    textposition="inside",
    insidetextfont=dict(size=28, color=TEXT),
    pull=[0.04, 0],
    showlegend=True,
    sort=False,
))

fig.update_layout(
    title="73% of U.S. commercial buildings fail ADA compliance",
    showlegend=True,
    legend=dict(
        orientation="h",
        y=-0.08, x=0.5, xanchor="center",
        font=dict(size=18, color=TEXT),
    ),
    margin=dict(l=40, r=40, t=90, b=200),
    annotations=[
        dict(
            text="<b>73%</b><br><span style='font-size:18px'>non-compliant</span>",
            x=0.5, y=0.5,
            font=dict(size=52, color=ACCENT_RED),
            showarrow=False,
        ),
        dict(
            text="Industry estimate: Building Principles CASp, 2024",
            showarrow=False, x=0.5, y=-0.18, xref="paper", yref="paper",
            font=dict(size=12, color=MUTED),
        ),
    ],
)

fig.write_image("output/chart_b_compliance_donut.png", **EXPORT_KW)
print("wrote output/chart_b_compliance_donut.png")
