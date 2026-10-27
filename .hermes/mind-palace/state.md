# WuBuPad — STATE (v1, 2026-07-29)

## Purpose
Live truth table for WuBuPad build/test status.

## Key Numbers (VERIFIED)
- 22 ctest tests, 22 pass, 0.06s — VERIFIED BY RUNNING ctest -j4
- lex.c: 289 lines, 2 lexers (C, JSON) — VERIFIED
- apps/wubupad/main.c references to 9 wubuos-only features: ALL 0 — VERIFIED
- Features existing in src/ui/ with green tests:
  - ui_atom.c (110 lines, test_atom.c) — command palette
  - fuzzy/fuzzy.c (69 lines, test_fuzzy.c)
  - treeview/ (129 lines, test_treeview.c)
  - snippet/ (98 lines, test_snippet.c)
  - multicursor/ (36 lines, test_multicursor.c)
  - minimap/ (49 lines, test_minimap.c)
  - mdpreview/ (136 lines, test_mdpreview.c)
  - autoindent/ (52 lines, test_autoindent.c)
  - pkgmgr/ (189 lines, test_pkgmgr.c)

## Build
- `cmake -S . -B build && cmake --build build`
- `ctest --test-dir build -j4` — fast (0.06s all 22)

## Ref Repository Tracings
- ref/major/scintillua/lexers/ — 160 ready-to-study lexer patterns (Lua, but patterns portable)
- ref/major/scintilla — text editing component reference (call tips, indicators, folding)
- ref/major/notepad-plus-plus — full editor reference (architecture study, code is C++)
- ref/lesser-known/mujs — embeddable JS engine (912KB) for scripting
- ref/lesser-known/fredbuf — piece-tree alternative for from-scratch study
