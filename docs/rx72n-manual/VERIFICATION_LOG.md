# RX72N Verification Log

Chronological record of every verification session: what was checked, what was found,
what was fixed. Update this file as you work.

**Format per entry:**
```
## YYYY-MM-DD -- <Section Name>

**Pages read:** NNNN-MMMM
**Code file(s):** path/to/file.h

### Findings
- [OK] <register name> = 0xXXXX -- matches manual p.NNN
- [FIXED] <register name>: was 0xXXXX, manual p.NNN says 0xYYYY -- corrected
- [AVAILABLE] <feature name> -- added to AVAILABLE_FEATURES.md

### Commit(s)
- <commit hash> -- <commit message>
```

---

## 2026-04-13 -- Setup

**Pages read:** none yet (PDF split still in progress at session start)
**Code file(s):** N/A

### Findings
- Branch `feat/rx72n-manual-verification` created
- VERIFICATION_PLAN.md, AVAILABLE_FEATURES.md, VERIFICATION_LOG.md created
- PDF split in progress: `pdfseparate` splitting 3,233 pages into
  `docs/rx72n-manual/pages/page-NNNN.pdf`
- Full firmware inventory completed:
  - 28 libraries, 76 source files, 115 header files
  - All peripheral register headers identified (see VERIFICATION_PLAN.md)
  - Approximate chapter-to-page mapping created (needs confirmation from TOC)
- Two-pass strategy finalized:
  - Pass 1: PDF -> CODE (manual-driven, read every manual page)
  - Pass 2: CODE -> PDF (code-driven, read every source file)

### Commits
- 6bd48a498 -- docs: add RX72N manual and verification plan

---

(future sessions go below this line)
