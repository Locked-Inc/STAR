"""Chart (f): STAR real-pipeline Sankey.

Corrected to reflect the actual distributed architecture:
  sensors -> RX72N (real-time) / Pi5 (high-level) -> ROS2 SLAM + Nav2 ->
  compliance engine -> PDF audit report. One check implemented end-to-end;
  stretch and architected checks labeled distinctly.
"""

import plotly.graph_objects as go
from theme import (
    ACCENT_BLUE, ACCENT_PURPLE, ACCENT_GOLD, ACCENT_RED, ACCENT_GREEN,
    MAROON, TEXT, BG, EXPORT_KW,
)

labels = [
    # sensors on the chassis / camera board (0-4)
    "RPLiDAR C1",
    "IMX219-83 stereo",
    "ICM20948 IMU",
    "BNO055 IMU",
    "HC-SR04 x4",
    # real-time compute (5-6)
    "RX72N firmware (ThreadX)",
    "SPI 10 Mbps + HARQ/FEC (nanopb)",
    # RPi5 high-level compute (7-9)
    "RPi5 ROS2 Jazzy",
    "slam_toolbox + robot_localization EKF",
    "/map + /odom + TF chain",
    # compliance engine inputs (10)
    "ADA compliance engine",
    # checks (11-17)
    "Ramp slope 405.2 [IMPLEMENTED]",
    "Trip hazard 303 [STRETCH]",
    "Path width 403.5 [STRETCH]",
    "Ramp width 405.5 [ARCHITECTED]",
    "Ramp landing 405.7 [ARCHITECTED]",
    "Door clear 404.2.3 [ARCHITECTED]",
    "Door threshold 404.2.5 [ARCHITECTED]",
    # report out (18)
    "PDF audit report",
]

# source -> target flows
src = []
tgt = []
val = []

# Sensors into their compute host
src += [0];       tgt += [7];   val += [40]     # RPLiDAR C1 -> RPi5
src += [1];       tgt += [7];   val += [35]     # stereo -> RPi5
src += [2];       tgt += [7];   val += [10]     # ICM20948 -> RPi5
src += [3];       tgt += [5];   val += [10]     # BNO055 -> RX72N
src += [4];       tgt += [5];   val += [8]      # HC-SR04 -> RX72N

# RX72N -> SPI -> RPi5
src += [5];       tgt += [6];   val += [20]
src += [6];       tgt += [7];   val += [20]

# RPi5 -> SLAM+EKF -> Map/Odom/TF
src += [7];       tgt += [8];   val += [125]
src += [8];       tgt += [9];   val += [125]

# Map/Odom -> compliance engine
src += [9];       tgt += [10];  val += [125]

# Compliance engine -> checks (implemented / stretch / architected)
src += [10, 10, 10, 10, 10, 10, 10]
tgt += [11, 12, 13, 14, 15, 16, 17]
val += [40, 20, 20, 12, 12, 12, 9]

# All checks -> PDF report
for ck in [11, 12, 13, 14, 15, 16, 17]:
    src.append(ck)
    tgt.append(18)
    val.append({11: 40, 12: 20, 13: 20, 14: 12, 15: 12, 16: 12, 17: 9}[ck])

# Color per node
IMPLEMENTED_COLOR = ACCENT_GREEN
STRETCH_COLOR = ACCENT_GOLD
ARCHITECTED_COLOR = "#6c6c82"  # muted grey-purple

node_colors = [
    ACCENT_BLUE, ACCENT_BLUE, ACCENT_BLUE, ACCENT_BLUE, ACCENT_BLUE,   # sensors
    MAROON, MAROON,                                                     # RX72N + SPI
    ACCENT_PURPLE, ACCENT_PURPLE, ACCENT_PURPLE,                       # RPi5 ROS2
    MAROON,                                                             # compliance engine
    IMPLEMENTED_COLOR,                                                  # ramp slope
    STRETCH_COLOR, STRETCH_COLOR,                                       # trip + path
    ARCHITECTED_COLOR, ARCHITECTED_COLOR, ARCHITECTED_COLOR, ARCHITECTED_COLOR,  # architected
    ACCENT_GREEN,                                                       # PDF report
]

# Link colors: green tint for implemented-flow, gold for stretch, grey for architected
def link_color(tgt_i):
    if tgt_i == 11 or (tgt_i == 18 and False):
        return "rgba(42,157,143,0.45)"
    if tgt_i in (12, 13):
        return "rgba(244,162,97,0.35)"
    if tgt_i in (14, 15, 16, 17):
        return "rgba(108,108,130,0.30)"
    if tgt_i == 18:
        return "rgba(42,157,143,0.35)"
    return "rgba(76,201,240,0.25)"

link_colors = [link_color(t) for t in tgt]

fig = go.Figure(go.Sankey(
    arrangement="snap",
    node=dict(
        label=labels, color=node_colors,
        pad=22, thickness=22,
        line=dict(color=BG, width=1),
    ),
    link=dict(
        source=src, target=tgt, value=val, color=link_colors,
    ),
))

fig.update_layout(
    title="STAR data flow: sensors -> RX72N + RPi5 -> SLAM -> compliance engine -> PDF audit",
    font=dict(size=14, color=TEXT),
    margin=dict(l=40, r=40, t=100, b=60),
)

fig.write_image("output/chart_f_pipeline_sankey.png", **EXPORT_KW)
print("wrote output/chart_f_pipeline_sankey.png")
