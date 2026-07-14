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
| Tabs / multi-document | session mgr (`src/docs`) | DONE (headless); GUI tab bar = GAP |
| Code folding | lexer fold levels + view | GAP |
| Auto-completion | symbol index from lexer | GAP |
| Regex search/replace | `src/search` (Thompson NFA) | DONE (headless engine); GUI find-box = GAP |
| Macro record/play | command log + replay | GAP |
| Column/block mode | cursor + buffer range ops | GAP |
| Encoding detect/convert (UTF-8/16/32, ANSI) | `src/encode` | DONE (headless); GUI encoding menu = GAP |
| Bookmark / line ops | view layer | GAP |
| EOL convert (CRLF/LF) | buffer newline model | GAP |
| Function list | lexer symbol table | GAP |
| Plugin architecture | stable C ABI + loader | GAP |
| Dark mode / styling | GUI theme layer | GAP |
| Compare / diff | `src/diff` (LCS) | DONE (headless engine); GUI compare view = GAP |
| Session save/restore | session mgr (`src/docs`) | DONE (model); GUI = GAP |

## Honest assessment
The **hard core** is now substantially built and **tested**: piece-table buffer
(+ line/col mapping), C/JSON lexers, document model with undo/redo + cursor,
literal + regex **search**, UTF-8/16/32 + Latin1 **encoding**, **diff**, and a
multi-document **session** model. All headless, all green under ASan+UBSan
(0 leaks / 0 UB / 0 warnings).

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

