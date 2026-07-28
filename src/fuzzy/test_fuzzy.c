/* test_fuzzy.c */
#include "fuzzy.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
int main(void){
    CK(fuzzy_match("tp","tree-view:toggle")==0, "subseq");
    CK(fuzzy_match("tvt","tree-view:toggle")==1, "subseq2");
    CK(fuzzy_score("xyz","abc")<0, "non-subseq scored -1");
    long s1 = fuzzy_score("tv","tree-view");
    long s2 = fuzzy_score("tv","trivial-view");
    CK(s1 >= 0 && s2 >= 0, "both match");
    CK(s1 > s2, "separator boundary scores higher");
    /* top-N */
    const char *items[] = {"editor:toggle-theme","tree-view:open","editor:find",
                           "markdown:preview","command:palette","git:commit"};
    size_t idx[6]; long sc[6];
    size_t n = fuzzy_top(items,6,"edt",idx,sc,6);
    CK(n>0, "top returns some");
    CK(strstr(items[idx[0]],"editor")!=NULL, "best starts with editor");
    /* empty query matches all, equal score */
    size_t n2 = fuzzy_top(items,6,"",idx,sc,6);
    CK(n2==6, "empty query -> all");
    if (fails){ printf("FUZZY TESTS FAILED (%d)\n",fails); return 1; }
    printf("FUZZY TESTS PASSED\n"); return 0;
}
