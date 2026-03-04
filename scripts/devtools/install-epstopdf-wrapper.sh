#!/bin/sh
# Install an epstopdf(1) wrapper that delegates to gs(1).
# Required because texlive-font-utils (which ships epstopdf) is not available
# as an apt package in the STAR devcontainer environment, but ghostscript is.
set -e

DEST=/usr/local/bin/epstopdf

if command -v epstopdf >/dev/null 2>&1; then
    echo "  epstopdf already present at $(command -v epstopdf) -- skipping"
    exit 0
fi

if ! command -v gs >/dev/null 2>&1; then
    echo "ERROR: gs (ghostscript) not found. Install it first: sudo apt-get install ghostscript"
    exit 1
fi

echo "  Installing epstopdf wrapper at $DEST ..."

sudo tee "$DEST" > /dev/null << 'EOF'
#!/bin/sh
# epstopdf wrapper: converts EPS to PDF using ghostscript.
# Usage: epstopdf [--outfile=output.pdf] input.eps
input=""
output=""
for arg in "$@"; do
    case "$arg" in
        --outfile=*) output="${arg#--outfile=}" ;;
        --outfile|-*) ;;
        *) input="$arg" ;;
    esac
done
[ -z "$input" ] && { echo "epstopdf: no input file"; exit 1; }
[ -z "$output" ] && output="${input%.eps}.pdf"
exec gs -q -dNOPAUSE -dBATCH -dSAFER \
    -sDEVICE=pdfwrite \
    -dEPSCrop \
    -sOutputFile="$output" \
    "$input"
EOF

sudo chmod +x "$DEST"
echo "  epstopdf installed at $DEST"
