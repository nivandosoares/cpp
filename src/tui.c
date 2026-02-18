#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <ncurses.h>
#endif

#ifndef _WIN32

typedef struct {
    TrackList list;
    size_t selected;
    size_t scroll;
} PreviewState;

typedef enum { FOCUS_FILES = 0, FOCUS_UTILS = 1 } UiFocus;
typedef enum { TAB_FILE = 0, TAB_OPTIONS, TAB_VIEW, TAB_TREE, TAB_HELP, TAB_COUNT } UiTab;

typedef enum {
    ACT_SET_INPUT = 0,
    ACT_SET_EXPORT,
    ACT_SET_URL,
    ACT_INSTALL_YTDLP,
    ACT_DOWNLOAD_URL,
    ACT_REFRESH_LIST,
    ACT_TOGGLE_CARSAFE,
    ACT_TOGGLE_DEDUPE,
    ACT_TOGGLE_FIXTAGS,
    ACT_SIM_FILENAME,
    ACT_ORG_TOGGLE_GENRE_ARTIST,
    ACT_ORG_ARTIST,
    ACT_ORG_ALBUM,
    ACT_ORG_FLAT,
    ACT_RUN_PIPELINE,
    ACT_QUIT,
    ACT_COUNT
} ActionId;

static const char *k_tabs[TAB_COUNT] = {"File", "Options", "Actions", "Tree", "Help"};
static const char *k_submenus[TAB_COUNT] = {
    "Open  Export  Refresh  Run  Quit",
    "Car-Safe  Dedupe  Fix-Tags  Organize",
    "Simulate  Refresh  Run",
    "Artist  Album  Flat  Genre/Artist  Refresh",
    "URL  Install yt-dlp  Download"
};

static const char *k_action_labels[ACT_COUNT] = {
    "Set Input Path/Drive (DIR)",
    "Set Export Path",
    "Set YouTube URL as Input",
    "Install yt-dlp (curl)",
    "Download YouTube URL now",
    "Refresh Eligible List",
    "Toggle Car-Safe",
    "Toggle Dedupe",
    "Toggle Fix Tags",
    "Simulate Filename",
    "Organize: Toggle Genre/Artist",
    "Organize: Artist",
    "Organize: Album",
    "Organize: Flat",
    "Run Pipeline",
    "Quit"
};

static const char *k_focus_name[2] = {"FILES", "ACTIONS"};

static const ActionId k_tab_actions[TAB_COUNT][8] = {
    {ACT_SET_INPUT, ACT_SET_EXPORT, ACT_REFRESH_LIST, ACT_RUN_PIPELINE, ACT_QUIT, ACT_COUNT, ACT_COUNT, ACT_COUNT},
    {ACT_TOGGLE_CARSAFE, ACT_TOGGLE_DEDUPE, ACT_TOGGLE_FIXTAGS, ACT_ORG_TOGGLE_GENRE_ARTIST, ACT_ORG_ARTIST, ACT_ORG_ALBUM, ACT_ORG_FLAT, ACT_COUNT},
    {ACT_SIM_FILENAME, ACT_REFRESH_LIST, ACT_RUN_PIPELINE, ACT_COUNT, ACT_COUNT, ACT_COUNT, ACT_COUNT, ACT_COUNT},
    {ACT_ORG_ARTIST, ACT_ORG_ALBUM, ACT_ORG_FLAT, ACT_ORG_TOGGLE_GENRE_ARTIST, ACT_REFRESH_LIST, ACT_COUNT, ACT_COUNT, ACT_COUNT},
    {ACT_SET_URL, ACT_INSTALL_YTDLP, ACT_DOWNLOAD_URL, ACT_COUNT, ACT_COUNT, ACT_COUNT, ACT_COUNT, ACT_COUNT}
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
    pv->selected = 0;
    pv->scroll = 0;
}

static void init_colors_if_possible(void) {
    if (!has_colors()) return;
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_CYAN);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
    init_pair(5, COLOR_YELLOW, -1);
}

