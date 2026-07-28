/* test_mdpreview.c */
#include "mdpreview.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)
int main(void){
    const char *md =
        "# Title\n\n"
        "A **bold** and *italic* and `code` line.\n\n"
        "## Section\n\n"
        "- one\n- two\n\n"
        "1. first\n2. second\n\n"
        "> a quote\n\n"
        "---\n\n"
        "```\ncode block\n```\n";
    char *html = mdpreview_render(md, "Doc");
    CK(html, "render");
    CK(strstr(html,"<!doctype html>")!=NULL, "doctype");
    CK(strstr(html,"<h1>Title</h1>")!=NULL, "h1");
    CK(strstr(html,"<h2>Section</h2>")!=NULL, "h2");
    CK(strstr(html,"<strong>bold</strong>")!=NULL, "bold");
    CK(strstr(html,"<em>italic</em>")!=NULL, "italic");
    CK(strstr(html,"<code>code</code>")!=NULL, "inline code");
    CK(strstr(html,"<li>one</li>")!=NULL, "ul item");
    CK(strstr(html,"<li>first</li>")!=NULL, "ol item");
    CK(strstr(html,"<blockquote>a quote</blockquote>")!=NULL, "blockquote");
    CK(strstr(html,"<hr>")!=NULL, "hr");
    CK(strstr(html,"<pre><code>code block")!=NULL, "fenced code");
    /* escaping */
    char *esc = mdpreview_render("a < b & c > d", NULL);
    CK(strstr(esc,"&lt;")!=NULL && strstr(esc,"&amp;")!=NULL && strstr(esc,"&gt;")!=NULL, "escaped");
    free(html); free(esc);
    if (fails){ printf("MDPREVIEW TESTS FAILED (%d)\n",fails); return 1; }
    printf("MDPREVIEW TESTS PASSED\n"); return 0;
}
