"""Chart (c): Manual CASp audit vs. STAR autonomous scan - cost and time."""

import plotly.graph_objects as go
from plotly.subplots import make_subplots
from theme import (
    ACCENT_RED, ACCENT_GOLD, TEXT, MUTED, EXPORT_KW,
)

methods = ["Manual CASp audit", "STAR autonomous scan"]
cost_low = [3000, 50]
cost_high = [8000, 50]
time_low = [2 * 8 * 60, 10]   # minutes (2 working days)
time_high = [5 * 8 * 60, 10]  # minutes (5 working days)

fig = make_subplots(
    rows=1, cols=2,
    subplot_titles=("Cost per building (USD)",
                    "On-site time per building (minutes)"),
)

fig.add_trace(
    go.Bar(x=methods, y=cost_high, name="Upper bound", marker_color=ACCENT_RED,
           text=[f"${v:,}" for v in cost_high], textposition="outside",
           textfont=dict(color=TEXT, size=16)),
    row=1, col=1,
)
fig.add_trace(
    go.Bar(x=methods, y=cost_low, name="Lower bound", marker_color=ACCENT_GOLD,
           text=[f"${v:,}" for v in cost_low], textposition="outside",
           textfont=dict(color=TEXT, size=16)),
    row=1, col=1,
)

fig.add_trace(
    go.Bar(x=methods, y=time_high, name="Upper bound", marker_color=ACCENT_RED,
           showlegend=False, text=["5 days", "10 min"], textposition="outside",
           textfont=dict(color=TEXT, size=16)),
    row=1, col=2,
)
fig.add_trace(
    go.Bar(x=methods, y=time_low, name="Lower bound", marker_color=ACCENT_GOLD,
           showlegend=False, text=["2 days", "10 min"], textposition="outside",
           textfont=dict(color=TEXT, size=16)),
    row=1, col=2,
)

fig.update_layout(
    title="STAR cuts ADA audit cost by ~99% and time by ~99.5%",
    barmode="group", bargap=0.35,
    margin=dict(l=80, r=60, t=110, b=170),
    annotations=[
        *fig.layout.annotations,
        dict(
            text="Source: CASp Inspectors pricing 2026; MBCS Orange County 2024. "
                 "Log scale. STAR amortized cost assumes 1,000 scans over robot lifetime.",
            showarrow=False, x=0, y=-0.14, xref="paper", yref="paper",
            xanchor="left", yanchor="top", align="left",
            font=dict(size=12, color=MUTED),
        ),
    ],
)
fig.update_yaxes(type="log", row=1, col=1)
fig.update_yaxes(type="log", row=1, col=2)

fig.write_image("output/chart_c_cost_time.png", **EXPORT_KW)
print("wrote output/chart_c_cost_time.png")
