#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <ncurses.h>
#endif

#define UTIL_COUNT 13

#ifndef _WIN32
typedef struct {
    TrackList list;
    size_t selected;
    size_t scroll;
} PreviewState;

typedef enum { FOCUS_FILES = 0, FOCUS_UTILS = 1 } UiFocus;
typedef enum { TAB_FILE = 0, TAB_OPTIONS, TAB_VIEW, TAB_TREE, TAB_HELP, TAB_COUNT } UiTab;

static const char *k_tabs[TAB_COUNT] = {"File", "Options", "View", "Tree", "Help"};
static const char *k_submenus[TAB_COUNT] = {
    "Open  Save  Export  Exit",
    "Car-Safe  Dedupe  Fix-Tags  Organize",
    "Simulate  Refresh  Stats",
    "Genre->Artist  Artist  Album  Flat",
    "Keys  About  Tips"
};

static const char *k_utils[UTIL_COUNT] = {
    "Set Input Path/Drive",
    "Set Export Path",
    "Set YouTube URL as Input",
    "Install yt-dlp (curl)",
    "Download YouTube URL now",
    "Refresh Eligible List",
    "Toggle Car-Safe",
    "Toggle Dedupe",
    "Toggle Fix Tags",
    "Simulate Filename",
    "Organize: Genre/Artist",
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
}

static void init_colors_if_possible(void) {
    if (!has_colors()) return;
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_CYAN);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
}

static int prompt_line(const char *label, char *out, size_t out_sz) {
    int ch;
    int y = LINES - 2;
    move(y, 0);
    clrtoeol();
    attron(COLOR_PAIR(3));
    mvprintw(y, 0, "%s", label);
    attroff(COLOR_PAIR(3));
    echo();
    curs_set(1);
    ch = getnstr(out, (int)out_sz - 1);
    noecho();
    curs_set(0);
    if (ch == ERR) return -1;
    return 0;
}

