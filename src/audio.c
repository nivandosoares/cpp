#include "cartag.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

static int has_ffmpeg(void) {
    int rc = system("ffmpeg -version > /dev/null 2>&1");
    return rc == 0;
}

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

AudioFormat audio_detect_format(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return FORMAT_UNKNOWN;
    ++dot;
    if (strcasecmp(dot, "mp3") == 0) return FORMAT_MP3;
    if (strcasecmp(dot, "flac") == 0) return FORMAT_FLAC;
    if (strcasecmp(dot, "wav") == 0) return FORMAT_WAV;
    if (strcasecmp(dot, "aac") == 0) return FORMAT_AAC;
    if (strcasecmp(dot, "m4a") == 0) return FORMAT_M4A;
    if (strcasecmp(dot, "ogg") == 0) return FORMAT_OGG;
    if (strcasecmp(dot, "wma") == 0) return FORMAT_WMA;
    return FORMAT_UNKNOWN;
}

const char *audio_format_name(AudioFormat fmt) {
    switch (fmt) {
        case FORMAT_MP3: return "MP3";
        case FORMAT_FLAC: return "FLAC";
        case FORMAT_WAV: return "WAV";
        case FORMAT_AAC: return "AAC";
        case FORMAT_M4A: return "M4A";
        case FORMAT_OGG: return "OGG";
        case FORMAT_WMA: return "WMA";
        default: return "UNKNOWN";
    }
}

int audio_can_play_car(AudioTrack *t, int car_safe, char *warn, size_t warn_sz) {
    if (car_safe && t->format != FORMAT_MP3) {
        snprintf(warn, warn_sz, "Formato %s pode falhar em player antigo", audio_format_name(t->format));
        return 0;
    }
    warn[0] = '\0';
    return 1;
}

int audio_convert_if_needed(AudioTrack *t, const CliOptions *opts, char *warn, size_t warn_sz) {
    char cmd[4096];
    char out[CARTAG_PATH_MAX];
    size_t path_len;

    if (!(opts->convert_mp3 || opts->car_safe)) return 0;
    if (t->format == FORMAT_MP3 && opts->keep_format) return 0;
    if (!has_ffmpeg()) {
        snprintf(warn, warn_sz, "ffmpeg ausente; conversao ignorada");
        return -1;
    }

    path_len = strlen(t->path);
    if (path_len + sizeof(".converted.mp3") > sizeof(out)) {
        snprintf(warn, warn_sz, "caminho muito longo para conversao");
        return -1;
    }

    str_copy(out, sizeof(out), t->path);
    strncat(out, ".converted.mp3", sizeof(out) - strlen(out) - 1);

    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -i \"%s\" -vn -ar %d -ac 2 -b:a 320k -id3v2_version 3 \"%s\" > /dev/null 2>&1",
             t->path,
             opts->car_safe ? 44100 : 48000,
             out);
    if (system(cmd) != 0) {
        snprintf(warn, warn_sz, "ffmpeg falhou ao converter");
        return -1;
    }

    str_copy(t->path, sizeof(t->path), out);
    t->format = FORMAT_MP3;
    snprintf(warn, warn_sz, "convertido para MP3");
    return 1;
}
