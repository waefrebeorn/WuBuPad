/* wubupad.c -- the AGI-facing entry point.
 *
 * wubuOS (or any agent) drives WuBuPad over stdin/stdout as an NDJSON
 * protocol: one JSON command per line in, one JSON result per line out. This
 * is the "machine GUI" — the document ingestion + regurgitation interface.
 * A human TUI/GUI can be added later on top of the same core. */
#include "agent.h"

#include <stdio.h>

int main(void) {
    Agent *a = agent_create();
    if (!a) { fprintf(stderr, "{\"error\":\"oom\"}\n"); return 1; }
    int rc = agent_serve(a, stdin, stdout);
    agent_free(a);
    return rc ? 1 : 0;
}
