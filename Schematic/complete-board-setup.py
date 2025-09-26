"""
Complete KiCad PCB setup script
- Sets board outline to configurable dimensions
- Adds 4 grounded mounting holes at STAR board corners (configurable clearance from pad edge)
- Adds 4 isolated mounting holes for other board (configurable pattern and offset)
- Groups holes and hides reference designators
- Removes existing mounting holes if they exist
"""

import pcbnew


# =============================================================================
# CONFIGURATION
# =============================================================================


# Other board mounting holes (isolated)
OTHER_BOARD_WIDTH_MM = 129  # Other board hole pattern width in mm
OTHER_BOARD_HEIGHT_MM = 79  # Other board hole pattern height in mm
OTHER_BOARD_OFFSET_MM = 2.5 # Offset up from STAR holes in mm

# Pin header connector
PIN_HEADER_LIB = "./Connector_PinHeader_2.54mm.pretty"
PIN_HEADER_FOOTPRINT = "PinHeader_2x20_P2.54mm_Vertical"
PIN_HEADER_OFFSET_X_MM = 30.5  # Distance from top-right OTHER_BOARD hole to center of pin header
PIN_HEADER_WIDTH_MM = 50.0    # Width of the pin header itself
PIN_HEADER_OFFSET_Y_MM = 0.0  # Y offset from top-right OTHER_BOARD hole (0 = same level)
PIN_HEADER_ROTATION_DEG = 270 # Rotation in degrees (90 = clockwise from vertical)

# Board dimensions and positioning
BOARD_WIDTH_MM = OTHER_BOARD_WIDTH_MM * 1.5
BOARD_HEIGHT_MM = OTHER_BOARD_HEIGHT_MM * 2
BOARD_CENTER_X_MM = 250     # Board center X position in mm
BOARD_CENTER_Y_MM = 200     # Board center Y position in mm

# STAR board mounting holes (grounded, at corners)
STAR_PAD_CLEARANCE_MM = 2.5 # Distance from pad edge to board edge in mm

# Footprint library and names
FOOTPRINT_LIB = './MountingHole.pretty'
GROUNDED_FOOTPRINT = "MountingHole_2.7mm_M2.5_Pad"  # For mounting to base board
ISOLATED_FOOTPRINT = "MountingHole_2.7mm_M2.5"      # For mounting other board on top

# =============================================================================


def get_footprint_bbox(footprint):
    """Get bounding box of footprint excluding text"""
    bbox = None
    
    for pad in footprint.Pads():
        if bbox is None:
            bbox = pad.GetBoundingBox()
        else:
            bbox.Merge(pad.GetBoundingBox())
    
    for graphic_item in footprint.GraphicalItems():
        if bbox is None:
            bbox = graphic_item.GetBoundingBox()
        else:
            bbox.Merge(graphic_item.GetBoundingBox())
    
    return bbox


