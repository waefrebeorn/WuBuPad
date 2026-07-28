/* treeview.h -- project tree view + git status (Atom "tree-view" +
 * "git" packages). Pure, headless-friendly: builds a tree of a directory and
 * (optionally) overlays git status parsed from `git status --porcelain`
 * output. No third-party deps; the git text can be supplied directly so the
 * status parser is unit-testable without invoking git. Opaque, C11. */
#ifndef WUBUPAD_TREEVIEW_H
#define WUBUPAD_TREEVIEW_H

#include <stddef.h>

typedef enum { TV_FILE, TV_DIR } TVKind;

typedef struct TreeNode TreeNode;       /* opaque node */
struct TreeNode {
    char name[256];
    TVKind kind;
    int git;                    /* 0 clean, 1 modified, 2 added/untracked, 3 ignored */
    TreeNode *child;           /* first child (dirs) */
    TreeNode *sibling;         /* next sibling */
};

typedef struct {
    TreeNode *root;            /* root directory node */
} TreeView;

/* Build a tree from `root_path`, recursing up to `max_depth` (0 = unlimited).
 * Returns NULL on error (e.g. not a directory). Caller frees via treeview_free.
 * `ignore_dot` skips dotfiles/dirs when non-zero. */
TreeView *treeview_build(const char *root_path, int max_depth, int ignore_dot);
void treeview_free(TreeView *t);

/* Overlay git status from raw `porcelain` text (output of
 * `git status --porcelain`). Paths are matched against tree node names. */
void treeview_apply_git(TreeView *t, const char *porcelain);

/* Count nodes / count nodes with non-clean git state (for status summary). */
size_t treeview_node_count(const TreeView *t);
size_t treeview_dirty_count(const TreeView *t);

/* Depth-first visit: cb receives (ctx, depth, node). Return non-zero to stop. */
typedef int (*tree_visit_fn)(void *ctx, int depth, const TreeNode *n);
void treeview_walk(const TreeView *t, tree_visit_fn cb, void *ctx);

/* Human git-flag label. */
const char *treeview_git_label(int git);

#endif /* WUBUPAD_TREEVIEW_H */
