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
| Clipboard (cut/copy/paste/paste-plain/select-all) | `src/ui` clipboard + SDL system clipboard | **CLOSED** (SDL2 clipboard wired in gfx backend; Ctrl+X/C/V/Shift+V/A + Ctrl+A) |
| Tabs / multi-document | session mgr (`src/docs`) | DONE (headless); **GUI tab bar = CLOSED** (wubuos shell + Editor multi-doc) |
| Code folding | lexer fold levels + view | **CLOSED** (wubuos: Ctrl+Shift+F folds brace block via `lex_folds`; hidden lines skipped + ▾ marker) |
| Auto-completion | symbol index from lexer | **CLOSED** (wubuos: builtin C words + doc-identifier scan) |
| Regex search/replace | `src/search` (Thompson NFA) | DONE (headless); **GUI find-box = CLOSED** |
| Macro record/play | command log + replay | **CLOSED** (wubuos: session-global op buffer) |
| Column/block mode | cursor + buffer range ops | **CLOSED** (wubuos: block selection model + render) |
| Encoding detect/convert | `src/encode` | DONE (headless); **GUI encoding menu = CLOSED** |
| Bookmark / line ops | view layer | **CLOSED** (wubuos: Ctrl+F2 toggle, F2/Shift+F2 jump) |
| EOL convert (CRLF/LF) | buffer newline model | DONE (headless); **GUI EOL convert = CLOSED** |
| Function list | lexer symbol table | **CLOSED** (wubuos: Ctrl+Shift+L panel via `lex_symbols`) |
| Plugin architecture | stable C ABI + loader | **CLOSED** (wubuos: `wuos_plugin.h` ABI v1, `dlopen` loader, sample `.so`, Ctrl+Shift+K runs) |
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
- Code folding: Ctrl+Shift+F folds the brace block at the cursor (hides body, ▾ marker).
- Function list: Ctrl+Shift+L toggles a right-hand panel listing `lex_symbols` (name : Ln).
- Session save/restore: Ctrl+Shift+S save, restore on launch (WUBUOS_RESTORE=1).
- Plugin architecture: `wuos_plugin.h` stable C ABI (v1) + `dlopen` loader (`plugin.c`) scanning `~/.wubuos/plugins/*.so`; sample plugin builds to `sample_plugin.so`; Ctrl+Shift+K runs the next loaded plugin (host log + exec toast). Headless `test_plugin_abi` + `test_view` plugin block verify load→init→exec.

## Atom absorption (package-driven, hackable editor)
Atom is sunsetting; WuBuPad now absorbs its defining capabilities as real,
opaque, headless-tested modules (no third-party deps), wired into the `ui`
controller via `src/ui/ui_atom.c`:

| Atom feature            | Module (`src/...`)            | Status |
|-------------------------|-------------------------------|--------|
| Command registry (spine) | `command/`                 | CLOSED (named commands: `editor:toggle-theme`, etc.) |
| Command palette (Cmd-Shift-P) | `palette/` + `fuzzy/` | CLOSED (fuzzy filter over registry; runs chosen cmd) |
| Fuzzy finder (Cmd-T/P)  | `fuzzy/`                     | CLOSED (subsequence scorer, word-boundary bonus) |
| Package manager         | `pkgmgr/`                    | CLOSED (scans `~/.wubupad/packages/*`, parses package.json, dlopen C-ABI plugins, registers manifest cmds) |
| Tree view + git status  | `treeview/`                  | CLOSED (recursive dir walk; git `--porcelain` status by basename, nested files) |
| Snippets                | `snippet/`                   | CLOSED (`${1:default}` tabstops + mirror fields, Tab cycles) |
| Multiple cursors        | `multicursor/`               | CLOSED (parallel insert across N carets) |
| Auto-indent / smart typing | `autoindent/`             | CLOSED (brace/paren-aware newline + indent) |
| Minimap                 | `minimap/`                   | CLOSED (downsampled lit-row overview) |
| Markdown preview        | `mdpreview/`                 | CLOSED (MD->HTML: headings, bold/italic/code, lists, quote, hr, fenced code, escaping) |

Wiring: `ui_create()` builds the registry + palette + discovers packages;
`ui_apply()` routes all keystrokes to the palette while it is open; built-in
editor commands are registered so the palette lists + runs them. `UI_KEY_PALETTE`
(Ctrl-Shift-P) opens it. Each module ships a headless test; `test_atom` drives
the palette end-to-end through the UI controller. All 22 ctest suites pass.


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

