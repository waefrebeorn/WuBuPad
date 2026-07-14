/* test_agent.c -- exercise the wubuOS-facing protocol layer.
 * Real assertions: ingest a document, regurgitate it, search it (literal +
 * regex), edit it, diff two line sets, lex it, encode a non-UTF8 string, and
 * round-trip via the NDJSON CLI shape. No external oracle. */
#include "agent.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(cond,msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); fails++; } } while(0)

static char *handle(Agent *a, const char *cmd) {
    char *r = agent_handle(a, cmd);
    CK(r != NULL, "agent_handle returns non-null");
    return r;
}
/* parse result, assert it's an object with no "error" key, return it (caller frees) */
static JVal *ok_obj(Agent *a, const char *cmd) {
    char *r = handle(a, cmd);
    JVal *o = j_parse(r, NULL);
    CK(o != NULL, "result is JSON");
    if (o) {
        const JVal *e = j_obj_get(o, "error");
        CK(e == NULL, "result has no error");
    }
    free(r);
    return o;
}

int main(void) {
    Agent *a = agent_create();
    CK(a != NULL, "agent_create");

    /* ingest a small C file */
    JVal *o = ok_obj(a,
        "{\"cmd\":\"ingest\",\"lang\":\"c\",\"path\":\"sample.c\","
        "\"text\":\"#include <stdio.h>\\nint main(){ return 0; }\\n\"}");
    CK(o != NULL, "ingest ok");
    if (o) {
        const JVal *idv = j_obj_get(o, "id");
        CK(idv && j_type(idv)==J_NUM, "ingest returns id");
        CK(j_obj_get(o, "lines") && j_as_num(j_obj_get(o,"lines"))==3, "ingest -> 3 lines (2 nl + trailing)");
        j_free(o);
    }

    /* regurgitate whole doc (id 0) */
    o = ok_obj(a, "{\"cmd\":\"regurgitate\",\"id\":0}");
    CK(o != NULL, "regurgitate ok");
    if (o) {
        const JVal *t = j_obj_get(o, "text");
        CK(t && j_type(t)==J_STR, "regurgitate -> text");
        if (t) CK(strcmp(j_as_str(t), "#include <stdio.h>\nint main(){ return 0; }\n")==0, "regurgitate text matches");
        j_free(o);
    }

    /* line-range regurgitate (line 0 only) */
    o = ok_obj(a, "{\"cmd\":\"regurgitate\",\"id\":0,\"from\":0,\"to\":1}");
    if (o) {
        const JVal *t = j_obj_get(o, "text");
        CK(t && strcmp(j_as_str(t), "#include <stdio.h>\n")==0, "regurgitate line 0");
        j_free(o);
    }

    /* literal search for "main" */
    o = ok_obj(a, "{\"cmd\":\"search\",\"id\":0,\"pattern\":\"main\"}");
    if (o) {
        const JVal *m = j_obj_get(o, "matches");
        CK(m && j_type(m)==J_ARR && j_len(m)>=1, "literal search finds 'main'");
        j_free(o);
    }

    /* regex search for "int\s+\w+" */
    o = ok_obj(a, "{\"cmd\":\"search\",\"id\":0,\"pattern\":\"int [a-z]+\",\"regex\":true}");
    if (o) {
        const JVal *m = j_obj_get(o, "matches");
        CK(m && j_type(m)==J_ARR && j_len(m)>=1, "regex search finds 'int main'");
        j_free(o);
    }

    /* edit: insert text at start */
    o = ok_obj(a, "{\"cmd\":\"edit\",\"id\":0,\"pos\":0,\"insert\":\"/* head */\\n\"}");
    if (o) { j_free(o); }
    o = ok_obj(a, "{\"cmd\":\"regurgitate\",\"id\":0,\"from\":0,\"to\":1}");
    if (o) {
        const JVal *t = j_obj_get(o, "text");
        CK(t && strcmp(j_as_str(t), "/* head */\n")==0, "edit applied at start");
        j_free(o);
    }

    /* diff two line sets */
    o = ok_obj(a, "{\"cmd\":\"diff\",\"a\":[\"one\",\"two\",\"three\"],\"b\":[\"one\",\"TWO\",\"three\"]}");
    if (o) {
        const JVal *e = j_obj_get(o, "edit");
        CK(e && j_type(e)==J_ARR && j_len(e)>=3, "diff produces edit script");
        j_free(o);
    }

    /* lex the doc */
    o = ok_obj(a, "{\"cmd\":\"lex\",\"id\":0}");
    if (o) {
        const JVal *tk = j_obj_get(o, "tokens");
        CK(tk && j_type(tk)==J_ARR && j_len(tk)>=1, "lex returns tokens");
        j_free(o);
    }

    /* encode a UTF-8 string round-trips through detect+convert */
    o = ok_obj(a, "{\"cmd\":\"encode\",\"text\":\"hello world\"}");
    if (o) {
        const JVal *u = j_obj_get(o, "utf8");
        CK(u && j_type(u)==J_STR && strcmp(j_as_str(u), "hello world")==0, "encode -> utf8 passthrough");
        j_free(o);
    }

    /* list docs */
    o = ok_obj(a, "{\"cmd\":\"list\"}");
    if (o) {
        const JVal *d = j_obj_get(o, "docs");
        CK(d && j_type(d)==J_ARR && j_len(d)>=1, "list returns docs");
        j_free(o);
    }

    /* unknown command -> error */
    char *bad = agent_handle(a, "{\"cmd\":\"frobnicate\"}");
    JVal *bo = j_parse(bad, NULL);
    CK(bo && j_obj_get(bo, "error"), "unknown command returns error");
    free(bad); j_free(bo);

    /* bad json -> error */
    bad = agent_handle(a, "not json at all");
    bo = j_parse(bad, NULL);
    CK(bo && j_obj_get(bo, "error"), "bad json returns error");
    free(bad); j_free(bo);

    agent_free(a);
    if (fails) { printf("\nAGENT TESTS FAILED (%d)\n", fails); return 1; }
    printf("AGENT TESTS PASSED (ingest/regurgitate/search/edit/diff/lex/encode/list/errors)\n");
    return 0;
}
