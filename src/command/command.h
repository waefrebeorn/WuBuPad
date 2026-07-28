/* command.h -- central command registry (the Atom spine).
 *
 * Every feature -- built-in or shipped by a package -- is exposed as a named
 * command ("editor:toggle-theme", "tree-view:open", "snippets:expand").
 * The command palette fuzzy-matches a name and invokes the registered
 * callback. Packages register commands at load; the palette is just a
 * consumer. Opaque registry, clean C11. */
#ifndef WUBUPAD_COMMAND_H
#define WUBUPAD_COMMAND_H

#include <stddef.h>

typedef struct CommandRegistry CommandRegistry;

/* A command callback. `arg` is an opaque caller pointer (e.g. the UI*),
 * so packages can receive context without the registry knowing its type. */
typedef int (*command_fn)(void *arg);

/* Create an empty registry. */
CommandRegistry *command_registry_create(void);
void command_registry_free(CommandRegistry *r);

/* Register `name` -> fn. Returns 0 on success, -1 if name already exists or
 * nulls. `name` is copied. */
int command_register(CommandRegistry *r, const char *name, command_fn fn);

/* Remove a command by name (e.g. on package unload). Returns 0 if removed. */
int command_unregister(CommandRegistry *r, const char *name);

/* Invoke a command by name. Returns the callback's result, or -1 if the
 * command is unknown / registry null. */
int command_run(CommandRegistry *r, const char *name, void *arg);

/* True if `name` is registered. */
int command_exists(const CommandRegistry *r, const char *name);

/* Enumerate: call `cb` for each (name, index) pair. `cb` may return non-zero
 * to stop iteration early. */
typedef int (*command_enum_fn)(void *ctx, size_t index, const char *name);
void command_enumerate(const CommandRegistry *r, command_enum_fn cb, void *ctx);

/* Count of registered commands. */
size_t command_count(const CommandRegistry *r);

#endif /* WUBUPAD_COMMAND_H */
