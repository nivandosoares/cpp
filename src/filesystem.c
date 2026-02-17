#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static uint64_t hash_file_quick(const char *path) {
    FILE *f = fopen(path, "rb");
    uint64_t h = 1469598103934665603ULL;
    unsigned char buf[4096];
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf), f);
    for (size_t i = 0; i < n; ++i) {
        h ^= buf[i];
        h *= 1099511628211ULL;
    }
    if (fseek(f, -((long)sizeof(buf)), SEEK_END) == 0) {
        n = fread(buf, 1, sizeof(buf), f);
        for (size_t i = 0; i < n; ++i) {
            h ^= buf[i];
            h *= 1099511628211ULL;
        }
    }
    fclose(f);
    return h;
}

static int is_audio_ext(const char *name) {
    AudioFormat f = audio_detect_format(name);
    return f != FORMAT_UNKNOWN;
}

static int tracklist_push(TrackList *list, const AudioTrack *track) {
    AudioTrack *new_mem;
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 512;
        new_mem = (AudioTrack *)realloc(list->tracks, new_cap * sizeof(AudioTrack));
        if (!new_mem) return -1;
        list->tracks = new_mem;
        list->capacity = new_cap;
    }
    list->tracks[list->count++] = *track;
    return 0;
}

static int scan_recursive(const char *root, const char *base, TrackList *list, int depth) {
    DIR *dir;
    struct dirent *ent;
    char full[CARTAG_PATH_MAX];
    char rel[CARTAG_PATH_MAX];

    if (depth > 16) return 0;

    dir = opendir(root);
    if (!dir) return -1;

    while ((ent = readdir(dir)) != NULL) {
        struct stat st;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", root, ent->d_name);
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_recursive(full, base, list, depth + 1);
        } else if (S_ISREG(st.st_mode) && is_audio_ext(ent->d_name)) {
            AudioTrack t;
            memset(&t, 0, sizeof(t));
            snprintf(t.path, sizeof(t.path), "%s", full);
            if (strncmp(full, base, strlen(base)) == 0) {
                snprintf(rel, sizeof(rel), "%s", full + strlen(base) + 1);
            } else {
                snprintf(rel, sizeof(rel), "%s", ent->d_name);
            }
            snprintf(t.rel_path, sizeof(t.rel_path), "%s", rel);
            snprintf(t.filename, sizeof(t.filename), "%s", ent->d_name);
            t.format = audio_detect_format(ent->d_name);
            t.size_bytes = (uint64_t)st.st_size;
            t.quick_hash = hash_file_quick(full);
            tags_fix_from_filename(&t);
            tags_standardize(&t);
            tracklist_push(list, &t);
        }
    }

    closedir(dir);
    return 0;
}

int fs_scan_audio(const char *root, TrackList *list) {
    memset(list, 0, sizeof(*list));
    return scan_recursive(root, root, list, 0);
}

int fs_ensure_directory(const char *path) {
    char tmp[CARTAG_PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = '\0';
            MKDIR(tmp);
            tmp[i] = c;
        }
    }
    return MKDIR(tmp);
}

int fs_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;
    char parent[CARTAG_PATH_MAX];

    if (!in) return -1;

    snprintf(parent, sizeof(parent), "%s", dst);
    for (int i = (int)strlen(parent) - 1; i >= 0; --i) {
        if (parent[i] == '/' || parent[i] == '\\') {
            parent[i] = '\0';
            fs_ensure_directory(parent);
            break;
        }
    }

    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}
