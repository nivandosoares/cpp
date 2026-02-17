#include "cartag.h"

#include <stdio.h>
#include <string.h>

static void draw_screen(const CliOptions *opts) {
    printf("\033[2J\033[H");
    printf("+----------------------------------------------------------------------------+\n");
    printf("| CARTAG RETRO MODE                                                          |\n");
    printf("| [F1] Car-Safe  [F2] Dedupe  [F3] Fix-Tags  [F4] Simulate  [F5] Export     |\n");
    printf("+----------------------------------------------------------------------------+\n");
    printf("|     .-=========-.                                                          |\n");
    printf("|     \\'-=======-'/     ____              ____               __              |\n");
    printf("|     _|   .=.   |_    / ___|__ _ _ __   / ___|__ _ _ __   / _|             |\n");
    printf("|    ((|  {{1}}  |))  | |   / _` | '__| | |   / _` | '_ \\ | |_              |\n");
    printf("|     \\|   /|\\   |/   | |__| (_| | |    | |__| (_| | | | ||  _|             |\n");
    printf("|      \\__ '`' __/     \\____\\__,_|_|     \\____\\__,_|_| |_||_|              |\n");
    printf("|    _`) )_ _(`_                                                           🚗|\n");
    printf("|  _/_______\\_                                                             ||\n");
    printf("| /___________\\                                                            ||\n");
    printf("+----------------------------------------------------------------------------+\n");
    printf("Entrada: %s\n", opts->input[0] ? opts->input : "(nao definida)");
    printf("Export : %s\n", opts->export_path[0] ? opts->export_path : "(nao definido)");
    printf("Flags  : car-safe=%s dedupe=%s fix-tags=%s convert-mp3=%s\n",
           opts->car_safe ? "on" : "off",
           opts->dedupe ? "on" : "off",
           opts->fix_tags ? "on" : "off",
           opts->convert_mp3 ? "on" : "off");
    printf("\nMenu:\n");
    printf("  1) Definir entrada (pasta ou URL)\n");
    printf("  2) Definir exportacao USB\n");
    printf("  3) Toggle Car-Safe\n");
    printf("  4) Toggle Dedupe\n");
    printf("  5) Toggle Fix-Tags\n");
    printf("  6) Toggle Convert-MP3\n");
    printf("  7) Simulacao por nome\n");
    printf("  8) Organizar por artista\n");
    printf("  9) Iniciar pipeline\n");
    printf("  q) Sair\n");
    printf("\nEscolha: ");
    fflush(stdout);
}

int tui_run(CliOptions *opts) {
    char line[128];
    char buf[CARTAG_PATH_MAX];
    for (;;) {
        draw_screen(opts);
        if (!fgets(line, sizeof(line), stdin)) return -1;
        if (line[0] == 'q' || line[0] == 'Q') return -1;

        if (line[0] == '1') {
            printf("Entrada: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                snprintf(opts->input, sizeof(opts->input), "%s", buf);
            }
        } else if (line[0] == '2') {
            printf("Exportacao USB: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                snprintf(opts->export_path, sizeof(opts->export_path), "%s", buf);
            }
        } else if (line[0] == '3') {
            opts->car_safe = !opts->car_safe;
            if (opts->car_safe) {
                opts->convert_mp3 = 1;
                opts->fix_tags = 1;
                opts->strip_art = 1;
                opts->limit_name = 1;
                opts->prefix = 1;
                opts->organize = ORG_ARTIST;
            }
        } else if (line[0] == '4') {
            opts->dedupe = !opts->dedupe;
        } else if (line[0] == '5') {
            opts->fix_tags = !opts->fix_tags;
        } else if (line[0] == '6') {
            opts->convert_mp3 = !opts->convert_mp3;
        } else if (line[0] == '7') {
            opts->simulate = SIM_FILENAME;
        } else if (line[0] == '8') {
            opts->organize = ORG_ARTIST;
        } else if (line[0] == '9') {
            if (opts->input[0] == '\0') {
                snprintf(opts->input, sizeof(opts->input), ".");
            }
            return 0;
        }
    }
}
