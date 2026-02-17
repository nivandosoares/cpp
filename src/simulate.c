#include "cartag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_filename(const void *a, const void *b) {
    const AudioTrack *ta = (const AudioTrack *)a;
    const AudioTrack *tb = (const AudioTrack *)b;
    return strcmp(ta->out_path[0] ? ta->out_path : ta->filename, tb->out_path[0] ? tb->out_path : tb->filename);
}

static int cmp_generic(const void *a, const void *b) {
    const AudioTrack *ta = (const AudioTrack *)a;
    const AudioTrack *tb = (const AudioTrack *)b;
    int c = strcmp(ta->artist, tb->artist);
    if (c != 0) return c;
    return strcmp(ta->title, tb->title);
}

void dedupe_mark(TrackList *list, LibraryStats *stats) {
    for (size_t i = 0; i < list->count; ++i) {
        AudioTrack *a = &list->tracks[i];
        if (a->duplicate) continue;
        for (size_t j = i + 1; j < list->count; ++j) {
            AudioTrack *b = &list->tracks[j];
            if (a->quick_hash == b->quick_hash && a->size_bytes == b->size_bytes) {
                b->duplicate = 1;
                stats->removed_duplicates++;
            }
        }
    }
}

void simulate_print(const TrackList *list, SimulateMode mode, LibraryStats *stats) {
    AudioTrack *tmp;
    size_t out_idx = 0;
    if (mode == SIM_NONE) return;

    tmp = (AudioTrack *)malloc(list->count * sizeof(AudioTrack));
    if (!tmp) return;
    memcpy(tmp, list->tracks, list->count * sizeof(AudioTrack));

    if (mode == SIM_FILENAME || mode == SIM_FAT) {
        qsort(tmp, list->count, sizeof(AudioTrack), cmp_filename);
    } else {
        qsort(tmp, list->count, sizeof(AudioTrack), cmp_generic);
    }

    printf("\nSimulacao de ordem (%s):\n", mode == SIM_GENERIC ? "generic" : (mode == SIM_FAT ? "fat" : "filename"));
    for (size_t i = 0; i < list->count; ++i) {
        if (tmp[i].duplicate) continue;
        printf("%03zu | %s - %s\n", ++out_idx, tmp[i].artist, tmp[i].title);
    }
    printf("Total de faixas: %zu\n", out_idx);
    printf("Duracao total (estimada): %llus\n", (unsigned long long)stats->total_duration);

    free(tmp);
}

void diagnostics_print(const TrackList *list) {
    for (size_t i = 0; i < list->count; ++i) {
        const AudioTrack *t = &list->tracks[i];
        if (t->duplicate) continue;
        if (strlen(t->filename) > 64) {
            printf("[WARN] nome longo: %s\n", t->filename);
        }
        if (strpbrk(t->filename, "<>:\\|?*\"")) {
            printf("[WARN] caracteres invalidos: %s\n", t->filename);
        }
        if (t->artist[0] == '\0' || t->title[0] == '\0') {
            printf("[WARN] tags ausentes: %s\n", t->filename);
        }
    }
}

void stats_print(const LibraryStats *stats) {
    printf("\nEstatisticas:\n");
    printf("Tracks: %zu\n", stats->total_tracks);
    printf("Duration: %llus\n", (unsigned long long)stats->total_duration);
    printf("Formats: MP3(%zu) FLAC(%zu) WAV(%zu) AAC(%zu) M4A(%zu) OGG(%zu) WMA(%zu)\n",
           stats->format_count[FORMAT_MP3],
           stats->format_count[FORMAT_FLAC],
           stats->format_count[FORMAT_WAV],
           stats->format_count[FORMAT_AAC],
           stats->format_count[FORMAT_M4A],
           stats->format_count[FORMAT_OGG],
           stats->format_count[FORMAT_WMA]);
    printf("Duplicates removed: %zu\n", stats->removed_duplicates);
}
