/* test_palette.c */
#include "palette.h"
#include "command.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
static int ran=0;
static int c_theme(void *a){ (void)a; ran|=1; return 0; }
static int c_open(void *a){ (void)a; ran|=2; return 0; }
static int c_find(void *a){ (void)a; ran|=4; return 0; }
int main(void){
    CommandRegistry *r = command_registry_create();
    command_register(r,"editor:toggle-theme",c_theme);
    command_register(r,"tree-view:open",c_open);
    command_register(r,"editor:find",c_find);
    Palette *p = palette_create(r);
    CK(p, "create");
    palette_open(p);
    CK(palette_is_open(p), "open");
    /* type "tog" -> should narrow to editor:toggle-theme */
    const char *q="tog"; for (size_t i=0;q[i];i++) palette_feed(p,q[i],0,NULL);
    CK(palette_count(p)==1, "narrowed to 1");
    CK(strcmp(palette_name_at(p,0),"editor:toggle-theme")==0, "right candidate");
    /* confirm with Enter */
    int rc = palette_feed(p,'\n',0,NULL);
    CK(rc==1, "confirm runs");
    CK(ran==1, "theme command ran");
    CK(!palette_is_open(p), "closed after run");
    /* reopen, type, backspace, pick open */
    ran=0; palette_open(p);
    const char *q2="tree"; for (size_t i=0;q2[i];i++) palette_feed(p,q2[i],0,NULL);
    CK(palette_count(p)>=1, "tree narrowed");
    /* move highlight down if needed then enter -> first is tree-view:open */
    int rc2 = palette_feed(p,'\n',0,NULL);
    CK(rc2==1 && (ran&2), "open command ran");
    palette_free(p); command_registry_free(r);
    if (fails){ printf("PALETTE TESTS FAILED (%d)\n",fails); return 1; }
    printf("PALETTE TESTS PASSED\n"); return 0;
}
