# STAR Style Guides

For comprehensive style guide documentation, refer to the LaTeX files in the main documentation:

## Protocol Buffers Style Guide
See: `docs/sections/04_style_guide.tex`

Covers:
- Protocol Buffers naming conventions (Boston Dynamics style)
- MKS unit suffixes
- Enum zero value conventions
- Request/Response patterns
- Breaking change policy

## C/ESP32 Style Guide
See: `docs/sections/05_esp32_style.tex`

Covers:
- C naming conventions
- Embedded systems best practices
- Real-time control considerations
- Memory management rules
- Safety-critical coding standards

## Generating Documentation

To build the complete PDF documentation:

```bash
cd docs
pdflatex star_documentation.tex
# Or use:
latexmk -pdf star_documentation.tex
```

The generated PDF (`star_documentation.pdf`) contains the authoritative style guides for all languages used in the STAR project.
