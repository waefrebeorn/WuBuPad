/* test_minimap.c */
#include "minimap.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
int main(void){
    Minimap *m = minimap_create(1000, 20);
    CK(m, "create");
    const char *text = "int main() {\n\treturn 0;\n}\n\n\n// comment\n";
    size_t n = minimap_update(m, text);
    CK(n==6, "mapped 6 lines");
    CK(minimap_lit(m,0)==1, "line0 lit (code)");
    CK(minimap_lit(m,3)==0, "line3 blank (unlit)");
    CK(minimap_lit(m,5)==1, "comment lit");
    CK(minimap_lit_count(m)==4, "4 lit rows (2 blank lines)");
    CK(minimap_rows(m)==6, "rows");
    minimap_free(m);
    if (fails){ printf("MINIMAP TESTS FAILED (%d)\n",fails); return 1; }
    printf("MINIMAP TESTS PASSED\n"); return 0;
}
