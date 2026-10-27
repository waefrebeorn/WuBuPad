# Triple Devil's Advocate Gap Plan — WuBuPad
> Mirror of /home/wubu/WuBuOffice/.hermes/mind-palace/da_plan_v1.md focusing on WuBuPad.
> Source: GAPS_REAL.md + 22/22 green ctest + ref/major/scintillua (160 lexers), ref/major/notepad-plus-plus (64M), ref/major/scintilla (6.5M).

## Verified state (DA-1, executed)
- 22/22 tests green, 0.06s — VERIFIED BY RUNNING `ctest -j4`.
- `lex.c` 289 lines, only C+JSON lexers confirmed by source.
- `apps/wubupad/main.c` references ZERO of: `ui_atom`, `fuzzy`, `treeview`, `snippet`, `multicursor`, `minimap`, `mdpreview`, `autoindent`. Confirmed.
- All 9 features exist and have their own green tests in `src/` (e.g. test_atom.c, test_fuzzy.c).

## Kevin Bacon trace (DA-2)
Every gap closes with either existing src/ code or scintillua pattern study:

| # | Gap | Ref | Path | Effort |
|---|---|---|---|---|
| 1 | 2 → 18 lexers | `ref/major/scintillua/lexers/*.lua` (160 patterns) | Port keyword sets + comment/string markers as C state machines. New `lex_python.c`, `lex_js.c`, etc. Dispatch via `lex_init(lang)`. | M |
| 2 | Command palette | own `src/ui/ui_atom.c` (110 lines) | 10-line wiring in `main.c` for `Ctrl+Shift+P`. | L |
| 3 | Fuzzy finder | own `src/fuzzy/` (69 lines) | 15-line wiring for `Ctrl+P`. | L |
| 4 | Tree view | own `src/treeview/` (129 lines) | 25-line side-panel wiring in `ui_gfx.c`. | L |
| 5 | Snippets | own `src/snippet/` (98 lines) | 10-line wiring for `Ctrl+J`. | L |
| 6 | Multiple cursors | own `src/multicursor/` (36 lines) | 10-line wiring for `Ctrl+Alt+Up/Down`. | L |
| 7 | Minimap | own `src/minimap/` (49 lines) | 25-line right-panel wiring. | L |
| 8 | Markdown preview | own `src/mdpreview/` (136 lines) | 15-line wiring for `Ctrl+M`. | L |
| 9 | Auto-indent | own `src/autoindent/` (52 lines) | 10-line wiring on Enter. | L |
| 10 | Plugin API | own `src/pkgmgr/` (189 lines) + `apps/wubuos/plugin.c` sample | 30-line port to `apps/wubupad/plugin.c`. | L |
| 11 | Bracket matching | self-contained | 30 lines, scan for {} on caret move. | L |
| 12 | Indentation guides | self-contained | 15 lines, `|` after tab stops. | L |
| 13 | Whitespace viz | self-contained | 20 lines, `·` toggle. | L |
| 14 | Call tips | `ref/major/scintilla` SCI_CALLTIP | 50 lines, floating rect with signature. | M |
| 15 | Indicators (squiggle) | own `src/wubuspell` red-underline pattern | 30 lines, `underline_in_rect(x0,y0,x1,y1,color)` util. | L |
| 16 | Print | `popen("lpr", ...)` | System print via PostScript render. | M |
| 17 | JS/Lua scripting | `ref/lesser-known/mujs` (912KB) | Embed mujs, expose `doc_*` API only. | M |

## DA-3 — Adversarial pass for WuBuPad
- **Risk**: WuBuPad becomes wubuos clone. **Mitigation**: WuBuPad stays a *text editor*. Shared editor core lives in `src/`, both apps link to it. WuBuPad commands = editor commands. WuBuOffice commands = office commands.
- **Risk**: Lexer extension breaks `test_lex_fold`/`test_lex_symbols`. **Mitigation**: New lexers in `lex_X.c` files, dispatched from `lex_init(lang)`. Existing tests untouched.
- **Risk**: mujs attack surface. **Mitigation**: No file/network bindings, only `doc_*` exposed.
- **Risk**: notepad-plus-plus reference is C++; WuBuPad is C11. **Mitigation**: We study the *architecture* (plugin discovery, lexer dispatch, palette pattern), don't copy code. C11 implementation is our own.

## Sequenced plan (P0→P3)
1. **P0**: 9 standalone wirings (#2-#10) — all L, ~1 day. Tests pass after each.
2. **P1**: Text affordances (#11-#13, #15) — small, share code with WuBuOffice.
3. **P2**: Lexer expansion (#1) — port Python first (highest leverage), then JS, HTML, CSS, Rust, Go, SQL, XML, YAML, TOML, Bash, Markdown.
4. **P2**: Call tips (#14), Print (#16) — M each.
5. **P3**: Scripting (#17), plugin manager polish (#10), FTP/SFTP.
6. **Defer**: Accessibility IA2, large file virtual rendering.
