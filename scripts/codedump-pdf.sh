#!/usr/bin/env bash
# scripts/codedump-pdf.sh -- syntax-highlighted source-listing PDF builder.
#
# Walks a directory, finds source files, and produces a single PDF whose
# table of contents has one entry per file. Used by the per-subsystem
# combined-PDF Makefile targets (gateway, compliance, ui, proto, matlab).
#
# Toolchain:
#   pygments  (python3 -m pygments) -- per-file syntax highlighting -> LaTeX
#   xelatex                          -- compile master tex -> PDF
#
# Usage:
#   scripts/codedump-pdf.sh <root_dir> <output_pdf> <title> <file_globs...>
#
# Example:
#   scripts/codedump-pdf.sh \
#     star-gateway docs/pdfs/gateway/STAR_Gateway.pdf \
#     "STAR Gateway (Go)" "*.go"
#
# All flags after the title are passed to `find` as `-name <glob> -o ...`.
# Always-excluded paths (regardless of glob): node_modules, build, dist,
# .git, vendor, gen, .next, .cache, __pycache__, target, .venv.
#
# Exits non-zero on any failure. ASCII-only output.

set -euo pipefail

if [ $# -lt 4 ]; then
  echo "Usage: $0 <root_dir> <output_pdf> <title> <file_glob> [<file_glob> ...]"
  exit 2
fi

ROOT_DIR="$1"
OUT_PDF="$2"
TITLE_RAW="$3"
shift 3
GLOBS=("$@")

# LaTeX-escape the title (underscores, ampersands, etc.)
TITLE=$(printf '%s' "$TITLE_RAW" \
        | sed 's/\\/\\textbackslash{}/g; s/_/\\_/g; s/&/\\&/g; s/#/\\#/g; s/\$/\\$/g; s/%/\\%/g')
# For PDF metadata (no LaTeX escapes; just strip backslashes)
TITLE_META=$(printf '%s' "$TITLE_RAW" | tr -d '\\')

if [ ! -d "$ROOT_DIR" ]; then
  echo "ERROR: root dir not found: $ROOT_DIR"
  exit 1
fi

# Resolve to absolute paths
ROOT_DIR="$(cd "$ROOT_DIR" && pwd)"
case "$OUT_PDF" in
  /*) OUT_ABS="$OUT_PDF" ;;
  *)  OUT_ABS="$(pwd)/$OUT_PDF" ;;
esac
mkdir -p "$(dirname "$OUT_ABS")"

# Tool checks
if ! command -v xelatex >/dev/null 2>&1; then
  echo "ERROR: xelatex not found"
  exit 1
fi
if ! python3 -m pygments -h >/dev/null 2>&1; then
  echo "ERROR: pygments not available (pip3 install pygments)"
  exit 1
fi

WORK="$(mktemp -d -t codedump-pdf-XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

# Build find expression: -type f \( -name g1 -o -name g2 ... \)
FIND_EXPR=( -type f \( )
first=1
for g in "${GLOBS[@]}"; do
  if [ $first -eq 1 ]; then
    FIND_EXPR+=( -name "$g" )
    first=0
  else
    FIND_EXPR+=( -o -name "$g" )
  fi
done
FIND_EXPR+=( \) )

# Always-excluded directory names
PRUNE_DIRS=( node_modules build dist .git vendor gen .next .cache \
             __pycache__ target .venv venv .pytest_cache .mypy_cache \
             docs/doxygen docs/pdfs )

PRUNE_EXPR=( \( )
fp=1
for d in "${PRUNE_DIRS[@]}"; do
  if [ $fp -eq 1 ]; then
    PRUNE_EXPR+=( -path "*/$d" )
    fp=0
  else
    PRUNE_EXPR+=( -o -path "*/$d" )
  fi
done
PRUNE_EXPR+=( \) -prune )

# Collect files (sorted, relative)
FILES_LIST="$WORK/files.txt"
( cd "$ROOT_DIR" && find . "${PRUNE_EXPR[@]}" -o "${FIND_EXPR[@]}" -print ) \
  | sed 's|^\./||' | LC_ALL=C sort > "$FILES_LIST"

NUM_FILES=$(wc -l < "$FILES_LIST" | tr -d ' ')
if [ "$NUM_FILES" = "0" ]; then
  echo "ERROR: no source files found under $ROOT_DIR matching: ${GLOBS[*]}"
  exit 1
fi
echo "  Collected $NUM_FILES source files under $ROOT_DIR"

# Map glob/extension -> Pygments lexer name (most are auto-detected, but we
# pass an explicit -l for robustness).
ext_to_lexer() {
  case "$1" in
    *.go)                echo go ;;
    *.py)                echo python ;;
    *.ts|*.tsx)          echo typescript ;;
    *.js|*.jsx|*.mjs)    echo javascript ;;
    *.cpp|*.cc|*.cxx)    echo cpp ;;
    *.hpp|*.hh|*.hxx)    echo cpp ;;
    *.c)                 echo c ;;
    *.h)                 echo c ;;
    *.proto)             echo protobuf ;;
    *.m)                 echo matlab ;;
    *.sh|*.bash)         echo bash ;;
    *.yaml|*.yml)        echo yaml ;;
    *.json)              echo json ;;
    *.md)                echo markdown ;;
    *.cmake|CMakeLists.txt) echo cmake ;;
    *.xml)               echo xml ;;
    *.html|*.htm)        echo html ;;
    *.css)               echo css ;;
    *)                   echo text ;;
  esac
}

# Convert relative path -> per-file safe basename for tex include (no dots/slashes)
safe_name() {
  echo "$1" | tr '/.' '__' | tr -c 'A-Za-z0-9_' '_'
}

LATEX_DIR="$WORK/tex"
mkdir -p "$LATEX_DIR"

