#!/bin/bash

# Script to plot all KiCad schematic files to PDF
# Usage: ./plot_schematics.sh

for schematic in *.kicad_sch; do
  base_name=$(basename "$schematic" .kicad_sch)
  echo "Plotting $base_name..."
  kicad-cli sch export pdf -o "${base_name}.pdf" "$schematic"
done

echo "All schematics have been plotted to PDF!"