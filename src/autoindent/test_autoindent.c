/* test_autoindent.c */
#include "autoindent.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
int main(void){
    int ind, extra;
    autoindent_on_key("    if (x) {", '\n', &ind, &extra);
    CK(ind==8, "enter after open brace -> +4");
    CK(extra==1, "extra closer line flagged");
    autoindent_on_key("        foo();", '\n', &ind, &extra);
    CK(ind==8, "enter on indented non-block -> same indent");
    autoindent_on_key("}", '\n', &ind, &extra);
    CK(ind==0, "enter after lone closer -> dedent");
    autoindent_on_key("    foo(", '(', &ind, &extra);
    CK(ind==8 && extra==1, "typing ( at EOL -> +4 + extra");
    CK(autoindent_continued("    a = b +")==8, "hanging operator -> +4");
    CK(autoindent_continued("    return x")==0, "no hanging op -> 0");
    if (fails){ printf("AUTOINDENT TESTS FAILED (%d)\n",fails); return 1; }
    printf("AUTOINDENT TESTS PASSED\n"); return 0;
}
