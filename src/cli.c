#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int is_flag(const char *arg, const char *name) {
    return strcmp(arg, name) == 0;
}

int cli_parse(int argc, char **argv, CliOptions *opts) {
    int i;
    memset(opts, 0, sizeof(*opts));
    opts->organize = ORG_NONE;
    opts->simulate = SIM_NONE;

    if (argc < 2) {
        return -1;
    }

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (is_flag(arg, "--help") || is_flag(arg, "-h")) {
            return -1;
        } else if (is_flag(arg, "--keep-format")) {
            opts->keep_format = 1;
        } else if (is_flag(arg, "--convert-mp3")) {
            opts->convert_mp3 = 1;
        } else if (is_flag(arg, "--group-by-format")) {
            opts->group_by_format = 1;
        } else if (is_flag(arg, "--fix-tags")) {
            opts->fix_tags = 1;
        } else if (is_flag(arg, "--dedupe")) {
            opts->dedupe = 1;
        } else if (is_flag(arg, "--normalize-volume")) {
            opts->normalize_volume = 1;
        } else if (is_flag(arg, "--strip-art")) {
            opts->strip_art = 1;
        } else if (is_flag(arg, "--resize-art") && i + 1 < argc) {
            opts->resize_art = atoi(argv[++i]);
        } else if (is_flag(arg, "--extract-art")) {
            opts->extract_art = 1;
        } else if (is_flag(arg, "--car-safe")) {
            opts->car_safe = 1;
            opts->convert_mp3 = 1;
            opts->fix_tags = 1;
            opts->strip_art = 1;
            opts->limit_name = 1;
            opts->prefix = 1;
            if (opts->organize == ORG_NONE) opts->organize = ORG_ARTIST;
        } else if (is_flag(arg, "--organize") && i + 1 < argc) {
            const char *mode = argv[++i];
            if (strcmp(mode, "artist") == 0) opts->organize = ORG_ARTIST;
            else if (strcmp(mode, "album") == 0) opts->organize = ORG_ALBUM;
            else if (strcmp(mode, "flat") == 0) opts->organize = ORG_FLAT;
        } else if (is_flag(arg, "--simulate") && i + 1 < argc) {
            const char *mode = argv[++i];
            if (strcmp(mode, "generic") == 0) opts->simulate = SIM_GENERIC;
            else if (strcmp(mode, "fat") == 0) opts->simulate = SIM_FAT;
            else if (strcmp(mode, "filename") == 0) opts->simulate = SIM_FILENAME;
        } else if (is_flag(arg, "--export") && i + 1 < argc) {
            snprintf(opts->export_path, sizeof(opts->export_path), "%s", argv[++i]);
        } else if (arg[0] == '-') {
            fprintf(stderr, "Aviso: flag desconhecida: %s\n", arg);
        } else {
            snprintf(opts->input, sizeof(opts->input), "%s", arg);
        }
    }

    if (opts->input[0] == '\0') {
        snprintf(opts->input, sizeof(opts->input), ".");
    }

    return 0;
}

void cli_print_help(void) {
    printf("cartag - organizador offline para pendrive automotivo\n\n");
    printf("Uso: cartag <path-ou-url> [opcoes]\n");
    printf("  --keep-format\n");
    printf("  --convert-mp3\n");
    printf("  --group-by-format\n");
    printf("  --fix-tags\n");
    printf("  --dedupe\n");
    printf("  --normalize-volume\n");
    printf("  --strip-art\n");
    printf("  --resize-art <px>\n");
    printf("  --extract-art\n");
    printf("  --organize artist|album|flat\n");
    printf("  --simulate generic|fat|filename\n");
    printf("  --car-safe\n");
    printf("  --export <destino>\n");
}
