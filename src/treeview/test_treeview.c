/* test_treeview.c -- uses a temp dir tree + fake porcelain. */
#include "treeview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)

typedef struct { int mod, add; } Ctx;
static int tv_collect(void *ctx, int depth, const TreeNode *n) {
    Ctx *c = ctx;
    if (strcmp(n->name, "main.c") == 0 && n->git == 1) c->mod = 1;
    if (strcmp(n->name, "readme.md") == 0 && n->git == 2) c->add = 1;
    (void)depth;
    return 0;
}

int main(void){
    char dir[512]; snprintf(dir,sizeof dir,"/tmp/wubupad_tree_%d",(int)getpid());
    mkdir(dir,0755);
    mkdir("/tmp/x",0755); /* dummy */
    char sub[600]; snprintf(sub,sizeof sub,"%s/src",dir); mkdir(sub,0755);
    char a[700]; snprintf(a,sizeof a,"%s/main.c",sub); FILE*f=fopen(a,"w"); if(f){fputs("x",f);fclose(f);}
    char b[700]; snprintf(b,sizeof b,"%s/readme.md",dir); f=fopen(b,"w"); if(f){fputs("x",f);fclose(f);}
    char c[700]; snprintf(c,sizeof c,"%s/ignored.log",dir); f=fopen(c,"w"); if(f){fputs("x",f);fclose(f);}

    TreeView *t = treeview_build(dir, 0, 1 /* ignore dot */);
    CK(t, "build");
    CK(treeview_node_count(t) >= 4, "node count");
    /* git porcelain: main.c modified, readme.md added */
    const char *porcelain =
        " M src/main.c\n"
        "A  readme.md\n"
        "?? ignored.log\n";
    treeview_apply_git(t, porcelain);
    CK(treeview_dirty_count(t) == 3, "3 dirty (main M + readme A + ignored ?)");
    Ctx ctx={0,0};
    treeview_walk(t, tv_collect, &ctx);
    CK(ctx.mod==1, "main.c modified");
    CK(ctx.add==1, "readme added");
    CK(strcmp(treeview_git_label(1),"M")==0, "git label M");
    CK(strcmp(treeview_git_label(2),"A")==0, "git label A");
    treeview_free(t);
    remove(a); remove(b); remove(c); remove(sub); remove(dir);
    if (fails){ printf("TREEVIEW TESTS FAILED (%d)\n",fails); return 1; }
    printf("TREEVIEW TESTS PASSED\n"); return 0;
}
