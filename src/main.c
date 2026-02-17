#include "cartag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    CliOptions opts;
    TrackList list;
    LibraryStats stats;

    memset(&stats, 0, sizeof(stats));

    if (cli_parse(argc, argv, &opts) != 0) {
        cli_print_help();
        return 1;
    }

    if (opts.interactive_tui) {
        if (tui_run(&opts) != 0) {
            printf("Encerrado pelo usuario.\n");
            return 0;
        }
    }

    if (downloader_is_url(opts.input)) {
        char warn[256];
        const char *download_dir = ".cartag/downloads";
        if (downloader_fetch_audio(opts.input, download_dir, warn, sizeof(warn)) != 0) {
            fprintf(stderr, "Erro download: %s\n", warn);
            return 2;
        }
        printf("[INFO] %s\n", warn);
        snprintf(opts.input, sizeof(opts.input), "%s", download_dir);
    }

    if (fs_scan_audio(opts.input, &list) != 0) {
        fprintf(stderr, "Falha ao escanear entrada: %s\n", opts.input);
        return 3;
    }

    for (size_t i = 0; i < list.count; ++i) {
        AudioTrack *t = &list.tracks[i];
        char warn[256];

        sanitize_track(t, opts.limit_name || opts.car_safe);
        if (opts.fix_tags || opts.car_safe) {
            tags_fix_from_filename(t);
            tags_standardize(t);
        }

        audio_can_play_car(t, opts.car_safe, warn, sizeof(warn));
        if (warn[0]) printf("[WARN] %s: %s\n", t->filename, warn);

        warn[0] = '\0';
        audio_convert_if_needed(t, &opts, warn, sizeof(warn));
        if (warn[0]) printf("[INFO] %s: %s\n", t->filename, warn);

        stats.total_tracks++;
        stats.total_duration += (uint64_t)t->duration_seconds;
        stats.format_count[t->format]++;
    }

    if (opts.dedupe || opts.car_safe) dedupe_mark(&list, &stats);

    organizer_plan(&list, &opts);
    if (opts.prefix || opts.car_safe) organizer_apply_prefix(&list);

    if (opts.normalize_volume) {
        printf("[INFO] normalize-volume habilitado: usar ffmpeg loudnorm quando disponivel (TODO detalhado).\n");
    }
    if (opts.strip_art || opts.resize_art || opts.extract_art) {
        printf("[INFO] gerenciamento de capas solicitado (strip/resize/extract) em modo leve.\n");
    }

    diagnostics_print(&list);
    simulate_print(&list, opts.simulate, &stats);
    exporter_run(&list, &opts);
    stats_print(&stats);

    free(list.tracks);
    return 0;
}
