# Formal Technical Proposal LaTeX Template

A professional LaTeX template for creating formal technical proposals, based on the ESET-419 Senior Design Project format (Texas A&M University).

## Features

- **MLA 9th Edition citations** (using biblatex with mla-new style)
- **Professional formatting**: Times New Roman font, 1.5 line spacing, 1" margins
- **Complete document structure**: Cover page, table of contents, 15 body sections, appendix
- **Example tables and figures**: Ready-to-use templates for WBS, budgets, milestones, etc.
- **Custom commands**: Section authors, requirements formatting, phase color boxes

## Quick Start

### On Overleaf
1. Upload all files maintaining the directory structure
2. Set compiler to **pdfLaTeX** with **Biber** as bibliography processor
3. Compile and start editing

### Local Compilation
```bash
# Install dependencies (macOS)
brew install --cask mactex

# Compile
latexmk -pdf main.tex
```

## Directory Structure

```
overleaf_template/
├── main.tex              # Main document file
├── preamble.sty          # Packages and configuration
├── references.bib        # MLA bibliography (example entries)
├── figures/
│   └── placeholder.png   # Placeholder for your figures
└── sections/
    ├── 01_cover.tex      # Cover page and cover letter
    ├── 00_introduction.tex
    ├── 02_problem_solution.tex
    ├── 03_scope.tex
    ├── 04_wbs.tex        # Work Breakdown Structure
    ├── 05_cbd.tex        # Conceptual Block Diagram
    ├── 06_nld.tex        # Network Logic Diagram
    ├── 07_gantt.tex      # Gantt Chart
    ├── 08_milestones.tex
    ├── 09_critical_path.tex
    ├── 10_resources.tex  # Resource Allocation Matrix
    ├── 11_budget.tex     # Bill of Materials, Labor, Budget
    ├── 12_risks.tex      # Risk Assessment
    ├── 13_assumptions.tex
    ├── 14_implementation.tex
    ├── 14b_lessons_learned.tex
    ├── 15_appendix.tex
    └── 16_references.tex # Works Cited
```

## Customization

### 1. Update Metadata (preamble.sty)
Edit the document metadata near the end of `preamble.sty`:
```latex
\title{Your Project Title\\Formal Technical Proposal}
\author{Team Member 1 \and Team Member 2 \and ...}
\date{December 8, 2025}
```

### 2. Update Cover Page (sections/01_cover.tex)
- Replace "Your University Name", "Your Project Title", etc.
- Add your team logo: uncomment `\includegraphics` and add your logo file
- Update the cover letter with your project details

### 3. Replace Placeholder Content
- Replace "Lorem ipsum" text with your actual content
- Replace "XX" in `\sectionauthors{XX, YY}` with team initials
- Update placeholder values in tables (0, X, DATE, etc.)

### 4. Add Your Figures
- Place your figure files in `figures/`
- Replace `placeholder.png` references with your actual filenames

### 5. Update Bibliography
Replace example entries in `references.bib` with your sources.

## Custom Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `\sectionauthors{CM, JB}` | After section title | Author initials |
| `\requirement{FR-001}{text}` | Requirements | SMART requirement format |
| `\money{100.00}` | Budget | Currency formatting |
| `\milestone{1}` | Milestones | Milestone marker (M1) |
| `\taskid{1.2.3}` | WBS | Task ID in monospace |
| `\phasebox{color}{text}` | Anywhere | Colored phase box |

## Phase Colors

Available colors for `\phasebox` and `\cellcolor`:
- `researchblue` - #E3F2FD
- `designyellow` - #FFF9C4
- `buildorange` - #FFCCBC
- `testgreen` - #C8E6C9
- `closepink` - #F8BBD0
- `pmoverhead` - #D1C4E9
- `brandcolor` - #500000 (customize for your brand)

## MLA Citations

Use these commands for in-text citations:
```latex
According to \textcite{key}, lorem ipsum...    % Smith argues that...
This is documented \parencite{key}.            % (Smith)
```

## Requirements

- LaTeX distribution (TeX Live 2020+, MiKTeX, or MacTeX)
- `biblatex` with `biber` backend
- `biblatex-mla` package for MLA formatting

## License

This template is provided as-is for educational purposes.
