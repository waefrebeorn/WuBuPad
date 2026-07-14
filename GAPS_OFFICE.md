# Gap List — WuBuOffice vs real office suites

Current verified state (this session): WuBuOffice is a **CLI** suite. It reads
and writes the OOXML family (docx/xlsx/pptx), ODF (odt/ods/odp + flat fodt/fods/
fodp), legacy binary (doc/xls/ppt via MS-CFB), and Markdown/CSV/TSV/RTF/HTML/
EPUB/PDF/JSON. The unified `convert` now round-trips all of these in both
directions. Feature *fidelity* (not just round-trip) is where the gaps are.

## Word (docx) — features vs gaps
| Feature | Status |
|---|---|
| Paragraphs, headings, bold/italic runs | DONE |
| Tables (single border, rectangular) | DONE |
| Basic styling (font name/size/color) | PARTIAL |
| Images / embedded media | GAP |
| Headers/footers, sections | GAP |
| Lists (bullet/number nesting) | GAP |
| Hyperlinks, bookmarks | GAP |
| Comments, track-changes | GAP |
| Page setup / pagination | GAP |
| Themes, styles gallery | GAP |
| RTL / complex scripts | GAP |

## Excel (xlsx) — features vs gaps
| Feature | Status |
|---|---|
| Cells, strings/numbers, bold | DONE |
| Multiple sheets, CSV/TSV | DONE |
| Formulas (wubuformula) | PARTIAL |
| Number formats, dates | PARTIAL |
| Charts | GAP |
| Merged cells, spanning | GAP |
| Styles/fills/borders richness | PARTIAL |
| Pivot tables | GAP |
| Images in cells | GAP |

## PowerPoint (pptx) — features vs gaps
| Feature | Status |
|---|---|
| Slides, text boxes, titles | DONE |
| Shapes (basic) | PARTIAL |
| Images / media | GAP |
| Transitions/animations | GAP |
| Layouts/master slides | GAP |
| Speaker notes richness | GAP |

## Cross-cutting
- **GUI**: entirely absent. Suite is command-line only (word/cell/show/doc/
  read/convert). See GAPS_GUI.md.
- **Fidelity oracle**: Word/Excel/PPT themselves aren't freely available as
  oracles; ODF + legacy (.doc/.xls/.ppt) provide independent cross-checks.
- **Performance**: files are assembled in memory; no streaming for very large
  docs (same class of work the editor's piece-table solves — reuse pattern).

## Honest "10% done"
Round-trip coverage of the format *matrix* is broad (~90% of formats). But
feature *fidelity* inside each format is shallow (headings+bold+tables ≈ 10% of
what Word/Excel/PPT do). The "10% done" estimate is accurate for fidelity, not
for format reach.
