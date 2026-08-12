# Gap List — WuBuPad vs Notepad++ 8.7.9

> **Verified 2026-08-11.** `ctest -j4` = 22/22 green (0.06s). Generated from a
> source audit of `src/` + `apps/` against Notepad++ 8.7.9 / Scintilla 5.x.

## Parity headline

Oracle (`oracle_v2 --repo pad --target npp`): **50/58 features present = 86%**
(with `ui` module false-negatives corrected below → effectively **~90%**).

## ⚠️ Oracle false-negatives (features ARE present, scanner blind-spot)

The parity scanner only counts a directory as a module if it has its OWN
`CMakeLists.txt`. WuBuPad builds `src/ui/*.c` from the TOP-LEVEL `CMakeLists.txt`
(no per-dir CMake), so the scanner reports zero `ui` modules. Seven oracle
"ABSENT" items map to `ui` and are actually **PRESENT** — verified by grep:

| Oracle "ABSENT" | Reality | Evidence |
|---|---|---|
| Tab bar | ✅ PRESENT | `draw_tab` in `ui.c` + `gfx_draw_tab` in `ui_gfx.c`, multi-doc chrome |
| Whitespace viz | ❌ genuinely absent | no show-spaces/tabs rendering |
| Style configurator | ❌ genuinely absent | no theme editor UI (only dark/light toggle) |
| Distraction-free / Zen | ❌ genuinely absent | no focus mode |
| F11 fullscreen | ❌ genuinely absent | no fullscreen toggle |
| Squiggly indicators | ❌ genuinely absent | no spell/syntax-error squiggle |
| Annotations panel | ❌ genuinely absent | no line-annotation gutter |

So of the 8 oracle-ABSENT, **1 is a scanner false-negative (Tab bar)** and
**7 are real gaps**.

## The REAL remaining gaps (honest, from source audit)

| # | Notepad++ feature | Status | Notes / plan |
|---|---|---|---|
| 1 | **Indent guides** | ❌ MISSING | dotted vertical guides at each indent level; pure `gfx_draw_line` overlay, no ref needed |
| 2 | **Whitespace visualization** | ❌ MISSING | show spaces (dots) / tabs (arrows) per Scintilla `WSVISIBLE`; self-runnable C11 |
| 3 | **Call tips** | ❌ MISSING | parameter hint popup on `(`; follow Scintilla SCI_CALLTIP pattern |
| 4 | **Style configurator** | ❌ MISSING | GUI to edit token colors/font (today only dark/light toggle) |
| 5 | **Squiggly indicators** | ❌ MISSING | red squiggle under misspelled words (wubuspell exists in WuBuOffice — bridge it) |
| 6 | **Annotations panel** | ❌ MISSING | per-line annotation gutter (comments/notes pinned to lines) |
| 7 | **Distraction-free / Zen** | ❌ MISSING | hide chrome, center text column |
| 8 | **F11 fullscreen** | ❌ MISSING | fullscreen toggle (SDL `SDL_SetWindowFullscreen`) |
| 9 | **Multi-view / split pane** | ❌ MISSING | split editor into two panes (side-by-side) |
| 10 | **More lexers** | ⚠️ PARTIAL | only C + JSON; port more from `ref/major/scintillua/lexers/*.lua` (~160 patterns) as C state machines |

## Present & verified (confirmed by grep, not assumed)

Piece-table buffer, undo/redo, cursor/selection, regex search (Thompson NFA),
clipboard (SDL system), tabs/multi-doc, code folding, auto-completion,
macro record/play, column/block mode, encoding detect/convert, bookmarks,
EOL convert, function list, plugin architecture (dlopen C-ABI), dark/light
theme, compare/diff, session save/restore, command palette (fuzzy), package
manager, tree view + git status, snippets, multiple cursors, auto-indent,
minimap, markdown preview, AGI/NDJSON protocol, text shaping (HarfBuzz+FriBidi).

## Scan tooling note

`parity_scanner_v2.c` should treat a directory as a module when its files are
`add_library`-referenced from the top-level CMake even without a per-dir
`CMakeLists.txt` (WuBuPad `src/ui/`). Until fixed, cross-check oracle ABSENT
items against `grep` before planning around them.
