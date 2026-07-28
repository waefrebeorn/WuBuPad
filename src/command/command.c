/* command.c -- command registry. See command.h. */
#include "command.h"
#include <stdlib.h>
#include <string.h>

#define CMD_MAX 1024

struct Cmd { char *name; command_fn fn; };

struct CommandRegistry {
    struct Cmd items[CMD_MAX];
    size_t n;
};

CommandRegistry *command_registry_create(void) {
    CommandRegistry *r = calloc(1, sizeof *r);
    return r;
}
void command_registry_free(CommandRegistry *r) {
    if (!r) return;
    for (size_t i = 0; i < r->n; i++) free(r->items[i].name);
    free(r);
}

static int find(CommandRegistry *r, const char *name) {
    for (size_t i = 0; i < r->n; i++)
        if (strcmp(r->items[i].name, name) == 0) return (int)i;
    return -1;
}

int command_register(CommandRegistry *r, const char *name, command_fn fn) {
    if (!r || !name || !fn) return -1;
    if (r->n >= CMD_MAX) return -1;
    if (find(r, name) >= 0) return -1;
    r->items[r->n].name = strdup(name);
    if (!r->items[r->n].name) return -1;
    r->items[r->n].fn = fn;
    r->n++;
    return 0;
}

int command_unregister(CommandRegistry *r, const char *name) {
    int i = find(r, name);
    if (i < 0) return -1;
    free(r->items[i].name);
    /* swap-remove keeps the array dense */
    r->items[i] = r->items[r->n - 1];
    r->n--;
    return 0;
}

int command_run(CommandRegistry *r, const char *name, void *arg) {
    int i = find(r, name);
    if (i < 0) return -1;
    return r->items[i].fn(arg);
}

int command_exists(const CommandRegistry *r, const char *name) {
    return r && find((CommandRegistry *)r, name) >= 0;
}

void command_enumerate(const CommandRegistry *r, command_enum_fn cb, void *ctx) {
    if (!r || !cb) return;
    for (size_t i = 0; i < r->n; i++)
        if (cb(ctx, i, r->items[i].name)) return;
}

size_t command_count(const CommandRegistry *r) { return r ? r->n : 0; }
