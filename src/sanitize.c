#include "cartag.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char latin1_fold(unsigned char c) {
    const char *from = "áàâãäåÁÀÂÃÄÅéèêëÉÈÊËíìîïÍÌÎÏóòôõöÓÒÔÕÖúùûüÚÙÛÜçÇñÑ";
    const char *to   = "aaaaaaAAAAAAeeeeEEEEiiiiIIIIoooooOOOOOuuuuUUUUcCnN";
    const char *p = strchr(from, c);
    if (!p) return 0;
    return to[p - from];
}

static void str_remove_pattern(char *s, const char *pat) {
    char *p;
    while ((p = strstr(s, pat)) != NULL) {
        memmove(p, p + strlen(pat), strlen(p + strlen(pat)) + 1);
    }
}

void sanitize_filename(char *name, size_t max_len) {
    char out[CARTAG_NAME_MAX];
    size_t j = 0;
    size_t n = strlen(name);

    str_remove_pattern(name, "(Official Video)");
    str_remove_pattern(name, "[HD]");
    str_remove_pattern(name, "(Visualizer)");

    for (size_t i = 0; i < n && j + 1 < sizeof(out); ++i) {
        unsigned char c = (unsigned char)name[i];
        char folded;
        if (c >= 128) {
            folded = latin1_fold(c);
            if (folded) out[j++] = folded;
            continue;
        }
        if (c < 32) continue;
        if (strchr("<>:\\|?*\"", c)) c = '_';
        if (c == '(' && strstr(name + i, "(ft.")) {
            while (name[i] && name[i] != ')') ++i;
            continue;
        }
        if (c == 'h' && strncmp(name + i, "http", 4) == 0) break;
        if (isalnum(c) || strchr(" .-_[]()", c)) out[j++] = (char)c;
    }

    out[j] = '\0';

    while (j > 0 && isspace((unsigned char)out[j - 1])) out[--j] = '\0';
    if (max_len > 0 && j >= max_len) out[max_len - 1] = '\0';
    snprintf(name, max_len, "%s", out);
}

void sanitize_track(AudioTrack *t, int limit_name) {
    sanitize_filename(t->filename, sizeof(t->filename));
    sanitize_filename(t->artist, sizeof(t->artist));
    sanitize_filename(t->album, sizeof(t->album));
    sanitize_filename(t->title, sizeof(t->title));
    if (limit_name && strlen(t->filename) > 64) {
        t->filename[64] = '\0';
    }
}
