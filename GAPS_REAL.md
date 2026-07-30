# GAPS_REAL — Verified parity (2026-07-30)

Last verified by `parity_scanner_v2.c` + `oracle_v2.c` from `/home/wubu/tooling/`,
run with `--all-exes` mode against the full repo.

## Headline numbers

| Metric | WuBuPad |
|---|---|
| Total modules | **20** |
| CTest cases   | **22** |
| CTest passing | **22** |
| REAL  (linked + called in view + main) | **2 (10%)** |
| BIN   (linked, no view/main callers)   | **18 (90%)** |
| TEST  (only in tests)                  | **0 (0%)** |
| GAP   (not in any binary)              | **0 (0%)** |

## Oracle parity (verified)

| Target | REAL parity | Notes |
|---|---|---|
| Notepad++ 8.7.9 | **1%** (1/58)  | all 18 atom modules are BIN, not REAL |
| Scintilla       | **0%** (0/39)  | |
| VS Code         | **0%** (0/48)  | |
| Kate            | **2%** (1/39)  | |
| Lite XL         | **3%** (1/29)  | |
| SciTE           | **0%** (0/24)  | |

## WuBuPad modules — full classification (20/20, zero GAPs)

| Module | Class | in_bin | view_hits |
|---|---|---|---|
| src/agent.c        | REAL | 1 | 9  |
| src/docs.c         | REAL | 1 | 18 |
| autoindent         | BIN  | 1 | 0  |
| command            | BIN  | 1 | 0  |
| fuzzy              | BIN  | 1 | 0  |
| mdpreview          | BIN  | 1 | 0  |
| minimap            | BIN  | 1 | 0  |
| multicursor        | BIN  | 1 | 0  |
| palette            | BIN  | 1 | 0  |
| pkgmgr             | BIN  | 1 | 0  |
| snippet            | BIN  | 1 | 0  |
| src/buffer.c       | BIN  | 1 | 0  |
| src/complete.c     | BIN  | 1 | 0  |
| src/diff.c         | BIN  | 1 | 0  |
| src/doc.c          | BIN  | 1 | 0  |
| src/encode.c       | BIN  | 1 | 0  |
| src/json.c         | BIN  | 1 | 0  |
| src/lex.c          | BIN  | 1 | 0  |
| src/search.c       | BIN  | 1 | 0  |
| treeview           | BIN  | 1 | 0  |

The 18 BIN modules are the Atom subsystem + the headless core. They're
linked into `wubupad_atom` and `wubupad_core` but the standalone `wubupad`
binary doesn't call them directly — they're invoked through `src/agent.c`
and `src/ui/ui.c` which are cross-compiled into WuBuOffice's `wubuos`.

## Why the REAL count is low

The standalone `wubupad` binary (`apps/wubupad/main.c`) is a thin NDJSON
CLI: it reads commands from stdin and dispatches them through `agent.c`.
The heavy lifting (atom modules, doc model, lex, search) is consumed by
WuBuOffice's Editor tab via the cross-compile bridge in
`apps/wubupad_bridge/`. From the scanner's perspective, only
`src/agent.c` and `src/docs.c` are directly called from `apps/wubupad/`
(view hits > 0), so they're the only REAL modules.

The other 18 modules ARE linked into the binary (`in_bin=1`) — they just
don't have direct callers from `apps/wubupad/main.c`. They're
transitively wired through `agent.c` and `wubupad_atom`.

## v2.1 scanner changes (2026-07-30)

The scanner v2.1 (in `/home/wubu/tooling/parity_scanner_v2.c`) was patched:

1. Added `--all-exes` mode: builds closure over ALL `add_executable` targets.
2. Added `apps/<dir>/` to module scanning.
3. Added `$<TARGET_OBJECTS:lib>` generator-expression parsing.
4. Fixed prefix-match bug: `target_link_libraries(<name>` (no trailing space)
   matched `<name>_core`, `<name>_agent`, etc. as a prefix. This was
   causing WuBuPad's closure to miss the entire atom subsystem because
   the scanner found `target_link_libraries(wubupad_agent ...)` when
   looking for `target_link_libraries(wubupad ...)` and parsed `_agent`
   as a separate lib.

## How to reproduce

```sh
cd /home/wubu/tooling
./parity_scanner_v2 /home/wubu/WuBuPad --json --all-exes > /tmp/pad.json
./oracle_v2 /tmp/pad.json --repo pad --target npp
```
