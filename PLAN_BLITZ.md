# PLAN_BLITZ — feature blitz (UX/UI + fidelity)

Derived from RESEARCH_UX_UI.md. Ordered by dependency: gfx backend first
(needed by everything visual), then find bar + theming (bind to DONE engines),
then editor features, then cross-repo wiring. Each task is a cohesive unit.

## Phase A — Graphics backend (WuBuPad, `src/ui/ui_gfx.c`)
A1. Add SDL2+FreeType deps via rootexec (`libsdl2-dev`, `libfreetype6-dev`).
A2. `ui_gfx.c`: window + GL/SDL_Renderer surface; implement `UI_Backend`
    vtable (`init/destroy/draw_line/draw_caret/present/get_key/resize`).
A3. Glyph atlas: FreeType load -> cache glyph textures; per-line shape cache;
    dirty-row redraw only. Reuse `wubufont` cache where possible.
A4. Caret blink + selection highlight (token-driven).
A5. ctest: add `test_ui_gfx` (headless-record mode asserts draw calls fire).

## Phase B — Find/Replace UX (binds DONE regex engine)
B1. Inline find bar (Ctrl-F): incremental, live "n/total", prev/next.
B2. Replace bar (Ctrl-H): replace / replace-all / replace-next with confirm.
B3. Regex + case toggles; wire to `regex_compile/find/find_from`,
    `search_literal`.
B4. ctest: `test_find` asserts match navigation + replace correctness.

## Phase C — Layout + theming
C1. Tab bar over `docs.c` (multi-doc switch, dirty markers).
C2. Line-number gutter + status bar (Ln/Col, encoding, dirty).
C3. Semantic token table (Light + Dark); avoid pure-white-on-black; persist to
    config file (`~/.wubupadrc`).
C4. Keymap table (Notepad++-like default); IME composition -> `doc_type`.

## Phase D — Editor features (GAPS_NOTEPAD, post-viewport)
D1. EOL convert (CRLF/LF) on buffer newline model.
D2. Column/block selection mode (cursor + buffer range ops).
D3. Macro record/play (command log + replay).
D4. Code folding (lexer fold levels + view).   [larger]
D5. Auto-completion (lexer symbol index).       [larger]
D6. More lexers (extend `src/lex.c`).

## Phase E — Cross-repo (WuBuOffice in-app editor)
E1. Compile WuBuPad core + `ui.h` into WuBuOffice; expose `wubuoffice --edit`.
E2. Reuse `ui_gfx.c` backend; one codebase, two products.

## Verification gate (every phase)
- WuBuPad: `ctest --test-dir build -j$(nproc)` green; release build 0 warnings.
- WuBuOffice: `ctest --test-dir build_ninja -j$(nproc)` stays 71/71.
- Commits per phase; no monolith; opaque modules.

## Out of scope (v1)
Plugin ABI, function list, office-format fidelity (charts/merged/RTL) — separate
tracks, scheduled after E.
