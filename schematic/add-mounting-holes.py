"""
KiCad script to automatically add mounting holes to PCB corners
Based on: https://github.com/mmccoo/kicad_mmccoo/blob/master/instantiate_footprint/instantiate_footprint.py
"""

import pcbnew


def get_rect_corners(rect):
    """Get the four corners of a rectangle"""
    center_x, center_y = rect.Centre().x, rect.Centre().y
    half_width, half_height = rect.GetWidth() / 2, rect.GetHeight() / 2
    
    return [
        pcbnew.wxPoint(int(center_x - half_width), int(center_y - half_height)),  # Bottom-left
        pcbnew.wxPoint(int(center_x - half_width), int(center_y + half_height)),  # Top-left
        pcbnew.wxPoint(int(center_x + half_width), int(center_y + half_height)),  # Top-right
        pcbnew.wxPoint(int(center_x + half_width), int(center_y - half_height)),  # Bottom-right
    ]


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


def add_mounting_holes():
    """Add mounting holes to the four corners of the PCB"""
    footprint_lib = './MountingHole.pretty'
    grounded_footprint = "MountingHole_2.7mm_M2.5_Pad"  # For mounting to base board
    isolated_footprint = "MountingHole_2.7mm_M2.5"      # For mounting other board on top
    
    board = pcbnew.GetBoard()
    
    # Find board boundary from Edge.Cuts layer
    board_rect = None
    for drawing in board.GetDrawings():
        if drawing.GetLayerName() == "Edge.Cuts":
            if board_rect is None:
                board_rect = drawing.GetBoundingBox()
            else:
                board_rect.Merge(drawing.GetBoundingBox())
    
    if board_rect is None:
        print("Error: No Edge.Cuts layer found!")
        return
    
    print(f"Board boundary: center={board_rect.Centre()}, width={board_rect.GetWidth()}, height={board_rect.GetHeight()}")
    print(f"Bounds: left={board_rect.GetLeft()}, bottom={board_rect.GetBottom()}, right={board_rect.GetRight()}, top={board_rect.GetTop()}")
    
    # Load footprint and adjust placement rectangle
    io = pcbnew.PCB_IO_KICAD_SEXPR()
    temp_footprint = io.FootprintLoad(footprint_lib, grounded_footprint)
    footprint_bbox = get_footprint_bbox(temp_footprint)
    
    # Calculate STAR board corner positions (2.5mm from pad edge to board edge)
    clearance_2_5mm = int(2.5 * 1000000)  # Convert 2.5mm to KiCad units
    pad_radius = footprint_bbox.GetWidth() // 2  # Half the pad width
    
    total_inset = clearance_2_5mm + pad_radius  # 2.5mm clearance + pad radius
    
    star_left = board_rect.GetLeft() + total_inset
    star_right = board_rect.GetRight() - total_inset
    star_bottom = board_rect.GetBottom() - total_inset
    star_top = board_rect.GetTop() + total_inset
    
    print(f"STAR board hole positions: left={star_left}, right={star_right}, bottom={star_bottom}, top={star_top}")
    
    # STAR board corner positions (grounded holes)
    star_corners = [
        pcbnew.wxPoint(star_left, star_bottom),   # Bottom-left
        pcbnew.wxPoint(star_left, star_top),      # Top-left  
        pcbnew.wxPoint(star_right, star_top),     # Top-right
        pcbnew.wxPoint(star_right, star_bottom),  # Bottom-right
    ]
    
    # Calculate other board hole positions (129mm x 79mm rectangle)
    other_width_mm = 129
    other_height_mm = 79
    other_half_width = int((other_width_mm / 2) * 1000000)  # Convert to KiCad units
    other_height = int(other_height_mm * 1000000)
    
    # Horizontal: centered on STAR board
    board_center_x = board_rect.Centre().x
    other_left = board_center_x - other_half_width
    other_right = board_center_x + other_half_width
    
    # Vertical: bottom holes of other board offset 2.5mm inward from STAR board bottom holes
    # In KiCad: Y increases downward, so bottom > top in Y coordinates
    offset_2_5mm = int(2.5 * 1000000)  # Convert 2.5mm to KiCad units
    other_bottom = star_bottom - offset_2_5mm  # 2.5mm above (smaller Y) STAR bottom holes
    other_top = other_bottom - other_height  # 79mm above the bottom holes
    
    print(f"Other board hole positions: left={other_left}, right={other_right}, bottom={other_bottom}, top={other_top}")
    
    # Other board corner positions (isolated holes)
    other_corners = [
        pcbnew.wxPoint(other_left, other_bottom),   # Bottom-left
        pcbnew.wxPoint(other_left, other_top),      # Top-left  
        pcbnew.wxPoint(other_right, other_top),     # Top-right
        pcbnew.wxPoint(other_right, other_bottom),  # Bottom-right
    ]
    
    # Add grounded mounting holes (for mounting this board to base board)
    print("Adding grounded mounting holes for STAR board corners (2.5mm from edges)...")
    grounded_group = pcbnew.PCB_GROUP(board)
    grounded_group.SetName("STAR Board Mounting Holes")
    
    for i, corner in enumerate(star_corners):
        footprint = io.FootprintLoad(footprint_lib, grounded_footprint)
        footprint_bbox = get_footprint_bbox(footprint)
        
        # Hide the reference designator
        footprint.Reference().SetVisible(False)
        
        # Adjust position to center footprint at corner
        pos_x = corner.x - footprint_bbox.Centre().x + footprint.GetPosition().x
        pos_y = corner.y - footprint_bbox.Centre().y + footprint.GetPosition().y
        position = pcbnew.VECTOR2I(int(pos_x), int(pos_y))
        footprint.SetPosition(position)
        
        board.Add(footprint)
        grounded_group.AddItem(footprint)
        print(f"Added grounded mounting hole {i+1} at position {position}")
    
    board.Add(grounded_group)
    
    # Add isolated mounting holes (for mounting other board on top)
    print("Adding isolated mounting holes for other board (129mm x 79mm pattern)...")
    isolated_group = pcbnew.PCB_GROUP(board)
    isolated_group.SetName("Other Board Mounting Holes")
    
    for i, corner in enumerate(other_corners):
        footprint = io.FootprintLoad(footprint_lib, isolated_footprint)
        footprint_bbox = get_footprint_bbox(footprint)
        
        # Hide the reference designator
        footprint.Reference().SetVisible(False)
        
        # Adjust position to center footprint at corner
        pos_x = corner.x - footprint_bbox.Centre().x + footprint.GetPosition().x
        pos_y = corner.y - footprint_bbox.Centre().y + footprint.GetPosition().y
        position = pcbnew.VECTOR2I(int(pos_x), int(pos_y))
        footprint.SetPosition(position)
        
        board.Add(footprint)
        isolated_group.AddItem(footprint)
        print(f"Added isolated mounting hole {i+1} at position {position}")
    
    board.Add(isolated_group)
    
    # Refresh board connectivity and display
    board.BuildConnectivity()
    pcbnew.Refresh()
    print("All mounting holes added successfully!")
    print("- 4 grounded holes at STAR board corners (2.5mm from edges)")
    print("- 4 isolated holes in 129mm x 79mm pattern (centered)")


if __name__ == "__main__":
    add_mounting_holes()