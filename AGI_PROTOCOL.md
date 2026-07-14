# WuBuPad AGI Protocol (the "AGI GUI")

WuBuPad's first UI is **machine-facing, not pixel-facing**. wubuOS (and any agent)
drives WuBuPad as a **document ingestion + regurgitation engine** over a line-
oriented JSON protocol on stdin/stdout. No GUI, no TTY — just structured I/O.

This is the `ui_headless` layer from the architecture: the human TUI/GUI can be
added later on top of the *same* verified core. The protocol is the contract.

## Transport
- One JSON command object per line on **stdin**.
- One JSON result object per line on **stdout**.
- A command with `{"cmd":"quit"}` ends the session.
- Lines are NDJSON (`\n`-delimited). Whitespace within a line is fine.

Build: `cmake -S . -B build && cmake --build build && ./build/wubupad`
Run wubuOS side: `echo '{"cmd":"ingest",...}' | ./wubupad`

## Commands

| cmd | params | result |
|---|---|---|
| `ingest` (alias `open`) | `text`, `path?`, `lang?` | `{"id", "path", "lang", "dirty", "lines", "bytes"}` |
| `regurgitate` (alias `get`) | `id`, `from?`, `to?` | `{"id", "text"}` — exact byte range of lines [from,to); full doc if no range |
| `list` | — | `{"docs":[{doc summary}...]}` |
| `close` | `id` | `{"ok"}` |
| `edit` | `id`, `pos`, `insert` | `{"ok", "bytes"}` — insert text at byte pos |
| `replace` | `id`, `from`, `to`, `text` | `{"ok"}` — replace byte range with text |
| `search` | `id`, `pattern`, `regex?`, `icase?` | `{"matches":[{"start","end","line"}...]}` |
| `lines` | `id` | `{"lines"}` |
| `lex` | `id` | `{"tokens":[{"kind","start","end","text"}...]}` |
| `encode` | `text` | `{"utf8"}` — detect+convert to UTF-8 |
| `diff` | `a:[..]`, `b:[..]` | `{"edit":[{"op","a","b"}...]}` — LCS line diff |
| `save` | `id` | `{"ok", "path"}` |

Errors return `{"error":"<msg>"}`. Unknown command or malformed JSON also error.

## Notes
- `regurgitate` is **byte-exact**: re-emitting a range never invents or drops
  bytes, including trailing newlines. This is what makes it safe as an
  ingestion/regurgitation engine — wubuOS gets back exactly what it put in.
- `id` is the 0-based session index from `ingest`/`list` (the active doc model
  is a multi-document session, same as editor tabs).
- `lang` selects the lexer (c, json, ...). Unknown langs fall back to c.

## Reuse
`src/json.{c,h}` is a tiny self-contained JSON parse/emit layer with no deps —
wubuOS can vendor it. `src/agent.{c,h}` is the dispatcher and is the only place
that maps protocol verbs onto the headless core (`Docs`/`Doc`/`search`/`encode`/
`diff`/`lex`).