# 1. Generate per-file pygments .tex snippets
echo "  Highlighting files with pygments..."
gen_count=0
while IFS= read -r rel; do
  src_abs="$ROOT_DIR/$rel"
  lex=$(ext_to_lexer "$rel")
  safe=$(safe_name "$rel")
  out="$LATEX_DIR/file_${safe}.tex"
  # Use -O texcomments=False to avoid pygments interpreting LaTeX comments
  if ! python3 -m pygments -l "$lex" -f latex -O "linenos=true,style=default" \
       -o "$out" "$src_abs" 2>/dev/null; then
    # Fallback: dump as plain verbatim if pygments choked
    {
      echo '\begin{Verbatim}[commandchars=\\\{\}]'
      sed 's/\\/\\textbackslash{}/g; s/{/\\{/g; s/}/\\}/g' "$src_abs" || true
      echo '\end{Verbatim}'
    } > "$out"
  fi
  gen_count=$((gen_count + 1))
done < "$FILES_LIST"
echo "  Highlighted $gen_count files."

# 2. Get pygments style defs (once)
python3 -m pygments -S default -f latex > "$LATEX_DIR/pygments_style.tex"

# 3. Build master.tex
MASTER="$LATEX_DIR/master.tex"
{
  cat <<'TEXHDR'
\documentclass[10pt,letterpaper]{article}
\usepackage[a4paper,margin=0.75in]{geometry}
\usepackage{fancyvrb}
\usepackage{fvextra}
\usepackage{xcolor}
\usepackage{textcomp}
\usepackage{upquote}
\usepackage[T1]{fontenc}
\usepackage{lmodern}
\usepackage{titlesec}
\usepackage{etoolbox}
\usepackage{longtable}
\usepackage[bookmarks=true,bookmarksopen=true,bookmarksnumbered=true,colorlinks=true,linkcolor=blue,urlcolor=blue]{hyperref}

% Pygments style defs
\input{pygments_style}

% Allow long lines to break in the Verbatim env
\fvset{breaklines=true,breakanywhere=true,fontsize=\footnotesize,frame=single,framesep=2mm}

% Compact section spacing -- we have a lot of files
\titlespacing*{\section}{0pt}{1.5ex plus 0.4ex minus 0.2ex}{0.8ex plus 0.2ex}
\titlespacing*{\subsection}{0pt}{1.0ex plus 0.3ex minus 0.2ex}{0.5ex plus 0.1ex}

% Don't crash on wide lines / underfull boxes
\setlength{\emergencystretch}{3em}
\sloppy

\setcounter{tocdepth}{2}

TEXHDR

  # PDF metadata (use raw-ish form -- strip TeX escapes)
  echo "\hypersetup{pdftitle={${TITLE_META}}}"

  # Title page
  cat <<TITLEPAGE
\title{\Huge\bfseries ${TITLE}\\\\[0.3em]\Large Source Listing}
\author{STAR Development Team \\\\ Locked Inc.}
\date{Version 1.1 \\\\ April 2026 \\\\ MIT License}

\begin{document}
\begin{titlepage}
\centering
\vspace*{2.5cm}
{\Huge\bfseries ${TITLE}\par}
\vspace{0.4cm}
{\LARGE Source Listing\par}
\vspace{0.6cm}
{\large Auto-generated syntax-highlighted listing\par}
\vfill
{\large Version 1.1 \par}
{\large April 2026 \par}
\vspace{0.4cm}
{\large MIT License\par}
\vfill
{\normalsize STAR Development Team \\\\ Locked Inc.\par}
\vspace{0.6cm}
{\small\itshape Generated by \texttt{scripts/codedump-pdf.sh}.\par}
\end{titlepage}

\tableofcontents
\newpage

TITLEPAGE

  # Group files by top-level subdir for nicer TOC
  prev_top=""
  while IFS= read -r rel; do
    top="${rel%%/*}"
    if [ "$top" = "$rel" ]; then
      top="(root)"
    fi
    if [ "$top" != "$prev_top" ]; then
      # Escape underscores for LaTeX
      top_esc=$(printf '%s' "$top" | sed 's/_/\\_/g; s/&/\\&/g; s/#/\\#/g; s/\$/\\$/g; s/%/\\%/g')
      echo ""
      echo "\\section{${top_esc}}"
      prev_top="$top"
    fi
    safe=$(safe_name "$rel")
    rel_esc=$(printf '%s' "$rel" | sed 's/_/\\_/g; s/&/\\&/g; s/#/\\#/g; s/\$/\\$/g; s/%/\\%/g')
    echo "\\subsection*{\\texttt{${rel_esc}}}"
    echo "\\addcontentsline{toc}{subsection}{\\texttt{${rel_esc}}}"
    echo "\\input{file_${safe}}"
    echo ""
  done < "$FILES_LIST"

  echo "\end{document}"
} > "$MASTER"

# 4. Compile (twice for TOC)
echo "  Compiling with xelatex..."
cd "$LATEX_DIR"
xelatex -interaction=nonstopmode -halt-on-error master.tex >master.log1 2>&1 \
  || { tail -60 master.log1; echo "ERROR: xelatex pass 1 failed"; exit 1; }
xelatex -interaction=nonstopmode -halt-on-error master.tex >master.log2 2>&1 \
  || { tail -60 master.log2; echo "ERROR: xelatex pass 2 failed"; exit 1; }

if [ ! -f master.pdf ]; then
  echo "ERROR: master.pdf not produced"
  exit 1
fi

cp master.pdf "$OUT_ABS"
echo "  Wrote: $OUT_ABS"
if command -v pdfinfo >/dev/null 2>&1; then
  pdfinfo "$OUT_ABS" | grep -E '^(Pages|File size):' || true
fi
