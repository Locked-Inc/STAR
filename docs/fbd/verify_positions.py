#!/usr/bin/env python3
"""
Verify that all block positions are aligned to the grid.
Grid: x from -17 to 20, y from -32 to 6

ALIGNED VALUES (verified 2024):

X-AXIS ALIGNMENT:
- Left stack x-axis: ALIGNED
  - Battery+ wire at x=-17 (offset=-2.0125cm)
  - Block left edges at x=-15 (wide blocks) and x=-14 (Battery, Battery Button)
  - Block right edges at x=-9 (wide blocks) and x=-10 (Battery, Battery Button)
  - Block widths: 5.985cm (wide), 3.985cm (narrow)
  - Battery xshift: -5.0075cm

Y-AXIS ALIGNMENT (Fuel Gauge & Temp Sensor):
- Fuel Gauge: bottom at y=0, top at y~3.58
  - Height: 3.582cm
  - BB-FG gap: 0.6425cm
- Temp Sensor: bottom at y=4, top at y=6
  - Height: 1.985cm
  - FG-TS gap: 0.39cm

TODO: Verify y-axis alignment for remaining blocks (Battery, AFE, Gate Driver, Battery Button)
"""

# Grid bounds
GRID_LEFT = -17
GRID_RIGHT = 20
GRID_TOP = 6
GRID_BOTTOM = -32
TOLERANCE = 0.05  # alignment tolerance in cm

# =============================================================================
# BASE POSITIONS (from LaTeX)
# =============================================================================
usbc_x, usbc_y = 0.035, 0.365
usbc_width, usbc_height = 6.0, 2.8

# =============================================================================
# BUCK CONVERTER STACK (center column)
# =============================================================================
# buckboost: below=2cm of usbc, xshift=-2cm
buckboost_x = usbc_x - 2  # xshift=-2cm
usbc_south_y = usbc_y - usbc_height / 2
buckboost_north_y = usbc_south_y - 2  # 2cm below
buckboost_height = 2.4
buckboost_y = buckboost_north_y - buckboost_height / 2
buckboost_width = 6.0
buckboost_west_x = buckboost_x - buckboost_width / 2

# Buck converters stack below buckboost with anchor=north west at previous.south west
buck6v_width, buck6v_height = 5.0, 2.0
buck6v_north_y = buckboost_y - buckboost_height / 2 - 0.5  # yshift=-0.5cm
buck6v_y = buck6v_north_y - buck6v_height / 2
buck6v_west_x = buckboost_west_x
buck6v_x = buck6v_west_x + buck6v_width / 2

buck5v_width, buck5v_height = 5.0, 2.0
buck5v_north_y = buck6v_y - buck6v_height / 2 - 0.5
buck5v_y = buck5v_north_y - buck5v_height / 2
buck5v_x = buck6v_x

buck3v3_width, buck3v3_height = 5.0, 2.0
buck3v3_north_y = buck5v_y - buck5v_height / 2 - 0.5
buck3v3_y = buck3v3_north_y - buck3v3_height / 2
buck3v3_west_x = buckboost_west_x
buck3v3_x = buck3v3_west_x + buck3v3_width / 2

# =============================================================================
# LEFT STACK (Battery Management) - X-AXIS ALIGNED
# =============================================================================
# Battery: anchor=east at [xshift=-5.0075cm, yshift=0.4cm]buck3v3.west
BATTERY_XSHIFT = -5.0075  # TUNED VALUE
battery_width, battery_height = 4.0, 2.0
battery_east_x = buck3v3_west_x + BATTERY_XSHIFT
battery_y = buck3v3_y + 0.4
battery_x = battery_east_x - battery_width / 2
battery_west_x = battery_x - battery_width / 2

# AFE: anchor=south at [yshift=0.5cm]battery.north
afe_width, afe_height = 5.985, 3.6  # TUNED WIDTH
battery_north_y = battery_y + battery_height / 2
afe_south_y = battery_north_y + 0.5
afe_y = afe_south_y + afe_height / 2
afe_x = battery_x  # same x as battery center
afe_west_x = afe_x - afe_width / 2
afe_east_x = afe_x + afe_width / 2