static void draw_ui(const CliOptions *opts,
                    const PreviewState *pv,
                    UiFocus focus,
                    int util_sel,
                    int util_scroll,
                    UiTab tab_sel,
                    const char *msg) {
    int w = COLS;
    int top = 0;
    int tree_top = 4;
    int util_top;
    int left_w;
    int right_w;
    int file_rows;
    int i;
    char line[256];
    char timebuf[16];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);

    erase();

    attron(COLOR_PAIR(1));
    mvhline(top, 0, ' ', w);
    mvprintw(top, 1, "%s  %s  %s  %s  %s",
             tab_sel == TAB_FILE ? "[File]" : k_tabs[TAB_FILE],
             tab_sel == TAB_OPTIONS ? "[Options]" : k_tabs[TAB_OPTIONS],
             tab_sel == TAB_VIEW ? "[View]" : k_tabs[TAB_VIEW],
             tab_sel == TAB_TREE ? "[Tree]" : k_tabs[TAB_TREE],
             tab_sel == TAB_HELP ? "[Help]" : k_tabs[TAB_HELP]);
    mvprintw(top, w - 18, "MS-DOS Shell");
    attroff(COLOR_PAIR(1));

    attron(COLOR_PAIR(2));
    mvhline(1, 0, ' ', w);
    mvprintw(1, 1, "C:\\ [A:] [B:] [C:] [D:] [E:] [F:] [USB:]");
    mvprintw(1, w - 12, "Cartag UI");
    mvhline(2, 0, ' ', w);
    mvprintw(2, 1, "%s", k_submenus[(int)tab_sel]);
    attroff(COLOR_PAIR(2));

    left_w = w / 2 - 1;
    right_w = w - left_w - 2;
    util_top = tree_top + 12;
    file_rows = util_top - tree_top - 3;

    mvprintw(3, 0, "+"); mvhline(3, 1, '-', w - 2); mvprintw(3, w - 1, "+");
    mvprintw(4, 0, "| Directory Tree");
    snprintf(line, sizeof(line), "| input: %-.30s | export: %-.20s |", opts->input[0] ? opts->input : "(not set)", opts->export_path[0] ? opts->export_path : "(not set)");
    mvprintw(4, left_w + 1, "%-*.*s", right_w, right_w, line);
    mvprintw(5, 0, "| [DIR] %-.20s", opts->input[0] ? opts->input : "C:\\");
    snprintf(line, sizeof(line), "| Genre->Artist=%s", opts->organize == ORG_GENRE_ARTIST ? "ON" : "OFF");
    mvprintw(5, left_w + 1, "%-*.*s", right_w, right_w, line);

    for (i = 0; i < file_rows; ++i) {
        size_t idx = pv->scroll + (size_t)i;
        mvprintw(tree_top + 2 + i, 0, "| %-*.*s|", left_w, left_w, "");
        mvprintw(tree_top + 2 + i, left_w + 1, " %-*.*s|", right_w - 1, right_w - 1, "");

        if (idx < pv->list.count) {
            const AudioTrack *t = &pv->list.tracks[idx];
            snprintf(line, sizeof(line), "%c %03zu %-24.24s %-4s %8llu",
                     idx == pv->selected ? '>' : ' ',
                     idx + 1,
                     t->filename,
                     audio_format_name(t->format),
                     (unsigned long long)t->size_bytes);
            if (focus == FOCUS_FILES && idx == pv->selected) attron(A_REVERSE);
            mvprintw(tree_top + 2 + i, left_w + 2, "%-*.*s", right_w - 2, right_w - 2, line);
            if (focus == FOCUS_FILES && idx == pv->selected) attroff(A_REVERSE);
        }
    }

    mvprintw(util_top - 1, 0, "+"); mvhline(util_top - 1, 1, '-', w - 2); mvprintw(util_top - 1, w - 1, "+");
    mvprintw(util_top, 0, "| Disk Utilities");
    mvhline(util_top, 15, ' ', w - 16);

    for (i = 0; i < 8; ++i) {
        int idx = util_scroll + i;
        const char *txt = (idx < UTIL_COUNT) ? k_utils[idx] : "";
        if (focus == FOCUS_UTILS && idx == util_sel) attron(A_REVERSE);
        mvprintw(util_top + 1 + i, 0, "| %c %-*.*s|", idx == util_sel ? '>' : ' ', w - 5, w - 5, txt);
        if (focus == FOCUS_UTILS && idx == util_sel) attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(2));
    mvhline(LINES - 1, 0, ' ', w);
    if (tmv) strftime(timebuf, sizeof(timebuf), "%H:%M", tmv); else str_copy(timebuf, sizeof(timebuf), "--:--");
    mvprintw(LINES - 1, 1, "F10=Actions TAB=Switch <-/->=Tabs ENTER=Select ESC=Exit");
    mvprintw(LINES - 1, w - 6, "%5s", timebuf);
    attroff(COLOR_PAIR(2));

    mvprintw(LINES - 2, 0, "Message: %-*.*s", w - 10, w - 10, msg ? msg : "");

    refresh();
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
            if (downloader_install(warn, sizeof(warn)) == 0) str_copy(msg, msg_sz, warn);
            else str_copy(msg, msg_sz, warn);
            break;
        case 4:
            if (prompt_line("YouTube URL para download: ", buf, sizeof(buf)) != 0 || !buf[0]) {
                str_copy(msg, msg_sz, "URL nao informada.");
                break;
            }
            if (!downloader_is_url(buf)) {
                str_copy(msg, msg_sz, "URL invalida.");
                break;
            }
            if (downloader_fetch_audio(buf, ".", warn, sizeof(warn)) == 0) {
                str_copy(opts->input, sizeof(opts->input), ".");
                preview_load(opts, pv);
                str_copy(msg, msg_sz, "Download concluido no diretorio atual.");
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
                opts->convert_mp3 = 1;
                opts->fix_tags = 1;
                opts->strip_art = 1;
                opts->limit_name = 1;
                opts->prefix = 1;
                if (opts->organize == ORG_NONE) opts->organize = ORG_ARTIST;
            }
            str_copy(msg, msg_sz, opts->car_safe ? "Car-safe ON" : "Car-safe OFF");
            break;
        case 7:
            opts->dedupe = !opts->dedupe;
            str_copy(msg, msg_sz, opts->dedupe ? "Dedupe ON" : "Dedupe OFF");
            break;
        case 8:
            opts->fix_tags = !opts->fix_tags;
            str_copy(msg, msg_sz, opts->fix_tags ? "Fix tags ON" : "Fix tags OFF");
            break;
        case 9:
            opts->simulate = SIM_FILENAME;
            str_copy(msg, msg_sz, "Simulate filename ON");
            break;
        case 10:
            opts->organize = (opts->organize == ORG_GENRE_ARTIST) ? ORG_ARTIST : ORG_GENRE_ARTIST;
            str_copy(msg, msg_sz, opts->organize == ORG_GENRE_ARTIST ? "Organize Genre/Artist ON" : "Organize Artist ON");
            break;
        case 11:
            if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), ".");
            return 1;
        case 12:
            return -1;
        default:
            break;
    }
    return 0;
}

