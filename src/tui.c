#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

#define FILE_ROWS 8
#define UTIL_ROWS 8
#define UTIL_COUNT 12

typedef struct {
    TrackList list;
    size_t selected;
    size_t scroll;
} PreviewState;

typedef enum { FOCUS_FILES = 0, FOCUS_UTILS = 1 } UiFocus;
typedef enum { KEY_NONE = 0, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_TAB, KEY_ENTER, KEY_ESC, KEY_CHAR } UiKey;
typedef struct { UiKey key; int ch; } KeyEvent;

#ifdef _WIN32
static HANDLE g_hout = NULL;
static WORD g_default_attr = 7;
#else
static struct termios g_orig_term;
static int g_raw_mode = 0;
#endif

static const char *k_utils[UTIL_COUNT] = {
    "Set Input Path/Drive",
    "Set Export Path",
    "Set YouTube URL as Input",
    "Install yt-dlp",
    "Download YouTube URL now",
    "Refresh Eligible List",
    "Toggle Car-Safe",
    "Toggle Dedupe",
    "Toggle Fix Tags",
    "Simulate Filename",
    "Run Pipeline",
    "Quit"
};

static void str_copy(char *dst, size_t dst_sz, const char *src) {
    size_t n;
    if (!dst || dst_sz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

#ifdef _WIN32
static void ui_win_init(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!g_hout) g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hout && GetConsoleScreenBufferInfo(g_hout, &csbi)) g_default_attr = csbi.wAttributes;
}

static void ui_clear_home(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD w = 0;
    DWORD cells;
    COORD home = {0, 0};
    ui_win_init();
    if (!g_hout || !GetConsoleScreenBufferInfo(g_hout, &csbi)) return;
    cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    FillConsoleOutputCharacterA(g_hout, ' ', cells, home, &w);
    FillConsoleOutputAttribute(g_hout, g_default_attr, cells, home, &w);
    SetConsoleCursorPosition(g_hout, home);
}

static void ui_set_attr(WORD attr) { ui_win_init(); if (g_hout) SetConsoleTextAttribute(g_hout, attr); }
static void ui_reset_attr(void) { ui_set_attr(g_default_attr); }
#else
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
    if (opts->input[0] == '\0' || downloader_is_url(opts->input)) { preview_free(pv); return; }
    if (fs_scan_audio(opts->input, &fresh) != 0) { preview_free(pv); return; }
    preview_free(pv);
    pv->list = fresh;
}

static void draw_line(const char *s) { printf("%-78.78s\n", s); }

