#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

#define UI_W 78
#define FILE_ROWS 10
#define UTIL_COUNT 10

typedef struct {
    TrackList list;
    size_t selected;
    size_t scroll;
} PreviewState;

typedef enum {
    FOCUS_FILES = 0,
    FOCUS_UTILS = 1
} UiFocus;

typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_TAB,
    KEY_ENTER,
    KEY_ESC,
    KEY_CHAR
} UiKey;

typedef struct {
    UiKey key;
    int ch;
} KeyEvent;

#ifndef _WIN32
static struct termios g_orig_term;
static int g_raw_mode = 0;
#endif

static const char *k_utils[UTIL_COUNT] = {
    "Set Input Path/Drive",
    "Set Export Path",
    "Refresh Eligible List",
    "Toggle Car-Safe",
    "Toggle Dedupe",
    "Toggle Fix Tags",
    "Simulate Filename",
    "Organize by Artist",
    "Run Pipeline",
    "Quit"
};

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

#ifndef _WIN32
static int ui_enable_raw_mode(void) {
    struct termios raw;
    if (!isatty(STDIN_FILENO)) return 0;
    if (tcgetattr(STDIN_FILENO, &g_orig_term) != 0) return -1;
    raw = g_orig_term;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    g_raw_mode = 1;
    return 0;
}

static void ui_disable_raw_mode(void) {
    if (g_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_term);
        g_raw_mode = 0;
    }
}
#endif

static void preview_free(PreviewState *pv) {
    free(pv->list.tracks);
    memset(pv, 0, sizeof(*pv));
}

static void preview_load(const CliOptions *opts, PreviewState *pv) {
    TrackList fresh;
    memset(&fresh, 0, sizeof(fresh));

    if (opts->input[0] == '\0' || downloader_is_url(opts->input)) {
        preview_free(pv);
        return;
    }

    if (fs_scan_audio(opts->input, &fresh) != 0) {
        preview_free(pv);
        return;
    }

    preview_free(pv);
    pv->list = fresh;
    pv->selected = 0;
    pv->scroll = 0;
}

static void format_clock(char *buf, size_t sz) {
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    if (!tmv) {
        str_copy(buf, sz, "--:--");
        return;
    }
    strftime(buf, sz, "%H:%M", tmv);
}

static void draw_line_plain(const char *s) {
    printf("%-78.78s\n", s);
}

static void draw_status_bar(void) {
    char line[128];
    char clockbuf[16];
    format_clock(clockbuf, sizeof(clockbuf));
    snprintf(line, sizeof(line), " F10=Actions  TAB=Switch Pane  ENTER=Select  ESC=Quit      %s ", clockbuf);
    printf("\033[46;30m");
    draw_line_plain(line);
    printf("\033[0m");
}

static void draw_ui(const CliOptions *opts, const PreviewState *pv, UiFocus focus, int util_sel, const char *msg) {
    char line[256];
    size_t i;

    printf("\033[2J\033[H");
    printf("\033[44;37m");
    draw_line_plain(" File  Options  View  Tree  Help                                   MS-DOS Shell ");
    printf("\033[46;30m");
    draw_line_plain(" C:\\                                                                  [Cartag DOS UI] ");
    printf("\033[0m");

    draw_line_plain("+----------------------------- Directory Tree -------------------------------+");
    snprintf(line, sizeof(line), "| Input : %-67.67s |", opts->input[0] ? opts->input : "(not set)");
    draw_line_plain(line);
    snprintf(line, sizeof(line), "| Export: %-67.67s |", opts->export_path[0] ? opts->export_path : "(not set)");
    draw_line_plain(line);
    snprintf(line, sizeof(line), "| Flags : car-safe=%-3s dedupe=%-3s fix-tags=%-3s simulate=%-8s          |",
             opts->car_safe ? "on" : "off",
             opts->dedupe ? "on" : "off",
             opts->fix_tags ? "on" : "off",
             opts->simulate == SIM_FILENAME ? "filename" : "none");
    draw_line_plain(line);

    draw_line_plain("+-------------------------- Eligible Files (Pane 1) ------------------------+");
    if (opts->input[0] == '\0') {
        draw_line_plain("| Set input first (select utility: Set Input Path/Drive).                  |");
        for (i = 1; i < FILE_ROWS; ++i) draw_line_plain("|                                                                            |");
    } else if (downloader_is_url(opts->input)) {
        draw_line_plain("| URL mode: files will appear here after yt-dlp download.                  |");
        for (i = 1; i < FILE_ROWS; ++i) draw_line_plain("|                                                                            |");
    } else if (pv->list.count == 0) {
        draw_line_plain("| No eligible files found (mp3/flac/wav/aac/m4a/ogg/wma).                  |");
        for (i = 1; i < FILE_ROWS; ++i) draw_line_plain("|                                                                            |");
    } else {
        for (i = 0; i < FILE_ROWS; ++i) {
            size_t idx = pv->scroll + i;
            if (idx < pv->list.count) {
                const AudioTrack *t = &pv->list.tracks[idx];
                char row[160];
                snprintf(row, sizeof(row), "%c %4zu %-31.31s %-6s %10llu",
                         idx == pv->selected ? '>' : ' ',
                         idx + 1,
                         t->filename,
                         audio_format_name(t->format),
                         (unsigned long long)t->size_bytes);

                if (focus == FOCUS_FILES && idx == pv->selected) printf("\033[7m");
                snprintf(line, sizeof(line), "| %-74.74s |", row);
                draw_line_plain(line);
                if (focus == FOCUS_FILES && idx == pv->selected) printf("\033[0m");
            } else {
                draw_line_plain("|                                                                            |");
            }
        }
    }

    draw_line_plain("+-------------------------- Disk Utilities (Pane 2) ------------------------+");
    for (i = 0; i < UTIL_COUNT; ++i) {
        char row[96];
        snprintf(row, sizeof(row), "%c %-45.45s", i == (size_t)util_sel ? '>' : ' ', k_utils[i]);
        if (focus == FOCUS_UTILS && i == (size_t)util_sel) printf("\033[7m");
        snprintf(line, sizeof(line), "| %-74.74s |", row);
        draw_line_plain(line);
        if (focus == FOCUS_UTILS && i == (size_t)util_sel) printf("\033[0m");
    }

    snprintf(line, sizeof(line), " Message: %-65.65s", msg ? msg : "Use arrows, TAB and ENTER.");
    draw_line_plain(line);
    draw_status_bar();
}

