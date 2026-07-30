# Gap List — GUI gaps & C11 plan

> ⚠️ **STALE — DO NOT USE FOR PLANNING.** This document contains outdated
> "CLOSED" markers. Many items marked CLOSED are WUBOOS-only — the code exists
> in `src/ui/` and is tested but is NOT in the standalone `wubupad` binary.
>
> **Use [GAPS_REAL.md](GAPS_REAL.md) instead** — it is the verified, honest gap
> list generated from source audit + build + ctest (22/22 green) + web research
> against Notepad++ 8.7.9, Scintilla 5.x.
>
> This document is kept for historical reference only.

WuBuPad is currently **headless** (CLI / library). A real code editor needs a GUI. This document lists the GUI gaps and the clean-C11 strategy to close them without forking a toolkit.

## Screenshots

See [`docs/screenshot_default.png`](docs/screenshot_default.png) for the live SDL2/FreeType2 GUI window (dark theme, C editor, line numbers, folding, completion, search box, encoding menu, compare view, and Notepad++-parity keyboard shortcuts). A headless screenshot is produced by the `shot` tool (SDL dummy driver, no display required).

## GUI gaps (both products)
| Gap | Notes |
|---|---|
| Windowing / event loop | SDL2 window + event loop (DONE — `ui_gfx.c`) |
| Text + shape rendering | FreeType2 glyphs + cached line shapes (DONE) |
| Document view (scroll, caret) | DONE — full viewport with scrolling |
| Editing widgets (caret, selection paint) | DONE |
| File dialogs, menus | DONE — menu bar with File/Edit/View/Help + Ctrl+O/S and file dialog |
| Clipboard (cut/copy/paste/paste-plain/select-all) | **CLOSED** — SDL2 system clipboard wired; Ctrl+X/C/V/Shift+V/A + Ctrl+A |
| Theming / HiDPI | **CLOSED** — Ctrl+\` toggles dark/light; HiDPI via SDL_WINDOW_ALLOW_HIGHDPI |
| Headless screenshot tool | **CLOSED** — `shot` uses SDL_VIDEODRIVER=dummy, produces PNG matching the live GUI |
| Real-GUI backend (SDL2+FreeType) | **CLOSED** (`src/ui/ui_gfx.c`) with glyph atlas, dirty-row redraw, clipboard |
| Notepad++ keyboard parity | **CLOSED** — all shortcuts wired in both gfx and tty backends |
| Menu bar (File/Edit/View/Help) | **CLOSED** — dropdown menus with working handlers, Ctrl+Z/Y undo-redo |

## C11 strategy (no external GUI fork)
Keep the **domain core** GUI-free and unit-tested (already done for the editor: buffer/lex/doc/cursor). Add one thin **platform abstraction** so the same core backs multiple front-ends:

```
src/ui/
  ui.h          -- opaque wubu_ui; renderer + input vtable
  ui.c         command dispatch (cut/copy/paste/select-all via Doc API)
  ui_headless.c null/recording backend (tests, CI)
  ui_tty.c     minimal terminal (curses-free) backend
  ui_gfx.c     real backend: window + draw + input + clipboard
```

Backends implement a small vtable:
- `create/destroy`
- `draw_glyph(x,y,style,text)`, `draw_rect`, `draw_caret`
- `on_key/scroll/resize` → feed the headless core's cursor/command API

### Real-GUI backend options (C11, clean)
1. **SDL2 + FreeType** — portable, no toolkit fork, pure C bindings. WuBuPad owns the widget/editor view; SDL2 only provides window+GL surface, FreeType provides glyphs. Best fit for "clean code, no forks."
2. **Raw X11/Wayland + FreeType** — maximal control, more code, Linux-only.
3. **Nuklear** (single-header immediate-mode, C) — fast to stand up, but it is a third-party dependency; acceptable only as a *backend* behind `ui.h`, never baked into the core.

### Decision
Default to **SDL2 + FreeType** behind `ui.h`. The core (buffer/lex/doc/cursor) is reused verbatim; only `ui_gfx.c` is new. `ui_tty.c` ships first so editing is demonstrable + testable before the graphics backend lands.

## Why this satisfies "C11 that"
- Core: strict C11, opaque structs, no god headers, sanitizer-gated (ASan/UBSan 0 leaks/0 UB).
- UI layer: same standard; platform specifics isolated in one backend file; core never includes platform headers.
- Reuse-never-duplicate: editor core is shared by WuBuPad and (later) any WuBuOffice in-app editor.