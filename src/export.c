#include "cartag.h"

#include <stdio.h>
#include <string.h>

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
    if (dst_sz == 0) return;
    dst[0] = '\0';
    str_append(dst, dst_sz, a ? a : "");
    str_append(dst, dst_sz, "/");
    str_append(dst, dst_sz, b ? b : "");
}

int exporter_run(const TrackList *list, const CliOptions *opts) {
    size_t copied = 0;
    if (opts->export_path[0] == '\0') return 0;

    for (size_t i = 0; i < list->count; ++i) {
        const AudioTrack *t = &list->tracks[i];
        char dst[CARTAG_PATH_MAX];
        if (t->duplicate) continue;
        path_join2(dst, sizeof(dst), opts->export_path, t->out_path[0] ? t->out_path : t->filename);
        if (fs_copy_file(t->path, dst) == 0) {
            copied++;
        } else {
            fprintf(stderr, "Falha ao copiar: %s\n", t->filename);
        }
    }
    printf("Exportadas %zu faixas para %s\n", copied, opts->export_path);
    return 0;
}
