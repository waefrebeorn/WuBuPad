# Gap List — GUI gaps & C11 plan

WuBuOffice and WuBuPad are currently **headless** (CLI / library). A real
office + code product needs a GUI. This document lists the GUI gaps and the
clean-C11 strategy to close them without forking a toolkit.

## GUI gaps (both products)
| Gap | Notes |
|---|---|
| Windowing / event loop | No platform layer at all |
| Text + shape rendering | No font/shapes rasterization |
| Document view (scroll, pages) | N/A without a view |
| Editing widgets (caret, selection paint) | N/A |
| File dialogs, menus, ribbons | N/A |
| Accessibility (IME, a11y) | N/A |
| Theming / HiDPI | N/A |

## C11 strategy (no external GUI fork)
Keep the **domain core** GUI-free and unit-tested (already done for the editor:
buffer/lex/doc/cursor). Add one thin **platform abstraction** so the same core
backs multiple front-ends:

```
src/ui/
  ui.h          -- opaque wubu_ui; renderer + input interface (vtable)
  ui_headless.c -- null/recording backend (tests, CI)
  ui_tty.c      -- minimal terminal (curses-free) backend to prove editing E2E
  ui_gfx.c      -- real backend: window + draw + input
```

Backends implement a small vtable:
- `create/destroy`
- `draw_glyph(x,y,style,text)`, `draw_rect`, `draw_caret`
- `on_key/scroll/resize` → feed the headless core's cursor/command API

### Real-GUI backend options (C11, clean)
1. **SDL2 + FreeType** — portable, no toolkit fork, pure C bindings. WuBuPad
   owns the widget/editor view; SDL2 only provides window+GL surface, FreeType
   provides glyphs. Best fit for "clean code, no forks."
2. **Raw X11/Wayland + FreeType** — maximal control, more code, Linux-only.
3. **Nuklear** (single-header immediate-mode, C) — fast to stand up, but it is a
   third-party dependency; acceptable only as a *backend* behind `ui.h`, never
   baked into the core.

### Decision
Default to **SDL2 + FreeType** behind `ui.h`. The core (buffer/lex/doc/cursor)
is reused verbatim; only `ui_gfx.c` is new. `ui_tty.c` ships first so editing is
demonstrable + testable before the graphics backend lands.

## Why this satisfies "C11 that"
- Core: strict C11, opaque structs, no god headers, sanitizer-gated (ASan/
  UBSan 0 leaks/0 UB).
- UI layer: same standard; platform specifics isolated in one backend file;
  core never includes platform headers.
- Reuse-never-duplicate: editor core is shared by WuBuPad and (later) any
  WuBuOffice in-app editor.
