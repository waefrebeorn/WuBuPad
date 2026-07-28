/* shot.c -- headless WuBuPad GUI screenshot tool.
 *
 * Opens the real gfx backend (SDL2 + FreeType2) under SDL_VIDEODRIVER=dummy,
 * seeds a document, renders one frame, and writes a pixel-faithful PNG of the
 * editor window. No display required. Used to produce docs/screenshots.
 * Self-contained PNG writer (no external lib). Clean C11.
 *
 * Usage:
 *   shot <out.png> [file] [--light|--dark] [WxH]
 *
 * Theme defaults to dark. If no file is given, the built-in Notepad++-style
 * sample is used. WxH overrides the default window size (e.g. 1280x800). */
#include "ui/ui.h"
#include "ui/ui_gfx.h"
#include "doc.h"
#include "docs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- dump raw RGBA (converted to PNG by external tool, e.g. PIL) ---- */
static int png_write(const char *path, int w, int h,
                      const unsigned char *rgba){
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    /* 8-byte header: "RGBA" + uint16 w + uint16 h */
    unsigned char hdr[8] = {'R','G','B','A',
        (w>>8)&255, w&255, (h>>8)&255, h&255};
    fwrite(hdr, 1, 8, f);
    fwrite(rgba, 1, (size_t)w*h*4, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv){
    const char *out = NULL, *file = NULL, *theme = "dark";
    int win_w = 1280, win_h = 800;
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--light")) theme = "light";
        else if (!strcmp(argv[i], "--dark")) theme = "dark";
        else if (strchr(argv[i],'x') && atoi(argv[i])>0){
            sscanf(argv[i],"%dx%d",&win_w,&win_h);
        }
        else if (!out) out = argv[i];
        else if (!file) file = argv[i];
    }
    if (!out){ fprintf(stderr, "usage: shot <out.png> [file] [--light|--dark] [WxH]\n"); return 2; }

    Docs *docs = docs_create();
    const char *lang = file ? "c" : NULL;
    if (file){
        if (docs_load_file(docs, file, lang) == (size_t)-1){
            fprintf(stderr, "cannot open %s\n", file);
            docs_free(docs); return 1;
        }
    } else {
        docs_open(docs, NULL,
            "/* WuBuPad -- clean-room editor (Notepad++ parity) */\n"
            "#include <stdio.h>\n\n"
            "int main(int argc, char **argv) {\n"
            "    int total = 0;\n"
            "    for (int i = 0; i < argc; i++) {\n"
            "        total += (int)strlen(argv[i]);\n"
            "    }\n"
            "    printf(\"total chars: %d\\n\", total);\n"
            "    return 0;   /* done */\n"
            "}\n", lang);
    }
    Doc *d = docs_doc(docs, docs_active(docs));

    const UI_Backend *be = ui_gfx_backend();
    UI *ui = ui_create(d, be, 0, 0);
    if (!ui){ fprintf(stderr, "ui_create failed\n"); docs_free(docs); return 1; }
    ui_set_docs(ui, docs);
    ui_set_theme(ui, !strcmp(theme, "light") ? 0 : 1);

    unsigned char *rgba = NULL; int w = 0, h = 0;
    if (ui_capture(ui, &rgba, &w, &h) != 0 || !rgba){
        fprintf(stderr, "capture failed\n");
        ui_free(ui); docs_free(docs); return 1;
    }
    if (png_write(out, w, h, rgba) != 0){
        fprintf(stderr, "png write failed: %s\n", out);
        free(rgba); ui_free(ui); docs_free(docs); return 1;
    }
    fprintf(stderr, "wrote %s (%dx%d)\n", out, w, h);
    free(rgba);
    ui_free(ui);
    docs_free(docs);
    return 0;
}
