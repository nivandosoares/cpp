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
        return 0;
    }

    fs_ensure_directory(".cartag");
    fs_ensure_directory(".cartag/bin");

#ifdef _WIN32
    snprintf(exe, exe_sz, ".cartag\\bin\\yt-dlp.exe");
    if (ACCESS(exe, 0) == 0) return 0;
    if (!cmd_ok("powershell -Command \"Invoke-WebRequest -Uri https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe -OutFile .cartag\\bin\\yt-dlp.exe\"")) {
        snprintf(warn, warn_sz, "nao foi possivel baixar yt-dlp automaticamente");
        return -1;
    }
#else
    snprintf(exe, exe_sz, "./.cartag/bin/yt-dlp");
    if (ACCESS(exe, X_OK) == 0) return 0;
    if (!cmd_ok("curl -L --fail https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o .cartag/bin/yt-dlp > /dev/null 2>&1") &&
        !cmd_ok("wget -q https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -O .cartag/bin/yt-dlp") &&
        !cmd_ok("python3 -c \"import urllib.request as r;u='https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp';open('.cartag/bin/yt-dlp','wb').write(r.urlopen(u,timeout=30).read())\" > /dev/null 2>&1")) {
        snprintf(warn, warn_sz, "nao foi possivel baixar yt-dlp automaticamente");
        return -1;
    }
    cmd_ok("chmod +x .cartag/bin/yt-dlp");
#endif

    snprintf(warn, warn_sz, "yt-dlp baixado automaticamente");
    return 0;
}

int downloader_fetch_audio(const char *url, const char *out_dir, char *warn, size_t warn_sz) {
    char exe[CARTAG_PATH_MAX];
    char cmd[4096];

    if (ensure_ytdlp(exe, sizeof(exe), warn, warn_sz) != 0) {
        return -1;
    }

    fs_ensure_directory(out_dir);
    snprintf(cmd, sizeof(cmd),
             "%s -x --audio-format mp3 --audio-quality 0 --embed-metadata --no-playlist -o \"%s/%%(title)s.%%(ext)s\" \"%s\" > /dev/null 2>&1",
             exe,
             out_dir,
             url);

    if (system(cmd) != 0) {
        snprintf(warn, warn_sz, "falha ao baixar audio com yt-dlp");
        return -1;
    }

    snprintf(warn, warn_sz, "download concluido via yt-dlp");
    return 0;
}
