#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void str_copy(char *dst, size_t dst_sz, const char *src) {
    size_t n;
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void tags_fix_from_filename(AudioTrack *t) {
    char base[CARTAG_NAME_MAX];
    char *dot;
    char *sep;

    str_copy(base, sizeof(base), t->filename);
    dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    sep = strstr(base, " - ");
    if (sep) {
        *sep = '\0';
        str_copy(t->artist, sizeof(t->artist), base);
        str_copy(t->title, sizeof(t->title), sep + 3);
    } else {
        str_copy(t->artist, sizeof(t->artist), "Unknown Artist");
        str_copy(t->title, sizeof(t->title), base);
    }

    if (t->album[0] == '\0') str_copy(t->album, sizeof(t->album), "Singles");
    if (t->genre[0] == '\0') str_copy(t->genre, sizeof(t->genre), "Unknown");
    if (t->year == 0) t->year = 2000;
    if (t->track_no == 0) t->track_no = 1;
}

void tags_standardize(AudioTrack *t) {
    for (size_t i = 0; i < strlen(t->artist); ++i) {
        if (i == 0 || t->artist[i - 1] == ' ') t->artist[i] = (char)toupper((unsigned char)t->artist[i]);
    }
    for (size_t i = 0; i < strlen(t->title); ++i) {
        if (i == 0 || t->title[i - 1] == ' ') t->title[i] = (char)toupper((unsigned char)t->title[i]);
    }
}