static void draw_top(void) {
#ifdef _WIN32
    ui_clear_home();
    ui_set_attr(BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
    printf("\033[2J\033[H\033[44;37m");
#endif
    draw_line(" File  Options  View  Tree  Help                                   MS-DOS Shell ");
#ifdef _WIN32
    ui_set_attr(BACKGROUND_GREEN | BACKGROUND_BLUE);
#else
    printf("\033[46;30m");
#endif
    draw_line(" C:\\   [A:] [B:] [C:] [D:] [E:] [F:]                             Cartag UI   ");
#ifdef _WIN32
    ui_reset_attr();
#else
    printf("\033[0m");
#endif
}

static void draw_status(const char *msg) {
    char line[120];
    char h[16];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    if (tmv) strftime(h, sizeof(h), "%H:%M", tmv); else str_copy(h, sizeof(h), "--:--");
    snprintf(line, sizeof(line), " F10=Actions  TAB=Switch  ENTER=Select  ESC=Exit   %-24.24s %s ", msg ? msg : "", h);
#ifdef _WIN32
    ui_set_attr(BACKGROUND_GREEN | BACKGROUND_BLUE);
#else
    printf("\033[46;30m");
#endif
    draw_line(line);
#ifdef _WIN32
    ui_reset_attr();
#else
    printf("\033[0m");
#endif
}

static void draw_main(const CliOptions *opts, const PreviewState *pv, UiFocus focus, int util_sel, const char *msg) {
    char left[128], right[64], line[200];
    size_t i;

    draw_top();
    draw_line("+--------------------------------+-------------------------------------------+");
    draw_line("| Directory Tree                 | C:*.*                                      |");
    snprintf(line, sizeof(line), "| [DIR] %-25.25s | input: %-35.35s |", opts->input[0] ? opts->input : "(not set)", opts->input[0] ? opts->input : "(not set)");
    draw_line(line);

    for (i = 0; i < FILE_ROWS; ++i) {
        size_t idx = pv->scroll + i;
        snprintf(left, sizeof(left), "%c C:\\", (focus == FOCUS_FILES && idx == pv->selected) ? '>' : ' ');
        if (idx < pv->list.count) {
            const AudioTrack *t = &pv->list.tracks[idx];
            snprintf(right, sizeof(right), "%02zu %-20.20s %-4s", idx + 1, t->filename, audio_format_name(t->format));
        } else {
            str_copy(right, sizeof(right), "");
        }

        if (focus == FOCUS_FILES && idx == pv->selected) {
#ifdef _WIN32
            ui_set_attr(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
#else
            printf("\033[7m");
#endif
        }
        snprintf(line, sizeof(line), "| %-30.30s | %-41.41s |", left, right);
        draw_line(line);
        if (focus == FOCUS_FILES && idx == pv->selected) {
#ifdef _WIN32
            ui_reset_attr();
#else
            printf("\033[0m");
#endif
        }
    }

    draw_line("+--------------------------------+-------------------------------------------+");
    draw_line("| Disk Utilities                                                              |");
    for (i = 0; i < UTIL_ROWS; ++i) {
        size_t idx = (size_t)util_sel + i;
        if (idx >= UTIL_COUNT) idx = i;
        snprintf(line, sizeof(line), "%c %-72.72s", idx == (size_t)util_sel ? '>' : ' ', k_utils[idx]);
        if (focus == FOCUS_UTILS && idx == (size_t)util_sel) {
#ifdef _WIN32
            ui_set_attr(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
#else
            printf("\033[7m");
#endif
        }
        snprintf(left, sizeof(left), "| %-74.74s |", line);
        draw_line(left);
        if (focus == FOCUS_UTILS && idx == (size_t)util_sel) {
#ifdef _WIN32
            ui_reset_attr();
#else
            printf("\033[0m");
#endif
        }
    }
    draw_status(msg);
}

static KeyEvent read_key_event(void) {
    KeyEvent ev;
    memset(&ev, 0, sizeof(ev));
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 224) {
        int c2 = _getch();
        if (c2 == 72) ev.key = KEY_UP;
        else if (c2 == 80) ev.key = KEY_DOWN;
        else if (c2 == 75) ev.key = KEY_LEFT;
        else if (c2 == 77) ev.key = KEY_RIGHT;
        return ev;
    }
    if (c == 9) { ev.key = KEY_TAB; return ev; }
    if (c == 13) { ev.key = KEY_ENTER; return ev; }
    if (c == 27) { ev.key = KEY_ESC; return ev; }
    ev.key = KEY_CHAR; ev.ch = c; return ev;
#else
    unsigned char b[3];
    if (read(STDIN_FILENO, &b[0], 1) != 1) return ev;
    if (b[0] == '\t') { ev.key = KEY_TAB; return ev; }
    if (b[0] == '\n' || b[0] == '\r') { ev.key = KEY_ENTER; return ev; }
    if (b[0] == 27) {
        fd_set rfds; struct timeval tv; int rc;
        FD_ZERO(&rfds); FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0; tv.tv_usec = 30000;
        rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) { ev.key = KEY_ESC; return ev; }
        if (read(STDIN_FILENO, &b[1], 1) != 1 || read(STDIN_FILENO, &b[2], 1) != 1) { ev.key = KEY_ESC; return ev; }
        if (b[1] == '[' && b[2] == 'A') ev.key = KEY_UP;
        else if (b[1] == '[' && b[2] == 'B') ev.key = KEY_DOWN;
        else if (b[1] == '[' && b[2] == 'C') ev.key = KEY_RIGHT;
        else if (b[1] == '[' && b[2] == 'D') ev.key = KEY_LEFT;
        else ev.key = KEY_ESC;
        return ev;
    }
    ev.key = KEY_CHAR; ev.ch = b[0]; return ev;
#endif
}

static int prompt_line(const char *label, char *out, size_t out_sz) {
#ifndef _WIN32
    ui_disable_raw_mode();
#else
    ui_reset_attr();
#endif
    printf("\n%s", label);
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

static int do_utility(int idx, CliOptions *opts, PreviewState *pv, char *msg, size_t msg_sz) {
    char buf[CARTAG_PATH_MAX];
    char warn[256];

    switch (idx) {
        case 0:
            if (prompt_line("Input path/drive: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->input, sizeof(opts->input), buf);
                preview_load(opts, pv);
                str_copy(msg, msg_sz, "Input atualizado.");
            }
            break;
        case 1:
            if (prompt_line("Export path: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->export_path, sizeof(opts->export_path), buf);
                str_copy(msg, msg_sz, "Export atualizado.");
            }
            break;
        case 2:
            if (prompt_line("YouTube URL: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->input, sizeof(opts->input), buf);
                str_copy(msg, msg_sz, "URL setada como input.");
            }
            break;
        case 3:
            if (downloader_install(warn, sizeof(warn)) == 0) str_copy(msg, msg_sz, "yt-dlp pronto para uso.");
            else str_copy(msg, msg_sz, warn);
            break;
        case 4:
            if (!downloader_is_url(opts->input)) {
                str_copy(msg, msg_sz, "Input atual nao e URL.");
            } else if (downloader_fetch_audio(opts->input, ".cartag/downloads", warn, sizeof(warn)) == 0) {
                str_copy(opts->input, sizeof(opts->input), ".cartag/downloads");
                preview_load(opts, pv);
                str_copy(msg, msg_sz, "Download concluido e lista atualizada.");
            } else {
                str_copy(msg, msg_sz, warn);
            }
            break;
        case 5:
            preview_load(opts, pv);
            str_copy(msg, msg_sz, "Lista atualizada.");
            break;
        case 6:
            opts->car_safe = !opts->car_safe;
            if (opts->car_safe) {
                opts->convert_mp3 = 1; opts->fix_tags = 1; opts->strip_art = 1;
                opts->limit_name = 1; opts->prefix = 1;
                if (opts->organize == ORG_NONE) opts->organize = ORG_ARTIST;
            }
            str_copy(msg, msg_sz, opts->car_safe ? "Car-safe ON" : "Car-safe OFF");
            break;
        case 7: opts->dedupe = !opts->dedupe; str_copy(msg, msg_sz, opts->dedupe ? "Dedupe ON" : "Dedupe OFF"); break;
        case 8: opts->fix_tags = !opts->fix_tags; str_copy(msg, msg_sz, opts->fix_tags ? "Fix tags ON" : "Fix tags OFF"); break;
        case 9: opts->simulate = SIM_FILENAME; str_copy(msg, msg_sz, "Simulate filename ON"); break;
        case 10: if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), "."); return 1;
        case 11: return -1;
        default: break;
    }
    return 0;
}

static int tui_run_fallback(CliOptions *opts) {
    char cmd[64], buf[CARTAG_PATH_MAX], msg[128];
    PreviewState pv;
    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "fallback mode");

    for (;;) {
        printf("\n[CARTAG DOS fallback] input=%s export=%s msg=%s\n", opts->input[0] ? opts->input : "(not set)", opts->export_path[0] ? opts->export_path : "(not set)", msg);
        printf("d=input e=export y=url i=install l=list r=run q=quit > ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (cmd[0] == 'q') break;
        if (cmd[0] == 'd') { printf("Input: "); if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->input, sizeof(opts->input), buf); } }
        else if (cmd[0] == 'e') { printf("Export: "); if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->export_path, sizeof(opts->export_path), buf); } }
        else if (cmd[0] == 'y') { printf("URL: "); if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->input, sizeof(opts->input), buf); } }
        else if (cmd[0] == 'i') { do_utility(3, opts, &pv, msg, sizeof(msg)); }
        else if (cmd[0] == 'l') { preview_load(opts, &pv); printf("Eligible tracks: %zu\n", pv.list.count); }
        else if (cmd[0] == 'r') { preview_free(&pv); if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), "."); return 0; }
    }
    preview_free(&pv);
    return -1;
}

