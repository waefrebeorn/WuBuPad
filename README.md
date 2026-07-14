# WuBuPad

A **clean, ground-up C11** code editor being built to rival Notepad++ feature
coverage — without forking it. Headless, GUI-agnostic core first; rendering is
a layer that plugs in later (see `GAPS_GUI.md`).

## Design rules (same bar as WuBuOffice)
- Strict **C11**, opaque structs, minimal includes.
- **No god headers**, no monolithic files — every module self-contained.
- **Reuse never duplicate**: one buffer, one lexer registry, one doc model.
- **Sanitizer-gated**: `ctest` must pass green under ASan + UBSan
  (0 leaks, 0 UB, 0 warnings) before any change ships.

## Architecture
```
src/
  buffer.h / buffer.c   -- piece-table text storage (Scintilla-style)
  lex.h    / lex.c      -- syntax token model + C / JSON lexers
  doc.h    / doc.c      -- document: buffer + undo/redo + cursor/selection
tests/
  test_core.c           -- buffer/doc/lex acceptance tests
```
The core is **headless** (no platform headers) so it is unit-testable and
reusable by any front-end.

## Build & test
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build

# with the leak/UB sanitizer gate
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DWITH_SANITIZER=ON
cmake --build build-san -j4
ctest --test-dir build-san
```

## Status (honest)
The hard editing core is built and tested: piece-table buffer, C + JSON
lexers, linear undo/redo, byte-level cursor + selection. The GUI/UX surface
(tabs, folding, completion, search, encoding, plugins) is unstarted — see
`GAPS_NOTEPAD.md`, `GAPS_OFFICE.md`, `GAPS_GUI.md`.

## Reference
Notepad++ source is cloned read-only to `../ref/notepad-plus-plus` for feature
parity reference only. WuBuPad is original code.
