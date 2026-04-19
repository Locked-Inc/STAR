#!/usr/bin/env bash
# Generate every chart PNG into ./output/.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p output
for f in chart_a_lawsuit_trend.py \
         chart_b_compliance_donut.py \
         chart_c_cost_time.py \
         chart_d_disability_breakdown.py \
         chart_e_sensor_fusion.py \
         chart_f_pipeline_sankey.py; do
    echo ">>> $f"
    python3 "$f"
done
echo
echo "Done. PNGs in: $(pwd)/output/"
ls -lh output/
