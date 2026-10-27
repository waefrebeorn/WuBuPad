── WUBUPAD — GOAL MANTRA (v1, 2026-07-29) ──
Path: /home/wubu/WuBuPad | Tests: 22/22 green (verified 0.06s)
Build: cmake --build build && cd build && ctest -j4

=== STATE ===
✅ VERIFIED: 22/22 tests green (ctest -j4, 0.06s, 0 fails)
✅ VERIFIED: piece-table, C+JSON lex, regex search, multi-doc, fold, bookmarks, etc.
✅ VERIFIED: 9 features exist in src/ui/ (command palette, fuzzy, treeview, snippets,
   multicursor, minimap, mdpreview, autoindent, plugin pkgmgr)
❌ GAP: apps/wubupad/main.c references ZERO of those 9 (0 hits each)
❌ GAP: 2 lexers only (vs Notepad++ 800+, scintillua 160 ready patterns)
❌ GAP: No bracket matching, indentation guides, whitespace viz, call tips

=== PRIORITIES ===
P0: Wire 9 features into main.c (9 × ~15 lines = 1 day total)
P1: Text affordances — bracket match, indent guides, whitespace viz, indicators
P2: Lexer expansion — port 12 scintillua patterns as C state machines
P2: Call tips (ref: scintilla SCI_CALLTIP)
P3: Embed mujs for JS scripting; plugin manager polish
Defer: accessibility IA2, FTP/SFTP, large file virtual rendering

=== THE LOOP ===
locate feature in src/ → wire key handler in main.c → add to test → ctest → commit
NO porting wubuos commands — wubupad stays text-editor focused.
NO monolithic main.c — split ui cluster code if main.c exceeds ~600 lines.

=== FULL CONTEXT ===
Read .hermes/mind-palace/da_plan_v1.md
Read GAPS_REAL.md
