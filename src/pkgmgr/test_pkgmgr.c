/* test_pkgmgr.c -- uses a temp dir with a fake package.json. */
#include "pkgmgr.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
static void mk(const char *path, const char *content){
    FILE *f=fopen(path,"w"); if(f){ fputs(content,f); fclose(f); }
}
int main(void){
    char dir[512]; snprintf(dir,sizeof dir,"/tmp/wubupad_pkgs_%d",(int)getpid());
    mkdir(dir,0755);
    char p1[600]; snprintf(p1,sizeof p1,"%s/wubu-snippets",dir); mkdir(p1,0755);
    mk("/tmp/x",""); /* placeholder */
    char pj[700]; snprintf(pj,sizeof pj,"%s/package.json",p1);
    mk(pj,"{\"name\":\"wubu-snippets\",\"version\":\"1.2.3\",\"wubupad\":{\"commands\":[\"snippets:expand\",\"snippets:next\"]}}");
    char p2[600]; snprintf(p2,sizeof p2,"%s/wubu-git",dir); mkdir(p2,0755);
    char pj2[700]; snprintf(pj2,sizeof pj2,"%s/package.json",p2);
    mk(pj2,"{\"name\":\"wubu-git\",\"version\":\"0.9.0\"}");

    CommandRegistry *r = command_registry_create();
    PackageManager *m = pkgmgr_create(r, dir);
    CK(m, "create");
    size_t n = pkgmgr_discover(m);
    CK(n==2, "discovered 2");
    /* order-independent: find the snippet package by name */
    size_t si = (size_t)-1;
    for (size_t k = 0; k < n; k++) if (strcmp(pkgmgr_name_at(m,k),"wubu-snippets")==0) si = k;
    CK(si != (size_t)-1, "found wubu-snippets");
    CK(strcmp(pkgmgr_version_at(m,si),"1.2.3")==0, "version parsed");
    CK(pkgmgr_enable(m,"wubu-snippets")==0, "enable");
    CK(pkgmgr_enabled_count(m)==1, "enabled count");
    CK(pkgmgr_is_enabled(m,"wubu-snippets"), "is enabled");
    CK(command_exists(r,"snippets:expand"), "manifest cmd registered");
    CK(pkgmgr_enable(m,"nope")==-1, "enable unknown fails");
    pkgmgr_disable(m,"wubu-snippets");
    CK(!pkgmgr_is_enabled(m,"wubu-snippets"), "disabled");
    CK(!command_exists(r,"snippets:expand"), "manifest cmd unregistered");
    pkgmgr_free(m); command_registry_free(r);
    /* cleanup */
    remove(pj); remove(pj2); remove(p1); remove(p2); remove(dir);
    if (fails){ printf("PKGMGR TESTS FAILED (%d)\n",fails); return 1; }
    printf("PKGMGR TESTS PASSED\n"); return 0;
}
