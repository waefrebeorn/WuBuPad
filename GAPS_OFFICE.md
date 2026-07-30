# Gap List — WuBuPad in the Office Suite

> ⚠️ **STALE — DO NOT USE FOR PLANNING.** This document contains outdated
> "GAP" markers. Some items marked GAP are actually MODULE-level implementations
> that exist in `src/` but are NOT wired into the `wubuos` GUI shell.
>
> **Use [GAPS_REAL.md](../WuBuOffice/GAPS_REAL.md) instead** — it is the
> verified, honest gap list generated from source audit + build + ctest
> (71/71 green for WuBuOffice, 22/22 green for WuBuPad) + web research
> against LibreOffice 26.2, OnlyOffice, Microsoft Office 2025.
>
> This document is kept for historical reference only.

Current verified state (this session): WuBuPad is a **standalone C11 editor**
that is embedded as the Editor view inside WuBuOffice (`wubuos`). Its piece-table
buffer, lexers, undo/redo, cursor/selection, search, encoding, line diff, and
multi-doc session are reused verbatim by the office suite's Editor tab.

## WuBuPad's role in the office suite

| Component | Status | Notes |
|-----------|--------|-------|
| Piece-table buffer | REAL | Embedded in `apps/wubuos/view_editor.c` (1233 lines) |
| C/JSON lexers | REAL | 2 lexers only |
| Undo/redo | REAL | Linear LIFO |
| Cursor + selection | REAL | |
| Regex search/replace | REAL | Thompson NFA |
| Clipboard | REAL | SDL2 system clipboard |
| Multi-document tabs | REAL | |
| Code folding | REAL | |
| Auto-completion | REAL | Builtin C words + doc identifiers |
| Function list | REAL | |
| Macro record/play | REAL | |
| Column/block selection | REAL | |
| EOL convert | REAL | |
| Bookmarks | REAL | |
| Encoding detect/convert | REAL | |
| Diff (LCS) | REAL | Used by Compare view |
| Session save/restore | REAL | |
| Dark/light theme | REAL | |
| AGI protocol (JSON) | REAL | Headless agent protocol |

## What WuBuPad does NOT provide to the office suite

| Feature | Status | Notes |
|---------|--------|-------|
| More than 2 lexers | GAP | Only C + JSON; office suite needs Python, JS, HTML, etc. |
| Plugin manager | GAP | Sample .so loader exists but no package manager |
| Command palette | WUBOOS | Only in wubuos shell, not standalone wubupad |
| Fuzzy finder | WUBOOS | Only in wubuos shell |
| Tree view + git status | WUBOOS | Only in wubuos shell |
| Snippets | WUBOOS | Only in wubuos shell |
| Multiple cursors | WUBOOS | Only in wubuos shell |
| Markdown preview | WUBOOS | Only in wubuos shell |
| Minimap | WUBOOS | Only in wubuos shell |
| Auto-indent | WUBOOS | Only in wubuos shell |
| Print support | GAP | Does not exist |
| Accessibility (IA2) | GAP | No screen reader support |

## Cross-cutting (office suite)

- **GUI**: `wubuos` SDL2 shell exists with 7 views (Document, Editor, Cell, Slide, OCR, Compare, Settings). But 40+ modules in `src/` are NOT wired into the GUI.
- **Fidelity oracle**: Word/Excel/PPT themselves aren't freely available as oracles; ODF + legacy (.doc/.xls/.ppt) provide independent cross-checks.
- **Performance**: files are assembled in memory; no streaming for very large docs (same class of work the editor's piece-table solves — reuse pattern).

## Honest assessment

Round-trip coverage of the format *matrix* is broad (~90% of formats). But
feature *fidelity* inside each format is shallow (headings+bold+tables ≈ 10% of
what Word/Excel/PPT do). The "10% done" estimate is accurate for fidelity, not
for format reach.

See [GAPS_REAL.md](../WuBuOffice/GAPS_REAL.md) for the full verified gap list.
