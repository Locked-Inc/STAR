#!/usr/bin/env python3
import fitz
from PIL import Image
import numpy as np

import os
script_dir = os.path.dirname(os.path.abspath(__file__))
doc = fitz.open(os.path.join(script_dir, 'star_fbd.pdf'))
page = doc[0]
mat = fitz.Matrix(2, 2)
pix = page.get_pixmap(matrix=mat)
img = Image.frombytes("RGB", [pix.width, pix.height], pix.samples)
arr = np.array(img)

height, width = arr.shape[:2]
print(f"Image dimensions: {width} x {height} pixels")

threshold = 252
non_white = np.any(arr < threshold, axis=2)

rows_with_content = np.any(non_white, axis=1)
cols_with_content = np.any(non_white, axis=0)

top = np.argmax(rows_with_content)
bottom = height - np.argmax(rows_with_content[::-1]) - 1
left = np.argmax(cols_with_content)
right = width - np.argmax(cols_with_content[::-1]) - 1

margins = {'Top': top, 'Bottom': height - bottom - 1, 'Left': left, 'Right': width - right - 1}

print(f"\nMargins (pixels from edge to content):")
for name, val in margins.items():
    print(f"  {name}: {val} px")

avg = sum(margins.values()) / 4
print(f"\nAverage: {avg:.1f}px")
print(f"Max deviation: {max(abs(v - avg) for v in margins.values()):.1f}px")

doc.close()
