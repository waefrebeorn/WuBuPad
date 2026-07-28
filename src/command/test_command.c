/* test_command.c */
#include "command.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
static int ran=0;
static int cmd_theme(void *a){ (void)a; ran++; return 7; }
static int cmd_open(void *a){ (void)a; ran+=10; return 0; }
int main(void){
    CommandRegistry *r = command_registry_create();
    CK(r, "create");
    CK(command_register(r,"editor:toggle-theme",cmd_theme)==0, "reg theme");
    CK(command_register(r,"tree-view:open",cmd_open)==0, "reg open");
    CK(command_register(r,"editor:toggle-theme",cmd_theme)==-1, "dup rejected");
    CK(command_exists(r,"editor:toggle-theme"), "exists");
    CK(!command_exists(r,"nope"), "missing");
    CK(command_run(r,"editor:toggle-theme",NULL)==7, "run returns cb result");
    CK(ran==1, "callback fired once");
    CK(command_count(r)==2, "count 2");
    CK(command_unregister(r,"tree-view:open")==0, "unreg");
    CK(command_count(r)==1, "count 1 after unreg");
    CK(command_run(r,"tree-view:open",NULL)==-1, "run after unreg fails");
    command_registry_free(r);
    if (fails){ printf("COMMAND TESTS FAILED (%d)\n",fails); return 1; }
    printf("COMMAND TESTS PASSED\n"); return 0;
}