static int prompt_line(const char *label, char *out, size_t out_sz) {
    int y = LINES - 2;
    int rc;
    move(y, 0);
    clrtoeol();
    attron(COLOR_PAIR(3));
    mvprintw(y, 0, "%s", label);
    attroff(COLOR_PAIR(3));
    echo();
    curs_set(1);
    rc = getnstr(out, (int)out_sz - 1);
    noecho();
    curs_set(0);
    return rc == ERR ? -1 : 0;
}

static int get_tab_item_count(UiTab tab_sel) {
    int i;
    for (i = 0; i < 8; ++i) {
        if (k_tab_actions[(int)tab_sel][i] == ACT_COUNT) return i;
    }
    return 8;
}

static ActionId get_tab_action(UiTab tab_sel, int index) {
    return k_tab_actions[(int)tab_sel][index];
}

static const char *organize_name(OrganizeMode m) {
    switch (m) {
        case ORG_ARTIST: return "artist";
        case ORG_ALBUM: return "album";
        case ORG_FLAT: return "flat";
        case ORG_GENRE_ARTIST: return "genre/artist";
        default: return "none";
    }
}

static void draw_ui(const CliOptions *opts,
                    const PreviewState *pv,
                    UiFocus focus,
                    UiTab tab_sel,
                    int util_sel,
                    const char *msg) {
    int w = COLS;
    int h = LINES;
    int files_top = 4;
    int files_h = (h >= 28) ? 12 : 9;
    int util_top = files_top + files_h;
    int util_h = h - util_top - 2;
    int left_w = w / 2;
    int right_w = w - left_w;
    int file_rows = files_h - 3;
    int util_rows = util_h - 2;
    int util_count = get_tab_item_count(tab_sel);
    int i;
    char line[256];
    char timebuf[16];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);

    erase();

    attron(COLOR_PAIR(1));
    mvhline(0, 0, ' ', w);
    mvprintw(0, 1, "%s  %s  %s  %s  %s",
             tab_sel == TAB_FILE ? "[File]" : k_tabs[TAB_FILE],
             tab_sel == TAB_OPTIONS ? "[Options]" : k_tabs[TAB_OPTIONS],
             tab_sel == TAB_VIEW ? "[Actions]" : k_tabs[TAB_VIEW],
             tab_sel == TAB_TREE ? "[Tree]" : k_tabs[TAB_TREE],
             tab_sel == TAB_HELP ? "[Help]" : k_tabs[TAB_HELP]);
    mvprintw(0, w - 16, "MS-DOS Shell");
    attroff(COLOR_PAIR(1));

    attron(COLOR_PAIR(2));
    mvhline(1, 0, ' ', w);
    mvprintw(1, 1, "Drives: [A:] [B:] [C:] [D:] [E:] [F:] [G:]");
    mvprintw(1, w - 11, "Cartag UI");
    mvhline(2, 0, ' ', w);
    mvprintw(2, 1, "%s", k_submenus[(int)tab_sel]);
    attroff(COLOR_PAIR(2));

    mvhline(3, 0, ACS_HLINE, w);
    mvprintw(3, 2, "Directory Tree");
    snprintf(line, sizeof(line), "input=%-.21s export=%-.14s org=%s focus=%s",
             opts->input[0] ? opts->input : "(not set)",
             opts->export_path[0] ? opts->export_path : "(not set)",
             organize_name(opts->organize),
             k_focus_name[(int)focus]);
    mvprintw(3, left_w + 2, "%s", line);

    mvaddch(files_top, 0, ACS_ULCORNER);
    mvhline(files_top, 1, ACS_HLINE, left_w - 1);
    mvaddch(files_top, left_w, ACS_TTEE);
    mvhline(files_top, left_w + 1, ACS_HLINE, right_w - 2);
    mvaddch(files_top, w - 1, ACS_URCORNER);

    for (i = 1; i < files_h - 1; ++i) {
        mvaddch(files_top + i, 0, ACS_VLINE);
        mvaddch(files_top + i, left_w, ACS_VLINE);
        mvaddch(files_top + i, w - 1, ACS_VLINE);
    }

    mvaddch(files_top + files_h - 1, 0, ACS_LLCORNER);
    mvhline(files_top + files_h - 1, 1, ACS_HLINE, left_w - 1);
    mvaddch(files_top + files_h - 1, left_w, ACS_BTEE);
    mvhline(files_top + files_h - 1, left_w + 1, ACS_HLINE, right_w - 2);
    mvaddch(files_top + files_h - 1, w - 1, ACS_LRCORNER);

    mvprintw(files_top + 1, 2, "> C:\\");
    mvprintw(files_top + 2, 4, "%s", opts->organize == ORG_GENRE_ARTIST ? "GENRE" : "ARTIST");
    mvprintw(files_top + 3, 4, "ALBUM");

    for (i = 0; i < file_rows; ++i) {
        size_t idx = pv->scroll + (size_t)i;
        int y = files_top + 1 + i;
        if (idx < pv->list.count) {
            const AudioTrack *t = &pv->list.tracks[idx];
            snprintf(line, sizeof(line), "%c %03zu %-24.24s %-4s %8llu",
                     idx == pv->selected ? '>' : ' ', idx + 1, t->filename,
                     audio_format_name(t->format), (unsigned long long)t->size_bytes);
            if (focus == FOCUS_FILES && idx == pv->selected) attron(A_REVERSE);
            mvprintw(y, left_w + 2, "%-*.*s", right_w - 4, right_w - 4, line);
            if (focus == FOCUS_FILES && idx == pv->selected) attroff(A_REVERSE);
        }
    }

    mvaddch(util_top, 0, ACS_ULCORNER);
    mvhline(util_top, 1, ACS_HLINE, w - 2);
    mvaddch(util_top, w - 1, ACS_URCORNER);
    mvprintw(util_top, 2, "Command Menu");

    for (i = 1; i < util_h - 1; ++i) {
        mvaddch(util_top + i, 0, ACS_VLINE);
        mvaddch(util_top + i, w - 1, ACS_VLINE);
    }

    mvaddch(util_top + util_h - 1, 0, ACS_LLCORNER);
    mvhline(util_top + util_h - 1, 1, ACS_HLINE, w - 2);
    mvaddch(util_top + util_h - 1, w - 1, ACS_LRCORNER);

    for (i = 0; i < util_rows && i < util_count; ++i) {
        ActionId aid = get_tab_action(tab_sel, i);
        int y = util_top + 1 + i;
        if (focus == FOCUS_UTILS && i == util_sel) attron(A_REVERSE);
        mvprintw(y, 2, "%c %-*.*s", i == util_sel ? '>' : ' ', w - 6, w - 6, k_action_labels[(int)aid]);
        if (focus == FOCUS_UTILS && i == util_sel) attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(2));
    mvhline(h - 1, 0, ' ', w);
    if (tmv) strftime(timebuf, sizeof(timebuf), "%H:%M", tmv); else str_copy(timebuf, sizeof(timebuf), "--:--");
    mvprintw(h - 1, 1, "F1=Help TAB=Switch <-/->=Tabs ENTER=Exec R=Run Q=Quit ESC=Exit");
    mvprintw(h - 1, w - 6, "%5s", timebuf);
    attroff(COLOR_PAIR(2));

    attron(COLOR_PAIR(5));
    mvhline(h - 2, 0, ' ', w);
    mvprintw(h - 2, 0, "C:\\CARTAG> %-*.*s", w - 12, w - 12, msg ? msg : "");
    attroff(COLOR_PAIR(5));

    refresh();
}