static int tui_run_fallback(CliOptions *opts) {
    char cmd[64], buf[CARTAG_PATH_MAX], msg[128];
    PreviewState pv;
    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "fallback mode");

    for (;;) {
        printf("\n[CARTAG fallback] input=%s export=%s msg=%s\n",
               opts->input[0] ? opts->input : "(not set)",
               opts->export_path[0] ? opts->export_path : "(not set)",
               msg);
        printf("d=input e=export y=url i=install l=list r=run q=quit > ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (cmd[0] == 'q') break;
        if (cmd[0] == 'd') {
            printf("Input: ");
            if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->input, sizeof(opts->input), buf); }
        } else if (cmd[0] == 'e') {
            printf("Export: ");
            if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->export_path, sizeof(opts->export_path), buf); }
        } else if (cmd[0] == 'y') {
            printf("URL: ");
            if (fgets(buf, sizeof(buf), stdin)) { buf[strcspn(buf, "\r\n")] = '\0'; str_copy(opts->input, sizeof(opts->input), buf); }
        } else if (cmd[0] == 'i') {
            do_utility(3, opts, &pv, msg, sizeof(msg));
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
    UiFocus focus = FOCUS_UTILS;
    UiTab tab_sel = TAB_FILE;
    int util_sel = 0;
    int util_scroll = 0;
    char msg[128];

    if (!isatty(0)) return tui_run_fallback(opts);

    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "Use TAB/setas/ENTER. UI com ncurses.");

    initscr();
    keypad(stdscr, TRUE);
    cbreak();
    noecho();
    curs_set(0);
    init_colors_if_possible();

    preview_load(opts, &pv);

    for (;;) {
        int ch;
        int rc;

        draw_ui(opts, &pv, focus, util_sel, util_scroll, tab_sel, msg);
        ch = getch();

        if (ch == 27) {
            endwin();
            preview_free(&pv);
            return -1;
        }
        if (ch == '\t') {
            focus = (focus == FOCUS_FILES) ? FOCUS_UTILS : FOCUS_FILES;
            continue;
        }
        if (ch == KEY_LEFT) {
            tab_sel = (tab_sel == 0) ? (TAB_COUNT - 1) : (UiTab)(tab_sel - 1);
            continue;
        }
        if (ch == KEY_RIGHT) {
            tab_sel = (UiTab)((tab_sel + 1) % TAB_COUNT);
            continue;
        }

        if (focus == FOCUS_FILES) {
            if (ch == KEY_UP && pv.selected > 0) pv.selected--;
            if (ch == KEY_DOWN && pv.selected + 1 < pv.list.count) pv.selected++;
            if (pv.selected < pv.scroll) pv.scroll = pv.selected;
            if (pv.selected >= pv.scroll + 8) pv.scroll = pv.selected - 7;
            if (ch == 'r' || ch == 'R') {
                rc = do_utility(11, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
                    endwin();
                    preview_free(&pv);
                    return 0;
                }
            }
        } else {
            if (ch == KEY_UP && util_sel > 0) util_sel--;
            if (ch == KEY_DOWN && util_sel + 1 < UTIL_COUNT) util_sel++;
            if (util_sel < util_scroll) util_scroll = util_sel;
            if (util_sel >= util_scroll + 8) util_scroll = util_sel - 7;
            if (ch == 10 || ch == KEY_ENTER || ch == 'r' || ch == 'R' || ch == 'q' || ch == 'Q') {
                if (ch == 'r' || ch == 'R') util_sel = 11;
                if (ch == 'q' || ch == 'Q') util_sel = 12;
                rc = do_utility(util_sel, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
                    endwin();
                    preview_free(&pv);
                    return 0;
                }
                if (rc < 0) {
                    endwin();
                    preview_free(&pv);
                    return -1;
                }
            }
        }
    }
}

