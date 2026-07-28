/* test_snippet.c -- exercises the snippet engine over a simple buffer shim. */
#include "snippet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)

/* tiny buffer shim */
static char buf[4096]; static size_t blen=0;
static void ins(void *b, size_t pos, const char *t, size_t l){
    (void)b; memmove(buf+pos+l, buf+pos, blen-pos+1); memcpy(buf+pos,t,l); blen+=l;
}
static void del(void *b, size_t from, size_t to){ (void)b; memmove(buf+from,buf+to,blen-to+1); blen-= (to-from); }
static size_t lenfn(void *b){ (void)b; return blen; }

int main(void){
    SnippetEngine *e = snippet_create();
    CK(e, "create");
    snippet_add(e, "for", "for (${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n\t${0}\n}");
    blen=0; buf[0]=0;
    int n = snippet_expand(e, NULL, ins, del, lenfn, "for", 0);
    CK(n >= 1, "expanded with tabstops");
    CK(snippet_active_p(e), "active after expand");
    size_t f,t; snippet_active(e,&f,&t);
    CK(f==5, "first tabstop at 'i' start (for (|)");
    /* the body should contain 'for (i = 0; i < n; i++) {' with i's as mirrors */
    CK(strstr(buf,"for (i = 0; i < n; i++) {")!=NULL, "body expanded with mirrors");
    /* next tabstop */
    int ok = snippet_next(e,&f,&t);
    CK(ok==1, "next tabstop");
    CK(t-f==1, "second tabstop 'n' width 1");
    /* finish */
    snippet_next(e,&f,&t);
    CK(!snippet_active_p(e), "done after last");
    snippet_free(e);
    if (fails){ printf("SNIPPET TESTS FAILED (%d)\n",fails); return 1; }
    printf("SNIPPET TESTS PASSED\n"); return 0;
}
