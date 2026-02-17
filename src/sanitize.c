#include "cartag.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char fold_utf8_c3(unsigned char next) {
    switch (next) {
        case 0xA1: case 0xA0: case 0xA2: case 0xA3: case 0xA4: return 'a';
        case 0x81: case 0x80: case 0x82: case 0x83: case 0x84: return 'A';
        case 0xA9: case 0xA8: case 0xAA: case 0xAB: return 'e';
        case 0x89: case 0x88: case 0x8A: case 0x8B: return 'E';
        case 0xAD: case 0xAC: case 0xAE: case 0xAF: return 'i';
        case 0x8D: case 0x8C: case 0x8E: case 0x8F: return 'I';
        case 0xB3: case 0xB2: case 0xB4: case 0xB5: case 0xB6: return 'o';
        case 0x93: case 0x92: case 0x94: case 0x95: case 0x96: return 'O';
        case 0xBA: case 0xB9: case 0xBB: case 0xBC: return 'u';
        case 0x9A: case 0x99: case 0x9B: case 0x9C: return 'U';
        case 0xA7: return 'c';
        case 0x87: return 'C';
        case 0xB1: return 'n';
        case 0x91: return 'N';
        default: return 0;
    }
}

static void str_remove_pattern(char *s, const char *pat) {
    char *p;
    while ((p = strstr(s, pat)) != NULL) {
        memmove(p, p + strlen(pat), strlen(p + strlen(pat)) + 1);
    }
}

void sanitize_filename(char *name, size_t max_len) {
    char out[CARTAG_NAME_MAX];
    size_t j = 0;

    str_remove_pattern(name, "(Official Video)");
    str_remove_pattern(name, "[HD]");
    str_remove_pattern(name, "(Visualizer)");

    for (size_t i = 0; name[i] && j + 1 < sizeof(out); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c == 0xC3 && name[i + 1]) {
            char f = fold_utf8_c3((unsigned char)name[i + 1]);
            if (f) out[j++] = f;
            ++i;
            continue;
        }
        if (c >= 128) continue;
        if (c < 32) continue;
        if (strchr("<>:\\|?*\"", c)) c = '_';
        if (c == '(' && strstr(name + i, "(ft.")) {
            while (name[i] && name[i] != ')') ++i;
            continue;
        }
        if ((c == 'h' || c == 'H') && (strncmp(name + i, "http", 4) == 0 || strncmp(name + i, "HTTP", 4) == 0)) break;
        if (isalnum(c) || strchr(" .-_[]()", c)) out[j++] = (char)c;
    }

    out[j] = '\0';
    while (j > 0 && isspace((unsigned char)out[j - 1])) out[--j] = '\0';
    if (max_len > 0 && j >= max_len) out[max_len - 1] = '\0';
    snprintf(name, max_len, "%s", out);
}

void sanitize_track(AudioTrack *t, int limit_name) {
    sanitize_filename(t->filename, sizeof(t->filename));
    sanitize_filename(t->artist, sizeof(t->artist));
    sanitize_filename(t->album, sizeof(t->album));
    sanitize_filename(t->title, sizeof(t->title));
    if (limit_name && strlen(t->filename) > 64) t->filename[64] = '\0';
}
