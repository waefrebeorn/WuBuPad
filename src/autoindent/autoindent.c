/* autoindent.c -- smart typing / auto-indent. See autoindent.h. */
#include "autoindent.h"
#include <string.h>
#include <ctype.h>

static int leading_spaces(const char *line) {
    int n = 0; while (line[n] == ' ') n++; return n;
}

void autoindent_on_key(const char *line, char key, int *out_indent, int *out_extra) {
    *out_extra = 0;
    if (!line) { *out_indent = 0; return; }
    int base = leading_spaces(line);
    if (key == '\n') {
        /* Enter: new line inherits current indent, plus one level if the
         * current line opens a block and isn't already just an opener. */
        int opens = 0;
        for (int i = 0; line[i]; i++) {
            if (line[i]=='{'||line[i]=='('||line[i]=='[') opens++;
            else if (line[i]=='}'||line[i]==')'||line[i]==']') opens--;
        }
        if (opens > 0) { *out_indent = base + 4; *out_extra = 1; }
        else *out_indent = base;
        return;
    }
    if (key == '{' || key == '(' || key == '[') {
        /* typing an opener at end of line: indent next line */
        *out_indent = base + 4; *out_extra = 1;
        return;
    }
    if (key == '}' || key == ')' || key == ']') {
        /* closing: drop one level if line would be just the closer */
        int only = 1; for (int i=0; line[i] && line[i]!='}' && line[i]!=')' && line[i]!=']'; i++)
            if (!isspace((unsigned char)line[i])) { only = 0; break; }
        *out_indent = only ? (base >= 4 ? base - 4 : 0) : base;
        return;
    }
    *out_indent = base;
}

int autoindent_continued(const char *line) {
    if (!line) return 0;
    int n = (int)strlen(line);
    int trailing = 0;
    while (n > 0 && (line[n-1]==' '||line[n-1]=='\t')) { trailing++; n--; }
    if (n == 0) return 0;
    char last = line[n-1];
    if (last == '+' || last == '-' || last == '*' || last == '/' ||
        last == '=' || last == ',' || last == '.')
        return leading_spaces(line) + 4;
    return 0;
}