#else
static void win_str_copy(char *dst, size_t dst_sz, const char *src) {
    size_t n;
    if (!dst || dst_sz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int tui_run(CliOptions *opts) {
    char cmd[64];
    char buf[CARTAG_PATH_MAX];
    char warn[256];

    for (;;) {
        printf("\n=== CARTAG DOS UI (Windows fallback) ===\n");
        printf("input=%s | export=%s\n",
               opts->input[0] ? opts->input : "(not set)",
               opts->export_path[0] ? opts->export_path : "(not set)");
        printf("tabs: [F]ile [O]ptions [V]iew [T]ree [H]elp\n");
        printf("utils: d=input e=export y=url i=install-yt l=list g=genre-artist r=run q=quit\n> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) return -1;

        if (cmd[0] == 'q' || cmd[0] == 'Q') return -1;
        if (cmd[0] == 'd' || cmd[0] == 'D') {
            printf("Input path: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0]) win_str_copy(opts->input, sizeof(opts->input), buf);
            }
        } else if (cmd[0] == 'e' || cmd[0] == 'E') {
            printf("Export path: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0]) win_str_copy(opts->export_path, sizeof(opts->export_path), buf);
            }
        } else if (cmd[0] == 'y' || cmd[0] == 'Y') {
            printf("YouTube URL: ");
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0]) win_str_copy(opts->input, sizeof(opts->input), buf);
            }
        } else if (cmd[0] == 'i' || cmd[0] == 'I') {
            if (downloader_install(warn, sizeof(warn)) == 0) printf("[OK] %s\n", warn);
            else printf("[WARN] %s\n", warn);
        } else if (cmd[0] == 'l' || cmd[0] == 'L') {
            TrackList list;
            memset(&list, 0, sizeof(list));
            if (opts->input[0] == '\0') win_str_copy(opts->input, sizeof(opts->input), ".");
            if (fs_scan_audio(opts->input, &list) == 0) {
                printf("Eligible tracks: %zu\n", list.count);
            } else {
                printf("Falha ao listar: %s\n", opts->input);
            }
            free(list.tracks);
        } else if (cmd[0] == 'g' || cmd[0] == 'G') {
            opts->organize = (opts->organize == ORG_GENRE_ARTIST) ? ORG_ARTIST : ORG_GENRE_ARTIST;
            printf("Organize mode: %s\n", opts->organize == ORG_GENRE_ARTIST ? "genre/artist" : "artist");
        } else if (cmd[0] == 'r' || cmd[0] == 'R') {
            if (opts->input[0] == '\0') win_str_copy(opts->input, sizeof(opts->input), ".");
            return 0;
        }
    }
}
#endif
