#include "cartag.h"

#include <stdio.h>
#include <string.h>

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
        } else {
            snprintf(t->out_path, sizeof(t->out_path), "%s", t->filename);
        }

        if (opts->group_by_format) {
            char tmp[CARTAG_PATH_MAX];
            snprintf(tmp, sizeof(tmp), "%s/%s", audio_format_name(t->format), t->out_path);
            snprintf(t->out_path, sizeof(t->out_path), "%s", tmp);
        }
    }
}

void organizer_apply_prefix(TrackList *list) {
    for (size_t i = 0; i < list->count; ++i) {
        AudioTrack *t = &list->tracks[i];
        char tmp[CARTAG_PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%03zu_%s", i + 1, t->out_path);
        snprintf(t->out_path, sizeof(t->out_path), "%s", tmp);
    }
}
