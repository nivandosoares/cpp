#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PREVIEW_MAX 24

typedef struct {
    AudioTrack rows[PREVIEW_MAX];
    size_t total;
    size_t shown;
    size_t page;
} PreviewState;

static void format_clock(char *buf, size_t sz) {
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    if (!tmv) {
        snprintf(buf, sz, "--:--");
        return;
    }
    strftime(buf, sz, "%H:%M", tmv);
}

static void draw_line(const char *s) {
    printf("%-78s\n", s);
}

static void load_preview(const CliOptions *opts, PreviewState *pv) {
    TrackList list;
    memset(pv, 0, sizeof(*pv));

    if (opts->input[0] == '\0' || downloader_is_url(opts->input)) return;
    if (fs_scan_audio(opts->input, &list) != 0) return;

    pv->total = list.count;
    for (size_t i = 0; i < list.count && i < PREVIEW_MAX; ++i) {
        pv->rows[pv->shown++] = list.tracks[i];
    }

    free(list.tracks);
}

static void draw_header(void) {
    printf("\033[2J\033[H");
    printf("\033[44;37m");
    draw_line(" File  Options  View  Tree  Help                                   MS-DOS Shell ");
    printf("\033[46;30m");
    draw_line(" C:\\                                                                  [Cartag DOS UI] ");
    printf("\033[0m");
}

static void draw_panels(const CliOptions *opts, const PreviewState *pv) {
    char line[256];
    char clockbuf[16];
    size_t start = pv->page * PREVIEW_MAX;
    size_t end = start + PREVIEW_MAX;
    if (end > pv->total) end = pv->total;

    draw_line("+------------------------------ Directory Tree ------------------------------+");
    snprintf(line, sizeof(line), "| Input: %-67s |", opts->input[0] ? opts->input : "(not set)");
    draw_line(line);
    snprintf(line, sizeof(line), "| Export: %-66s |", opts->export_path[0] ? opts->export_path : "(not set)");
    draw_line(line);
    draw_line("+------------------------------ Eligible Files -------------------------------+");

    if (opts->input[0] == '\0') {
        draw_line("| Define uma unidade/pasta primeiro (tecla D).                               |");
    } else if (downloader_is_url(opts->input)) {
        draw_line("| Entrada URL detectada: listagem sera apos download yt-dlp.                 |");
    } else if (pv->total == 0) {
        draw_line("| Nenhum arquivo elegivel encontrado (mp3/flac/wav/aac/m4a/ogg/wma).         |");
    } else {
        for (size_t i = start; i < end; ++i) {
            size_t idx = i - start;
            snprintf(line, sizeof(line), "| %02zu %-35.35s %-7s %10llu bytes          |",
                     i + 1,
                     pv->rows[idx].filename,
                     audio_format_name(pv->rows[idx].format),
                     (unsigned long long)pv->rows[idx].size_bytes);
            draw_line(line);
        }
    }

    draw_line("+------------------------------ Disk Utilities -------------------------------+");
    snprintf(line, sizeof(line), "| [D] Drive/Input  [E] Export Path  [L] List Eligible  [C] Toggle Car-Safe  |");
    draw_line(line);
    snprintf(line, sizeof(line), "| [G] Toggle Dedupe [T] Toggle FixTags [M] Sim Filename  [O] Organize Artist|");
    draw_line(line);
    snprintf(line, sizeof(line), "| [R] Run pipeline   [N/P] Next/Prev page   [Q] Quit                          |");
    draw_line(line);

    format_clock(clockbuf, sizeof(clockbuf));
    printf("\033[46;30m");
    snprintf(line, sizeof(line), " F10=Actions  Shift+F9=Command Prompt                      %s ", clockbuf);
    draw_line(line);
    printf("\033[0m");
}

static void prompt_text(const char *label, char *out, size_t out_sz) {
    printf("%s", label);
    fflush(stdout);
    if (fgets(out, (int)out_sz, stdin)) {
        out[strcspn(out, "\r\n")] = '\0';
    }
}

int tui_run(CliOptions *opts) {
    char cmd[64];
    char buf[CARTAG_PATH_MAX];
    PreviewState pv;

    memset(&pv, 0, sizeof(pv));

    for (;;) {
        draw_header();
        draw_panels(opts, &pv);
        printf("Command> ");
        fflush(stdout);

        if (!fgets(cmd, sizeof(cmd), stdin)) return -1;
        if (cmd[0] == 'q' || cmd[0] == 'Q') return -1;

        if (cmd[0] == 'd' || cmd[0] == 'D') {
            prompt_text("Drive/Pasta (ex.: C:\\Musicas ou /media/usb): ", buf, sizeof(buf));
            if (buf[0]) snprintf(opts->input, sizeof(opts->input), "%s", buf);
            pv.page = 0;
            load_preview(opts, &pv);
        } else if (cmd[0] == 'e' || cmd[0] == 'E') {
            prompt_text("Destino export (ex.: E:\\): ", buf, sizeof(buf));
            if (buf[0]) snprintf(opts->export_path, sizeof(opts->export_path), "%s", buf);
        } else if (cmd[0] == 'l' || cmd[0] == 'L') {
            pv.page = 0;
            load_preview(opts, &pv);
        } else if (cmd[0] == 'n' || cmd[0] == 'N') {
            if ((pv.page + 1) * PREVIEW_MAX < pv.total) pv.page++;
        } else if (cmd[0] == 'p' || cmd[0] == 'P') {
            if (pv.page > 0) pv.page--;
        } else if (cmd[0] == 'c' || cmd[0] == 'C') {
            opts->car_safe = !opts->car_safe;
            if (opts->car_safe) {
                opts->convert_mp3 = 1;
                opts->fix_tags = 1;
                opts->strip_art = 1;
                opts->limit_name = 1;
                opts->prefix = 1;
                opts->organize = ORG_ARTIST;
            }
        } else if (cmd[0] == 'g' || cmd[0] == 'G') {
            opts->dedupe = !opts->dedupe;
        } else if (cmd[0] == 't' || cmd[0] == 'T') {
            opts->fix_tags = !opts->fix_tags;
        } else if (cmd[0] == 'm' || cmd[0] == 'M') {
            opts->simulate = SIM_FILENAME;
        } else if (cmd[0] == 'o' || cmd[0] == 'O') {
            opts->organize = ORG_ARTIST;
        } else if (cmd[0] == 'r' || cmd[0] == 'R') {
            if (opts->input[0] == '\0') snprintf(opts->input, sizeof(opts->input), ".");
            return 0;
        }
    }
}