# Gate Driver: anchor=south at [yshift=0.5cm]afe.north
gatedriver_width, gatedriver_height = 5.985, 3.2  # TUNED WIDTH
afe_north_y = afe_y + afe_height / 2
gd_south_y = afe_north_y + 0.5
gd_y = gd_south_y + gatedriver_height / 2
gd_x = afe_x
gd_west_x = gd_x - gatedriver_width / 2
gd_east_x = gd_x + gatedriver_width / 2

# Battery Button: anchor=south at [yshift=0.5cm]gatedriver.north
bb_width, bb_height = 3.985, 1.6  # TUNED WIDTH
gd_north_y = gd_y + gatedriver_height / 2
bb_south_y = gd_north_y + 0.5
bb_y = bb_south_y + bb_height / 2
bb_x = gd_x
bb_west_x = bb_x - bb_width / 2
bb_east_x = bb_x + bb_width / 2

# Fuel Gauge: anchor=south at [yshift=0.6425cm]batterybutton.north
# TUNED: height=3.582cm, gap=0.6425cm for y-axis alignment (bottom at y=0)
fg_width, fg_height = 5.985, 3.582  # TUNED WIDTH AND HEIGHT
BB_FG_GAP = 0.6425  # TUNED GAP
bb_north_y = bb_y + bb_height / 2
fg_south_y = bb_north_y + BB_FG_GAP
fg_y = fg_south_y + fg_height / 2
fg_x = bb_x
fg_west_x = fg_x - fg_width / 2
fg_east_x = fg_x + fg_width / 2

# Temp Sensor: anchor=south at [yshift=0.39cm]fuelgauge.north
# TUNED: height=1.985cm, gap=0.39cm for y-axis alignment (bottom at y=4, top at y=6)
ts_width, ts_height = 5.985, 1.985  # TUNED WIDTH AND HEIGHT
FG_TS_GAP = 0.39  # TUNED GAP
fg_north_y = fg_y + fg_height / 2
ts_south_y = fg_north_y + FG_TS_GAP
ts_y = ts_south_y + ts_height / 2
ts_x = fg_x
ts_west_x = ts_x - ts_width / 2
ts_east_x = ts_x + ts_width / 2
ts_north_y = ts_y + ts_height / 2

# =============================================================================
# WIRE BUS POSITIONS
# =============================================================================
BATTERY_PLUS_OFFSET = -2.0125  # TUNED VALUE for x=-17 alignment

battery_plus_x = afe_west_x + BATTERY_PLUS_OFFSET
cell_sense_x = afe_west_x - 1.2
charge_enable_x = afe_west_x - 0.4
charge_disable_x = afe_west_x - 0.8
fet_control_x = afe_west_x - 1.2

# =============================================================================
# HELPER FUNCTIONS
# =============================================================================
def check_alignment(name, val, axis, target=None):
    """Check if value is aligned to integer grid or specific target"""
    if target is not None:
        diff = val - target
        if abs(diff) < TOLERANCE:
            return f"ALIGNED: {name} {axis}={val:.3f} (target: {target})"
        else:
            return f"OFF:    {name} {axis}={val:.3f} (target: {target}, off by {diff:+.3f})"
    else:
        rounded = round(val)
        diff = val - rounded
        if abs(diff) < TOLERANCE:
            return f"GRID:   {name} {axis}={val:.3f} (on grid line {rounded})"
        else:
            return f"OFF:    {name} {axis}={val:.3f} (nearest grid: {rounded}, off by {diff:+.3f})"

# =============================================================================
# OUTPUT
# =============================================================================
print("=" * 70)
print("STAR FBD GRID ALIGNMENT VERIFICATION")
print("=" * 70)
print(f"Grid bounds: x=[{GRID_LEFT}, {GRID_RIGHT}], y=[{GRID_BOTTOM}, {GRID_TOP}]")
print(f"Tolerance: {TOLERANCE}cm")

