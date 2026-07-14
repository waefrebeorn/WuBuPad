# WuBuPad

A **clean, ground-up C11** code editor being built to rival Notepad++ feature
coverage — without forking it. The **first UI is the AGI interface**: WuBuPad
ships as a document **ingestion + regurgitation engine** for the wubuOS AGI OS,
driven over a line-oriented JSON protocol (see `AGI_PROTOCOL.md`). The human
TUI/GUI plugs in later on top of the same verified headless core.

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
  search.h / search.c   -- literal + Thompson-NFA regex find
  encode.h / encode.c   -- UTF-8/16/32 detect + convert to UTF-8
  diff.h   / diff.c     -- LCS line diff (unified output)
  docs.h   / docs.c     -- multi-document session (editor tabs model)
  json.h   / json.c     -- tiny self-contained JSON parse/emit (reusable)
  agent.h  / agent.c    -- wubuOS-facing protocol dispatcher (the AGI GUI)
apps/
  wubupad/main.c        -- NDJSON stdin -> NDJSON stdout CLI
tests/
  test_core.c           -- buffer/doc/lex/search/encode/diff/docs tests
  test_agent.c          -- AGI protocol acceptance tests
```
The core is **headless** (no platform headers) so it is unit-testable and
reusable by any front-end — machine or human.

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

## AGI usage (the first UI)
```sh
echo '{"cmd":"ingest","lang":"c","text":"int main(){return 0;}\n"}' | ./build/wubupad
```
WuBuPad ingests, regurgitates, searches, edits, diffs and lexes documents on
command — byte-exact and sanitizer-clean. Full command set in `AGI_PROTOCOL.md`.

## Status (honest)
- **Headless core**: buffer, C/JSON lexers, undo/redo, cursor/selection,
  search (literal+regex), encoding, line diff, multi-doc session — built and
  verified (0 leaks/UB/warnings).
- **AGI protocol** (`agent.c` + `wubupad` CLI): built and tested — the
  ingestion/regurgitation engine for wubuOS is live.
- **Human UI** (tabs, folding, completion, search box, encoding menu, compare
  view, plugins): unstarted — see `GAPS_NOTEPAD.md`, `GAPS_OFFICE.md`,
  `GAPS_GUI.md`.

## Reference
Notepad++ source is cloned read-only to `../ref/notepad-plus-plus` for feature
parity reference only. WuBuPad is original code.
