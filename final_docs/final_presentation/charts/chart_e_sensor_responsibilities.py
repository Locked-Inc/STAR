"""Chart (e): Sensor responsibility matrix for the seven ADA checks.

Replaces the earlier fabricated MAE comparison (LiDAR 0.8"/Stereo 1.2"/
Fused 0.3") which had no measured data behind it. This chart is a
truthful mapping: for each of the seven ADA checks, which sensor(s) are
primary, which are secondary, and which are not used. Implementation
status is badged on the y-axis labels.
"""

import plotly.graph_objects as go
from theme import (
    ACCENT_GREEN, ACCENT_GOLD, ACCENT_BLUE, ACCENT_RED, MAROON, GRID,
    TEXT, MUTED, EXPORT_KW,
)

sensors = [
    "RPLiDAR C1",
    "IMX219-83 stereo",
    "BNO055 (chassis IMU)",
    "ICM20948 (camera IMU)",
    "HC-SR04 (ultrasonic)",
    "slam_toolbox occupancy grid",
]

checks = [
    "Ramp slope (405.2)  [IMPLEMENTED]",
    "Trip hazard (303)  [STRETCH]",
    "Accessible path width (403.5)  [STRETCH]",
    "Ramp width (405.5)  [ARCHITECTED]",
    "Ramp landing (405.7)  [ARCHITECTED]",
    "Door clear width (404.2.3)  [ARCHITECTED]",
    "Door threshold (404.2.5)  [ARCHITECTED]",
]

# Rows = sensors, cols = checks. Values: 2 primary, 1 secondary, 0 not used.
z = [
    # ramp slope, trip hazard, path width, ramp width, ramp landing, door clear width, door threshold
    [2,           2,           0,          2,          2,            0,                1],   # RPLiDAR C1
    [0,           0,           0,          0,          0,            2,                2],   # stereo
    [2,           1,           0,          0,          0,            0,                0],   # BNO055
    [1,           0,           0,          0,          0,            1,                0],   # ICM20948
    [0,           1,           1,          0,          0,            0,                0],   # ultrasonic
    [0,           0,           2,          0,          0,            0,                0],   # occupancy grid
]

# Manual color scale: 0 = muted/dark, 1 = gold (secondary), 2 = green (primary)
colorscale = [
    [0.00, "#222036"],
    [0.49, "#222036"],
    [0.50, ACCENT_GOLD],
    [0.99, ACCENT_GOLD],
    [1.00, ACCENT_GREEN],
]

annotations_txt = [[["-", "secondary", "PRIMARY"][v] for v in row] for row in z]

fig = go.Figure(go.Heatmap(
    z=z,
    x=checks,
    y=sensors,
    colorscale=colorscale,
    zmin=0, zmax=2,
    showscale=False,
    xgap=3, ygap=3,
    hovertemplate="%{y}<br>%{x}<br>role=%{customdata}<extra></extra>",
    customdata=annotations_txt,
))

# Overlay text labels in each cell
for r, sensor in enumerate(sensors):
    for c, check in enumerate(checks):
        label = annotations_txt[r][c]
        color = TEXT if z[r][c] >= 1 else MUTED
        weight = "<b>" if z[r][c] == 2 else ""
        close = "</b>" if z[r][c] == 2 else ""
        fig.add_annotation(
            x=check, y=sensor,
            text=f"{weight}{label}{close}",
            showarrow=False,
            font=dict(color=color, size=14),
        )

fig.update_layout(
    title="Which sensors handle which ADA check",
    xaxis=dict(side="top", tickfont=dict(size=12, color=TEXT),
               showgrid=False, tickangle=-20),
    yaxis=dict(tickfont=dict(size=13, color=TEXT), showgrid=False,
               autorange="reversed"),
    margin=dict(l=260, r=60, t=160, b=160),
    annotations=[
        *fig.layout.annotations,
        dict(
            text="PRIMARY = measurement authority; secondary = cross-validation or fallback.  "
                 "Only RAMP SLOPE is implemented end-to-end for the capstone demo; "
                 "STRETCH = in-progress; ARCHITECTED = specified for deployment-phase implementation.",
            showarrow=False, x=0, y=-0.22, xref="paper", yref="paper",
            xanchor="left", yanchor="top", align="left",
            font=dict(size=12, color=MUTED),
        ),
    ],
)

fig.write_image("output/chart_e_sensor_responsibilities.png", **EXPORT_KW)
print("wrote output/chart_e_sensor_responsibilities.png")