static KeyEvent read_key_event(void) {
    KeyEvent ev;
    int c;
    memset(&ev, 0, sizeof(ev));

#ifdef _WIN32
    c = _getch();
    if (c == 0 || c == 224) {
        int c2 = _getch();
        if (c2 == 72) ev.key = KEY_UP;
        else if (c2 == 80) ev.key = KEY_DOWN;
        else if (c2 == 75) ev.key = KEY_LEFT;
        else if (c2 == 77) ev.key = KEY_RIGHT;
        else ev.key = KEY_NONE;
        return ev;
    }
    if (c == 9) { ev.key = KEY_TAB; return ev; }
    if (c == 13) { ev.key = KEY_ENTER; return ev; }
    if (c == 27) { ev.key = KEY_ESC; return ev; }
    ev.key = KEY_CHAR; ev.ch = c; return ev;
#else
    unsigned char b[3];
    ssize_t n0 = read(STDIN_FILENO, &b[0], 1);
    if (n0 != 1) return ev;
    c = b[0];
    if (c == '\t') { ev.key = KEY_TAB; return ev; }
    if (c == '\n' || c == '\r') { ev.key = KEY_ENTER; return ev; }
    if (c == 27) {
        fd_set rfds;
        struct timeval tv;
        int rc;

        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 30000;
        rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) { ev.key = KEY_ESC; return ev; }

        if (read(STDIN_FILENO, &b[1], 1) != 1) { ev.key = KEY_ESC; return ev; }
        if (read(STDIN_FILENO, &b[2], 1) != 1) { ev.key = KEY_ESC; return ev; }
        if (b[1] == '[') {
            if (b[2] == 'A') ev.key = KEY_UP;
            else if (b[2] == 'B') ev.key = KEY_DOWN;
            else if (b[2] == 'C') ev.key = KEY_RIGHT;
            else if (b[2] == 'D') ev.key = KEY_LEFT;
            else ev.key = KEY_ESC;
            return ev;
        }
        ev.key = KEY_ESC;
        return ev;
    }
    ev.key = KEY_CHAR;
    ev.ch = c;
    return ev;
#endif
}

static int prompt_line(const char *label, char *out, size_t out_sz) {
#ifndef _WIN32
    ui_disable_raw_mode();
#endif
    printf("\033[0m\n%s", label);
    fflush(stdout);
    if (!fgets(out, (int)out_sz, stdin)) {
#ifndef _WIN32
        ui_enable_raw_mode();
#endif
        return -1;
    }
    out[strcspn(out, "\r\n")] = '\0';
#ifndef _WIN32
    ui_enable_raw_mode();
#endif
    return 0;
}

