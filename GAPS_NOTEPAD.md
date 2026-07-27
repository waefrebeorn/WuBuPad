# Gap List — WuBuPad vs Notepad++

Goal: a **clean C11** code editor that rivals Notepad++ feature coverage.
Notepad++ itself is GPL and built on **Scintilla** (editing engine, C++) +
Boost + Win32. We are NOT forking it; we are building a ground-up, fork-free,
clean-code rival. This document is the feature/gap inventory that drives the
module plan.

## What Notepad++ actually is (from its source tree, cloned to ref/)
- Editing engine: **Scintilla** (`scintilla/`, 6.3M, vendored). The buffer is a
  **piece-chain** (`scintilla/src/CellBuffer.cxx` = `CellBuffer`, `Document.cxx`
  = `Document`, `Editor.cxx` = `Editor`, `UniConversion` for UTF). WuBuPad's
  `src/buffer.c` is the SAME architecture (piece table) — this is the proven
  design, not a coincidence; we implement it clean-room in C11.
- UI: Win32 dialogs (`Notepad_plus_Window`, `NppWindow`), plus `DarkMode/` +
  `dpiManagerV2` (HiDPI), `EncodingMapper` (UTF-8/16/32 + code pages),
  `pugixml` for config, and a `ScintillaComponent` layer.
- Lexers/highlighting: Scintilla's `Lex*` modules (one per language).
- Not C11, not portable off Windows without Scintilla. We deliberately build a
  fork-free, C11, GUI-agnostic core instead.

## Feature inventory → WuBuPad module plan

| Notepad++ feature | WuBuPad module | Status |
|---|---|---|
| Large-file text buffer (piece-chain) | `src/buffer` (piece table) | DONE (core, tested) |
| Multi-language syntax highlighting | `src/lex` (C + JSON done) | PARTIAL |
| Undo/redo (linear + grouped) | `src/doc` undo stack | DONE (linear LIFO, tested) |
| Cursor + selection + edit ops | `src/doc` cursor | DONE (byte-level, tested) |
| Tabs / multi-document | session mgr (`src/docs`) | DONE (headless); **GUI tab bar = CLOSED** (wubuos shell + Editor multi-doc) |
| Code folding | lexer fold levels + view | GAP — needs lexer fold-level API (not yet in `lex.h`) |
| Auto-completion | symbol index from lexer | **CLOSED** (wubuos: builtin C words + doc-identifier scan) |
| Regex search/replace | `src/search` (Thompson NFA) | DONE (headless); **GUI find-box = CLOSED** |
| Macro record/play | command log + replay | **CLOSED** (wubuos: session-global op buffer) |
| Column/block mode | cursor + buffer range ops | **CLOSED** (wubuos: block selection model + render) |
| Encoding detect/convert | `src/encode` | DONE (headless); **GUI encoding menu = CLOSED** |
| Bookmark / line ops | view layer | **CLOSED** (wubuos: Ctrl+F2 toggle, F2/Shift+F2 jump) |
| EOL convert (CRLF/LF) | buffer newline model | DONE (headless); **GUI EOL convert = CLOSED** |
| Function list | lexer symbol table | GAP — needs lexer symbol-table API (not yet in `lex.h`) |
| Plugin architecture | stable C ABI + loader | GAP — needs ABI + loader (separate work) |
| Dark mode / styling | GUI theme layer | **CLOSED** (wubuos: Ctrl+` theme toggle) |
| Compare / diff | `src/diff` (LCS) | DONE (headless); **GUI compare view = CLOSED** |
| Session save/restore | session mgr (`src/docs`) | DONE (model); **GUI = CLOSED** (wubuos: Ctrl+Shift+S save, restore on launch) |

## Closures landed in `apps/wubuos` (cross-repo GUI shell)
- GUI tab bar: click-switch tabs over all 5 engines + Compare.
- GUI find-box: Ctrl+F find, Ctrl+H replace, F3/Shift+F3, Ctrl+R replace-all, regex+literal.
- Go-to-line: Ctrl+G.
- EOL convert: Ctrl+E toggles LF<->CRLF; status shows LF/CRLF + detected encoding.
- Encoding: on file load, `enc_detect` labels encoding; status shows it.
- Compare view: `wubuos compare <a> <b>` diffs two files via `src/diff`.
- Dark theme: Ctrl+` toggles light/dark.
- Multi-document: Ctrl+T new, Ctrl+W close, Ctrl+Tab/Ctrl+Shift+Tab cycle, drawn doc-tab strip.
- Bookmarks: Ctrl+F2 toggle, F2 next, Shift+F2 prev (gutter disc).
- Column/block selection: Ctrl+Alt+C on, arrows extend rectangular block.
- Macro record/play: Ctrl+Shift+R record, Ctrl+Shift+P play (session-globalopter).
- Auto-completion: Ctrl+Space popup (builtin C words + doc identifiers).
- Session save/restore: Ctrl+Shift+S save, restore on launch (WUBUOS_RESTORE=1).

## Remaining gaps (require engine extensions, not just GUI)
- **Code folding** — needs `lex.h` to expose per-line fold levels.
- **Function list** — needs `lex.h` to expose a symbol table.
- **Plugin ABI / loader** — separate C-ABI + dynamic-loader effort.


What remains is the **UX layer** on top: the GUI itself (tabs bar, find box,
encoding menu, compare view, folding, completion, plugins) plus more lexers and
editor features (column mode, EOL convert, macro replay). The foundation is
deliberately headless so those layer on without disturbing the verified core.

## Reference, not dependency
The notepad-plus-plus source at `../ref/notepad-plus-plus` is read-only
reference for feature parity. No code is copied; WuBuPad is original C11.

## Validation of the core design
Notepad++/Scintilla uses a piece-chain buffer (`CellBuffer.cxx`). WuBuPad's
`src/buffer.c` is a clean-room piece table with the same properties (O(1)-ish
edits, full undo). This confirms the architecture choice is the industry-
proven one, independently of us.

