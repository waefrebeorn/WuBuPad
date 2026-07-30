# WuBuPad

**Clean-room C11 code editor** — Notepad++ parity, fork-free,
zero runtime dependencies beyond POSIX + SDL2/FreeType for the GUI.

> **SLERM** (verb): take someone's full work and build your own
> version from scratch. Not a fork — a clean-room reimplementation.

WuBuPad's headless core (piece-table buffer, C/JSON lexers, undo/redo,
cursor/selection, search, encoding, line diff, multi-doc session) is
designed to be embedded — notably as the Editor view inside WuBuOffice.

## License

**Waefrebeorn Umbrella License v3.0** — custom source-available license.
Not OSI-approved, not FSF-approved. See [LICENSE](LICENSE).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Sanitizer build (ASan+UBSan, 0 leaks/UB):

```sh
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DWITH_SANITIZER=ON
cmake --build build-san -j$(nproc)
ctest --test-dir build-san --output-on-failure
```

Requires a C11 compiler (tested: gcc 13.3). POSIX `open_memstream`/`strdup`
used via `_POSIX_C_SOURCE=200809L` (no GNU extensions). SDL2 + FreeType2
needed only for the GUI backend (`wubupad` target).

## Binaries

| Target | Description |
|--------|-------------|
| `wubupad` | Full editor: CLI headless + interactive TUI (`--edit`) + SDL2 GUI |
| `shot` | Headless screenshot tool (SDL dummy driver, no display needed) |
| `test_ui`, `test_core`, `test_lex` etc. | 22 ctest suites, all green |

## Quick start

```sh
# headless (agent-driven, JSON protocol)
echo '{"cmd":"ingest","lang":"c","text":"int main(){return 0;}\n"}' | ./build/wubupad

# interactive TUI
./build/wubupad --edit somefile.c

# GUI (SDL2 + FreeType2 window)
./build/wubupad --gui somefile.c

# screenshot
./build/shot docs/screenshot_default.png
```

## Architecture

```
src/
  buffer.c      piece-table buffer (O(1)-ish edits, full undo)
  doc.c         document model: cursor, selection, undo/redo stack
  lex.c         lexer registry + C/JSON lexers (Thompson-NFA tokens)
  search.c      literal + regex (Thompson NFA with match tracking)
  encode.c      UTF-8/16/32 + Latin1 detection + conversion
  diff.c        LCS line diff
  docs.c        multi-document session manager
  agent.c       JSON protocol dispatcher (stdin/stdout)
  json.c/h      self-contained JSON parse/emit, no deps
  ui/
    ui.h        opaque UI vtable (backend interface)
    ui.c        command dispatch (cut/copy/paste/select-all via Doc API)
    ui_headless.c null/recording backend (CI tests)
    ui_tty.c    curses-free ANSI terminal backend
    ui_gfx.c    SDL2/FreeType2 backend (window, draw, input, clipboard)
  shot.c        headless screenshot tool (SDL dummy driver → PNG)
```

## Keyboard shortcuts (Notepad++ parity)

| Shortcut | Action |
|----------|--------|
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Ctrl+Shift+V | Paste plain (no formatting) |
| Ctrl+A | Select all |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| Ctrl+G | Go to line |
| Ctrl+E | EOL convert (LF↔CRLF) |
| Ctrl+` | Theme toggle (dark/light) |
| Ctrl+T | New tab |
| Ctrl+W | Close tab |
| Ctrl+Tab / Ctrl+Shift+Tab | Cycle tabs |
| Ctrl+Shift+F | Code fold (brace block) |
| Ctrl+Space | Auto-completion |
| Ctrl+Shift+L | Function list |
| F2 / Shift+F2 | Next/prev bookmark |
| Ctrl+F2 | Toggle bookmark |

Clipboard is wired to the system SDL clipboard in the GUI backend;
the TTY backend uses Ctrl+C for copy (no system clipboard).

## Status

| Component | Status |
|-----------|--------|
| Piece-table buffer, lexers, undo/redo, cursor/selection | ✅ Headless, tested, sanitizer-clean |
| AGI protocol (JSON stdin/stdout) | ✅ Live, tested |
| UI abstraction vtable (`src/ui/`) | ✅ Headless + TTY + SDL2 backends |
| SDL2/FreeType2 GUI backend (`ui_gfx.c`) | ✅ Full window, tabs, folding, completion, search, clipboard |
| Notepad++ keyboard parity | ✅ All shortcuts wired |
| SDL system clipboard | ✅ Cut/copy write, paste reads |

## Reference

Notepad++ source is cloned read-only to `../ref/notepad-plus-plus` for
feature parity reference only. WuBuPad is original C11 code.

See also:
- [GAPS_REAL.md](GAPS_REAL.md) — **VERIFIED gap list** (source audit + build + ctest) — use this for planning
- [GAPS_NOTEPAD.md](GAPS_NOTEPAD.md) — feature inventory vs Notepad++ (STALE — see GAPS_REAL.md)
- [GAPS_GUI.md](GAPS_GUI.md) — GUI strategy and architecture decisions (STALE — see GAPS_REAL.md)
- [GAPS_OFFICE.md](GAPS_OFFICE.md) — cross-repo editor integration plan (STALE — see GAPS_REAL.md)
- [AGI_PROTOCOL.md](AGI_PROTOCOL.md) — machine-facing JSON protocol spec
- [RESEARCH_UX_UI.md](RESEARCH_UX_UI.md) — UX research and decisions
- [PLAN_BLITZ.md](PLAN_BLITZ.md) — development roadmap

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.