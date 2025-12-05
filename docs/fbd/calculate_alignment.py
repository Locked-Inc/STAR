#!/usr/bin/env python3
"""
Calculate optimal block heights and gaps for grid alignment.

Strategy:
- Work backwards from known aligned positions (TS top=6)
- For each block, find height that allows BOTH top and bottom on integer grid
- Heights must be integers or have .5 to allow both edges on grid
"""

# Known tuned values
TUNED_TS_HEIGHT = 1.985
TUNED_TS_GAP = 0.39
TUNED_FG_HEIGHT = 3.582
TUNED_FG_GAP = 0.6425

# Formula-based heights: H = 1.2 + N * 0.4
FORMULA_HEIGHTS = {
    "Battery": 2.0,        # 2 labels
    "AFE": 3.6,            # 6 labels
    "Gate Driver": 3.2,    # 5 labels
    "Battery Button": 1.6, # 1 label
    "Fuel Gauge": 3.6,     # 6 labels
    "Temp Sensor": 2.0,    # 2 labels
}

def find_optimal_integer_alignment():
    """
    Find block heights and gaps that put ALL edges on integer grid lines.

    Key insight: For both edges to be on integer grid, height must be an integer.
    If we allow slight adjustments to heights (keeping them integers), we can
    cascade from top to bottom.
    """

    print("=" * 70)
    print("OPTIMAL INTEGER GRID ALIGNMENT CALCULATOR")
    print("=" * 70)

    # Start from top: TS top = 6
    ts_top = 6

    # For TS: if top=6 and we want bottom on grid, height must be integer
    # Formula height is 2.0, so bottom = 6 - 2 = 4 (perfect!)
    ts_height_int = 2
    ts_bottom = ts_top - ts_height_int

    print(f"\nTemp Sensor: top={ts_top}, height={ts_height_int}, bottom={ts_bottom}")

    # For FG: top must connect to TS bottom via some gap
    # TS bottom = 4, so FG top = 4 - gap
    # For FG top to be on grid, gap must result in integer FG top
    # Formula FG height = 3.6, but for integer edges, try height = 4
    # If FG height = 4 and we want FG bottom = 0, then FG top = 4
    # So gap = TS bottom - FG top = 4 - 4 = 0 (blocks touching)
    # That's not ideal. Let's try FG bottom = 0, height = 3.6 → top = 3.6 (not integer)

    # Alternative: Accept that FG top won't be on integer grid
    # FG bottom = 0 (critical), height ≈ 3.6, top ≈ 3.6
    fg_bottom = 0
    fg_height = 4  # Round up to get integer
    fg_top = fg_bottom + fg_height  # = 4
    fg_ts_gap = ts_bottom - fg_top  # = 4 - 4 = 0

    print(f"Fuel Gauge (integer): top={fg_top}, height={fg_height}, bottom={fg_bottom}, gap_to_TS={fg_ts_gap}")
    print("  WARNING: Gap=0 means blocks touch!")

    # Let's try a different approach: keep some gap, accept non-integer top for FG
    print("\n--- Alternative: Keep gaps, FG top not on integer grid ---")

    # Work with the tuned values we have
    # TS: top=6, height≈2, bottom≈4
    # FG: bottom=0, height≈3.58, top≈3.58

    fg_bottom_target = 0
    fg_height_tuned = TUNED_FG_HEIGHT  # 3.582
    fg_top_tuned = fg_bottom_target + fg_height_tuned

    # Gap between FG and TS
    fg_ts_gap_needed = ts_bottom - fg_top_tuned
    print(f"FG bottom={fg_bottom_target}, height={fg_height_tuned:.3f}, top={fg_top_tuned:.3f}")
    print(f"Gap FG→TS needed: {fg_ts_gap_needed:.3f}")

    # Now work backwards for BB, GD, AFE, Battery
    # BB top → FG bottom - gap
    # We want BB top on integer grid

    print("\n" + "=" * 70)
    print("CALCULATING FROM FG BOTTOM = 0")
    print("=" * 70)

    results = {}

    # Battery Button: want top=-1, bottom=-3 (height=2) or top=-1, bottom=-2 (height=1)
    # Formula height is 1.6, so closest integer is 2
    bb_options = [
        {"top": -1, "height": 2, "bottom": -3},
        {"top": -1, "height": 1, "bottom": -2},  # too short for label
        {"top": 0, "height": 2, "bottom": -2},   # top at 0 means touching FG
    ]

    print("\nBattery Button options (formula height=1.6):")
    for opt in bb_options:
        gap = fg_bottom_target - opt["top"]
        print(f"  top={opt['top']}, height={opt['height']}, bottom={opt['bottom']}, gap_to_FG={gap}")

    # Best option: top=-1, height=2, bottom=-3, gap=1
    bb = {"top": -1, "height": 2, "bottom": -3}
    bb_fg_gap = fg_bottom_target - bb["top"]  # = 0 - (-1) = 1
    results["Battery Button"] = {**bb, "gap_above": bb_fg_gap}

    print(f"\n  SELECTED: top={bb['top']}, height={bb['height']}, bottom={bb['bottom']}, gap={bb_fg_gap}")

    # Gate Driver: want top on integer grid, height close to 3.2
    # BB bottom = -3, so GD top = -3 - gap
    # If gap=0, GD top = -3
    # Height = 3 → bottom = -6
    gd_options = [
        {"top": -3, "height": 3, "bottom": -6, "gap": 0},
        {"top": -4, "height": 3, "bottom": -7, "gap": 1},
        {"top": -3, "height": 4, "bottom": -7, "gap": 0},
    ]

    print("\nGate Driver options (formula height=3.2):")
    for opt in gd_options:
        print(f"  top={opt['top']}, height={opt['height']}, bottom={opt['bottom']}, gap_to_BB={opt['gap']}")

    # Best: top=-3, height=3, bottom=-6, gap=0 (blocks touch but height close to 3.2)
    # Or: top=-4, height=3, bottom=-7, gap=1 (has gap)
    gd = {"top": -4, "height": 3, "bottom": -7}
    gd_bb_gap = bb["bottom"] - gd["top"]  # = -3 - (-4) = 1
    results["Gate Driver"] = {**gd, "gap_above": gd_bb_gap}

    print(f"\n  SELECTED: top={gd['top']}, height={gd['height']}, bottom={gd['bottom']}, gap={gd_bb_gap}")

    # AFE: want top on integer grid, height close to 3.6
    # GD bottom = -7, so AFE top = -7 - gap
    afe_options = [
        {"top": -7, "height": 4, "bottom": -11, "gap": 0},
        {"top": -8, "height": 3, "bottom": -11, "gap": 1},
        {"top": -7, "height": 3, "bottom": -10, "gap": 0},
    ]

    print("\nAFE options (formula height=3.6):")
    for opt in afe_options:
        print(f"  top={opt['top']}, height={opt['height']}, bottom={opt['bottom']}, gap_to_GD={opt['gap']}")

    # Best: top=-7, height=4, bottom=-11, gap=0 (height 4 is close to 3.6)
    afe = {"top": -7, "height": 4, "bottom": -11}
    afe_gd_gap = gd["bottom"] - afe["top"]  # = -7 - (-7) = 0
    results["AFE"] = {**afe, "gap_above": afe_gd_gap}

    print(f"\n  SELECTED: top={afe['top']}, height={afe['height']}, bottom={afe['bottom']}, gap={afe_gd_gap}")

    # Battery: want top on integer grid, height close to 2.0
    # AFE bottom = -11, so Battery top = -11 - gap
    bat_options = [
        {"top": -11, "height": 2, "bottom": -13, "gap": 0},
        {"top": -12, "height": 2, "bottom": -14, "gap": 1},
    ]

    print("\nBattery options (formula height=2.0):")
    for opt in bat_options:
        print(f"  top={opt['top']}, height={opt['height']}, bottom={opt['bottom']}, gap_to_AFE={opt['gap']}")

    bat = {"top": -11, "height": 2, "bottom": -13}
    bat_afe_gap = afe["bottom"] - bat["top"]  # = -11 - (-11) = 0
    results["Battery"] = {**bat, "gap_above": bat_afe_gap}

    print(f"\n  SELECTED: top={bat['top']}, height={bat['height']}, bottom={bat['bottom']}, gap={bat_afe_gap}")

    # Summary
    print("\n" + "=" * 70)
    print("RECOMMENDED INTEGER-ALIGNED VALUES")
    print("=" * 70)

    print("\n--- Heights (adjusted to integers for grid alignment) ---")
    print(f"Battery        : {results['Battery']['height']}cm (formula: 2.0)")
    print(f"AFE            : {results['AFE']['height']}cm (formula: 3.6)")
    print(f"Gate Driver    : {results['Gate Driver']['height']}cm (formula: 3.2)")
    print(f"Battery Button : {results['Battery Button']['height']}cm (formula: 1.6)")
    print(f"Fuel Gauge     : {TUNED_FG_HEIGHT}cm (tuned, non-integer)")
    print(f"Temp Sensor    : {TUNED_TS_HEIGHT}cm (tuned, non-integer)")

    print("\n--- Gaps (yshift values) ---")
    print(f"Battery → AFE        : {results['AFE']['gap_above']}cm")
    print(f"AFE → Gate Driver    : {results['Gate Driver']['gap_above']}cm")
    print(f"Gate Driver → BB     : {results['Battery Button']['gap_above']}cm")
    print(f"BB → Fuel Gauge      : {results['Battery Button']['gap_above']}cm (adjust from {TUNED_FG_GAP})")
    print(f"Fuel Gauge → TS      : {TUNED_TS_GAP}cm (tuned)")

    print("\n--- Target Grid Positions ---")
    print(f"Battery        : y={results['Battery']['bottom']} to y={results['Battery']['top']}")
    print(f"AFE            : y={results['AFE']['bottom']} to y={results['AFE']['top']}")
    print(f"Gate Driver    : y={results['Gate Driver']['bottom']} to y={results['Gate Driver']['top']}")
    print(f"Battery Button : y={results['Battery Button']['bottom']} to y={results['Battery Button']['top']}")
    print(f"Fuel Gauge     : y=0 to y≈3.58")
    print(f"Temp Sensor    : y=4 to y=6")

    print("\n" + "=" * 70)
    print("NOTE: Using integer heights may affect label spacing inside blocks.")
    print("The tuned FG/TS values preserve the formula-based internal spacing.")
    print("=" * 70)

    return results


if __name__ == "__main__":
    find_optimal_integer_alignment()
