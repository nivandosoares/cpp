#include "cartag.h"

#include <stdio.h>
#include <string.h>

int exporter_run(const TrackList *list, const CliOptions *opts) {
    size_t copied = 0;
    if (opts->export_path[0] == '\0') return 0;

    for (size_t i = 0; i < list->count; ++i) {
        const AudioTrack *t = &list->tracks[i];
        char dst[CARTAG_PATH_MAX];
        if (t->duplicate) continue;
        snprintf(dst, sizeof(dst), "%s/%s", opts->export_path, t->out_path[0] ? t->out_path : t->filename);
        if (fs_copy_file(t->path, dst) == 0) {
            copied++;
        } else {
            fprintf(stderr, "Falha ao copiar: %s\n", t->filename);
        }
    }
    printf("Exportadas %zu faixas para %s\n", copied, opts->export_path);
    return 0;
}
