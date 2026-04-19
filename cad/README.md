# STAR Chassis CAD

Fusion 360 sources for the STAR robot chassis assembly.

## Files

- `STAR.f3z` -- top-level chassis assembly (Fusion 360 archive, all
  components bundled). Stored via Git LFS; install LFS before cloning:
  ```
  git lfs install
  git clone https://github.com/Locked-Inc/STAR.git
  ```

## Working with the archive

Open `STAR.f3z` directly in Fusion 360 (File -> Open) to load the whole
assembly with its component tree. Editing in Fusion creates a new cloud
copy -- to re-archive after changes, re-export as `.f3z` and overwrite
this file, then commit.

## Neutral exports

When the assembly changes, also export a STEP file for downstream tools
that do not read Fusion native format (KiCad 3D viewer, mechanical review,
vendors without Autodesk licenses). Place it alongside as `STAR.step`.
