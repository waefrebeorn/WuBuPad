# Research: UX / UI Scenario for WuBuPad + WuBuOffice Editor

Purpose: before the feature blitz, reaffirm internal research (GAPS docs + actual
code state) and fold in an online research pass to decide the BEST UX/UI shape
for the editor. This is the decision record; the blitz plan lives in PLAN_BLITZ.md.

## 1. Internal state (re-affirmed this session)

### Core (DONE + tested, headless, sanitizer-clean — WuBuPad)
- `src/buffer.c` piece-table; `src/doc.c` model (cursor/selection/undo/redo);
  `src/lex.c` C+JSON lexers; `src/search.c` Thompson-NFA literal + regex;
  `src/encode.c` UTF-8/16/32 + Latin1; `src/diff.c` LCS; `src/docs.c` multi-doc
  session. 2,462 LOC. 3 ctests pass (core/ui/agent), release build 0 warnings.
- Public surface the UI must bind to:
  - Cursor/selection: `doc_cursor`, `doc_set_cursor`, `doc_set_selection`,
    `doc_clear_selection`, `doc_has_selection`.
  - Edit: `doc_type`, `doc_insert`, `doc_delete`, `doc_replace`, `doc_undo`,
    `doc_redo`.
  - Search: `regex_compile/find/find_from`, `search_literal` — regex engine is
    DONE and tested; only the find *box* is GAP.

### UI abstraction (EXISTS, partial — `src/ui/`)
- `ui.h` defines the backend vtable: `init, destroy, draw_line, draw_caret,
  present, get_key, resize` + `UI_KEY_*` symbolics (incl `UI_KEY_FIND`).
- Backends present: `ui_headless.c` (null/recording, for CI), `ui_tty.c`
  (curses-free terminal). Missing: `ui_gfx.c` (real window+draw+input).

### WuBuOffice (DONE, but GUI GAP — per GAPS_OFFICE.md)
- CLI suite round-trips docx/xlsx/pptx/odt/ods/odp/doc/xls/ppt/md/csv/rtf/html/
  epub/pdf/json (~90% format reach). Fidelity inside each format shallow
  (~10% for fidelity). GUI entirely absent. WuBuPad's editor core is the
  intended in-app editor backend for WuBuOffice (cross-reuse, unstarted).

## 2. Online research signals (2024-2025)

### Editor market / what users expect
- Stack Overflow 2025 Dev Survey: VS Code 75.9%, Notepad++ 27.4% (overlapping
  usage). VS Code dominates for dev; Notepad++ dominates for quick edits.
  Implication: target the "fast, capable everyday editor" slot — not a full IDE.
- Recurring praise for Notepad++: macro record/play, hex view, low footprint,
  instant launch. Recurring complaints about VS Code: heavy, slow startup,
  remote-dev focus overkill for editing one file.
- Our wedge: strict-C11, fork-free, sub-second launch, no Electron. That is a
  real differentiated UX promise (privacy/footprint/offline).

### Rendering backend choice (GAPS_GUI.md already picked SDL2+FreeType)
Online confirms:
- SDL2 + FreeType2 is the standard clean-C path for a portable window+GL
  surface + glyphs, no toolkit fork. (SDL discourse, reddit r/C_Programming)
- For performance: build a **glyph atlas** (texture sheet of cached glyphs);
  redraw only dirty rows. (SDL discourse "render text documents";
  r/GraphicsProgramming; Warp blog on kerning+atlases)
- Respect **kerning** at the line level: shape per line, cache line raster, not
  just per-glyph, so text isn't poorly kerned (Warp: ignoring shaping = unhappy
  users). For our C/JSON lexer we can shape per line and cache.
- FreeType directly (not SDL_ttf) gives finer control (SDL discourse). We
  already have `wubufont` (sfnt→SVG) in WuBuOffice; a shared glyph cache module
  is a natural reuse.

