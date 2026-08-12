/* transform.h -- text transforms: sort lines + case conversion.
 * Headless, reusable by the agent protocol and the interactive UI. */
#ifndef WUBUPAD_TRANSFORM_H
#define WUBUPAD_TRANSFORM_H

#include "doc.h"

/* which values for transform_case */
enum { TRANSFORM_UPPER = 0, TRANSFORM_LOWER, TRANSFORM_TITLE };
/* which values for transform_sort_lines */
enum { SORT_ASC = 0, SORT_DESC, SORT_ASC_IC, SORT_DESC_IC };

/* Convert the selected text (or whole doc) to upper/lower/title case.
 * Returns 1 if the doc changed, 0 if no-op, -1 on allocation failure. */
int transform_case(Doc *d, int which);

/* Sort the selected lines (or whole doc) ascending/descending, case-sensitive
 * or insensitive. Returns 1 if changed, 0 if already sorted, -1 on error. */
int transform_sort_lines(Doc *d, int which);

#endif /* WUBUPAD_TRANSFORM_H */
