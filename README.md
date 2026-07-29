# WuBuPad

---

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

<<<<<<< HEAD
The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.
=======
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
- **UI abstraction layer** (`src/ui/`): built. Opaque `UI` owns viewport state
  and translates commands into Doc mutations; backends implement a small
  vtable (`ui_headless.c` null/recording for tests, `ui_tty.c` a curses-free
  ANSI terminal editor). `wubupad --edit <file>` launches the interactive
  TUI; the same core backs the agent. `test_ui` drives the whole stack under
  ASan+UBSan (0 leaks/UB).
- **Human GUI** (`apps/wubupad/`, SDL2/FreeType): the gfx backend
  (`ui_gfx.c`) is live — full window with tabs, folding, completion,
  search box, encoding menu, compare view, and Notepad++-parity
  keyboard shortcuts (Ctrl+X/C/V for cut/copy/paste, Ctrl+Shift+V
  for paste-plain, Ctrl+A for select-all). SDL clipboard wired
  (cut/copy write to system clipboard; paste reads from it).
  `GAPS_NOTEPAD.md`, `GAPS_OFFICE.md`, `GAPS_GUI.md` track remaining
  gaps. Screenshots in `docs/`.

## Reference
Notepad++ source is cloned read-only to `../ref/notepad-plus-plus` for feature
parity reference only. WuBuPad is original code.
>>>>>>> 4057ae9 (feat: clipboard/keyboard shortcut parity (Ctrl+X/C/V/Shift+V/A + Ctrl+A))