static int execute_action(ActionId aid, CliOptions *opts, PreviewState *pv, char *msg, size_t msg_sz) {
    char buf[CARTAG_PATH_MAX];
    char warn[256];

    switch (aid) {
        case ACT_SET_INPUT:
            if (prompt_line("Input path/drive: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->input, sizeof(opts->input), buf);
                preview_load(opts, pv);
                str_copy(msg, msg_sz, "Input atualizado.");
            }
            break;
        case ACT_SET_EXPORT:
            if (prompt_line("Export path: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->export_path, sizeof(opts->export_path), buf);
                str_copy(msg, msg_sz, "Export atualizado.");
            }
            break;
        case ACT_SET_URL:
            if (prompt_line("YouTube URL: ", buf, sizeof(buf)) == 0 && buf[0]) {
                str_copy(opts->input, sizeof(opts->input), buf);
                str_copy(msg, msg_sz, "URL setada como input.");
            }
            break;
        case ACT_INSTALL_YTDLP:
            downloader_install(warn, sizeof(warn));
            str_copy(msg, msg_sz, warn);
            break;
        case ACT_DOWNLOAD_URL:
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
        case ACT_REFRESH_LIST:
            preview_load(opts, pv);
            str_copy(msg, msg_sz, "Lista atualizada.");
            break;
        case ACT_TOGGLE_CARSAFE:
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
        case ACT_TOGGLE_DEDUPE:
            opts->dedupe = !opts->dedupe;
            str_copy(msg, msg_sz, opts->dedupe ? "Dedupe ON" : "Dedupe OFF");
            break;
        case ACT_TOGGLE_FIXTAGS:
            opts->fix_tags = !opts->fix_tags;
            str_copy(msg, msg_sz, opts->fix_tags ? "Fix tags ON" : "Fix tags OFF");
            break;
        case ACT_SIM_FILENAME:
            opts->simulate = SIM_FILENAME;
            str_copy(msg, msg_sz, "Simulate filename ON");
            break;
        case ACT_ORG_TOGGLE_GENRE_ARTIST:
            opts->organize = (opts->organize == ORG_GENRE_ARTIST) ? ORG_ARTIST : ORG_GENRE_ARTIST;
            str_copy(msg, msg_sz, opts->organize == ORG_GENRE_ARTIST ? "Organize Genre/Artist ON" : "Organize Artist ON");
            break;
        case ACT_ORG_ARTIST:
            opts->organize = ORG_ARTIST;
            str_copy(msg, msg_sz, "Organize artist selecionado");
            break;
        case ACT_ORG_ALBUM:
            opts->organize = ORG_ALBUM;
            str_copy(msg, msg_sz, "Organize album selecionado");
            break;
        case ACT_ORG_FLAT:
            opts->organize = ORG_FLAT;
            str_copy(msg, msg_sz, "Organize flat selecionado");
            break;
        case ACT_RUN_PIPELINE:
            if (opts->input[0] == '\0') str_copy(opts->input, sizeof(opts->input), ".");
            return 1;
        case ACT_QUIT:
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
    str_copy(msg, sizeof(msg), "DOS fallback mode");

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
            execute_action(ACT_INSTALL_YTDLP, opts, &pv, msg, sizeof(msg));
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
    char msg[128];

    if (!isatty(0)) return tui_run_fallback(opts);

    memset(&pv, 0, sizeof(pv));
    str_copy(msg, sizeof(msg), "DOSSHELL: TAB troca foco, <-/-> troca menu, ENTER executa.");

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
        int util_count;

        util_count = get_tab_item_count(tab_sel);
        if (util_sel >= util_count) util_sel = util_count > 0 ? util_count - 1 : 0;

        draw_ui(opts, &pv, focus, tab_sel, util_sel, msg);
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
                rc = execute_action(ACT_RUN_PIPELINE, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
                    endwin();
                    preview_free(&pv);
                    return 0;
                }
            }
        } else {
            if (ch == KEY_UP && util_sel > 0) util_sel--;
            if (ch == KEY_DOWN && util_sel + 1 < util_count) util_sel++;
            if (ch == 10 || ch == KEY_ENTER) {
                ActionId aid = get_tab_action(tab_sel, util_sel);
                rc = execute_action(aid, opts, &pv, msg, sizeof(msg));
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
            if (ch == 'q' || ch == 'Q') {
                endwin();
                preview_free(&pv);
                return -1;
            }
            if (ch == 'r' || ch == 'R') {
                rc = execute_action(ACT_RUN_PIPELINE, opts, &pv, msg, sizeof(msg));
                if (rc == 1) {
                    endwin();
                    preview_free(&pv);
                    return 0;
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
