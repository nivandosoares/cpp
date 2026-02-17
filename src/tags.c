#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

void tags_fix_from_filename(AudioTrack *t) {
    char base[CARTAG_NAME_MAX];
    char *dot;
    char *sep;
    snprintf(base, sizeof(base), "%s", t->filename);
    dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    sep = strstr(base, " - ");
    if (sep) {
        *sep = '\0';
        snprintf(t->artist, sizeof(t->artist), "%s", base);
        snprintf(t->title, sizeof(t->title), "%s", sep + 3);
    } else {
        snprintf(t->artist, sizeof(t->artist), "Unknown Artist");
        snprintf(t->title, sizeof(t->title), "%s", base);
    }

    if (t->album[0] == '\0') snprintf(t->album, sizeof(t->album), "Singles");
    if (t->genre[0] == '\0') snprintf(t->genre, sizeof(t->genre), "Unknown");
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