static int execute_utility(int util_sel, CliOptions *opts, PreviewState *pv, char *msg, size_t msg_sz) {
    char buf[CARTAG_PATH_MAX];
    switch (util_sel) {
        case 0:
            if (prompt_line("Input path/drive: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->input, sizeof(opts->input), buf);
                preview_load(opts, pv);
                str_copy(msg, msg_sz, "Input atualizado.");
            }
            break;
        case 1:
            if (prompt_line("Export path (ex.: E:\\): ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->export_path, sizeof(opts->export_path), buf);
                str_copy(msg, msg_sz, "Destino de exportacao atualizado.");
            }
            break;
        case 2:
            preview_load(opts, pv);
            str_copy(msg, msg_sz, "Lista elegivel atualizada.");
            break;
        case 3:
            opts->car_safe = !opts->car_safe;
            if (opts->car_safe) {
                opts->convert_mp3 = 1;
                opts->fix_tags = 1;
                opts->strip_art = 1;
                opts->limit_name = 1;
                opts->prefix = 1;
                if (opts->organize == ORG_NONE) opts->organize = ORG_ARTIST;
            }
            str_copy(msg, msg_sz, opts->car_safe ? "Car-safe habilitado." : "Car-safe desabilitado.");
            break;
        case 4:
            opts->dedupe = !opts->dedupe;
            str_copy(msg, msg_sz, opts->dedupe ? "Dedupe habilitado." : "Dedupe desabilitado.");
            break;
        case 5:
            opts->fix_tags = !opts->fix_tags;
            str_copy(msg, msg_sz, opts->fix_tags ? "Fix-tags habilitado." : "Fix-tags desabilitado.");
            break;
        case 6:
            opts->simulate = SIM_FILENAME;
            str_copy(msg, msg_sz, "Simulacao filename habilitada.");
            break;
        case 7:
            opts->organize = ORG_ARTIST;
            str_copy(msg, msg_sz, "Organizacao por artista selecionada.");
            break;
        case 8:
            if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), ".");
            return 1;
        case 9:
            return -1;
        default:
            break;
    }
    return 0;
}

static int tui_run_fallback(CliOptions *opts) {
    char cmd[64];
    char buf[CARTAG_PATH_MAX];
    PreviewState pv;
    memset(&pv, 0, sizeof(pv));

    for (;;) {
        printf("\n[CARTAG DOS fallback] input=%s export=%s\n",
               opts->input[0] ? opts->input : "(not set)",
               opts->export_path[0] ? opts->export_path : "(not set)");
        printf("d=set input, e=set export, l=list, r=run, q=quit > ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (cmd[0] == 'q') break;
        if (cmd[0] == 'd') {
            printf("Input: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                str_copy(opts->input, sizeof(opts->input), buf);
            }
        } else if (cmd[0] == 'e') {
            printf("Export: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                str_copy(opts->export_path, sizeof(opts->export_path), buf);
            }
        } else if (cmd[0] == 'l') {
            preview_load(opts, &pv);
            printf("Eligible tracks: %zu\n", pv.list.count);
        } else if (cmd[0] == 'r') {
            preview_free(&pv);
            if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), ".");
            return 0;
        }
    }
    preview_free(&pv);
    return -1;
}

int tui_run(CliOptions *opts) {
    PreviewState pv;
    UiFocus focus = FOCUS_FILES;
    int util_sel = 0;
    char msg[128];

    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "TAB alterna foco. ENTER executa. ESC sai.");

#ifndef _WIN32
    if (!isatty(STDIN_FILENO)) {
        return tui_run_fallback(opts);
    }
    if (ui_enable_raw_mode() != 0) {
        return tui_run_fallback(opts);
    }
#endif

    preview_load(opts, &pv);

    for (;;) {
        KeyEvent ev;
        int act;

        draw_ui(opts, &pv, focus, util_sel, msg);
        ev = read_key_event();

        if (ev.key == KEY_ESC) {
#ifndef _WIN32
            ui_disable_raw_mode();
#endif
            preview_free(&pv);
            return -1;
        }

        if (ev.key == KEY_TAB || ev.key == KEY_LEFT || ev.key == KEY_RIGHT) {
            focus = (focus == FOCUS_FILES) ? FOCUS_UTILS : FOCUS_FILES;
            continue;
        }

        if (focus == FOCUS_FILES) {
            if (ev.key == KEY_UP) {
                if (pv.selected > 0) pv.selected--;
                if (pv.selected < pv.scroll) pv.scroll = pv.selected;
            } else if (ev.key == KEY_DOWN) {
                if (pv.selected + 1 < pv.list.count) pv.selected++;
                if (pv.selected >= pv.scroll + FILE_ROWS) pv.scroll = pv.selected - FILE_ROWS + 1;
            } else if (ev.key == KEY_CHAR) {
                if (ev.ch == 'r' || ev.ch == 'R') {
                    act = execute_utility(8, opts, &pv, msg, sizeof(msg));
                    if (act == 1) {
#ifndef _WIN32
                        ui_disable_raw_mode();
#endif
                        preview_free(&pv);
                        return 0;
                    }
                }
            }
        } else {
            if (ev.key == KEY_UP) {
                if (util_sel > 0) util_sel--;
            } else if (ev.key == KEY_DOWN) {
                if (util_sel + 1 < UTIL_COUNT) util_sel++;
            } else if (ev.key == KEY_ENTER || ev.key == KEY_CHAR) {
                if (ev.key == KEY_CHAR && (ev.ch == 'q' || ev.ch == 'Q')) util_sel = 9;
                if (ev.key == KEY_CHAR && (ev.ch == 'r' || ev.ch == 'R')) util_sel = 8;
                act = execute_utility(util_sel, opts, &pv, msg, sizeof(msg));
                if (act == 1) {
#ifndef _WIN32
                    ui_disable_raw_mode();
#endif
                    preview_free(&pv);
                    return 0;
                }
                if (act < 0) {
#ifndef _WIN32
                    ui_disable_raw_mode();
#endif
                    preview_free(&pv);
                    return -1;
                }
            }
        }
    }
}
