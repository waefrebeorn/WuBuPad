/* test_multicursor.c -- parallel insert over a buffer shim. */
#include "multicursor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)

static char buf[4096]; static size_t blen=0;
static void ins(void *b, size_t pos, const char *t, size_t l){
    (void)b; memmove(buf+pos+l, buf+pos, blen-pos+1); memcpy(buf+pos,t,l); blen+=l;
}
static void setc(void *b, size_t cur, size_t anc){ (void)b; (void)cur; (void)anc; }

int main(void){
    MultiCursor *m = multicursor_create();
    CK(m, "create");
    blen=0; strcpy(buf,"aaa bbb ccc"); blen = strlen(buf);
    /* add carets at each space (positions 3 and 7) */
    multicursor_add(m, 3, 3);
    multicursor_add(m, 7, 7);
    CK(multicursor_count(m)==2, "2 carets");
    multicursor_insert_all(m, NULL, ins, setc, ";", 1);
    /* expect "aaa; bbb; ccc" */
    CK(strcmp(buf,"aaa; bbb; ccc")==0, "parallel insert at both spaces");
    multicursor_clear(m);
    CK(multicursor_count(m)==0, "cleared");
    multicursor_free(m);
    if (fails){ printf("MULTICURSOR TESTS FAILED (%d)\n",fails); return 1; }
    printf("MULTICURSOR TESTS PASSED\n"); return 0;
}