### Find / Replace UX (close the regex-engine gap first)
- Incremental search = live highlight + navigation as you type; matches update
  per keystroke. (grokipedia "Incremental search"; ux.stackexchange)
- Best practice: a non-modal search bar (Ctrl-F) with live count "3/12",
  regex toggle, case toggle, prev/next, and replace-with-confirm. Avoid blocking
  modal dialogs; inline panel is the modern norm.

### Theming / dark mode
- Use **semantic color tokens**, not hardcoded colors: `surface-base`,
  `text-primary`, `accent`, `selection`, `line-no`, `caret`. One token name,
  two values (light/dark). (Muzli dark-mode guide; Figma/Config 2022; Tailwind
  semantic-color discussion)
- Text: avoid pure #FFF on true black (eye strain); per-attribute accents need
  dark-mode variants. We will define a token table consumed by `ui_gfx.c`.

### Accessibility (bake in, not bolt on)
- Keyboard-navigable controls + screen-reader output. (PatternFly code-editor
  a11y; VS Code a11y docs: VoiceOver/Orca; ACE `enableKeyboardAccessibility`)
- For a custom-rendered editor: expose a text/AX bridge that announces the
  current line + selection on caret move; route IME composition to `doc_type`
  incrementally. (SDL discourse: a11y tools consume typed text; we mirror that
  into our headless core so the core stays the single source of truth.)

### Headless-first architecture validated
- The headless-core + pluggable-frontend pattern is the same decoupling
  headless CMS / frontend-architecture writing recommends: core owns state,
  frontends are swappable. Our `ui.h` vtable is exactly this. Keep core free of
  platform headers (already true).

## 3. UX / UI DECISIONS (recommended, to reaffirm)

1. **Positioning**: "Instant, private, fork-free C11 editor." Sub-second cold
   start, offline, no telemetry. Compete on footprint, not IDE breadth.
2. **Backend**: `ui_gfx.c` on **SDL2 + FreeType2**, glyph atlas + per-line
   shape cache, dirty-row redraw. Reuse `wubufont` glyph cache. No toolkit fork.
3. **Layout**: single document area + **tab bar** (multi-doc, `docs.c` DONE) +
   left **line-number gutter** + bottom **status bar** (Ln/Col, encoding,
   dirty). One inline **command/find bar** (Ctrl-F, Ctrl-H) — non-modal.
4. **Find/Replace**: incremental, live "n/total" count, regex + case toggles,
   replace-with-confirm. Bind to the DONE `regex_*` engine. Highest-value first
   UX win (engine already works).
5. **Theming**: semantic token table in `ui_gfx.c`; ship Light + Dark; avoid
   pure-white-on-black; persist choice to a config file.
6. **Input**: keymap table (default Notepad++-like; vim/emu opt-in later).
   IME composition routed to `doc_type` incrementally. Caret blink, selection
   highlight via tokens.
7. **Accessibility**: AX bridge announcing caret line/selection; full keyboard
   nav; respects OS a11y (Orca/VoiceOver) where SDL backend allows.
8. **Cross-repo**: `ui.h` + core compiled into WuBuOffice's in-app editor
   (GAPS_OFFICE cross-cutting). Same backend, one codebase.

## 4. What is explicitly OUT of v1 scope (avoid scope creep)
- Plugin ABI/loader (note it; defer). 
- Code folding, auto-completion symbol index, macro record/play, column mode,
  EOL convert, function list — listed in GAPS_NOTEPAD as GAP; schedule AFTER the
  gfx backend + find bar + theming land (they depend on a working viewport).
- Office-format fidelity (charts, merged cells, RTL) stays WuBuOffice's track,
  not this editor's.

## 5. Risk / open questions
- SDL2 package availability on the build host (need `libsdl2-dev`,
  `libfreetype6-dev` — installable via rootexec, same as ccache).
- IME on Wayland vs X11 differs; target X11 first, Wayland best-effort.
- Glyph atlas memory vs. CJK font breadth — cap cache, LRU evict.