int tui_run(CliOptions *opts) {
    PreviewState pv;
    UiFocus focus = FOCUS_UTILS;
    int util_sel = 0;
    char msg[128];

    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "Selecione Run Pipeline para executar.");

#ifndef _WIN32
    if (!isatty(STDIN_FILENO)) return tui_run_fallback(opts);
    if (ui_enable_raw_mode() != 0) return tui_run_fallback(opts);
#endif

    preview_load(opts, &pv);

    for (;;) {
        KeyEvent ev;
        int rc;

        draw_main(opts, &pv, focus, util_sel, msg);
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
            if (ev.key == KEY_UP && pv.selected > 0) pv.selected--;
            if (ev.key == KEY_DOWN && pv.selected + 1 < pv.list.count) pv.selected++;
            if (pv.selected < pv.scroll) pv.scroll = pv.selected;
            if (pv.selected >= pv.scroll + FILE_ROWS) pv.scroll = pv.selected - FILE_ROWS + 1;
            if (ev.key == KEY_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
                rc = do_utility(10, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
#ifndef _WIN32
                    ui_disable_raw_mode();
#endif
                    preview_free(&pv);
                    return 0;
                }
            }
        } else {
            if (ev.key == KEY_UP && util_sel > 0) util_sel--;
            else if (ev.key == KEY_DOWN && util_sel + 1 < UTIL_COUNT) util_sel++;
            else if (ev.key == KEY_ENTER || ev.key == KEY_CHAR) {
                if (ev.key == KEY_CHAR && (ev.ch == 'q' || ev.ch == 'Q')) util_sel = 11;
                if (ev.key == KEY_CHAR && (ev.ch == 'r' || ev.ch == 'R')) util_sel = 10;
                rc = do_utility(util_sel, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
#ifndef _WIN32
                    ui_disable_raw_mode();
#endif
                    preview_free(&pv);
                    return 0;
                }
                if (rc < 0) {
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