def complete_board_setup():
    """Complete board setup: outline + mounting holes"""
    
    board = pcbnew.GetBoard()
    
    print("=== COMPLETE BOARD SETUP ===")
    print(f"Board: {BOARD_WIDTH_MM}mm x {BOARD_HEIGHT_MM}mm at ({BOARD_CENTER_X_MM}mm, {BOARD_CENTER_Y_MM}mm)")
    print("Mounting holes: 4 grounded + 4 isolated")
    
    # Board dimensions in KiCad units (nanometers)
    width = int(BOARD_WIDTH_MM * 1000000)
    height = int(BOARD_HEIGHT_MM * 1000000)
    center_x = int(BOARD_CENTER_X_MM * 1000000)
    center_y = int(BOARD_CENTER_Y_MM * 1000000)
    
    half_width = width // 2
    half_height = height // 2
    
    left = center_x - half_width
    right = center_x + half_width  
    bottom = center_y + half_height
    top = center_y - half_height
    
    # 1. CLEANUP - Remove existing edge cuts and mounting hole groups
    print("\n=== CLEANUP ===")
    
    # Remove existing edge cuts
    drawings_to_remove = []
    for drawing in board.GetDrawings():
        if drawing.GetLayerName() == "Edge.Cuts":
            drawings_to_remove.append(drawing)
    
    for drawing in drawings_to_remove:
        board.Remove(drawing)
    
    if drawings_to_remove:
        print(f"Removed {len(drawings_to_remove)} existing edge cuts")
    
    # Remove existing mounting hole groups
    groups_to_remove = []
    for group in board.Groups():
        group_name = group.GetName()
        if ("Mounting Holes" in group_name or 
            "mounting" in group_name.lower() or
            "STAR Board" in group_name or
            "Other Board" in group_name):
            groups_to_remove.append(group)
    
    for group in groups_to_remove:
        # Remove items from group
        items_to_remove = []
        for item in group.GetItems():
            items_to_remove.append(item)
        
        for item in items_to_remove:
            board.Remove(item)
        
        board.Remove(group)
        print(f"Removed group: '{group.GetName()}'")
    
    if groups_to_remove:
        print(f"Cleaned up {len(groups_to_remove)} existing mounting hole groups")
    
    # 2. CREATE BOARD OUTLINE
    print("\n=== BOARD OUTLINE ===")
    
    edge_cuts_layer = board.GetLayerID("Edge.Cuts")
    
    rectangle = pcbnew.PCB_SHAPE(board)
    rectangle.SetShape(pcbnew.SHAPE_T_RECT)
    rectangle.SetStart(pcbnew.VECTOR2I(left, top))
    rectangle.SetEnd(pcbnew.VECTOR2I(right, bottom))
    rectangle.SetLayer(edge_cuts_layer)
    rectangle.SetWidth(0)
    rectangle.SetFilled(False)
    board.Add(rectangle)
    
    print(f"✓ Created {BOARD_WIDTH_MM}mm x {BOARD_HEIGHT_MM}mm board outline")
    
    # 3. ADD MOUNTING HOLES
    print("\n=== MOUNTING HOLES ===")
    
    # Get board boundary
    board_rect = rectangle.GetBoundingBox()
    
    # Load footprint to get pad size
    io = pcbnew.PCB_IO_KICAD_SEXPR()
    temp_footprint = io.FootprintLoad(FOOTPRINT_LIB, GROUNDED_FOOTPRINT)
    footprint_bbox = get_footprint_bbox(temp_footprint)
    
    # Calculate STAR board corner positions (configurable clearance from pad edge to board edge)
    clearance = int(STAR_PAD_CLEARANCE_MM * 1000000)
    pad_radius = footprint_bbox.GetWidth() // 2
    total_inset = clearance + pad_radius
    
    star_left = board_rect.GetLeft() + total_inset
    star_right = board_rect.GetRight() - total_inset
    star_bottom = board_rect.GetBottom() - total_inset
    star_top = board_rect.GetTop() + total_inset
    
    star_corners = [
        pcbnew.wxPoint(star_left, star_bottom),   # Bottom-left
        pcbnew.wxPoint(star_left, star_top),      # Top-left  
        pcbnew.wxPoint(star_right, star_top),     # Top-right
        pcbnew.wxPoint(star_right, star_bottom),  # Bottom-right
    ]
    
    # Calculate other board hole positions (configurable rectangle pattern)
    other_half_width = int((OTHER_BOARD_WIDTH_MM / 2) * 1000000)
    other_height = int(OTHER_BOARD_HEIGHT_MM * 1000000)
    
    board_center_x = board_rect.Centre().x
    other_left = board_center_x - other_half_width
    other_right = board_center_x + other_half_width
    
    offset = int(OTHER_BOARD_OFFSET_MM * 1000000)
    other_bottom = star_bottom - offset
    other_top = other_bottom - other_height
    
    other_corners = [
        pcbnew.wxPoint(other_left, other_bottom),   # Bottom-left
        pcbnew.wxPoint(other_left, other_top),      # Top-left  
        pcbnew.wxPoint(other_right, other_top),     # Top-right
        pcbnew.wxPoint(other_right, other_bottom),  # Bottom-right
    ]
    
    # Add grounded mounting holes (STAR board corners)
    print("Adding grounded mounting holes...")
    grounded_group = pcbnew.PCB_GROUP(board)
    grounded_group.SetName("STAR Board Mounting Holes")
    
    for i, corner in enumerate(star_corners):
        footprint = io.FootprintLoad(FOOTPRINT_LIB, GROUNDED_FOOTPRINT)
        footprint.Reference().SetVisible(False)
        
        pos_x = corner.x - footprint_bbox.Centre().x + footprint.GetPosition().x
        pos_y = corner.y - footprint_bbox.Centre().y + footprint.GetPosition().y
        position = pcbnew.VECTOR2I(int(pos_x), int(pos_y))
        footprint.SetPosition(position)
        
        board.Add(footprint)
        grounded_group.AddItem(footprint)
        print(f"  ✓ Added grounded hole {i+1}")
    
    board.Add(grounded_group)
    
    # Add isolated mounting holes (other board pattern)
    print("Adding isolated mounting holes...")
    isolated_group = pcbnew.PCB_GROUP(board)
    isolated_group.SetName("Other Board Mounting Holes")
    
    for i, corner in enumerate(other_corners):
        footprint = io.FootprintLoad(FOOTPRINT_LIB, ISOLATED_FOOTPRINT)
        footprint.Reference().SetVisible(False)
        footprint_bbox = get_footprint_bbox(footprint)
        
        pos_x = corner.x - footprint_bbox.Centre().x + footprint.GetPosition().x
        pos_y = corner.y - footprint_bbox.Centre().y + footprint.GetPosition().y
        position = pcbnew.VECTOR2I(int(pos_x), int(pos_y))
        footprint.SetPosition(position)
        
        board.Add(footprint)
        isolated_group.AddItem(footprint)
        print(f"  ✓ Added isolated hole {i+1}")
    
    board.Add(isolated_group)
    
    # Add pin header connector
    # print("Adding pin header connector...")
    # try:
    #     # Calculate pin header position based on offset from top-right OTHER_BOARD hole
    #     # Get top-right other board hole position
    #     top_right_other_hole = other_corners[2]  # Top-right corner from other_corners array
    #     
    #     # Pin header center X position (offset from top-right hole)
    #     pin_header_center_x = top_right_other_hole.x + int(PIN_HEADER_OFFSET_X_MM * 1000000)
    #     
    #     # Y position with offset from top-right other board hole
    #     pin_header_y = top_right_other_hole.y + int(PIN_HEADER_OFFSET_Y_MM * 1000000)
    #     
    #     pin_header_x = pin_header_center_x
    #     
    #     # Load pin header footprint
    #     print(f"  Attempting to load: {PIN_HEADER_LIB}/{PIN_HEADER_FOOTPRINT}")
    #     pin_header = io.FootprintLoad(PIN_HEADER_LIB, PIN_HEADER_FOOTPRINT)
    #     
    #     if pin_header is None:
    #         print(f"  ! FootprintLoad returned None for {PIN_HEADER_LIB}/{PIN_HEADER_FOOTPRINT}")
    #     else:
    #         print(f"  ✓ Successfully loaded footprint: {type(pin_header)}")
    #     
    #     if pin_header is not None:
    #         pin_header.Reference().SetVisible(False)
    #         
    #         # Set position
    #         position = pcbnew.VECTOR2I(pin_header_x, pin_header_y)
    #         pin_header.SetPosition(position)
    #         
    #         # Set rotation (KiCad 9.0 approach - try different methods)
    #         if PIN_HEADER_ROTATION_DEG != 0:
    #             try:
    #                 # Method 1: Try with radians 
    #                 import math
    #                 rotation_radians = math.radians(PIN_HEADER_ROTATION_DEG)
    #                 angle = pcbnew.EDA_ANGLE(rotation_radians, pcbnew.RADIANS_T)
    #                 pin_header.SetOrientation(angle)
    #                 print(f"  ✓ Rotated {PIN_HEADER_ROTATION_DEG}° using radians")
    #             except:
    #                 try:
    #                     # Method 2: Try with decidegrees (10ths of degrees)
    #                     rotation_decideg = int(PIN_HEADER_ROTATION_DEG * 10)
    #                     angle = pcbnew.EDA_ANGLE(rotation_decideg, pcbnew.DECIDEGREES_T)
    #                     pin_header.SetOrientation(angle)
    #                     print(f"  ✓ Rotated {PIN_HEADER_ROTATION_DEG}° using decidegrees")
    #                 except:
    #                     try:
    #                         # Method 3: Direct rotation with SetOrientationDegrees if it exists
    #                         pin_header.SetOrientationDegrees(PIN_HEADER_ROTATION_DEG)
    #                         print(f"  ✓ Rotated {PIN_HEADER_ROTATION_DEG}° using SetOrientationDegrees")
    #                     except:
    #                         print(f"  ! Could not set rotation, using default orientation")
    #         
    #         board.Add(pin_header)
    #         print(f"  ✓ Added pin header at ({pin_header_x/1000000:.1f}, {pin_header_y/1000000:.1f})mm, rotated {PIN_HEADER_ROTATION_DEG}°")
    #     else:
    #         print(f"  ✗ Could not load pin header footprint: {PIN_HEADER_LIB}:{PIN_HEADER_FOOTPRINT}")
    #     
    # except Exception as e:
    #     print(f"  ✗ Failed to add pin header: {e}")
    
    # 4. FINALIZE
    print("\n=== FINALIZE ===")
    
    board.BuildConnectivity()
    pcbnew.Refresh()
    
    print("✅ COMPLETE!")
    print(f"✓ {BOARD_WIDTH_MM}mm x {BOARD_HEIGHT_MM}mm board outline")
    print(f"✓ 4 grounded mounting holes ({STAR_PAD_CLEARANCE_MM}mm clearance)")
    print(f"✓ 4 isolated mounting holes ({OTHER_BOARD_WIDTH_MM}x{OTHER_BOARD_HEIGHT_MM}mm pattern)")
    # print(f"✓ 2x20 pin header connector ({PIN_HEADER_OFFSET_X_MM}mm from top-right hole, {PIN_HEADER_WIDTH_MM}mm width)")
    print("✓ All components with hidden references")
    print("\n💾 Don't forget to save your PCB file!")


if __name__ == "__main__":
    complete_board_setup()
