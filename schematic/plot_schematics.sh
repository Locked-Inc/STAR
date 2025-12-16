#!/bin/bash

# Script to plot all KiCad schematic files to PDF
# Usage: ./plot_schematics.sh

for schematic in *.kicad_sch; do
  base_name=$(basename "$schematic" .kicad_sch)
  echo "Plotting $base_name..."
  kicad-cli sch export pdf -o "${base_name}.pdf" "$schematic"
  echo "Converting $base_name to A4..."
  gs -sDEVICE=pdfwrite -sPAPERSIZE=a4 -dFIXEDMEDIA -dPDFFitPage -dCompatibilityLevel=1.4 -dNOPAUSE -dQUIET -dBATCH -sOutputFile="${base_name}_A4.pdf" "${base_name}.pdf"
  echo "Converting $base_name to Letter..."
  gs -sDEVICE=pdfwrite -sPAPERSIZE=letter -dFIXEDMEDIA -dPDFFitPage -dCompatibilityLevel=1.4 -dNOPAUSE -dQUIET -dBATCH -sOutputFile="${base_name}_letter.pdf" "${base_name}.pdf"
done

echo "All schematics have been plotted to PDF!"