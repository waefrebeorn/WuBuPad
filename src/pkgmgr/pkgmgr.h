/* pkgmgr.h -- package manager (Atom's defining feature).
 *
 * Scans ~/.wubupad/packages/<name>/ for Atom-style packages. Each package
 * has a package.json (name, version, "main" C-plugin entry, and a
 * "wubupad": { "commands": [...] } list). A package may ship a native
 * plugin (a .so exposing `wubupad_package_init(PackageAPI*)`); the manager
 * dlopen's it, passes a C-ABI API (register command, get registry), and
 * unloads it on disable. Pure-JSON packages (command manifest only) also
 * register their declared commands so the palette lists them. Opaque, C11,
 * no third-party deps. */
#ifndef WUBUPAD_PKGMGR_H
#define WUBUPAD_PKGMGR_H

#include "command.h"
#include <stddef.h>

typedef struct PackageManager PackageManager;

/* C-ABI a package plugin receives on load. */
typedef struct {
    CommandRegistry *registry;          /* register commands here */
    const char      *pkg_dir;           /* this package's directory */
    void            *user;              /* opaque per-package state (owner) */
} PackageAPI;

/* A package plugin's single exported symbol. Returns 0 on success. */
typedef int (*pkg_init_fn)(PackageAPI *api);

PackageManager *pkgmgr_create(CommandRegistry *reg, const char *packages_dir);
void pkgmgr_free(PackageManager *m);

/* (Re)discover packages under the dir. Returns the count found. */
size_t pkgmgr_discover(PackageManager *m);

/* Enable a discovered package by name (loads native plugin if present, or
 * registers its manifest commands). Returns 0 on success, -1 if unknown. */
int pkgmgr_enable(PackageManager *m, const char *name);

/* Disable (and dlclose) a package; unregisters its commands. */
int pkgmgr_disable(PackageManager *m, const char *name);

/* Count of discovered / enabled packages. */
size_t pkgmgr_count(const PackageManager *m);
size_t pkgmgr_enabled_count(const PackageManager *m);

/* Name of discovered package i (0-based). */
const char *pkgmgr_name_at(const PackageManager *m, size_t i);
int         pkgmgr_is_enabled(const PackageManager *m, const char *name);
/* declared version of package i (or NULL). */
const char *pkgmgr_version_at(const PackageManager *m, size_t i);

#endif /* WUBUPAD_PKGMGR_H */
