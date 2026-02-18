#include "cartag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define ACCESS _access
#else
#include <unistd.h>
#define ACCESS access
#endif

static int cmd_ok(const char *cmd) {
    return system(cmd) == 0;
}

int downloader_is_url(const char *s) {
    if (!s) return 0;
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

static int ensure_ytdlp(char *exe, size_t exe_sz, char *warn, size_t warn_sz) {
    if (cmd_ok("yt-dlp --version > /dev/null 2>&1")) {
        snprintf(exe, exe_sz, "yt-dlp");
        snprintf(warn, warn_sz, "yt-dlp encontrado no sistema");
        return 0;
    }

#ifdef _WIN32
    snprintf(exe, exe_sz, "./yt-dlp.exe");
    if (ACCESS(exe, 0) == 0) {
        snprintf(warn, warn_sz, "yt-dlp local ja disponivel");
        return 0;
    }
    if (!cmd_ok("curl -L --fail https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe -o yt-dlp.exe > NUL 2>&1")) {
        snprintf(warn, warn_sz, "falha ao baixar yt-dlp.exe via curl");
        return -1;
    }
#else
    snprintf(exe, exe_sz, "./yt-dlp");
    if (ACCESS(exe, X_OK) == 0) {
        snprintf(warn, warn_sz, "yt-dlp local ja disponivel");
        return 0;
    }
    if (!cmd_ok("curl -L --fail https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o yt-dlp > /dev/null 2>&1")) {
        snprintf(warn, warn_sz, "falha ao baixar yt-dlp via curl");
        return -1;
    }
    cmd_ok("chmod +x yt-dlp");
#endif

    snprintf(warn, warn_sz, "yt-dlp instalado no diretorio atual");
    return 0;
}

int downloader_fetch_audio(const char *url, const char *out_dir, char *warn, size_t warn_sz) {
    char exe[CARTAG_PATH_MAX];
    char cmd[4096];

    if (!downloader_is_url(url)) {
        snprintf(warn, warn_sz, "URL invalida");
        return -1;
    }

    if (ensure_ytdlp(exe, sizeof(exe), warn, warn_sz) != 0) {
        return -1;
    }

    fs_ensure_directory(out_dir);
    snprintf(cmd, sizeof(cmd),
             "%s -x --audio-format mp3 --audio-quality 0 --embed-metadata --no-playlist -o \"%s/%%(title)s.%%(ext)s\" \"%s\"",
             exe,
             out_dir,
             url);

    if (system(cmd) != 0) {
        snprintf(warn, warn_sz, "falha ao baixar audio com yt-dlp");
        return -1;
    }

    snprintf(warn, warn_sz, "download concluido no diretorio atual");
    return 0;
}

int downloader_install(char *warn, size_t warn_sz) {
    char exe[CARTAG_PATH_MAX];
    return ensure_ytdlp(exe, sizeof(exe), warn, warn_sz);
}
