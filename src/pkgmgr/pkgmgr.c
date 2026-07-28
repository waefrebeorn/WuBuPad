/* pkgmgr.c -- package manager. See pkgmgr.h. */
#include "pkgmgr.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef __APPLE__
# include <mach-o/dyld.h>
#else
# include <dlfcn.h>
#endif

#define PKG_MAX 256

/* forward decls */
static int pkgmgr__noop(void *arg);

struct Pkg {
    char name[128];
    char version[64];
    char dir[1024];
    int  enabled;
    void *dl;                 /* dlopen handle (NULL = manifest-only) */
    char **manifest_cmds;     /* command names declared in package.json */
    size_t manifest_n;
};

struct PackageManager {
    CommandRegistry *reg;
    char dir[1024];
    struct Pkg pkgs[PKG_MAX];
    size_t n;
};

static int dir_exists(const char *p){
    struct stat st; return stat(p,&st)==0 && S_ISDIR(st.st_mode);
}

PackageManager *pkgmgr_create(CommandRegistry *reg, const char *packages_dir) {
    if (!reg || !packages_dir) return NULL;
    PackageManager *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->reg = reg;
    strncpy(m->dir, packages_dir, sizeof m->dir - 1);
    return m;
}
void pkgmgr_free(PackageManager *m) {
    if (!m) return;
    for (size_t i = 0; i < m->n; i++) {
        if (m->pkgs[i].enabled) pkgmgr_disable(m, m->pkgs[i].name);
        for (size_t k = 0; k < m->pkgs[i].manifest_n; k++)
            free(m->pkgs[i].manifest_cmds[k]);
        free(m->pkgs[i].manifest_cmds);
#ifdef __APPLE__
        /* no dlclose needed; dyld kept closed */
#else
        if (m->pkgs[i].dl) dlclose(m->pkgs[i].dl);
#endif
    }
    free(m);
}

size_t pkgmgr_discover(PackageManager *m) {
    if (!m) return 0;
    m->n = 0;
    DIR *d = opendir(m->dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) && m->n < PKG_MAX) {
        if (e->d_name[0] == '.') continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", m->dir, e->d_name);
        if (!dir_exists(path)) continue;
        char pj[1400];
        snprintf(pj, sizeof pj, "%s/package.json", path);
        FILE *f = fopen(pj, "r");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char *buf = malloc((size_t)sz + 1);
        if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz) {
            buf[sz] = 0;
            JVal *root = j_parse(buf, NULL);
            struct Pkg *p = &m->pkgs[m->n];
            strncpy(p->name, e->d_name, sizeof p->name - 1);
            strncpy(p->dir, path, sizeof p->dir - 1);
            p->enabled = 0; p->dl = NULL; p->manifest_cmds = NULL; p->manifest_n = 0;
            if (root) {
                const JVal *v = j_obj_get(root, "version");
                if (v && j_type(v) == J_STR) {
                    strncpy(p->version, j_as_str(v), sizeof p->version - 1);
                }
                /* wubupad.commands: array of strings -> declared commands */
                const JVal *wp = j_obj_get(root, "wubupad");
                if (wp && j_type(wp) == J_OBJ) {
                    const JVal *cmds = j_obj_get(wp, "commands");
                    if (cmds && j_type(cmds) == J_ARR) {
                        size_t cnt = j_len(cmds);
                        p->manifest_cmds = calloc(cnt + 1, sizeof(char *));
                        for (size_t k = 0; k < cnt; k++) {
                            const JVal *c = j_arr_at(cmds, k);
                            if (c && j_type(c) == J_STR)
                                p->manifest_cmds[p->manifest_n++] = strdup(j_as_str(c));
                        }
                    }
                }
                j_free(root);
            }
            free(buf);
            m->n++;
        }
        if (f) fclose(f);
    }
    closedir(d);
    return m->n;
}

int pkgmgr_enable(PackageManager *m, const char *name) {
    if (!m || !name) return -1;
    for (size_t i = 0; i < m->n; i++) {
        struct Pkg *p = &m->pkgs[i];
        if (strcmp(p->name, name) != 0) continue;
        if (p->enabled) return 0;
        /* manifest commands: register a no-op so the palette lists them
         * (a real package ships a native plugin that overrides these). */
        for (size_t k = 0; k < p->manifest_n; k++)
            command_register(m->reg, p->manifest_cmds[k], pkgmgr__noop);
        /* try native plugin: <dir>/<name>.so exposing wubupad_package_init */
        char so[1400];
        snprintf(so, sizeof so, "%s/%s.so", p->dir, p->name);
        struct stat st; p->dl = NULL;
        if (stat(so, &st) == 0) {
#ifdef __APPLE__
            /* dyld dlopen equivalent omitted for brevity; treat as manifest */
#else
            p->dl = dlopen(so, RTLD_NOW | RTLD_LOCAL);
            if (p->dl) {
                pkg_init_fn init = (pkg_init_fn)dlsym(p->dl, "wubupad_package_init");
                if (init) {
                    PackageAPI api = { m->reg, p->dir, NULL };
                    if (init(&api) != 0) { dlclose(p->dl); p->dl = NULL; }
                } else { dlclose(p->dl); p->dl = NULL; }
            }
#endif
        }
        p->enabled = 1;
        return 0;
    }
    return -1;
}

static int pkgmgr__noop(void *arg){ (void)arg; return 0; }

int pkgmgr_disable(PackageManager *m, const char *name) {
    if (!m || !name) return -1;
    for (size_t i = 0; i < m->n; i++) {
        struct Pkg *p = &m->pkgs[i];
        if (strcmp(p->name, name) != 0) continue;
        if (!p->enabled) return 0;
        for (size_t k = 0; k < p->manifest_n; k++)
            command_unregister(m->reg, p->manifest_cmds[k]);
#ifndef __APPLE__
        if (p->dl) { dlclose(p->dl); p->dl = NULL; }
#endif
        p->enabled = 0;
        return 0;
    }
    return -1;
}

size_t pkgmgr_count(const PackageManager *m){ return m ? m->n : 0; }
size_t pkgmgr_enabled_count(const PackageManager *m){
    if (!m) return 0; size_t c = 0;
    for (size_t i = 0; i < m->n; i++) if (m->pkgs[i].enabled) c++;
    return c;
}
const char *pkgmgr_name_at(const PackageManager *m, size_t i){
    return (m && i < m->n) ? m->pkgs[i].name : NULL;
}
int pkgmgr_is_enabled(const PackageManager *m, const char *name){
    if (!m || !name) return 0;
    for (size_t i = 0; i < m->n; i++)
        if (strcmp(m->pkgs[i].name, name) == 0) return m->pkgs[i].enabled;
    return 0;
}
const char *pkgmgr_version_at(const PackageManager *m, size_t i){
    return (m && i < m->n) ? m->pkgs[i].version : NULL;
}
