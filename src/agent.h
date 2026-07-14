/* agent.h -- wubuOS-facing protocol layer for WuBuPad.
 *
 * WuBuPad is now a document ingestion + regurgitation engine. The Agent is
 * the machine interface (the "AGI GUI"): wubuOS sends one JSON command per
 * line (NDJSON) on stdin, and WuBuPad emits one JSON result per line on
 * stdout. No pixels; pure protocol. The human TUI/GUI layers on later and
 * reuse the exact same core through Docs. Opaque. Clean C11. */
#ifndef WUBUPAD_AGENT_H
#define WUBUPAD_AGENT_H

#include "json.h"

typedef struct Agent Agent;

/* Create an agent with an empty multi-document session. */
Agent *agent_create(void);
void   agent_free(Agent *a);

/* Process one JSON command string (NUL-terminated). Returns a malloc'd JSON
 * result string (caller frees) or NULL on fatal error. The command must be a
 * JSON object with at least {"cmd":"<name>", ...}. Recognized commands:
 *   open    {path, text, lang}            -> {id, path, lang, lines, bytes}
 *   close   {id}                           -> {ok}
 *   list    {}                             -> {docs:[{id,path,lang,dirty,lines,bytes}]}
 *   ingest  {text, lang, path?}           -> open alias (ingestion)
 *   get     {id}                           -> {id, text, lines, bytes}
 *   regurgitate {id, from?, to?}          -> {id, text} (line range, 0-based)
 *   edit    {id, pos, insert}             -> {ok, bytes}
 *   replace {id, from, to, text}          -> {ok, bytes}
 *   search  {id, pattern, regex?, icase?} -> {matches:[{start,end,line}]}
 *   lines   {id}                           -> {lines:N}
 *   save    {id}                           -> {ok, path}
 *   encode  {text, enc?}                  -> {utf8} (detect+convert)
 *   diff    {a:[lines], b:[lines]}        -> {edit:[{op,a,b}]}
 *   lex     {id}                           -> {tokens:[{kind,start,end,text}]}
 * Unknown command -> {"error":"unknown command"}. Missing field -> error. */
char  *agent_handle(Agent *a, const char *command_json);

/* Convenience: run a line-buffered loop over `in` (FILE*) writing NDJSON
 * results to `out` (FILE*). Stops on EOF or the command {"cmd":"quit"}.
 * Returns 0 on clean EOF, -1 on error. */
int agent_serve(Agent *a, void *in, void *out);

#endif /* WUBUPAD_AGENT_H */