print("\n" + "=" * 70)
print("BUCK CONVERTER STACK (Center)")
print("=" * 70)
print(f"USB-C:      center=({usbc_x:.3f}, {usbc_y:.3f})")
print(f"Buck-Boost: center=({buckboost_x:.3f}, {buckboost_y:.3f})")
print(f"Buck 6V:    center=({buck6v_x:.3f}, {buck6v_y:.3f})")
print(f"Buck 5V:    center=({buck5v_x:.3f}, {buck5v_y:.3f})")
print(f"Buck 3.3V:  center=({buck3v3_x:.3f}, {buck3v3_y:.3f})")

print("\n" + "=" * 70)
print("LEFT STACK (Battery Management) - X-AXIS VERIFIED ALIGNED")
print("=" * 70)
print(f"Battery xshift: {BATTERY_XSHIFT}cm")
print(f"Block widths: {afe_width}cm (wide), {bb_width}cm (narrow)")
print(f"Battery+ wire offset: {BATTERY_PLUS_OFFSET}cm")
print()

left_stack = [
    ("Battery", battery_x, battery_y, battery_width, battery_height),
    ("AFE", afe_x, afe_y, afe_width, afe_height),
    ("Gate Driver", gd_x, gd_y, gatedriver_width, gatedriver_height),
    ("Battery Button", bb_x, bb_y, bb_width, bb_height),
    ("Fuel Gauge", fg_x, fg_y, fg_width, fg_height),
    ("Temp Sensor", ts_x, ts_y, ts_width, ts_height),
]

for name, cx, cy, w, h in left_stack:
    west = cx - w/2
    east = cx + w/2
    north = cy + h/2
    south = cy - h/2
    print(f"\n{name}:")
    print(f"  {check_alignment(name, west, 'west')}")
    print(f"  {check_alignment(name, east, 'east')}")
    print(f"  {check_alignment(name, north, 'north')}")
    print(f"  {check_alignment(name, south, 'south')}")

print("\n" + "=" * 70)
print("WIRE BUS POSITIONS")
print("=" * 70)
print(f"\n{check_alignment('Battery+ wire', battery_plus_x, 'x', target=GRID_LEFT)}")
print(f"{check_alignment('Cell Sense wire', cell_sense_x, 'x')}")
print(f"{check_alignment('Charge Enable wire', charge_enable_x, 'x')}")
print(f"{check_alignment('Charge Disable wire', charge_disable_x, 'x')}")

print("\n" + "=" * 70)
print("CRITICAL ALIGNMENT CHECKS")
print("=" * 70)
print(f"\n{check_alignment('Temp Sensor top', ts_north_y, 'y', target=GRID_TOP)}")
print(f"{check_alignment('Battery+ wire', battery_plus_x, 'x', target=GRID_LEFT)}")

print("\n" + "=" * 70)
print("BUCK CONVERTER STACK EDGES")
print("=" * 70)

buck_stack = [
    ("USB-C", usbc_x, usbc_y, usbc_width, usbc_height),
    ("Buck-Boost", buckboost_x, buckboost_y, buckboost_width, buckboost_height),
    ("Buck 6V", buck6v_x, buck6v_y, buck6v_width, buck6v_height),
    ("Buck 5V", buck5v_x, buck5v_y, buck5v_width, buck5v_height),
    ("Buck 3.3V", buck3v3_x, buck3v3_y, buck3v3_width, buck3v3_height),
]

for name, cx, cy, w, h in buck_stack:
    west = cx - w/2
    east = cx + w/2
    north = cy + h/2
    south = cy - h/2
    print(f"\n{name}:")
    print(f"  {check_alignment(name, west, 'west')}")
    print(f"  {check_alignment(name, east, 'east')}")
    print(f"  {check_alignment(name, north, 'north')}")
    print(f"  {check_alignment(name, south, 'south')}")

print("\n" + "=" * 70)
print("SUMMARY")
print("=" * 70)
print("""
X-AXIS STATUS:
  - Left stack (Battery to Temp Sensor): ALIGNED
  - Battery+ wire at x=-17: ALIGNED
  - Buck converter stack: NEEDS VERIFICATION

Y-AXIS STATUS:
  - Temp Sensor top at y=6: ALIGNED
  - All other blocks: NEEDS ALIGNMENT

NEXT STEPS:
  1. Verify buck converter stack x-axis alignment
  2. Align all block y-axis positions to integer grid
  3. Verify all wire positions
""")
