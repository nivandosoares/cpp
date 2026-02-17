#include "cartag.h"

#include <stdio.h>
#include <string.h>

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

static void str_append(char *dst, size_t dst_sz, const char *src) {
    size_t dlen;
    size_t slen;
    size_t n;
    if (!dst || dst_sz == 0 || !src) return;
    dlen = strlen(dst);
    if (dlen >= dst_sz - 1) return;
    slen = strlen(src);
    n = dst_sz - dlen - 1;
    if (slen < n) n = slen;
    memcpy(dst + dlen, src, n);
    dst[dlen + n] = '\0';
}

static void path_join2(char *dst, size_t dst_sz, const char *a, const char *b) {
    str_copy(dst, dst_sz, a ? a : "");
    str_append(dst, dst_sz, "/");
    str_append(dst, dst_sz, b ? b : "");
}

void organizer_plan(TrackList *list, const CliOptions *opts) {
    for (size_t i = 0; i < list->count; ++i) {
        AudioTrack *t = &list->tracks[i];
        const char *ext = strrchr(t->filename, '.');
        if (!ext) ext = ".mp3";

        if (opts->organize == ORG_ARTIST) {
            snprintf(t->out_path, sizeof(t->out_path), "%s/%s%s", t->artist, t->title, ext);
        } else if (opts->organize == ORG_ALBUM) {
            snprintf(t->out_path, sizeof(t->out_path), "%s/%s/%02d - %s%s", t->artist, t->album, t->track_no, t->title, ext);
        } else if (opts->organize == ORG_FLAT) {
            snprintf(t->out_path, sizeof(t->out_path), "%s%s", t->title, ext);
        } else if (opts->organize == ORG_GENRE_ARTIST) {
            snprintf(t->out_path, sizeof(t->out_path), "%s/%s/%s%s",
                     t->genre[0] ? t->genre : "Unknown",
                     t->artist[0] ? t->artist : "Unknown Artist",
                     t->title,
                     ext);
        } else {
            str_copy(t->out_path, sizeof(t->out_path), t->filename);
        }

        if (opts->group_by_format) {
            char tmp[CARTAG_PATH_MAX];
            path_join2(tmp, sizeof(tmp), audio_format_name(t->format), t->out_path);
            str_copy(t->out_path, sizeof(t->out_path), tmp);
        }
    }
}

void organizer_apply_prefix(TrackList *list) {
    for (size_t i = 0; i < list->count; ++i) {
        AudioTrack *t = &list->tracks[i];
        char tmp[CARTAG_PATH_MAX];
        int nw = snprintf(tmp, sizeof(tmp), "%03zu_", i + 1);
        size_t off = (nw > 0) ? (size_t)nw : 0;
        if (off >= sizeof(tmp)) off = sizeof(tmp) - 1;
        tmp[off] = '\0';
        str_append(tmp, sizeof(tmp), t->out_path);
        str_copy(t->out_path, sizeof(t->out_path), tmp);
    }
}
